// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CAPTURE_MODE_CAPTURE_MODE_CAMERA_CONTROLLER_H_
#define ASH_CAPTURE_MODE_CAPTURE_MODE_CAMERA_CONTROLLER_H_

#include <string>
#include <vector>

#include "ash/ash_export.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/system/system_monitor.h"
#include "media/capture/video/video_capture_device_info.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/video_capture/public/mojom/video_source_provider.mojom.h"

namespace ash {

class CaptureModeDelegate;

struct CameraInfo {
  CameraInfo(std::string device_id,
             std::string display_name,
             std::string model_id);

  // The ID of the camera device given to it by the system in its current
  // connection instance (e.g. "/dev/video2"). Note that the same camera device
  // can disconnect and reconnect with a different `device_id` (e.g. when the
  // cable is flaky).
  const std::string device_id;

  // The name of the camera device as shown to the end user (e.g. "Integrated
  // Webcam").
  const std::string display_name;

  // A unique hardware ID of the camera device in the form of
  // "[Vendor ID]:[Product ID]" (e.g. "0c45:6713"). Note that if multiple
  // cameras from the same vendor and of the same model are connected to the
  // device, they will all have the same `model_id`.
  const std::string model_id;
};

using CameraInfoList = std::vector<CameraInfo>;

// Controls detecting camera devices additions and removals and keeping a list
// of all currently connected cameras to the device. It also tracks all the
// capture mode selfie camera settings.
class ASH_EXPORT CaptureModeCameraController
    : public base::SystemMonitor::DevicesChangedObserver {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Called to notify the observer that the list of `available_cameras_` has
    // changed, and provides that list as `cameras`.
    virtual void OnAvailableCamerasChanged(const CameraInfoList& cameras) = 0;

   protected:
    ~Observer() override = default;
  };

  explicit CaptureModeCameraController(CaptureModeDelegate* delegate);
  CaptureModeCameraController(const CaptureModeCameraController&) = delete;
  CaptureModeCameraController& operator=(const CaptureModeCameraController&) =
      delete;
  ~CaptureModeCameraController() override;

  const CameraInfoList& available_cameras() const { return available_cameras_; }

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // base::SystemMonitor::DevicesChangedObserver:
  void OnDevicesChanged(base::SystemMonitor::DeviceType device_type) override;

 private:
  // Called to connect to the video capture services's video source provider for
  // the first time, or when the connection to it is lost. It also queries the
  // list of currently available cameras by calling the below
  // GetCameraDevices().
  void ReconnectToVideoSourceProvider();

  // Retrieves the list of currently available cameras from the video source
  // provider.
  void GetCameraDevices();

  // Called back asynchronously by the video source provider to give us the list
  // of currently available camera `devices`.
  void OnCameraDevicesReceived(
      const std::vector<media::VideoCaptureDeviceInfo>& devices);

  // Owned by CaptureModeController and guaranteed to be not null and to outlive
  // `this`.
  CaptureModeDelegate* const delegate_;

  // The remote end to the video source provider that exists in the video
  // capture service.
  mojo::Remote<video_capture::mojom::VideoSourceProvider>
      video_source_provider_remote_;

  CameraInfoList available_cameras_;

  base::ObserverList<Observer> observers_;

  // True if GetCameraDevices() was called and we're currently waiting for the
  // video source provider to give us the list.
  bool waiting_for_camera_devices_ = false;

  base::WeakPtrFactory<CaptureModeCameraController> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_CAPTURE_MODE_CAPTURE_MODE_CAMERA_CONTROLLER_H_
