// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/capture_mode_camera_controller.h"
#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/capture_mode/fake_video_source_provider.h"
#include "ash/capture_mode/test_capture_mode_delegate.h"
#include "ash/constants/ash_features.h"
#include "ash/test/ash_test_base.h"
#include "base/run_loop.h"
#include "base/system/system_monitor.h"
#include "base/test/scoped_feature_list.h"

namespace ash {

namespace {

TestCaptureModeDelegate* GetTestDelegate() {
  return static_cast<TestCaptureModeDelegate*>(
      CaptureModeController::Get()->delegate_for_testing());
}

CaptureModeCameraController* GetCameraController() {
  return CaptureModeController::Get()->camera_controller();
}

// Defines a waiter for the camera devices change notifications.
class CameraDevicesChangeWaiter : public CaptureModeCameraController::Observer {
 public:
  CameraDevicesChangeWaiter() { GetCameraController()->AddObserver(this); }
  CameraDevicesChangeWaiter(const CameraDevicesChangeWaiter&) = delete;
  CameraDevicesChangeWaiter& operator=(const CameraDevicesChangeWaiter&) =
      delete;
  ~CameraDevicesChangeWaiter() override {
    GetCameraController()->RemoveObserver(this);
  }

  void Wait() { loop_.Run(); }

  // CaptureModeCameraController::Observer:
  void OnAvailableCamerasChanged(const CameraInfoList& cameras) override {
    loop_.Quit();
  }

 private:
  base::RunLoop loop_;
};

}  // namespace

class CaptureModeCameraTest : public AshTestBase {
 public:
  CaptureModeCameraTest() = default;
  CaptureModeCameraTest(const CaptureModeCameraTest&) = delete;
  CaptureModeCameraTest& operator=(const CaptureModeCameraTest&) = delete;
  ~CaptureModeCameraTest() override = default;

  // AshTestBase:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        features::kCaptureModeSelfieCamera);
    AshTestBase::SetUp();
  }

  void AddFakeCamera(const std::string& device_id,
                     const std::string& display_name) {
    GetTestDelegate()->video_source_provider()->AddFakeCamera(device_id,
                                                              display_name);
  }

  void RemoveFakeCamera(const std::string& device_id) {
    GetTestDelegate()->video_source_provider()->RemoveFakeCamera(device_id);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::SystemMonitor system_monitor_;
};

TEST_F(CaptureModeCameraTest, CameraDevicesChanges) {
  auto* camera_controller = GetCameraController();
  ASSERT_TRUE(camera_controller);
  EXPECT_TRUE(camera_controller->available_cameras().empty());

  const std::string device_id = "/dev/video0";
  const std::string display_name = "Integrated Webcam";
  {
    CameraDevicesChangeWaiter waiter;
    AddFakeCamera(device_id, display_name);
    waiter.Wait();
  }

  EXPECT_EQ(1u, camera_controller->available_cameras().size());
  EXPECT_EQ(device_id, camera_controller->available_cameras()[0].device_id);
  EXPECT_EQ(display_name,
            camera_controller->available_cameras()[0].display_name);

  {
    CameraDevicesChangeWaiter waiter;
    RemoveFakeCamera(device_id);
    waiter.Wait();
  }

  EXPECT_TRUE(camera_controller->available_cameras().empty());
}

}  // namespace ash
