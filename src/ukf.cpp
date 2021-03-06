#include <iostream>
#include "ukf.h"
using namespace std;

/**
 * Initializes Unscented Kalman filter
 */
UKF::UKF() {
  // if this is false, laser measurements will be ignored (except during init)
  use_laser_ = true;

  // if this is false, radar measurements will be ignored (except during init)
  use_radar_ = true;

  ///* State dimension
  n_x_ = 5; // depends on radar v.s. lidar?

  ///* Augmented state dimension
  n_aug_ = 7; // depends on radar v.s. lidar?

  ///* Sigma point spreading parameter
  lambda_ = 3 - n_aug_; // to check

  // initial state vector
  x_ = VectorXd(n_x_);

  // initial covariance matrix
  P_ = MatrixXd::Identity(n_x_, n_x_);
  P_ = 0.05 * P_;

  // Process noise standard deviation longitudinal acceleration in m/s^2
  std_a_ = 2;

  // Process noise standard deviation yaw acceleration in rad/s^2
  std_yawdd_ = 1.5;

  // Laser measurement noise standard deviation position1 in m
  std_laspx_ = 0.2; //0.15;

  // Laser measurement noise standard deviation position2 in m
  std_laspy_ = 0.15;// 0.15;

  // Radar measurement noise standard deviation radius in m
  std_radr_ = 0.15; //0.3;

  // Radar measurement noise standard deviation angle in rad
  std_radphi_ = 0.02;//0.03;

  // Radar measurement noise standard deviation radius change in m/s
  std_radrd_ = 0.15;//0.3;

  /**
  TODO:

  Complete the initialization. See ukf.h for other member properties.

  Hint: one or more values initialized above might be wildly off...
  */

  ///* initially set to false, set to true in first call of ProcessMeasurement
  is_initialized_ = false;

  ///* predicted sigma points matrix
  Xsig_pred_ = MatrixXd(n_x_, 2 * n_aug_ + 1);

  ///* time when the state is true, in us
  time_us_ = 0;

  ///* Weights of sigma points
  weights_ = VectorXd(2*n_aug_ + 1);
  weights_(0) = lambda_ / (lambda_ + n_aug_);
  for (int i = 1; i < 2*n_aug_ + 1; i++) {
	  weights_(i) = 1 / (2*(lambda_ + n_aug_));
  }

  ///* the current NIS for radar
  NIS_radar_ = 0;

  ///* the current NIS for laser
  NIS_laser_ = 0;
}

UKF::~UKF() {}

/**
 * @param {MeasurementPackage} meas_package The latest measurement data of
 * either radar or laser.
 */
void UKF::ProcessMeasurement(MeasurementPackage meas_package) {
  /**
  TODO:

  Complete this function! Make sure you switch between lidar and radar
  measurements.
  */

   /*****************************************************************************
   *  Initialization
   ****************************************************************************/
  if (!is_initialized_) {
	/**
	TODO:
	  * Initialize the state ekf_.x_ with the first measurement.
	  * Create the covariance matrix.
	  * Remember: you'll need to convert radar from polar to cartesian coordinates.
	*/
	// first measurement

	if (meas_package.sensor_type_ == MeasurementPackage::RADAR) {
	  double rho = meas_package.raw_measurements_[0];
	  double phi = meas_package.raw_measurements_[1];
	  double rho_dot = meas_package.raw_measurements_[2];
	  double vx = rho_dot*cos(phi);
	  double vy = rho_dot*sin(phi);
	  double yaw = atan2(vy, vx);
	  double v = sqrt(vx * vx + vy * vy);

	  //x_ << rho*cos(phi), rho*sin(phi), rho_dot, 0.0, 0.0;
	  x_ << rho*cos(phi), rho*sin(phi), v, yaw, 0.0;
	}
	else if (meas_package.sensor_type_ == MeasurementPackage::LASER) {
	  x_ << meas_package.raw_measurements_[0], meas_package.raw_measurements_[1], 0.0, 0.0, 0.0;
	}

	// done initializing, no need to predict or update
	is_initialized_ = true;
	time_us_ = meas_package.timestamp_;
	return;
  }

  bool use_measurement = (use_laser_ && meas_package.sensor_type_ == MeasurementPackage::LASER) ||
		  (use_radar_ && meas_package.sensor_type_ == MeasurementPackage::RADAR);

  if (use_measurement) {

	  double dt = (meas_package.timestamp_ - time_us_)/ 1000000.0;
	  time_us_ = meas_package.timestamp_;

	  // in sample data 2, dt can be big and the rotation speed can become very big.
	  while (dt > 0.2)
	  {
		Prediction(0.1);
	    dt -= 0.1;
	  }

	  Prediction(dt);

	  if (meas_package.sensor_type_ == MeasurementPackage::RADAR) {
		  UpdateRadar(meas_package);
	  }

	  if (meas_package.sensor_type_ == MeasurementPackage::LASER) {
		  UpdateLidar(meas_package);
	  }

//	  // print
//	  cout << "x_ = " << x_ << endl;
//	  cout << "P_ = " << P_ << endl;

  }

}


