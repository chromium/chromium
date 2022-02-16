// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/capture_mode_bar_view.h"
#include "ash/capture_mode/capture_mode_camera_controller.h"
#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/capture_mode/capture_mode_session.h"
#include "ash/capture_mode/capture_mode_session_test_api.h"
#include "ash/capture_mode/capture_mode_settings_test_api.h"
#include "ash/capture_mode/capture_mode_settings_view.h"
#include "ash/capture_mode/capture_mode_test_util.h"
#include "ash/capture_mode/capture_mode_toggle_button.h"
#include "ash/capture_mode/capture_mode_types.h"
#include "ash/capture_mode/fake_video_source_provider.h"
#include "ash/capture_mode/test_capture_mode_delegate.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/capture_mode/capture_mode_test_api.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/system_monitor.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

constexpr char kDefaultCameraDeviceId[] = "/dev/videoX";
constexpr char kDefaultCameraDisplayName[] = "Default Cam";
constexpr char kDefaultCameraModelId[] = "0def:c000";

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

  int camera_change_event_count() const { return camera_change_event_count_; }
  int selected_camera_change_event_count() const {
    return selected_camera_change_event_count_;
  }

  void Wait() { loop_.Run(); }

  // CaptureModeCameraController::Observer:
  void OnAvailableCamerasChanged(const CameraInfoList& cameras) override {
    ++camera_change_event_count_;
    loop_.Quit();
  }

  void OnSelectedCameraChanged(const CameraId& camera_id) override {
    ++selected_camera_change_event_count_;
  }

 private:
  base::RunLoop loop_;

  // Tracks the number of times the observer call `OnAvailableCamerasChanged()`
  // was triggered.
  int camera_change_event_count_ = 0;

  // Tracks the number of times `OnSelectedCameraChanged()` was triggered.
  int selected_camera_change_event_count_ = 0;
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
    window_ = CreateTestWindow(gfx::Rect(30, 40, 300, 200));
  }

  void TearDown() override {
    window_.reset();
    AshTestBase::TearDown();
  }

  aura::Window* window() const { return window_.get(); }

  void StartRecordingFromSource(CaptureModeSource source) {
    auto* controller = CaptureModeController::Get();
    controller->SetSource(source);

    switch (source) {
      case CaptureModeSource::kFullscreen:
      case CaptureModeSource::kRegion:
        break;
      case CaptureModeSource::kWindow:
        GetEventGenerator()->MoveMouseTo(
            window_->GetBoundsInScreen().CenterPoint());
        break;
    }
    CaptureModeTestApi().PerformCapture();
    WaitForRecordingToStart();
    EXPECT_TRUE(controller->is_recording_in_progress());
  }

  void AddFakeCamera(const std::string& device_id,
                     const std::string& display_name,
                     const std::string& model_id) {
    CameraDevicesChangeWaiter waiter;
    GetTestDelegate()->video_source_provider()->AddFakeCamera(
        device_id, display_name, model_id);
    waiter.Wait();
  }

  void RemoveFakeCamera(const std::string& device_id) {
    CameraDevicesChangeWaiter waiter;
    GetTestDelegate()->video_source_provider()->RemoveFakeCamera(device_id);
    waiter.Wait();
  }

  void AddDefaultCamera() {
    AddFakeCamera(kDefaultCameraDeviceId, kDefaultCameraDisplayName,
                  kDefaultCameraModelId);
  }

  void RemoveDefaultCamera() { RemoveFakeCamera(kDefaultCameraDeviceId); }

  // Adds the default camera, sets it as the selected camera, then removes it,
  // which triggers the camera disconnection grace period. Returns a pointer to
  // the `CaptureModeCameraController`.
  CaptureModeCameraController* AddAndRemoveCameraAndTriggerGracePeriod() {
    AddDefaultCamera();
    auto* camera_controller = GetCameraController();
    camera_controller->SetSelectedCamera(CameraId(kDefaultCameraModelId, 1));
    RemoveDefaultCamera();
    return camera_controller;
  }

  void OpenSettingsView() {
    CaptureModeSession* session =
        CaptureModeController::Get()->capture_mode_session();
    DCHECK(session);
    ClickOnView(CaptureModeSessionTestApi(session)
                    .GetCaptureModeBarView()
                    ->settings_button(),
                GetEventGenerator());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::SystemMonitor system_monitor_;
  std::unique_ptr<aura::Window> window_;
};

