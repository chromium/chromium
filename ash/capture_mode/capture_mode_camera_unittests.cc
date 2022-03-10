// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/capture_mode_bar_view.h"
#include "ash/capture_mode/capture_mode_camera_controller.h"
#include "ash/capture_mode/capture_mode_constants.h"
#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/capture_mode/capture_mode_menu_group.h"
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
#include "ash/display/window_tree_host_manager.h"
#include "ash/public/cpp/capture_mode/capture_mode_test_api.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/files/file_path.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/system_monitor.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/coordinate_conversion.h"

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

// Returns the current root window where the current capture activities are
// hosted in.
aura::Window* GetCurrentRoot() {
  auto* controller = CaptureModeController::Get();
  if (controller->IsActive())
    return controller->capture_mode_session()->current_root();

  if (controller->is_recording_in_progress()) {
    return controller->video_recording_watcher_for_testing()
        ->window_being_recorded()
        ->GetRootWindow();
  }

  return Shell::GetPrimaryRootWindow();
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

  void DragPreviewToPoint(views::Widget* preview_widget,
                          const gfx::Point& screen_location,
                          bool by_touch_gestures = false,
                          bool drop = true) {
    DCHECK(preview_widget);
    auto* event_generator = GetEventGenerator();
    event_generator->set_current_screen_location(
        preview_widget->GetWindowBoundsInScreen().CenterPoint());
    if (by_touch_gestures) {
      event_generator->PressTouch();
      // Move the touch by an enough amount in X to make sure it generates a
      // serial of gesture scroll events instead of a fling event.
      event_generator->MoveTouchBy(50, 0);
      event_generator->MoveTouch(screen_location);
      if (drop)
        event_generator->ReleaseTouch();
    } else {
      event_generator->PressLeftButton();
      event_generator->MoveMouseTo(screen_location);
      if (drop)
        event_generator->ReleaseLeftButton();
    }
  }

  // Verifies that the camera preview is placed on the correct position based on
  // current preview snap position and the given `confine_bounds_in_screen`.
  void VerifyPreviewAlignment(const gfx::Rect& confine_bounds_in_screen) {
    auto* camera_controller = GetCameraController();
    const auto* preview_widget = camera_controller->camera_preview_widget();
    DCHECK(preview_widget);
    const gfx::Rect camera_preview_bounds =
        preview_widget->GetWindowBoundsInScreen();

    switch (camera_controller->camera_preview_snap_position()) {
      case CameraPreviewSnapPosition::kTopLeft: {
        gfx::Point expect_origin = confine_bounds_in_screen.origin();
        expect_origin.Offset(capture_mode::kSpaceBetweenCameraPreviewAndEdges,
                             capture_mode::kSpaceBetweenCameraPreviewAndEdges);
        EXPECT_EQ(expect_origin, camera_preview_bounds.origin());
        break;
      }
      case CameraPreviewSnapPosition::kBottomLeft: {
        const gfx::Point expect_bottom_left =
            gfx::Point(confine_bounds_in_screen.x() +
                           capture_mode::kSpaceBetweenCameraPreviewAndEdges,
                       confine_bounds_in_screen.bottom() -
                           capture_mode::kSpaceBetweenCameraPreviewAndEdges);
        EXPECT_EQ(expect_bottom_left, camera_preview_bounds.bottom_left());
        break;
      }
      case CameraPreviewSnapPosition::kBottomRight: {
        const gfx::Point expect_bottom_right =
            gfx::Point(confine_bounds_in_screen.right() -
                           capture_mode::kSpaceBetweenCameraPreviewAndEdges,
                       confine_bounds_in_screen.bottom() -
                           capture_mode::kSpaceBetweenCameraPreviewAndEdges);
        EXPECT_EQ(expect_bottom_right, camera_preview_bounds.bottom_right());
        break;
      }
      case CameraPreviewSnapPosition::kTopRight: {
        const gfx::Point expect_top_right =
            gfx::Point(confine_bounds_in_screen.right() -
                           capture_mode::kSpaceBetweenCameraPreviewAndEdges,
                       confine_bounds_in_screen.y() +
                           capture_mode::kSpaceBetweenCameraPreviewAndEdges);
        EXPECT_EQ(expect_top_right, camera_preview_bounds.top_right());
        break;
      }
    }
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

TEST_F(CaptureModeCameraTest, SelectingDifferentCameraCreatesNewPreviewWidget) {
  AddDefaultCamera();
  const std::string device_id = "/dev/video0";
  const std::string display_name = "Integrated Webcam";
  const std::string model_id = "0123:4567";
  AddFakeCamera(device_id, display_name, model_id);

  StartCaptureSession(CaptureModeSource::kFullscreen, CaptureModeType::kVideo);
  auto* camera_controller = GetCameraController();
  EXPECT_FALSE(camera_controller->camera_preview_widget());

  camera_controller->SetSelectedCamera(CameraId(kDefaultCameraModelId, 1));
  auto* current_preview_widget = camera_controller->camera_preview_widget();
  EXPECT_TRUE(current_preview_widget);

  // Selecting a different camera should result in the recreation of the preview
  // widget.
  camera_controller->SetSelectedCamera(CameraId(model_id, 1));
  EXPECT_TRUE(camera_controller->camera_preview_widget());
  EXPECT_NE(current_preview_widget, camera_controller->camera_preview_widget());
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

  // Verifies the camera preview's alignment with `kBottomRight` snap position
  // and `kFullscreen` capture source.
  const auto* capture_mode_session = controller->capture_mode_session();
  const gfx::Rect work_area =
      display::Screen::GetScreen()
          ->GetDisplayNearestWindow(capture_mode_session->current_root())
          .work_area();
  VerifyPreviewAlignment(work_area);

  // Switching to `kRegion` without capture region set, the preview widget
  // should not be shown.
  controller->SetSource(CaptureModeSource::kRegion);
  EXPECT_TRUE(controller->user_capture_region().IsEmpty());
  EXPECT_FALSE(preview_widget->IsVisible());

  // Verifies the camera preview's alignment with `kBottomRight` snap position
  // and `kRegion` capture source.
  const gfx::Rect capture_region(10, 20, 300, 200);
  controller->SetUserCaptureRegion(capture_region, /*by_user=*/true);
  VerifyPreviewAlignment(capture_region);

  // Verifies the camera preview's alignment after switching back to
  // `kFullscreen.`
  controller->SetSource(CaptureModeSource::kFullscreen);
  VerifyPreviewAlignment(work_area);

  // Verifies the camera preview's alignment with `kBottomLeft` snap position
  // and `kRegion` capture source.
  controller->SetSource(CaptureModeSource::kRegion);
  camera_controller->SetCameraPreviewSnapPosition(
      CameraPreviewSnapPosition::kBottomLeft);
  VerifyPreviewAlignment(capture_region);

  // Verifies the camera preview's alignment with `kTopLeft` snap position
  // and `kRegion` capture source.
  camera_controller->SetCameraPreviewSnapPosition(
      CameraPreviewSnapPosition::kTopLeft);
  VerifyPreviewAlignment(capture_region);

  // Verifies the camera preview's alignment with `kTopRight` snap position
  // and `kRegion` capture source.
  camera_controller->SetCameraPreviewSnapPosition(
      CameraPreviewSnapPosition::kTopRight);
  VerifyPreviewAlignment(capture_region);

  // Set capture region to empty, the preview should be hidden again.
  controller->SetUserCaptureRegion(gfx::Rect(), /*by_user=*/true);
  EXPECT_FALSE(preview_widget->IsVisible());

  // Verifies the camera preview's alignment with `kTopRight` snap position and
  // `kWindow` capture source.
  StartRecordingFromSource(CaptureModeSource::kWindow);
  const auto* window_being_recorded =
      controller->video_recording_watcher_for_testing()
          ->window_being_recorded();
  DCHECK(window_being_recorded);
  VerifyPreviewAlignment(window_being_recorded->GetBoundsInScreen());
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
      gfx::Rect(900, 0, 400, 300),
      display::Screen::GetScreen()->GetDisplayNearestWindow(
          Shell::GetAllRootWindows()[1]));
  StartRecordingFromSource(CaptureModeSource::kWindow);
  const auto* window_being_recorded =
      controller->video_recording_watcher_for_testing()
          ->window_being_recorded();
  EXPECT_TRUE(window_being_recorded->GetBoundsInScreen().Contains(
      preview_widget->GetWindowBoundsInScreen()));
}

// Tests that switching from `kImage` to `kVideo` with capture source `kWindow`,
// and capture window is already selected before the switch, the camera preview
// widget should be positioned and parented correctly.
TEST_F(CaptureModeCameraTest, CameraPreviewWidgetAfterTypeSwitched) {
  auto* controller =
      StartCaptureSession(CaptureModeSource::kWindow, CaptureModeType::kImage);
  auto* camera_controller = GetCameraController();
  GetEventGenerator()->MoveMouseToCenterOf(window());

  AddDefaultCamera();
  camera_controller->SetSelectedCamera(CameraId(kDefaultCameraModelId, 1));

  controller->SetType(CaptureModeType::kVideo);
  const auto* camera_preview_widget =
      camera_controller->camera_preview_widget();
  EXPECT_TRUE(camera_preview_widget);
  auto* parent = camera_preview_widget->GetNativeWindow()->parent();
  const auto* selected_window =
      controller->capture_mode_session()->GetSelectedWindow();
  ASSERT_EQ(parent, selected_window);

  // Verify that camera preview is at the bottom right corner of the window.
  VerifyPreviewAlignment(selected_window->GetBoundsInScreen());
}

// Tests that audio and camera menu groups should be hidden from the settings
// menu when there's a video recording in progress.
TEST_F(CaptureModeCameraTest,
       AudioAndCameraMenuGroupsAreHiddenWhenVideoRecordingInProgress) {
  auto* controller = StartCaptureSession(CaptureModeSource::kFullscreen,
                                         CaptureModeType::kVideo);
  auto* camera_controller = controller->camera_controller();
  StartVideoRecordingImmediately();
  EXPECT_TRUE(controller->is_recording_in_progress());

  // Verify there's no camera preview created, since we don't select any camera.
  EXPECT_FALSE(camera_controller->camera_preview_widget());

  // Check capture session is shut down after the video recording starts.
  EXPECT_FALSE(controller->IsActive());

  // Start a new session, check the type should be switched automatically to
  // kImage.
  controller->Start(CaptureModeEntryType::kQuickSettings);
  EXPECT_EQ(CaptureModeType::kImage, controller->type());

  // Verify there's no camera preview created after a new session started.
  EXPECT_FALSE(camera_controller->camera_preview_widget());

  OpenSettingsView();
  // Check the audio and camera menu groups are hidden from the settings.
  CaptureModeSettingsTestApi test_api_new;
  EXPECT_FALSE(test_api_new.GetCameraMenuGroup());
  EXPECT_FALSE(test_api_new.GetAudioInputMenuGroup());
  EXPECT_TRUE(test_api_new.GetSaveToMenuGroup());
}

// Verify that starting a new capture session and updating capture source won't
// affect the current camera preview when there's a video recording is progress.
TEST_F(CaptureModeCameraTest,
       CameraPreviewNotChangeWhenVideoRecordingInProgress) {
  auto* controller = StartCaptureSession(CaptureModeSource::kFullscreen,
                                         CaptureModeType::kVideo);
  auto* camera_controller = controller->camera_controller();
  AddDefaultCamera();
  OpenSettingsView();

  // Select the default camera for video recording.
  CaptureModeSettingsTestApi test_api;
  ClickOnView(test_api.GetCameraOption(kCameraDevicesBegin),
              GetEventGenerator());
  StartVideoRecordingImmediately();

  // Check that the camera preview is created.
  const auto* camera_preview_widget =
      camera_controller->camera_preview_widget();
  EXPECT_TRUE(camera_preview_widget);

  auto* preview_window = camera_preview_widget->GetNativeWindow();
  auto* parent = preview_window->parent();

  // Start a new capture session, and set capture source to `kFullscreen`,
  // verify the camera preview and its parent are not changed.
  controller->Start(CaptureModeEntryType::kQuickSettings);
  controller->SetSource(CaptureModeSource::kFullscreen);
  EXPECT_EQ(preview_window,
            camera_controller->camera_preview_widget()->GetNativeWindow());
  EXPECT_EQ(
      parent,
      camera_controller->camera_preview_widget()->GetNativeWindow()->parent());

  // Update capture source to `kRegion` and set the user capture region, verify
  // the camera preview and its parent are not changed.
  controller->SetSource(CaptureModeSource::kRegion);
  controller->SetUserCaptureRegion({100, 100, 200, 300}, /*by_user=*/true);
  EXPECT_EQ(preview_window,
            camera_controller->camera_preview_widget()->GetNativeWindow());
  EXPECT_EQ(
      parent,
      camera_controller->camera_preview_widget()->GetNativeWindow()->parent());

  // Update capture source to `kWindow` and move mouse on top the `window`,
  // verify that the camera preview and its parent are not changed.
  controller->SetSource(CaptureModeSource::kWindow);
  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseToCenterOf(window());
  EXPECT_EQ(preview_window,
            camera_controller->camera_preview_widget()->GetNativeWindow());
  EXPECT_EQ(
      parent,
      camera_controller->camera_preview_widget()->GetNativeWindow()->parent());
}

// Tests that changing the folder while there's a video recording in progress
// doesn't change the folder where the video being recorded will be saved to.
// It will only affect the image to be captured.
TEST_F(CaptureModeCameraTest, ChangeFolderWhileVideoRecordingInProgress) {
  auto* controller = StartCaptureSession(CaptureModeSource::kFullscreen,
                                         CaptureModeType::kVideo);
  StartVideoRecordingImmediately();

  // While the video recording is in progress, start a new capture session and
  // update the save-to folder to the custom folder.
  controller->Start(CaptureModeEntryType::kQuickSettings);
  controller->SetCustomCaptureFolder(
      CreateCustomFolderInUserDownloadsPath("test"));

  // Perform the image capture. Verify that the image is saved to the custom
  // folder.
  controller->PerformCapture();
  const base::FilePath& saved_image_file = WaitForCaptureFileToBeSaved();
  EXPECT_EQ(controller->GetCustomCaptureFolder(), saved_image_file.DirName());

  // End the video recoring and verify the video is still saved to the default
  // downloads folder.
  controller->EndVideoRecording(EndRecordingReason::kStopRecordingButton);
  const base::FilePath& saved_video_file = WaitForCaptureFileToBeSaved();
  EXPECT_EQ(controller->delegate_for_testing()->GetUserDefaultDownloadsFolder(),
            saved_video_file.DirName());
}

// Tests multiple scenarios to trigger selected window updates at located
// position. Camera preview's native window should be added to the ignore
// windows and no crash should happen in these cases.
TEST_F(CaptureModeCameraTest,
       UpdateSelectedWindowAtPositionWithCameraPreviewIgnored) {
  auto* controller =
      StartCaptureSession(CaptureModeSource::kWindow, CaptureModeType::kVideo);
  AddDefaultCamera();
  auto* camera_controller = GetCameraController();
  camera_controller->SetSelectedCamera(CameraId(kDefaultCameraModelId, 1));
  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseToCenterOf(window());
  EXPECT_TRUE(camera_controller->camera_preview_widget());

  // No camera preview when it is in `kImage`.
  controller->SetType(CaptureModeType::kImage);
  event_generator->MoveMouseToCenterOf(window());
  EXPECT_FALSE(camera_controller->camera_preview_widget());

  // The native window of camera preview widget should be ignored from the
  // candidates of the selected window. So moving the mouse to be on top of the
  // camera preview should not cause any crash or selected window changes.
  controller->SetType(CaptureModeType::kVideo);
  const auto* camera_preview_widget =
      camera_controller->camera_preview_widget();
  const auto* capture_mode_session = controller->capture_mode_session();
  event_generator->MoveMouseToCenterOf(
      camera_preview_widget->GetNativeWindow());
  EXPECT_EQ(window(), capture_mode_session->GetSelectedWindow());
  EXPECT_TRUE(window()->IsVisible());
  EXPECT_TRUE(camera_preview_widget->IsVisible());

  // Hide `window_` with camera preview on should not cause any crash and
  // selected window should be updated to nullptr.
  window()->Hide();
  EXPECT_FALSE(window()->IsVisible());
  EXPECT_FALSE(camera_preview_widget->IsVisible());
  EXPECT_FALSE(capture_mode_session->GetSelectedWindow());

  // Reshow `window_` without hovering over it should not set the selected
  // window. Camera preview should still be hidden as its parent hasn't been set
  // to `window_` yet.
  const auto* preview_native_window = camera_preview_widget->GetNativeWindow();
  window()->Show();
  EXPECT_TRUE(window()->IsVisible());
  EXPECT_FALSE(camera_preview_widget->IsVisible());
  EXPECT_FALSE(capture_mode_session->GetSelectedWindow());
  EXPECT_EQ(preview_native_window->parent(),
            preview_native_window->GetRootWindow()->GetChildById(
                kShellWindowId_UnparentedContainer));

  // Hovering over `window_` should set it to the selected window, camera
  // preview widget should be reparented to it as well. And the camera preview
  // widget should be visible now.
  event_generator->MoveMouseToCenterOf(window());
  EXPECT_EQ(preview_native_window->parent(),
            capture_mode_session->GetSelectedWindow());
  EXPECT_TRUE(camera_preview_widget->IsVisible());
  EXPECT_EQ(window(), capture_mode_session->GetSelectedWindow());
}

class CaptureModeCameraPreviewTest
    : public CaptureModeCameraTest,
      public testing::WithParamInterface<CaptureModeSource> {
 public:
  CaptureModeCameraPreviewTest() = default;
  CaptureModeCameraPreviewTest(const CaptureModeCameraPreviewTest&) = delete;
  CaptureModeCameraPreviewTest& operator=(const CaptureModeCameraPreviewTest&) =
      delete;
  ~CaptureModeCameraPreviewTest() override = default;

  void StartCaptureSessionWithParam() {
    auto* controller = CaptureModeController::Get();
    const gfx::Rect capture_region(10, 20, 1300, 750);
    controller->SetUserCaptureRegion(capture_region, /*by_user=*/true);
    // Set the window's bounds big enough here to make sure after display
    // rotation, the event is located on top of `window_`.
    // TODO(conniekxu): investigate why the position of the event received is
    // different than the position we pass.
    window()->SetBounds({30, 40, 1300, 750});

    StartCaptureSession(GetParam(), CaptureModeType::kVideo);
    if (GetParam() == CaptureModeSource::kWindow)
      GetEventGenerator()->MoveMouseToCenterOf(window());
  }

  // Based on the `CaptureModeSource`, it returns the current capture region's
  // bounds in screen.
  gfx::Rect GetCaptureBoundsInScreen() {
    auto* controller = CaptureModeController::Get();
    auto* root = GetCurrentRoot();

    switch (GetParam()) {
      case CaptureModeSource::kFullscreen:
        return display::Screen::GetScreen()
            ->GetDisplayNearestWindow(root)
            .work_area();

      case CaptureModeSource::kRegion: {
        auto* recording_watcher =
            controller->video_recording_watcher_for_testing();
        gfx::Rect capture_region =
            controller->is_recording_in_progress()
                ? recording_watcher->GetEffectivePartialRegionBounds()
                : controller->user_capture_region();
        wm::ConvertRectToScreen(root, &capture_region);
        return capture_region;
      }

      case CaptureModeSource::kWindow:
        return window()->GetBoundsInScreen();
    }
  }

  // Returns the cursor type when cursor is on top of the current capture
  // surface.
  ui::mojom::CursorType GetCursorTypeOnCaptureSurface() {
    DCHECK(CaptureModeController::Get()->IsActive());

    switch (GetParam()) {
      case CaptureModeSource::kFullscreen:
      case CaptureModeSource::kWindow:
        return ui::mojom::CursorType::kCustom;
      case CaptureModeSource::kRegion:
        return ui::mojom::CursorType::kMove;
    }
  }
};

// Tests that camera preview's bounds is updated after display rotations with
// two use cases, when capture session is active and when there's a video
// recording in progress.
TEST_P(CaptureModeCameraPreviewTest, DisplayRotation) {
  StartCaptureSessionWithParam();
  auto* camera_controller = GetCameraController();
  AddDefaultCamera();
  camera_controller->SetSelectedCamera(CameraId(kDefaultCameraModelId, 1));

  // Verify that the camera preview should be at the bottom right corner of
  // capture bounds.
  VerifyPreviewAlignment(GetCaptureBoundsInScreen());

  // Rotate the primary display by 90 degrees. Verify that the camera preview is
  // still at the bottom right corner of capture bounds.
  Shell::Get()->display_manager()->SetDisplayRotation(
      WindowTreeHostManager::GetPrimaryDisplayId(), display::Display::ROTATE_90,
      display::Display::RotationSource::USER);
  VerifyPreviewAlignment(GetCaptureBoundsInScreen());

  // Start video recording, verify camera preview's bounds is updated after
  // display rotations when there's a video recording in progress.
  StartVideoRecordingImmediately();
  EXPECT_FALSE(CaptureModeController::Get()->IsActive());

  // Rotate the primary display by 180 degrees. Verify that the camera preview
  // is still at the bottom right corner of capture bounds.
  Shell::Get()->display_manager()->SetDisplayRotation(
      WindowTreeHostManager::GetPrimaryDisplayId(),
      display::Display::ROTATE_180, display::Display::RotationSource::USER);
  VerifyPreviewAlignment(GetCaptureBoundsInScreen());

  // Rotate the primary display by 270 degrees. Verify that the camera preview
  // is still at the bottom right corner of capture bounds.
  Shell::Get()->display_manager()->SetDisplayRotation(
      WindowTreeHostManager::GetPrimaryDisplayId(),
      display::Display::ROTATE_270, display::Display::RotationSource::USER);
  VerifyPreviewAlignment(GetCaptureBoundsInScreen());
}

// Tests that when camera preview is being dragged, at the end of the drag, it
// should be snapped to the correct snap position. It tests two use cases, when
// capture session is active and when there's a video recording in progress
// including drag to snap by mouse and by touch.
TEST_P(CaptureModeCameraPreviewTest, CameraPreviewDragToSnap) {
  StartCaptureSessionWithParam();
  auto* camera_controller = GetCameraController();
  AddDefaultCamera();
  camera_controller->SetSelectedCamera(CameraId(kDefaultCameraModelId, 1));
  auto* preview_widget = camera_controller->camera_preview_widget();
  const gfx::Point capture_bounds_center_point =
      GetCaptureBoundsInScreen().CenterPoint();

  // Verify that by default the snap position should be `kBottomRight` and
  // camera preview is placed at the correct position.
  EXPECT_EQ(CameraPreviewSnapPosition::kBottomRight,
            camera_controller->camera_preview_snap_position());
  VerifyPreviewAlignment(GetCaptureBoundsInScreen());

  // Drag and drop camera preview by mouse to the top right of the
  // `capture_bounds_center_point`, verify that camera preview is snapped to the
  // top right with correct position.
  DragPreviewToPoint(preview_widget, {capture_bounds_center_point.x() + 20,
                                      capture_bounds_center_point.y() - 20});
  EXPECT_EQ(CameraPreviewSnapPosition::kTopRight,
            camera_controller->camera_preview_snap_position());
  VerifyPreviewAlignment(GetCaptureBoundsInScreen());

  // Now drag and drop camera preview by touch to the top left of the center
  // point, verify that camera preview is snapped to the top left with correct
  // position.
  DragPreviewToPoint(preview_widget,
                     {capture_bounds_center_point.x() - 20,
                      capture_bounds_center_point.y() - 20},
                     /*by_touch_gestures=*/true);
  EXPECT_EQ(CameraPreviewSnapPosition::kTopLeft,
            camera_controller->camera_preview_snap_position());
  VerifyPreviewAlignment(GetCaptureBoundsInScreen());

  // Start video recording, verify camera preview is snapped to the correct snap
  // position at the end of drag when there's a video recording in progress.
  StartVideoRecordingImmediately();
  EXPECT_FALSE(CaptureModeController::Get()->IsActive());

  // Drag and drop camera preview by mouse to the bottom left of the center
  // point, verify that camera preview is snapped to the bottom left with
  // correct position.
  DragPreviewToPoint(preview_widget, {capture_bounds_center_point.x() - 20,
                                      capture_bounds_center_point.y() + 20});
  EXPECT_EQ(CameraPreviewSnapPosition::kBottomLeft,
            camera_controller->camera_preview_snap_position());
  VerifyPreviewAlignment(GetCaptureBoundsInScreen());

  // Now drag and drop camera preview by touch to the bottom right of the center
  // point, verify that camera preview is snapped to the bottom right with
  // correct position.
  DragPreviewToPoint(preview_widget,
                     {capture_bounds_center_point.x() + 20,
                      capture_bounds_center_point.y() + 20},
                     /*by_touch_gestures=*/true);
  EXPECT_EQ(CameraPreviewSnapPosition::kBottomRight,
            camera_controller->camera_preview_snap_position());
  VerifyPreviewAlignment(GetCaptureBoundsInScreen());
}

TEST_P(CaptureModeCameraPreviewTest, CameraPreviewDragToSnapOnMultipleDisplay) {
  UpdateDisplay("800x700,801+0-800x700");

  const gfx::Point point_in_second_display = gfx::Point(1000, 500);
  auto* event_generator = GetEventGenerator();
  MoveMouseToAndUpdateCursorDisplay(point_in_second_display, event_generator);

  // Start capture mode on the second display.
  StartCaptureSessionWithParam();
  auto* camera_controller = GetCameraController();
  AddDefaultCamera();
  camera_controller->SetSelectedCamera(CameraId(kDefaultCameraModelId, 1));
  auto* preview_widget = camera_controller->camera_preview_widget();
  const gfx::Point capture_bounds_center_point =
      GetCaptureBoundsInScreen().CenterPoint();

  // Drag and drop camera preview by mouse to the top right of the
  // `capture_bounds_center_point`, verify that camera preview is snapped to the
  // top right with correct position.
  DragPreviewToPoint(preview_widget, {capture_bounds_center_point.x() + 20,
                                      capture_bounds_center_point.y() - 20});
  EXPECT_EQ(CameraPreviewSnapPosition::kTopRight,
            camera_controller->camera_preview_snap_position());
  VerifyPreviewAlignment(GetCaptureBoundsInScreen());
}

// Tests that when there's a video recording is in progress, start a new
// capture session will make camera preview not draggable.
TEST_P(CaptureModeCameraPreviewTest,
       DragPreviewInNewCaptureSessionWhileVideoRecordingInProgress) {
  StartCaptureSessionWithParam();
  auto* camera_controller = GetCameraController();
  AddDefaultCamera();
  camera_controller->SetSelectedCamera(CameraId(kDefaultCameraModelId, 1));
  auto* preview_widget = camera_controller->camera_preview_widget();
  const gfx::Point capture_bounds_center_point =
      GetCaptureBoundsInScreen().CenterPoint();

  StartVideoRecordingImmediately();
  EXPECT_FALSE(CaptureModeController::Get()->IsActive());
  // Start a new capture session while a video recording is in progress.
  auto* controller = CaptureModeController::Get();
  controller->Start(CaptureModeEntryType::kQuickSettings);

  const gfx::Rect preview_bounds_in_screen_before_drag =
      preview_widget->GetWindowBoundsInScreen();
  const auto snap_position_before_drag =
      camera_controller->camera_preview_snap_position();
  // Verify by default snap position is `kBottomRight`.
  EXPECT_EQ(snap_position_before_drag, CameraPreviewSnapPosition::kBottomRight);

  // Try to drag camera preview by mouse without dropping it, verify camera
  // preview is not draggable and its position is not changed.
  DragPreviewToPoint(preview_widget,
                     {preview_bounds_in_screen_before_drag.x() + 20,
                      preview_bounds_in_screen_before_drag.y() + 20},
                     /*by_touch_gestures=*/false,
                     /*drop=*/false);
  EXPECT_EQ(preview_widget->GetWindowBoundsInScreen(),
            preview_bounds_in_screen_before_drag);

  // Try to drag and drop camera preview by touch to the top left of the current
  // capture bounds' center point, verity it's not moved. Also verify the snap
  // position is not updated.
  DragPreviewToPoint(preview_widget,
                     {capture_bounds_center_point.x() - 20,
                      capture_bounds_center_point.y() - 20},
                     /*by_touch_gestures=*/true);
  EXPECT_EQ(preview_widget->GetWindowBoundsInScreen(),
            preview_bounds_in_screen_before_drag);
  EXPECT_EQ(camera_controller->camera_preview_snap_position(),
            snap_position_before_drag);
}

// Tests that when mouse event is on top of camera preview, cursor type should
// be updated accordingly.
TEST_P(CaptureModeCameraPreviewTest, CursorTypeUpdates) {
  StartCaptureSessionWithParam();
  auto* camera_controller = GetCameraController();
  AddDefaultCamera();
  camera_controller->SetSelectedCamera(CameraId(kDefaultCameraModelId, 1));

  auto* preview_widget = camera_controller->camera_preview_widget();
  const gfx::Rect preview_bounds_in_screen =
      preview_widget->GetWindowBoundsInScreen();
  const gfx::Point camera_preview_center_point =
      preview_bounds_in_screen.CenterPoint();
  const gfx::Point camera_preview_origin_point =
      preview_bounds_in_screen.origin();
  auto* event_generator = GetEventGenerator();

  // Verify that moving mouse on camera preview will update the cursor type to
  // `kPointer`.
  auto* cursor_manager = Shell::Get()->cursor_manager();
  event_generator->MoveMouseTo(camera_preview_center_point);
  EXPECT_EQ(cursor_manager->GetCursor(), ui::mojom::CursorType::kPointer);

  // Move mouse from camera preview to capture surface, verify cursor type is
  // updated to the correct type of the current capture source.
  event_generator->MoveMouseTo({camera_preview_origin_point.x() - 10,
                                camera_preview_origin_point.y() - 10});
  EXPECT_EQ(cursor_manager->GetCursor(), GetCursorTypeOnCaptureSurface());

  // Drag camera preview, verify that cursor type is updated to `kPointer`.
  DragPreviewToPoint(preview_widget,
                     {camera_preview_center_point.x() - 10,
                      camera_preview_center_point.y() - 10},
                     /*by_touch_gestures=*/false,
                     /*drop=*/false);
  EXPECT_EQ(cursor_manager->GetCursor(), ui::mojom::CursorType::kPointer);

  // Continue dragging and then drop camera preview, make sure cursor's position
  // is outside of camera preview after it's snapped. Verify cursor type is
  // updated to the correct type of the current capture source.
  DragPreviewToPoint(preview_widget, {camera_preview_origin_point.x() - 20,
                                      camera_preview_origin_point.y() - 20});
  EXPECT_EQ(cursor_manager->GetCursor(), GetCursorTypeOnCaptureSurface());
}

INSTANTIATE_TEST_SUITE_P(All,
                         CaptureModeCameraPreviewTest,
                         testing::Values(CaptureModeSource::kFullscreen,
                                         CaptureModeSource::kRegion,
                                         CaptureModeSource::kWindow));

}  // namespace ash
