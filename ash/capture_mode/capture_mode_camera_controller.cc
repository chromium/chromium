// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/capture_mode_camera_controller.h"

#include <algorithm>
#include <cstring>

#include "ash/public/cpp/capture_mode/capture_mode_delegate.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/check.h"
#include "base/strings/stringprintf.h"
#include "media/capture/video/video_capture_device_descriptor.h"

namespace ash {

namespace {

// Defines a map type to map a camera model ID (or display name) to the number
// of cameras of that model that are currently connected.
using ModelIdToCountMap = std::map<std::string, int>;

// Using the given `cam_models_map` which tracks the number of cameras connected
// of each model, returns the next `CameraId::number` for the given
// `model_id_or_display_name`.
int GetNextCameraNumber(const std::string& model_id_or_display_name,
                        ModelIdToCountMap* cam_models_map) {
  return ++(*cam_models_map)[model_id_or_display_name];
}

// Returns a reference to either the model ID (if available) or the display name
// from the given `descriptor`.
const std::string& PickModelIdOrDisplayName(
    const media::VideoCaptureDeviceDescriptor& descriptor) {
  return descriptor.model_id.empty() ? descriptor.display_name()
                                     : descriptor.model_id;
}

// Returns true if the `incoming_list` (supplied by the video source provider)
// contains different items than the ones in `current_list` (which is the
// currently `available_cameras_` maintained by `CaptureModeCameraController`).
bool DidDevicesChange(
    const std::vector<media::VideoCaptureDeviceInfo>& incoming_list,
    const CameraInfoList& current_list) {
  if (incoming_list.size() != current_list.size())
    return true;

  ModelIdToCountMap cam_models_map;
  for (const auto& incoming_camera : incoming_list) {
    const auto& device_id = incoming_camera.descriptor.device_id;
    const auto iter = std::find_if(current_list.begin(), current_list.end(),
                                   [device_id](const CameraInfo& info) {
                                     return info.device_id == device_id;
                                   });
    if (iter == current_list.end())
      return true;

    const auto& model_id_or_display_name =
        PickModelIdOrDisplayName(incoming_camera.descriptor);
    const int cam_number =
        GetNextCameraNumber(model_id_or_display_name, &cam_models_map);

    const CameraInfo& found_info = *iter;
    if (found_info.display_name != incoming_camera.descriptor.display_name() ||
        found_info.camera_id.model_id_or_display_name() !=
            model_id_or_display_name ||
        found_info.camera_id.number() != cam_number) {
      return true;
    }
  }

  return false;
}

// Returns the CameraInfo item in `list` whose ID is equal to the given `id`, or
// nullptr if no such item exists.
const CameraInfo* GetCameraInfoById(const CameraId& id,
                                    const CameraInfoList& list) {
  const auto iter = std::find_if(
      list.begin(), list.end(),
      [&id](const CameraInfo& info) { return info.camera_id == id; });
  return iter == list.end() ? nullptr : &(*iter);
}

}  // namespace

// -----------------------------------------------------------------------------
// CameraId:

CameraId::CameraId(std::string model_id_or_display_name, int number)
    : model_id_or_display_name_(std::move(model_id_or_display_name)),
      number_(number) {
  DCHECK(!model_id_or_display_name_.empty());
  DCHECK_GE(number, 1);
}

bool CameraId::operator<(const CameraId& rhs) const {
  const int result = std::strcmp(model_id_or_display_name_.c_str(),
                                 rhs.model_id_or_display_name_.c_str());
  return result != 0 ? result : (number_ < rhs.number_);
}

std::string CameraId::ToString() const {
  return base::StringPrintf("%s:%0d", model_id_or_display_name_.c_str(),
                            number_);
}

// -----------------------------------------------------------------------------
// CameraInfo:

CameraInfo::CameraInfo(CameraId camera_id,
                       std::string device_id,
                       std::string display_name)
    : camera_id(std::move(camera_id)),
      device_id(std::move(device_id)),
      display_name(std::move(display_name)) {}

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

void CaptureModeCameraController::SetSelectedCamera(CameraId camera_id) {
  selected_camera_ = std::move(camera_id);
  RefreshCameraPreview();
}

void CaptureModeCameraController::SetShouldShowPreview(bool value) {
  should_show_preview_ = value;
  RefreshCameraPreview();
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
  // Run the optional for-test closure at the exit of this function's scope.
  base::ScopedClosureRunner deferred_runner;
  if (on_camera_list_received_for_test_) {
    deferred_runner.ReplaceClosure(
        std::move(on_camera_list_received_for_test_));
  }

  DCHECK(waiting_for_camera_devices_);
  waiting_for_camera_devices_ = false;

  if (!DidDevicesChange(devices, available_cameras_))
    return;

  available_cameras_.clear();
  ModelIdToCountMap cam_models_map;
  for (const auto& device : devices) {
    const auto& descriptor = device.descriptor;
    const auto& model_id_or_display_name = PickModelIdOrDisplayName(descriptor);
    const int cam_number =
        GetNextCameraNumber(model_id_or_display_name, &cam_models_map);
    available_cameras_.emplace_back(
        CameraId(model_id_or_display_name, cam_number), descriptor.device_id,
        descriptor.display_name());
  }

  for (auto& observer : observers_)
    observer.OnAvailableCamerasChanged(available_cameras_);

  RefreshCameraPreview();
}

void CaptureModeCameraController::RefreshCameraPreview() {
  const CameraInfo* camera_info = GetCameraInfoForPreview();
  if (!camera_info) {
    // TODO(https://crbug.com/1290883): Destroy the preview if any.
    camera_preview_widget_ = false;
    return;
  }

  // TODO(https://crbug.com/1290883): Create the preview.
  camera_preview_widget_ = true;
}

const CameraInfo* CaptureModeCameraController::GetCameraInfoForPreview() const {
  if (!should_show_preview_ || !selected_camera_.is_valid())
    return nullptr;
  return GetCameraInfoById(selected_camera_, available_cameras_);
}

}  // namespace ash