MatrixXd UKF::GenerateSigmaPoints(double delta_t) {
	//create augmented mean vector
	VectorXd x_aug = VectorXd(n_aug_);

	//create augmented state covariance
	MatrixXd P_aug = MatrixXd(n_aug_, n_aug_);

	//create sigma point matrix
	MatrixXd Xsig_aug = MatrixXd(n_aug_, 2 * n_aug_ + 1);
	x_aug.head(5) = x_;
	x_aug(5) = 0;
	x_aug(6) = 0;

	//create augmented covariance matrix
	P_aug.fill(0.0);
	P_aug.topLeftCorner(5,5) = P_;
	MatrixXd Q = MatrixXd(2, 2);
	Q << std_a_ * std_a_, 0,
		 0, std_yawdd_ * std_yawdd_;
	P_aug.bottomRightCorner(2,2) = Q;

	//create square root matrix
	MatrixXd A = P_aug.llt().matrixL();

	//create augmented sigma points
	Xsig_aug.col(0)  = x_aug;
	//set remaining sigma points
	for (int i = 0; i < n_aug_; i++) {
      Xsig_aug.col(i + 1)     = x_aug + sqrt(lambda_ + n_aug_) * A.col(i);
	  Xsig_aug.col(i + 1 + n_aug_) = x_aug - sqrt(lambda_ + n_aug_) * A.col(i);
	}
	return Xsig_aug;
}


void UKF::PredictMeanVariance() {
	//predicted state mean
	x_.fill(0.0);
	for (int i = 0; i < 2 * n_aug_ + 1; i++) {  //iterate over sigma points
	  x_ = x_ + weights_(i) * Xsig_pred_.col(i);
	}


	//predicted state covariance matrix
	P_.fill(0.0);
	for (int i = 0; i < 2 * n_aug_ + 1; i++) {  //iterate over sigma points

	  // state difference
      VectorXd x_diff = Xsig_pred_.col(i) - x_;

	  //double signed_pi = std::copysign(M_PI,x_diff(3));
	  //x_diff(3) = std::fmod(x_diff(3) + signed_pi,(2.*M_PI)) - signed_pi;
      //angle normalization
	  while (x_diff(3)> M_PI) {
	    x_diff(3)-=2.*M_PI;
	  }
	  while (x_diff(3)<-M_PI) {
		x_diff(3) += 2.*M_PI;
	  }

	  P_ = P_ + weights_(i) * x_diff * x_diff.transpose() ;
	}
}

void UKF::PredictSigmaPoints(MatrixXd Xsig_aug, double delta_t){
	for (int i = 0; i < 2 * n_aug_ + 1; i++) {
		 double px = Xsig_aug(0,i);
		 double py = Xsig_aug(1,i);
		 double v = Xsig_aug(2,i);
		 double phi = Xsig_aug(3,i);
		 double phipoint = Xsig_aug(4,i);
		 double mu_a = Xsig_aug(5,i);
		 double mu_phipoint = Xsig_aug(6,i);
		 VectorXd x2(5);
		 if (fabs(phipoint) > 0.001) {
			 x2 << px + v / phipoint * (sin(phi + phipoint * delta_t) - sin(phi)) + .5 * (delta_t*delta_t) * cos(phi) * mu_a,
				   py + v / phipoint * (-cos(phi + phipoint * delta_t) + cos(phi)) + .5 * (delta_t*delta_t) * sin(phi) * mu_a,
				   v + delta_t * mu_a,
				   phi + phipoint * delta_t + .5 * (delta_t*delta_t) * mu_phipoint,
				   phipoint + delta_t * mu_phipoint;
		 } else {
			 x2 << px + v * cos(phi) * delta_t + .5 * (delta_t*delta_t) * cos(phi) * mu_a,
				   py + v * sin(phi) * delta_t + .5 * (delta_t*delta_t) * sin(phi) * mu_a,
				   v + delta_t * mu_a,
				   phi + phipoint * delta_t + .5 * (delta_t*delta_t) * mu_phipoint,
				   phipoint + delta_t * mu_phipoint;
		 }
		 Xsig_pred_.col(i) = x2;
	  }
}