TEST_F(CaptureModeCameraTest, CameraDevicesChanges) {
  auto* camera_controller = GetCameraController();
  ASSERT_TRUE(camera_controller);
  EXPECT_TRUE(camera_controller->available_cameras().empty());
  EXPECT_FALSE(camera_controller->selected_camera().is_valid());
  EXPECT_FALSE(camera_controller->should_show_preview());
  EXPECT_FALSE(camera_controller->camera_preview_widget());

  const std::string device_id = "/dev/video0";
  const std::string display_name = "Integrated Webcam";
  const std::string model_id = "0123:4567";
  AddFakeCamera(device_id, display_name, model_id);

  EXPECT_EQ(1u, camera_controller->available_cameras().size());
  EXPECT_TRUE(camera_controller->available_cameras()[0].camera_id.is_valid());
  EXPECT_EQ(model_id, camera_controller->available_cameras()[0]
                          .camera_id.model_id_or_display_name());
  EXPECT_EQ(1, camera_controller->available_cameras()[0].camera_id.number());
  EXPECT_EQ(device_id, camera_controller->available_cameras()[0].device_id);
  EXPECT_EQ(display_name,
            camera_controller->available_cameras()[0].display_name);
  EXPECT_FALSE(camera_controller->should_show_preview());
  EXPECT_FALSE(camera_controller->camera_preview_widget());

  RemoveFakeCamera(device_id);

  EXPECT_TRUE(camera_controller->available_cameras().empty());
  EXPECT_FALSE(camera_controller->should_show_preview());
  EXPECT_FALSE(camera_controller->camera_preview_widget());
}

TEST_F(CaptureModeCameraTest, CameraRemovedWhileWaitingForCameraDevices) {
  auto* camera_controller = GetCameraController();
  AddDefaultCamera();
  EXPECT_EQ(1u, camera_controller->available_cameras().size());

  // The system monitor can trigger several notifications about devices changes
  // for the same camera addition event. We will simulate a camera getting
  // removed right while we're still waiting for the video source provider to
  // send us the list. https://crbug.com/1295377.
  auto* video_source_provider = GetTestDelegate()->video_source_provider();
  {
    base::RunLoop loop;
    video_source_provider->set_on_replied_with_source_infos(
        base::BindLambdaForTesting([&]() { loop.Quit(); }));
    base::SystemMonitor::Get()->ProcessDevicesChanged(
        base::SystemMonitor::DEVTYPE_VIDEO_CAPTURE);
    loop.Run();
  }

  RemoveDefaultCamera();
  EXPECT_TRUE(camera_controller->available_cameras().empty());
}

TEST_F(CaptureModeCameraTest, SelectingUnavailableCamera) {
  StartCaptureSession(CaptureModeSource::kFullscreen, CaptureModeType::kVideo);
  auto* camera_controller = GetCameraController();
  EXPECT_FALSE(camera_controller->camera_preview_widget());

  // Selecting a camera that doesn't exist in the list shouldn't show its
  // preview.
  camera_controller->SetSelectedCamera(CameraId("model", 1));
  EXPECT_FALSE(camera_controller->camera_preview_widget());
}

TEST_F(CaptureModeCameraTest, SelectingAvailableCamera) {
  StartCaptureSession(CaptureModeSource::kFullscreen, CaptureModeType::kVideo);
  auto* camera_controller = GetCameraController();
  EXPECT_FALSE(camera_controller->camera_preview_widget());

  AddDefaultCamera();

  EXPECT_EQ(1u, camera_controller->available_cameras().size());
  EXPECT_FALSE(camera_controller->camera_preview_widget());

  // Selecting an available camera while showing the preview is allowed should
  // result in creating the preview widget.
  camera_controller->SetSelectedCamera(CameraId(kDefaultCameraModelId, 1));
  EXPECT_TRUE(camera_controller->camera_preview_widget());
}

