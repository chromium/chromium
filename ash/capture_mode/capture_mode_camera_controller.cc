// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/capture_mode_camera_controller.h"

#include <algorithm>

#include "ash/public/cpp/capture_mode/capture_mode_delegate.h"
#include "base/bind.h"
#include "base/check.h"

namespace ash {

namespace {

// Returns true if the `incoming_list` (supplied by the video source provider)
// contains different items than the ones in `current_list` (which is the
// currently `available_cameras_` maintained by `CaptureModeCameraController`).
bool DidDevicesChange(
    const std::vector<media::VideoCaptureDeviceInfo>& incoming_list,
    const CameraInfoList& current_list) {
  if (incoming_list.size() != current_list.size())
    return true;

  for (const auto& incoming_camera : incoming_list) {
    const auto& device_id = incoming_camera.descriptor.device_id;
    const auto iter = std::find_if(current_list.begin(), current_list.end(),
                                   [device_id](const CameraInfo& info) {
                                     return info.device_id == device_id;
                                   });
    if (iter == current_list.end())
      return true;

    const CameraInfo& found_info = *iter;
    if (found_info.display_name != incoming_camera.descriptor.display_name() ||
        found_info.model_id != incoming_camera.descriptor.model_id) {
      return true;
    }
  }

  return false;
}

}  // namespace

// -----------------------------------------------------------------------------
// CameraInfo:

CameraInfo::CameraInfo(std::string device_id,
                       std::string display_name,
                       std::string model_id)
    : device_id(std::move(device_id)),
      display_name(std::move(display_name)),
      model_id(std::move(model_id)) {}

// -----------------------------------------------------------------------------
// CaptureModeCameraController:

CaptureModeCameraController::CaptureModeCameraController(
    CaptureModeDelegate* delegate)
    : delegate_(delegate) {
  DCHECK(delegate_);
  DCHECK(base::SystemMonitor::Get())
      << "No instance of SystemMonitor exists. If this is a unit test, please "
         "create one.";

  base::SystemMonitor::Get()->AddDevicesChangedObserver(this);
  ReconnectToVideoSourceProvider();
}

CaptureModeCameraController::~CaptureModeCameraController() {
  base::SystemMonitor::Get()->RemoveDevicesChangedObserver(this);
}

void CaptureModeCameraController::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void CaptureModeCameraController::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void CaptureModeCameraController::OnDevicesChanged(
    base::SystemMonitor::DeviceType device_type) {
  if (device_type == base::SystemMonitor::DEVTYPE_VIDEO_CAPTURE)
    GetCameraDevices();
}

void CaptureModeCameraController::ReconnectToVideoSourceProvider() {
  video_source_provider_remote_.reset();
  waiting_for_camera_devices_ = false;
  delegate_->ConnectToVideoSourceProvider(
      video_source_provider_remote_.BindNewPipeAndPassReceiver());
  video_source_provider_remote_.set_disconnect_handler(base::BindOnce(
      &CaptureModeCameraController::ReconnectToVideoSourceProvider,
      base::Unretained(this)));
  GetCameraDevices();
}

void CaptureModeCameraController::GetCameraDevices() {
  DCHECK(video_source_provider_remote_);

  if (waiting_for_camera_devices_)
    return;

  waiting_for_camera_devices_ = true;
  video_source_provider_remote_->GetSourceInfos(
      base::BindOnce(&CaptureModeCameraController::OnCameraDevicesReceived,
                     weak_ptr_factory_.GetWeakPtr()));
}

void CaptureModeCameraController::OnCameraDevicesReceived(
    const std::vector<media::VideoCaptureDeviceInfo>& devices) {
  DCHECK(waiting_for_camera_devices_);
  waiting_for_camera_devices_ = false;

  if (!DidDevicesChange(devices, available_cameras_))
    return;

  available_cameras_.clear();
  for (const auto& device : devices) {
    const auto& descriptor = device.descriptor;
    available_cameras_.emplace_back(
        descriptor.device_id, descriptor.display_name(), descriptor.model_id);
  }

  for (auto& observer : observers_)
    observer.OnAvailableCamerasChanged(available_cameras_);
}

}  // namespace ash