/**
 * Predicts sigma points, the state, and the state covariance matrix.
 * @param {double} delta_t the change in time (in seconds) between the last
 * measurement and this one.
 */
void UKF::Prediction(double delta_t) {
  /**
  TODO:

  Complete this function! Estimate the object's location. Modify the state
  vector, x_. Predict sigma points, the state, and the state covariance matrix.
  */

  ////////////////////////////////////////////
  ///           Generate sigma points      ///
  ////////////////////////////////////////////
  MatrixXd Xsig_aug = GenerateSigmaPoints(delta_t);

  /////////////////////////////
  // PREDICTION OF SIGMA POINTS
  /////////////////////////////

  PredictSigmaPoints(Xsig_aug, delta_t);

  //////////////////////////////////
  // PREDICT MEAN AND COVARIANCE
  //////////////////////////////////

  PredictMeanVariance();
}



MatrixXd UKF::SigmaPointsPredMeasurementSpaceRadar(int n_z) {
  //create matrix for sigma points in measurement space
  MatrixXd Zsig = MatrixXd(n_z, 2 * n_aug_ + 1);

  //transform sigma points into measurement space
  for (int i = 0; i < 2 * n_aug_ + 1; i++) {  //2n+1 sigma points
    // extract values for better readability
	double p_x = Xsig_pred_(0,i);
	double p_y = Xsig_pred_(1,i);
	double v  = Xsig_pred_(2,i);
	double yaw = Xsig_pred_(3,i);

	double v1 = cos(yaw)*v;
	double v2 = sin(yaw)*v;

	// measurement model
	Zsig(0,i) = sqrt(p_x*p_x + p_y*p_y);  //r
	Zsig(1,i) = atan2(p_y,p_x);   	      //phi
	Zsig(2,i) = 0;
	if (fabs(Zsig(0,i)) >  0.0001) {
	  Zsig(2,i) = (p_x*v1 + p_y*v2 ) / Zsig(0,i);   //r_dot
	}
  }

  return Zsig;

}

MatrixXd UKF::SigmaPointsPredMeasurementSpaceLidar(int n_z) {
  //create matrix for sigma points in measurement space
  MatrixXd Zsig = MatrixXd(n_z, 2 * n_aug_ + 1);

  //transform sigma points into measurement space
  for (int i = 0; i < 2 * n_aug_ + 1; i++) {  //2n+1 simga points

	// measurement model
	Zsig(0,i) = Xsig_pred_(0,i); // p_x
	Zsig(1,i) = Xsig_pred_(1,i); // p_y
  }
return Zsig;

}

VectorXd UKF::MeanPredictedMeasurement(int n_z, MatrixXd Zsig) {
	//mean predicted measurement
	VectorXd z_pred = VectorXd(n_z);
	z_pred.fill(0.0);
	for (int i=0; i < 2*n_aug_+1; i++) {
	  z_pred = z_pred + weights_(i) * Zsig.col(i);
	}
	return z_pred;
}

MatrixXd UKF::CovarianceMatrixMeasurement(int n_z, MatrixXd R, MatrixXd Zsig, VectorXd z_pred) {
  //measurement covariance matrix S
  MatrixXd S = MatrixXd(n_z,n_z);
  S.fill(0.0);
  for (int i = 0; i < 2 * n_aug_ + 1; i++) {  //2n+1 simga points
    //residual
    VectorXd z_diff = Zsig.col(i) - z_pred;

	//angle normalization
	while (z_diff(1)> M_PI) z_diff(1)-=2.*M_PI;
	while (z_diff(1)<-M_PI) z_diff(1)+=2.*M_PI;

	S = S + weights_(i) * z_diff * z_diff.transpose();
  }

  //add measurement noise covariance matrix
  S = S + R;
  return S;
}