TEST_F(CaptureModeCameraTest, SelectedCameraBecomesAvailable) {
  StartCaptureSession(CaptureModeSource::kFullscreen, CaptureModeType::kVideo);
  auto* camera_controller = GetCameraController();
  EXPECT_FALSE(camera_controller->camera_preview_widget());

  camera_controller->SetSelectedCamera(CameraId(kDefaultCameraModelId, 1));
  EXPECT_FALSE(camera_controller->camera_preview_widget());

  // The selected camera becomes available while `should_show_preview_` is still
  // true. The preview should show in this case.
  AddDefaultCamera();
  EXPECT_TRUE(camera_controller->camera_preview_widget());

  // Clearing the selected camera should hide the preview.
  camera_controller->SetSelectedCamera(CameraId());
  EXPECT_FALSE(camera_controller->selected_camera().is_valid());
  EXPECT_FALSE(camera_controller->camera_preview_widget());
}

TEST_F(CaptureModeCameraTest, MultipleCamerasOfTheSameModel) {
  auto* camera_controller = GetCameraController();

  const std::string device_id_1 = "/dev/video0";
  const std::string display_name = "Integrated Webcam";
  const std::string model_id = "0123:4567";
  AddFakeCamera(device_id_1, display_name, model_id);

  const auto& available_cameras = camera_controller->available_cameras();
  EXPECT_EQ(1u, available_cameras.size());
  EXPECT_EQ(1, available_cameras[0].camera_id.number());
  EXPECT_EQ(model_id,
            available_cameras[0].camera_id.model_id_or_display_name());

  // Adding a new camera of the same model should be correctly tracked with a
  // different ID.
  const std::string device_id_2 = "/dev/video1";
  AddFakeCamera(device_id_2, display_name, model_id);

  EXPECT_EQ(2u, available_cameras.size());
  EXPECT_EQ(2, available_cameras[1].camera_id.number());
  EXPECT_EQ(model_id,
            available_cameras[1].camera_id.model_id_or_display_name());
  EXPECT_NE(available_cameras[0].camera_id, available_cameras[1].camera_id);
}

TEST_F(CaptureModeCameraTest, MissingCameraModelId) {
  auto* camera_controller = GetCameraController();

  const std::string device_id = "/dev/video0";
  const std::string display_name = "Integrated Webcam";
  AddFakeCamera(device_id, display_name, /*model_id=*/"");

  // The camera's display name should be used instead of a model ID when it's
  // missing.
  const auto& available_cameras = camera_controller->available_cameras();
  EXPECT_EQ(1u, available_cameras.size());
  EXPECT_TRUE(available_cameras[0].camera_id.is_valid());
  EXPECT_EQ(1, available_cameras[0].camera_id.number());
  EXPECT_EQ(display_name,
            available_cameras[0].camera_id.model_id_or_display_name());

  // If the SystemMonitor triggered a device change alert for some reason, but
  // the actual list of cameras didn't change, observers should never be
  // notified again.
  {
    base::RunLoop loop;
    camera_controller->SetOnCameraListReceivedForTesting(loop.QuitClosure());
    CameraDevicesChangeWaiter observer;
    base::SystemMonitor::Get()->ProcessDevicesChanged(
        base::SystemMonitor::DEVTYPE_VIDEO_CAPTURE);
    loop.Run();
    EXPECT_EQ(0, observer.camera_change_event_count());
  }
}

TEST_F(CaptureModeCameraTest, DisconnectSelectedCamera) {
  AddDefaultCamera();
  auto* camera_controller = GetCameraController();
  const CameraId camera_id(kDefaultCameraModelId, 1);
  camera_controller->SetSelectedCamera(camera_id);

  // Disconnect a selected camera, and expect that the grace period timer is
  // running.
  RemoveDefaultCamera();
  base::OneShotTimer* timer =
      camera_controller->camera_reconnect_timer_for_test();
  EXPECT_TRUE(timer->IsRunning());
  EXPECT_EQ(camera_id, camera_controller->selected_camera());

  // When the timer fires before the camera gets reconnected, the selected
  // camera ID is cleared.
  timer->FireNow();
  EXPECT_FALSE(camera_controller->selected_camera().is_valid());
  EXPECT_FALSE(timer->IsRunning());
}

