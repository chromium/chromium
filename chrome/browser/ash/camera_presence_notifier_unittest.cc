// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/camera_presence_notifier.h"

#include "ash/capture_mode/fake_video_source_provider.h"
#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "content/public/browser/video_capture_service.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/video_capture/public/mojom/video_capture_service.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

constexpr char kFakeCameraDeviceId[] = "/dev/videoX";
constexpr char kFakeCameraDisplayName[] = "Fake Camera";
constexpr char kFakeCameraModelId[] = "0def:c000";

}  // namespace

class MockCameraPresenceNotifierObserver
    : public ash::CameraPresenceNotifier::Observer {
 public:
  MockCameraPresenceNotifierObserver() = default;
  ~MockCameraPresenceNotifierObserver() override = default;
  MOCK_METHOD(void,
              OnCameraPresenceCheckDone,
              (bool is_camera_present),
              (override));
};

class FakeVideoCaptureService
    : public video_capture::mojom::VideoCaptureService {
 public:
  FakeVideoCaptureService() = default;
  ~FakeVideoCaptureService() override = default;

  void AddFakeCamera() {
    fake_provider_.AddFakeCameraWithoutNotifying(
        kFakeCameraDeviceId, kFakeCameraDisplayName, kFakeCameraModelId,
        media::MEDIA_VIDEO_FACING_NONE);
  }

  void RemoveFakeCamera() {
    fake_provider_.RemoveFakeCameraWithoutNotifying(kFakeCameraDeviceId);
  }

  // mojom::VideoCaptureService:
  void InjectGpuDependencies(
      mojo::PendingRemote<video_capture::mojom::AcceleratorFactory>
          accelerator_factory) override {}

  void ConnectToCameraAppDeviceBridge(
      mojo::PendingReceiver<cros::mojom::CameraAppDeviceBridge> receiver)
      override {}

  void ConnectToDeviceFactory(
      mojo::PendingReceiver<video_capture::mojom::DeviceFactory> receiver)
      override {}

  void ConnectToVideoSourceProvider(
      mojo::PendingReceiver<video_capture::mojom::VideoSourceProvider> receiver)
      override {
    fake_provider_.Bind(std::move(receiver));
  }

  void SetRetryCount(int32_t count) override {}

  void BindControlsForTesting(
      mojo::PendingReceiver<video_capture::mojom::TestingControls> receiver)
      override {}

 private:
  FakeVideoSourceProvider fake_provider_;
};

class CameraPresenceNotifierTest : public testing::Test {
 public:
  CameraPresenceNotifierTest() = default;

  CameraPresenceNotifierTest(const CameraPresenceNotifierTest&) = delete;
  CameraPresenceNotifierTest& operator=(const CameraPresenceNotifierTest&) =
      delete;

  ~CameraPresenceNotifierTest() override = default;

  void AddFakeCamera() { fake_service_.AddFakeCamera(); }

  void RemoveFakeCamera() { fake_service_.RemoveFakeCamera(); }

  // testing::Test:
  void SetUp() override {
    content::OverrideVideoCaptureServiceForTesting(&fake_service_);
  }

  void TearDown() override {
    content::OverrideVideoCaptureServiceForTesting(nullptr);
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  FakeVideoCaptureService fake_service_;
};

// Tests that the observer of CameraPresenceNotifier works correctly when the
// camera is added/removed.
TEST_F(CameraPresenceNotifierTest, TestObservers) {
  auto* notifier = CameraPresenceNotifier::GetInstance();

  MockCameraPresenceNotifierObserver mock_observer;
  {
    base::RunLoop run_loop;
    EXPECT_CALL(mock_observer, OnCameraPresenceCheckDone(false))
        .WillOnce(base::test::RunOnceClosure(run_loop.QuitClosure()));
    notifier->AddObserver(&mock_observer);
    run_loop.Run();
  }

  {
    base::RunLoop run_loop;
    EXPECT_CALL(mock_observer, OnCameraPresenceCheckDone(true))
        .WillOnce(base::test::RunOnceClosure(run_loop.QuitClosure()));
    AddFakeCamera();
    run_loop.Run();
  }

  {
    base::RunLoop run_loop;
    EXPECT_CALL(mock_observer, OnCameraPresenceCheckDone(false))
        .WillOnce(base::test::RunOnceClosure(run_loop.QuitClosure()));
    RemoveFakeCamera();
    run_loop.Run();
  }
}

}  // namespace ash