void UKF::UpdateState(int n_z, MatrixXd Zsig, VectorXd z_pred, MatrixXd S, VectorXd measurement) {
  //////////////////////////////////////////
  //create matrix for cross correlation Tc
  //////////////////////////////////////////
  MatrixXd Tc = MatrixXd(n_x_, n_z);

  //calculate cross correlation matrix
  Tc.fill(0.0);
  for (int i = 0; i < 2 * n_aug_ + 1; i++) {  //2n+1 simga points

    //residual
    VectorXd z_diff = Zsig.col(i) - z_pred;
	//angle normalization
	while (z_diff(1)> M_PI) z_diff(1)-=2.*M_PI;
	while (z_diff(1)<-M_PI) z_diff(1)+=2.*M_PI;

	// state difference
	VectorXd x_diff = Xsig_pred_.col(i) - x_;
	//angle normalization
	while (x_diff(3)> M_PI) x_diff(3)-=2.*M_PI;
	while (x_diff(3)<-M_PI) x_diff(3)+=2.*M_PI;

	Tc = Tc + weights_(i) * x_diff * z_diff.transpose();
  }

  //Kalman gain K;
  MatrixXd K = Tc * S.inverse();

  //residual
  VectorXd z_diff = measurement - z_pred;

  //angle normalization
  while (z_diff(1)> M_PI) z_diff(1)-=2.*M_PI;
  while (z_diff(1)<-M_PI) z_diff(1)+=2.*M_PI;

  //update state mean and covariance matrix
  x_ = x_ + K * z_diff;
  P_ = P_ - K*S*K.transpose();
}

/**
 * Updates the state and the state covariance matrix using a laser measurement.
 * @param {MeasurementPackage} meas_package
 */
void UKF::UpdateLidar(MeasurementPackage meas_package) {
  /**
  TODO:

  Complete this function! Use lidar data to update the belief about the object's
  position. Modify the state vector, x_, and covariance, P_.

  You'll also need to calculate the lidar NIS.
  */

  //set measurement dimension, lidar can measure px, py
  int n_z = 2;
  //create matrix for sigma points in measurement space
  MatrixXd Zsig = SigmaPointsPredMeasurementSpaceLidar(n_z);

  //mean predicted measurement
  VectorXd z_pred = MeanPredictedMeasurement(n_z, Zsig);

  //measurement covariance matrix S
  MatrixXd R = MatrixXd(n_z, n_z);
  R << std_laspx_ * std_laspx_, 0,
       0, std_laspy_ * std_laspy_;
  MatrixXd S = CovarianceMatrixMeasurement(n_z,R,Zsig,z_pred);

  // update state
  UpdateState(n_z, Zsig, z_pred, S, meas_package.raw_measurements_);

  // update state
  NIS_radar_ = CalculateNIS(n_z, z_pred, meas_package.raw_measurements_, S);
}

/**
 * Updates the state and the state covariance matrix using a radar measurement.
 * @param {MeasurementPackage} meas_package
 */
void UKF::UpdateRadar(MeasurementPackage meas_package) {
  /**
  TODO:

  Complete this function! Use radar data to update the belief about the object's
  position. Modify the state vector, x_, and covariance, P_.

  You'll also need to calculate the radar NIS.
  */

  //set measurement dimension, radar can measure r, phi, and r_dot
  int n_z = 3;
  //create matrix for sigma points in measurement space
  MatrixXd Zsig = SigmaPointsPredMeasurementSpaceRadar(n_z);
  //mean predicted measurement
  VectorXd z_pred = MeanPredictedMeasurement(n_z, Zsig);
  //measurement covariance matrix S
  MatrixXd R = MatrixXd(n_z,n_z);
  R << std_radr_*std_radr_, 0, 0,
  	   0, std_radphi_*std_radphi_, 0,
  	   0, 0,std_radrd_*std_radrd_;

  MatrixXd S = CovarianceMatrixMeasurement(n_z,R,Zsig,z_pred);
  // update state
  UpdateState(n_z, Zsig, z_pred, S, meas_package.raw_measurements_);

  // update state
  NIS_laser_ = CalculateNIS(n_z, z_pred, meas_package.raw_measurements_, S);

}

double UKF::CalculateNIS(int n_z, VectorXd z_pred, VectorXd measurement, MatrixXd S) {
  return (z_pred - measurement).transpose() * S.inverse() * (z_pred - measurement);
}