TEST_F(CaptureModeCameraTest, SelectUnavailableCameraDuringGracePeriod) {
  auto* camera_controller = AddAndRemoveCameraAndTriggerGracePeriod();
  base::OneShotTimer* timer =
      camera_controller->camera_reconnect_timer_for_test();
  EXPECT_TRUE(timer->IsRunning());

  // Selecting an unavailable camera during the grace period should keep the
  // timer running for another grace period.
  const CameraId new_camera_id("Different Camera", 1);
  camera_controller->SetSelectedCamera(new_camera_id);
  EXPECT_TRUE(timer->IsRunning());
  EXPECT_EQ(new_camera_id, camera_controller->selected_camera());

  // Once the timer fires the new ID will also be cleared.
  timer->FireNow();
  EXPECT_FALSE(camera_controller->selected_camera().is_valid());
  EXPECT_FALSE(timer->IsRunning());
}

TEST_F(CaptureModeCameraTest, SelectAvailableCameraDuringGracePeriod) {
  const std::string device_id = "/dev/video0";
  const std::string display_name = "Integrated Webcam";
  const CameraId available_camera_id(display_name, 1);
  AddFakeCamera(device_id, display_name, /*model_id=*/"");

  // This adds the default camera as the selected one, and removes it triggering
  // a grace period.
  auto* camera_controller = AddAndRemoveCameraAndTriggerGracePeriod();
  base::OneShotTimer* timer =
      camera_controller->camera_reconnect_timer_for_test();
  EXPECT_TRUE(timer->IsRunning());

  // Selecting the available camera during the grace period should stop the
  // timer immediately.
  camera_controller->SetSelectedCamera(available_camera_id);
  EXPECT_FALSE(timer->IsRunning());
  EXPECT_EQ(available_camera_id, camera_controller->selected_camera());
}

// This tests simulates a flaky camera connection.
TEST_F(CaptureModeCameraTest, ReconnectDuringGracePeriod) {
  StartCaptureSession(CaptureModeSource::kFullscreen, CaptureModeType::kVideo);
  auto* camera_controller = AddAndRemoveCameraAndTriggerGracePeriod();
  base::OneShotTimer* timer =
      camera_controller->camera_reconnect_timer_for_test();
  EXPECT_TRUE(timer->IsRunning());
  EXPECT_FALSE(camera_controller->camera_preview_widget());

  // Readd the camera during the grace period, the timer should stop, and the
  // preview should reshow.
  AddDefaultCamera();
  EXPECT_FALSE(timer->IsRunning());
  EXPECT_TRUE(camera_controller->camera_preview_widget());
}

TEST_F(CaptureModeCameraTest, SelectedCameraChangedObserver) {
  AddDefaultCamera();
  auto* camera_controller = GetCameraController();
  const CameraId camera_id(kDefaultCameraModelId, 1);

  CameraDevicesChangeWaiter observer;
  camera_controller->SetSelectedCamera(camera_id);
  EXPECT_EQ(1, observer.selected_camera_change_event_count());

  // Selecting the same camera ID again should not trigger an observer call.
  camera_controller->SetSelectedCamera(camera_id);
  EXPECT_EQ(1, observer.selected_camera_change_event_count());

  // Clearing the ID should.
  camera_controller->SetSelectedCamera(CameraId());
  EXPECT_EQ(2, observer.selected_camera_change_event_count());
}

TEST_F(CaptureModeCameraTest, ShouldShowPreviewTest) {
  auto* controller = CaptureModeController::Get();
  auto* camera_controller = GetCameraController();
  controller->SetSource(CaptureModeSource::kFullscreen);
  controller->SetType(CaptureModeType::kVideo);
  controller->Start(CaptureModeEntryType::kQuickSettings);
  // should_show_preview() should return true when CaptureModeSession is started
  // in video recording mode.
  EXPECT_TRUE(camera_controller->should_show_preview());
  // Switch to image capture mode, should_show_preview() should return false.
  controller->SetType(CaptureModeType::kImage);
  EXPECT_FALSE(camera_controller->should_show_preview());
  // Stop an existing capture session, should_show_preview() should return
  // false.
  controller->Stop();
  EXPECT_FALSE(camera_controller->should_show_preview());
  EXPECT_FALSE(controller->IsActive());

  // Start another capture session and start video recording,
  // should_show_preview() should return false when video recording ends.
  controller->SetType(CaptureModeType::kVideo);
  controller->Start(CaptureModeEntryType::kQuickSettings);
  EXPECT_TRUE(camera_controller->should_show_preview());
  controller->StartVideoRecordingImmediatelyForTesting();
  EXPECT_TRUE(camera_controller->should_show_preview());
  controller->EndVideoRecording(EndRecordingReason::kStopRecordingButton);
  EXPECT_FALSE(camera_controller->should_show_preview());
}

