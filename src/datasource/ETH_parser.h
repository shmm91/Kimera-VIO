/* ----------------------------------------------------------------------------
 * Copyright 2017, Massachusetts Institute of Technology,
 * Cambridge, MA 02139
 * All Rights Reserved
 * Authors: Luca Carlone, et al. (see THANKS for the full author list)
 * See LICENSE for the license information
 * -------------------------------------------------------------------------- */

/**
 * @file   ETH_parser.h
 * @brief  Reads ETH's Euroc dataset.
 * @author Antoni Rosinol, Luca Carlone, Chang
 */

#ifndef ETH_parser_H_
#define ETH_parser_H_

#include <stdlib.h>
#include <fstream>
#include <string>

#include <opencv2/core/core.hpp>
#include <opencv2/ml/ml.hpp>
#include <opencv2/calib3d/calib3d.hpp>
#include <opencv2/imgproc/imgproc.hpp>

#include <gtsam/geometry/Cal3DS2.h>
#include <gtsam/geometry/Pose3.h>

#include "Frame.h"
#include "StereoImuSyncPacket.h"
#include "datasource/DataSource.h"

namespace VIO {
/*
 * Parse all images and camera calibration for an ETH dataset.
 */
class ETHDatasetParser : public DataProvider {
public:
  ETHDatasetParser();
  ETHDatasetParser(const std::string& input_string);
  virtual ~ETHDatasetParser();

  // Gt data.
  GroundTruthData gtData_;

  // IMU data.
  ImuData imuData_;

  /// Getters
  inline std::string getDatasetName() const {
    return dataset_name_;
  }
  inline std::string getLeftImgName(const size_t& k) const {
    return getImgName("cam0", k);
  }
  inline std::string getRightImgName(const size_t& k) const {
    return getImgName("cam1", k);
  }
  // A bit risky to send refs to members... Can lead to dangling references.
  inline const gtsam::Pose3& getCamLPoseCamR() const {
    return camL_Pose_camR_;
  }
  inline const CameraParams& getLeftCamInfo() const {
    return camera_info_.at("cam0");
  }
  inline const CameraParams& getRightCamInfo() const {
    return camera_info_.at("cam1");
  }
  inline ImuParams getImuParams() const {
    return imu_params_;
  }

public:

  bool spin();

  void spinOnce(const FrameId& k,
                Timestamp& timestamp_last_frame,
                const StereoMatchingParams& stereo_matchiong_params,
                const bool equalize_image,
                const CameraParams& left_cam_info,
                const CameraParams& right_cam_info,
                const gtsam::Pose3& camL_pose_camR);

  // Helper function to parse Euroc dataset.
  void parse(size_t initial_k, size_t final_k);

  // Parse camera, gt, and imu data if using different Euroc format.
  bool parseDataset(const std::string& input_dataset_path,
                    const std::string& leftCameraName,
                    const std::string& rightCameraName,
                    const std::string& imuName,
                    const std::string& gtSensorName,
                    bool doParseImages = true);

  // Retrieve relative pose between timestamps.
  gtsam::Pose3 getGroundTruthRelativePose(
      const Timestamp& previousTimestamp,
      const Timestamp& currentTimestamp) const;

  // Retrieve absolute pose at timestamp.
  gtNavState getGroundTruthState(const Timestamp& timestamp) const;

  // Compute initialization errors and stats.
  const InitializationPerformance getInitializationPerformance(
                    const std::vector<Timestamp>& timestamps,
                    const std::vector<gtsam::Pose3>& poses_ba,
                    const gtNavState& init_nav_state,
                    const gtsam::Vector3& init_gravity);

  // Check if the ground truth is available.
  // (i.e., the timestamp is after the first gt state)
  bool isGroundTruthAvailable(const Timestamp& timestamp) const;

  // Get if the dataset has ground truth.
  bool isGroundTruthAvailable() const;

  // Compute error on the relative pose between two time stamps,
  // compared with the relative pose from ground truth.
  std::pair<double,double> computePoseErrors(
      const gtsam::Pose3& lkf_T_k_body,
      const bool isTrackingValid,
      const Timestamp& previousTimestamp,
      const Timestamp& currentTimestamp,
      const bool upToScale = false) const;

  // Get timestamp of a given pair of stereo images (synchronized).
  Timestamp timestampAtFrame(const FrameId& frame_number);

  // Print info about dataset.
  void print() const;

  // Parse IMU data of a given dataset.
  bool parseImuData(const std::string& input_dataset_path,
                    const std::string& imuName);

  // Parse ground truth data.
  bool parseGTdata(const std::string& input_dataset_path,
                   const std::string& gtSensorName);

public:
  // THIS IS ONLY HERE BECAUSE the pipeline needs to know what is this value.
  // But it should not need to!!
  // Put it as a static variable in the spin function.
  Timestamp timestamp_first_lkf_;

private:
  // Parse cam0, cam1 of a given dataset.
  bool parseCameraData(const std::string& input_dataset_path,
                       const std::string& leftCameraName,
                       const std::string& rightCameraName,
                       const bool doParseImages = true);

  // Parse IMU parameters.
  bool parseImuParams(const std::string& input_dataset_path,
                      const std::string& imuName);

  /// Getters.
  inline size_t getNumImages() const {
    return camera_image_lists_.at(camera_names_.at(0)).getNumImages();
  }
  inline std::string getImgName(const std::string& id, const size_t& k) const {
    return camera_image_lists_.at(id).img_lists.at(k).second;
  }

  // Retrieve absolute pose at timestamp.
  inline gtsam::Pose3 getGroundTruthPose(const Timestamp& timestamp) const {
    return getGroundTruthState(timestamp).pose_;
  }

  bool sanityCheckCameraData(
      const std::vector<std::string>& camera_names,
      const std::map<std::string, CameraParams>& camera_info,
      std::map<std::string, CameraImageLists>* camera_image_lists) const;

  // Sanity check: nr images is the same for left and right camera
  // Resizes left/right img lists to the minimum number of frames in case of
  // different list sizes.
  bool sanityCheckCamSize(CameraImageLists::ImgLists* left_img_lists,
                          CameraImageLists::ImgLists* right_img_lists) const;

  // Sanity check: time stamps are the same for left and right camera
  bool sanityCheckCamTimestamps(
      const CameraImageLists::ImgLists& left_img_lists,
      const CameraImageLists::ImgLists& right_img_lists,
      const CameraParams& left_cam_info) const;

private:
  /// Images data.
  // This matches the names of the folders in the dataset
  std::vector<std::string> camera_names_;
  // Map from camera name to its parameters
  std::map<std::string, CameraParams> camera_info_;
  // Map from camera name to its images
  std::map<std::string, CameraImageLists> camera_image_lists_;

  gtsam::Pose3 camL_Pose_camR_;

  bool is_gt_available_;
  std::string dataset_name_;
};

} // namespace VIO

#endif /* ETH_parser_H_ */