// Tests that the options on camera menu are shown and checked correctly when
// adding or removing cameras. Also tests that `selected_camera_` is updated
// correspondently.
TEST_F(CaptureModeCameraTest, CheckCameraOptions) {
  auto* camera_controller = GetCameraController();
  const std::string device_id_1 = "/dev/video0";
  const std::string display_name_1 = "Integrated Webcam";

  const std::string device_id_2 = "/dev/video1";
  const std::string display_name_2 = "Integrated Webcam 1";

  AddFakeCamera(device_id_1, display_name_1, display_name_1);
  AddFakeCamera(device_id_2, display_name_2, display_name_2);

  StartCaptureSession(CaptureModeSource::kFullscreen, CaptureModeType::kVideo);
  OpenSettingsView();

  // Check camera settings is shown. `Off` option is checked. Two camera options
  // are not checked.
  CaptureModeSettingsTestApi test_api;
  CaptureModeMenuGroup* camera_menu_group = test_api.GetCameraMenuGroup();
  EXPECT_TRUE(camera_menu_group && camera_menu_group->GetVisible());
  EXPECT_TRUE(camera_menu_group->IsOptionChecked(kCameraOff));
  EXPECT_FALSE(camera_menu_group->IsOptionChecked(kCameraDevicesBegin));
  EXPECT_FALSE(camera_menu_group->IsOptionChecked(kCameraDevicesBegin + 1));
  EXPECT_FALSE(camera_controller->selected_camera().is_valid());

  // Click the option for camera device 1, check its display name matches with
  // the camera device 1 display name and it's checked. Also check the selected
  // camera is valid now.
  ClickOnView(test_api.GetCameraOption(kCameraDevicesBegin),
              GetEventGenerator());
  EXPECT_FALSE(camera_menu_group->IsOptionChecked(kCameraOff));
  EXPECT_EQ(base::UTF16ToUTF8(camera_menu_group->GetOptionLabelForTesting(
                kCameraDevicesBegin)),
            display_name_1);
  EXPECT_TRUE(camera_menu_group->IsOptionChecked(kCameraDevicesBegin));
  EXPECT_TRUE(camera_controller->selected_camera().is_valid());

  // Now disconnect camera device 1.
  RemoveFakeCamera(device_id_1);

  // Check the camera device 1 is removed from the camera menu. There's only a
  // camera option for camera device 2. Selected camera is still valid. `Off`
  // option and option for camera device 2 are not checked.
  EXPECT_TRUE(test_api.GetCameraOption(kCameraDevicesBegin));
  EXPECT_FALSE(test_api.GetCameraOption(kCameraDevicesBegin + 1));
  EXPECT_EQ(base::UTF16ToUTF8(camera_menu_group->GetOptionLabelForTesting(
                kCameraDevicesBegin)),
            display_name_2);
  EXPECT_FALSE(camera_menu_group->IsOptionChecked(kCameraOff));
  EXPECT_FALSE(camera_menu_group->IsOptionChecked(kCameraDevicesBegin));
  EXPECT_TRUE(camera_controller->selected_camera().is_valid());

  // Now connect the camera device 1 again.
  AddFakeCamera(device_id_1, display_name_1, display_name_1);

  // Check the camera device 1 is added back to the camera menu and it's checked
  // automatically. Check the selected option's label matches with the camera
  // device 1's display name.
  EXPECT_TRUE(test_api.GetCameraOption(kCameraDevicesBegin + 1));
  EXPECT_FALSE(camera_menu_group->IsOptionChecked(kCameraOff));
  EXPECT_TRUE(camera_menu_group->IsOptionChecked(kCameraDevicesBegin + 1));
  EXPECT_EQ(base::UTF16ToUTF8(camera_menu_group->GetOptionLabelForTesting(
                kCameraDevicesBegin + 1)),
            display_name_1);
  EXPECT_TRUE(camera_controller->selected_camera().is_valid());
}

TEST_F(CaptureModeCameraTest, CameraPreviewWidgetStackingInFullscreen) {
  StartCaptureSession(CaptureModeSource::kFullscreen, CaptureModeType::kVideo);
  auto* camera_controller = GetCameraController();
  AddDefaultCamera();
  camera_controller->SetSelectedCamera(CameraId(kDefaultCameraModelId, 1));
  const auto* camera_preview_widget =
      camera_controller->camera_preview_widget();
  EXPECT_TRUE(camera_preview_widget);

  auto* preview_window = camera_preview_widget->GetNativeWindow();
  const auto* overlay_container = preview_window->GetRootWindow()->GetChildById(
      kShellWindowId_OverlayContainer);
  auto* parent = preview_window->parent();
  // Parent of the preview should be the OverlayContainer when capture mode
  // session is active with `kFullscreen` type. And the preview window should
  // be the top-most child of it.
  EXPECT_EQ(parent, overlay_container);
  EXPECT_EQ(overlay_container->children().back(), preview_window);

  StartRecordingFromSource(CaptureModeSource::kFullscreen);
  // Parent of the preview should be the OverlayContainer when video recording
  // in progress with `kFullscreen` type. And the preview window should be the
  // top-most child of it.
  preview_window = camera_preview_widget->GetNativeWindow();
  parent = preview_window->parent();
  EXPECT_EQ(parent, overlay_container);
  EXPECT_EQ(overlay_container->children().back(), preview_window);
}

TEST_F(CaptureModeCameraTest, CameraPreviewWidgetStackingInRegion) {
  auto* controller =
      StartCaptureSession(CaptureModeSource::kRegion, CaptureModeType::kVideo);
  auto* camera_controller = GetCameraController();
  AddDefaultCamera();
  camera_controller->SetSelectedCamera(CameraId(kDefaultCameraModelId, 1));
  const auto* camera_preview_widget =
      camera_controller->camera_preview_widget();
  EXPECT_TRUE(camera_preview_widget);

  auto* preview_window = camera_preview_widget->GetNativeWindow();
  // Parent of the preview should be the UnparentedContainer when the user
  // capture region is not set.
  EXPECT_TRUE(controller->user_capture_region().IsEmpty());
  EXPECT_EQ(preview_window->parent(),
            preview_window->GetRootWindow()->GetChildById(
                kShellWindowId_UnparentedContainer));
  EXPECT_FALSE(camera_preview_widget->IsVisible());

  controller->SetUserCaptureRegion(gfx::Rect(10, 20, 80, 60),
                                   /*by_user=*/true);
  StartRecordingFromSource(CaptureModeSource::kRegion);
  const auto* overlay_container = preview_window->GetRootWindow()->GetChildById(
      kShellWindowId_OverlayContainer);
  // Parent of the preview should be the OverlayContainer when video recording
  // in progress with `kRegion` type. And the preview window should be the
  // top-most child of it.
  ASSERT_EQ(preview_window->parent(), overlay_container);
  EXPECT_EQ(overlay_container->children().back(), preview_window);
}

TEST_F(CaptureModeCameraTest, CameraPreviewWidgetStackingInWindow) {
  auto* controller =
      StartCaptureSession(CaptureModeSource::kWindow, CaptureModeType::kVideo);
  auto* camera_controller = GetCameraController();
  AddDefaultCamera();
  camera_controller->SetSelectedCamera(CameraId(kDefaultCameraModelId, 1));
  const auto* camera_preview_widget =
      camera_controller->camera_preview_widget();
  EXPECT_TRUE(camera_preview_widget);

  auto* preview_window = camera_preview_widget->GetNativeWindow();
  // The parent of the preview should be the UnparentedContainer when selected
  // window is not set.
  ASSERT_FALSE(controller->capture_mode_session()->GetSelectedWindow());
  EXPECT_EQ(preview_window->parent(),
            preview_window->GetRootWindow()->GetChildById(
                kShellWindowId_UnparentedContainer));
  EXPECT_FALSE(camera_preview_widget->IsVisible());

  StartRecordingFromSource(CaptureModeSource::kWindow);
  // Parent of the preview widget should be the window being recorded when video
  // recording in progress with `kWindow` type. And the preview window should be
  // the top-most child of it.
  const auto* parent = preview_window->parent();
  const auto* window_being_recorded =
      controller->video_recording_watcher_for_testing()
          ->window_being_recorded();
  ASSERT_EQ(parent, window_being_recorded);
  EXPECT_EQ(window_being_recorded->children().back(), preview_window);
}

// Tests the visibility of camera menu when there's no camera connected.
TEST_F(CaptureModeCameraTest, CheckCameraMenuVisibility) {
  StartCaptureSession(CaptureModeSource::kFullscreen, CaptureModeType::kVideo);
  OpenSettingsView();

  // Check camera menu is hidden.
  CaptureModeSettingsTestApi test_api;
  CaptureModeMenuGroup* camera_menu_group = test_api.GetCameraMenuGroup();
  EXPECT_TRUE(camera_menu_group && !camera_menu_group->GetVisible());

  // Connect a camera.
  AddDefaultCamera();

  // Check the camera menu is shown. And there's an `Off` option and an option
  // for the connected camera.
  camera_menu_group = test_api.GetCameraMenuGroup();
  EXPECT_TRUE(camera_menu_group && camera_menu_group->GetVisible());
  EXPECT_TRUE(test_api.GetCameraOption(kCameraOff));
  EXPECT_TRUE(test_api.GetCameraOption(kCameraDevicesBegin));

  // Now disconnect the camera.
  RemoveDefaultCamera();

  // Check the menu group is hidden again and all options has been removed from
  // the menu.
  camera_menu_group = test_api.GetCameraMenuGroup();
  EXPECT_TRUE(camera_menu_group && !camera_menu_group->GetVisible());
  EXPECT_FALSE(test_api.GetCameraOption(kCameraOff));
  EXPECT_FALSE(test_api.GetCameraOption(kCameraDevicesBegin));
}

TEST_F(CaptureModeCameraTest, CameraPreviewWidgetBounds) {
  auto* controller = StartCaptureSession(CaptureModeSource::kFullscreen,
                                         CaptureModeType::kVideo);
  auto* camera_controller = GetCameraController();
  AddDefaultCamera();
  camera_controller->SetSelectedCamera(CameraId(kDefaultCameraModelId, 1));
  ASSERT_EQ(CameraPreviewSnapPosition::kBottomRight,
            camera_controller->camera_preview_snap_position());

  const auto* preview_widget = camera_controller->camera_preview_widget();
  EXPECT_TRUE(preview_widget);

  // When snap position is `kBottomRight` and capture source is `kFullscreen`,
  // the preview should at the bottom right corner of screen.
  const auto* capture_mode_session = controller->capture_mode_session();
  const gfx::Rect screen_bounds =
      capture_mode_session->current_root()->GetBoundsInScreen();
  EXPECT_EQ(preview_widget->GetWindowBoundsInScreen().bottom_right(),
            screen_bounds.bottom_right());

  // Switching to `kRegion` without capture region set, the preview widget
  // should not be shown.
  controller->SetSource(CaptureModeSource::kRegion);
  EXPECT_TRUE(controller->user_capture_region().IsEmpty());
  EXPECT_FALSE(preview_widget->IsVisible());

  // The preview should be shown at the bottom right corner of the capture
  // region when it is set.
  const gfx::Rect capture_region(10, 20, 300, 200);
  controller->SetUserCaptureRegion(capture_region, /*by_user=*/true);
  EXPECT_EQ(preview_widget->GetWindowBoundsInScreen().bottom_right(),
            capture_region.bottom_right());

  // Switching back to `kFullscreen`, the preview should be shown at the bottom
  // right of the screen again.
  controller->SetSource(CaptureModeSource::kFullscreen);
  EXPECT_EQ(preview_widget->GetWindowBoundsInScreen().bottom_right(),
            screen_bounds.bottom_right());

  // Switching back to `kRegion`, the preview should be shown at the bottom
  // right of the current capture region again.
  controller->SetSource(CaptureModeSource::kRegion);
  EXPECT_EQ(preview_widget->GetWindowBoundsInScreen().bottom_right(),
            capture_region.bottom_right());

  // Update the snap position should update the preview to the corresponding
  // position.
  camera_controller->SetCameraPreviewSnapPosition(
      CameraPreviewSnapPosition::kBottomLeft);
  EXPECT_EQ(preview_widget->GetWindowBoundsInScreen().bottom_left(),
            capture_region.bottom_left());

  // Set capture region to empty, the preview should be hidden again.
  controller->SetUserCaptureRegion(gfx::Rect(), /*by_user=*/true);
  EXPECT_FALSE(preview_widget->IsVisible());

  // Switching to `kWindow` and start the video recording. The preview should
  // stay at the bottom left corner of the recording window's bounds.
  StartRecordingFromSource(CaptureModeSource::kWindow);
  const auto* window_being_recorded =
      controller->video_recording_watcher_for_testing()
          ->window_being_recorded();
  DCHECK(window_being_recorded);
  EXPECT_EQ(preview_widget->GetWindowBoundsInScreen().bottom_left(),
            window_being_recorded->GetBoundsInScreen().bottom_left());
}

TEST_F(CaptureModeCameraTest, MultiDisplayCameraPreviewWidgetBounds) {
  UpdateDisplay("800x700,801+0-800x700");

  const gfx::Point point_in_second_display = gfx::Point(1000, 500);
  auto* event_generator = GetEventGenerator();
  MoveMouseToAndUpdateCursorDisplay(point_in_second_display, event_generator);

  // Start the capture session in the second display.
  auto* controller = StartCaptureSession(CaptureModeSource::kFullscreen,
                                         CaptureModeType::kVideo);
  auto* camera_controller = GetCameraController();
  AddDefaultCamera();
  camera_controller->SetSelectedCamera(CameraId(kDefaultCameraModelId, 1));

  const gfx::Rect second_display_bounds(801, 0, 800, 700);
  // The camera preview should reside inside the second display when we start
  // capture session in the second display.
  const auto* preview_widget = camera_controller->camera_preview_widget();
  EXPECT_TRUE(second_display_bounds.Contains(
      preview_widget->GetWindowBoundsInScreen()));

  // Move the capture session to the primary display should move the camera
  // preview to the primary display as well.
  MoveMouseToAndUpdateCursorDisplay(gfx::Point(10, 20), event_generator);
  EXPECT_TRUE(gfx::Rect(0, 0, 800, 700)
                  .Contains(preview_widget->GetWindowBoundsInScreen()));

  // Move back to the second display, switch to `kRegion` and set the capture
  // region. The camera preview should be moved back to the second display and
  // inside the capture region.
  MoveMouseToAndUpdateCursorDisplay(point_in_second_display, event_generator);
  controller->SetSource(CaptureModeSource::kRegion);
  // The capture region set through `controller` is in root coordinate.
  const gfx::Rect capture_region(100, 0, 200, 150);
  controller->SetUserCaptureRegion(capture_region, /*by_user=*/true);
  const gfx::Rect capture_region_in_screen(901, 0, 200, 150);
  const gfx::Rect preview_bounds = preview_widget->GetWindowBoundsInScreen();
  EXPECT_TRUE(second_display_bounds.Contains(preview_bounds));
  EXPECT_TRUE(capture_region_in_screen.Contains(preview_bounds));

  // Start the window recording inside the second display, the camera preview
  // should be inside the window that is being recorded inside the second
  // display.
  window()->SetBoundsInScreen(
      gfx::Rect(900, 0, 300, 200),
      display::Screen::GetScreen()->GetDisplayNearestWindow(
          Shell::GetAllRootWindows()[1]));
  StartRecordingFromSource(CaptureModeSource::kWindow);
  const auto* window_being_recorded =
      controller->video_recording_watcher_for_testing()
          ->window_being_recorded();
  EXPECT_TRUE(window_being_recorded->GetBoundsInScreen().Contains(
      preview_widget->GetWindowBoundsInScreen()));
}

}  // namespace ash
