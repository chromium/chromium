// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accessibility/accessibility_controller.h"
#include "ash/accessibility/autoclick/autoclick_controller.h"
#include "ash/capture_mode/capture_mode_bar_view.h"
#include "ash/capture_mode/capture_mode_camera_controller.h"
#include "ash/capture_mode/capture_mode_camera_preview_view.h"
#include "ash/capture_mode/capture_mode_constants.h"
#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/capture_mode/capture_mode_menu_group.h"
#include "ash/capture_mode/capture_mode_metrics.h"
#include "ash/capture_mode/capture_mode_session.h"
#include "ash/capture_mode/capture_mode_session_focus_cycler.h"
#include "ash/capture_mode/capture_mode_session_test_api.h"
#include "ash/capture_mode/capture_mode_settings_test_api.h"
#include "ash/capture_mode/capture_mode_settings_view.h"
#include "ash/capture_mode/capture_mode_test_util.h"
#include "ash/capture_mode/capture_mode_toast_controller.h"
#include "ash/capture_mode/capture_mode_types.h"
#include "ash/capture_mode/capture_mode_util.h"
#include "ash/capture_mode/fake_camera_device.h"
#include "ash/capture_mode/fake_folder_selection_dialog_factory.h"
#include "ash/capture_mode/fake_video_source_provider.h"
#include "ash/capture_mode/test_capture_mode_delegate.h"
#include "ash/constants/ash_features.h"
#include "ash/display/window_tree_host_manager.h"
#include "ash/public/cpp/capture_mode/capture_mode_test_api.h"
#include "ash/public/cpp/holding_space/holding_space_controller.h"
#include "ash/public/cpp/holding_space/holding_space_model.h"
#include "ash/public/cpp/holding_space/holding_space_prefs.h"
#include "ash/public/cpp/holding_space/holding_space_test_api.h"
#include "ash/public/cpp/holding_space/holding_space_util.h"
#include "ash/public/cpp/holding_space/mock_holding_space_client.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/tab_slider_button.h"
#include "ash/system/accessibility/autoclick_menu_bubble_controller.h"
#include "ash/system/notification_center/notification_center_test_api.h"
#include "ash/system/notification_center/notification_center_tray.h"
#include "ash/system/privacy/privacy_indicators_controller.h"
#include "ash/system/privacy/privacy_indicators_tray_item_view.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_util.h"
#include "ash/wm/window_state.h"
#include "base/ranges/algorithm.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/system_monitor.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/timer/timer.h"
#include "cc/paint/skia_paint_canvas.h"
#include "chromeos/ui/frame/frame_header.h"
#include "components/viz/test/test_in_process_context_provider.h"
#include "media/base/video_facing.h"
#include "media/base/video_frame.h"
#include "media/renderers/paint_canvas_video_renderer.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_types.h"
#include "ui/compositor/compositor_switches.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/message_center/message_center.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/coordinate_conversion.h"
#include "ui/wm/core/window_util.h"

namespace ash {

namespace {

constexpr char kTestUser[] = "user@test";

// The app ID used for the capture mode camera and microphone recording privacy
// indicators.
constexpr char kCaptureModePrivacyIndicatorId[] = "system-capture-mode";

// The minimum length (i.e. either width or height) of the user-selected region
// such that the camera preview can be visible, and intersecting with the
// capture button when it's positioned in the center of the region, regardless
// of whether the recording type drop down button is visible or not.
constexpr int kMinRegionLengthForCameraToIntersectLabelButton =
    capture_mode::kMinCaptureSurfaceShortSideLengthForVisibleCamera + 20;

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

bool IsWindowStackedRightBelow(aura::Window* window, aura::Window* sibling) {
  DCHECK_EQ(window->parent(), sibling->parent());
  const auto& children = window->parent()->children();
  const int sibling_index =
      base::ranges::find(children, sibling) - children.begin();
  return sibling_index > 0 && children[sibling_index - 1] == window;
}

gfx::Rect GetTooSmallToFitCameraRegion() {
  return {100, 100,
          capture_mode::kMinCaptureSurfaceShortSideLengthForVisibleCamera - 1,
          capture_mode::kMinCaptureSurfaceShortSideLengthForVisibleCamera - 1};
}

}  // namespace

class CaptureModeCameraTest : public AshTestBase {
 public:
  CaptureModeCameraTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  CaptureModeCameraTest(const CaptureModeCameraTest&) = delete;
  CaptureModeCameraTest& operator=(const CaptureModeCameraTest&) = delete;
  ~CaptureModeCameraTest() override = default;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();
    FakeFolderSelectionDialogFactory::Start();
    window_ = CreateTestWindow(gfx::Rect(30, 40, 600, 500));
  }

  void TearDown() override {
    window_.reset();
    FakeFolderSelectionDialogFactory::Stop();
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
    BaseCaptureModeSession* session =
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

  CameraPreviewResizeButton* GetPreviewResizeButton() const {
    return GetCameraController()->camera_preview_view()->resize_button();
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

  // Verifies that the icon image and the tooltip of the resize button gets
  // updated correctly when pressed.
  void VerifyResizeButton(bool is_collapsed,
                          CameraPreviewResizeButton* resize_button) {
    SkColor color =
        resize_button->GetColorProvider()->GetColor(kColorAshIconColorPrimary);
    const gfx::ImageSkia collapse_icon_image =
        gfx::CreateVectorIcon(kCaptureModeCameraPreviewCollapseIcon, color);
    const gfx::ImageSkia expand_icon_image =
        gfx::CreateVectorIcon(kCaptureModeCameraPreviewExpandIcon, color);

    const SkBitmap* expected_icon = is_collapsed ? expand_icon_image.bitmap()
                                                 : collapse_icon_image.bitmap();
    const SkBitmap* actual_icon =
        resize_button->GetImage(views::ImageButton::ButtonState::STATE_NORMAL)
            .bitmap();
    EXPECT_TRUE(gfx::test::AreBitmapsEqual(*actual_icon, *expected_icon));

    const auto expected_tooltip_text = l10n_util::GetStringUTF16(
        is_collapsed ? IDS_ASH_SCREEN_CAPTURE_TOOLTIP_EXPAND_SELFIE_CAMERA
                     : IDS_ASH_SCREEN_CAPTURE_TOOLTIP_COLLAPSE_SELFIE_CAMERA);
    EXPECT_EQ(resize_button->GetTooltipText(), expected_tooltip_text);
  }

  // Select capture region by pressing and dragging the mouse.
  void SelectCaptureRegion(const gfx::Rect& region, bool release_mouse = true) {
    auto* controller = CaptureModeController::Get();
    ASSERT_TRUE(controller->IsActive());
    ASSERT_EQ(CaptureModeSource::kRegion, controller->source());
    auto* event_generator = GetEventGenerator();
    event_generator->set_current_screen_location(region.origin());
    event_generator->PressLeftButton();
    event_generator->MoveMouseTo(region.bottom_right());
    if (release_mouse)
      event_generator->ReleaseLeftButton();
    EXPECT_EQ(region, controller->user_capture_region());
  }

  void ConvertToPipWindow(aura::Window* pip_window) {
    WindowState* window_state = WindowState::Get(pip_window);
    DCHECK(!window_state->IsPip());
    views::Widget::GetWidgetForNativeWindow(pip_window)
        ->SetZOrderLevel(ui::ZOrderLevel::kFloatingWindow);
    pip_window->SetProperty(kWindowPipTypeKey, true);
    DCHECK(window_state->IsPip());
  }

  bool IsCameraIndicatorIconVisible() const {
    auto* indicator_view = GetPrimaryDisplayPrivacyIndicatorsView();
    return indicator_view && indicator_view->GetVisible() &&
           PrivacyIndicatorsController::Get()->IsCameraUsed() &&
           indicator_view->camera_icon()->GetVisible();
  }

  bool IsMicrophoneIndicatorIconVisible() const {
    auto* indicator_view = GetPrimaryDisplayPrivacyIndicatorsView();
    return indicator_view && indicator_view->GetVisible() &&
           PrivacyIndicatorsController::Get()->IsMicrophoneUsed() &&
           indicator_view->microphone_icon()->GetVisible();
  }

  PrivacyIndicatorsTrayItemView* GetPrimaryDisplayPrivacyIndicatorsView()
      const {
    return Shell::GetPrimaryRootWindowController()
        ->GetStatusAreaWidget()
        ->notification_center_tray()
        ->privacy_indicators_view();
  }

 private:
  std::unique_ptr<aura::Window> window_;
};

TEST_F(CaptureModeCameraTest, SizeSpecsBigEnoughRegion) {
  gfx::Size confine_bounds_size(800, 700);
  {
    auto specs = capture_mode_util::CalculateCameraPreviewSizeSpecs(
        confine_bounds_size,
        /*is_collapsed=*/false);
    EXPECT_TRUE(specs.is_collapsible);
    EXPECT_TRUE(specs.should_be_visible);
    EXPECT_EQ(specs.size.width(), specs.size.height());
    EXPECT_EQ(specs.size.width(),
              700 / capture_mode::kCaptureSurfaceShortSideDivider);
  }

  // Transposing the confine bounds (e.g. due to rotation) should have no effect
  // since we always consider the shorter side.
  confine_bounds_size.Transpose();
  {
    auto specs = capture_mode_util::CalculateCameraPreviewSizeSpecs(
        confine_bounds_size,
        /*is_collapsed=*/false);
    EXPECT_TRUE(specs.is_collapsible);
    EXPECT_TRUE(specs.should_be_visible);
    EXPECT_EQ(specs.size.width(),
              700 / capture_mode::kCaptureSurfaceShortSideDivider);
  }

  {
    auto specs = capture_mode_util::CalculateCameraPreviewSizeSpecs(
        confine_bounds_size,
        /*is_collapsed=*/true);
    EXPECT_TRUE(specs.is_collapsible);
    EXPECT_TRUE(specs.should_be_visible);
    EXPECT_EQ(specs.size.width(), capture_mode::kMinCameraPreviewDiameter);
  }
}

TEST_F(CaptureModeCameraTest, SizeSpecsNotCollapsible) {
  gfx::Size confine_bounds_size(800, 500);
  auto specs = capture_mode_util::CalculateCameraPreviewSizeSpecs(
      confine_bounds_size,
      /*is_collapsed=*/false);
  EXPECT_FALSE(specs.is_collapsible);
  EXPECT_TRUE(specs.should_be_visible);
  EXPECT_EQ(specs.size.width(), specs.size.height());
  EXPECT_EQ(specs.size.width(),
            500 / capture_mode::kCaptureSurfaceShortSideDivider);
  EXPECT_FALSE(specs.is_surface_too_small);
}

TEST_F(CaptureModeCameraTest, SizeSpecsHiddenPreview) {
  gfx::Size confine_bounds_size(800, 170);
  auto specs = capture_mode_util::CalculateCameraPreviewSizeSpecs(
      confine_bounds_size,
      /*is_collapsed=*/false);
  EXPECT_FALSE(specs.is_collapsible);
  EXPECT_FALSE(specs.should_be_visible);
  EXPECT_EQ(specs.size.width(), specs.size.height());
  EXPECT_EQ(specs.size.width(), capture_mode::kMinCameraPreviewDiameter);
  EXPECT_TRUE(specs.is_surface_too_small);
}

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

TEST_F(CaptureModeCameraTest, CameraFramesFlipping) {
  StartCaptureSession(CaptureModeSource::kFullscreen, CaptureModeType::kVideo);
  auto* camera_controller = GetCameraController();
  int index = 0;
  for (const auto facing_mode :
       {media::MEDIA_VIDEO_FACING_NONE, media::MEDIA_VIDEO_FACING_USER,
        media::MEDIA_VIDEO_FACING_ENVIRONMENT}) {
    const std::string device_id = base::StringPrintf("/dev/video%d", index);
    const std::string display_name = base::StringPrintf("Camera %d", index);
    AddFakeCamera(device_id, display_name, display_name, facing_mode);
    camera_controller->SetSelectedCamera(CameraId(display_name, 1));
    EXPECT_TRUE(camera_controller->camera_preview_widget());
    const bool should_be_flipped =
        facing_mode != media::MEDIA_VIDEO_FACING_ENVIRONMENT;
    EXPECT_EQ(should_be_flipped, camera_controller->camera_preview_view()
                                     ->should_flip_frames_horizontally())
        << "Failed for facing mode: " << facing_mode;
    ++index;
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

  // Re-add the camera during the grace period, the timer should stop, and the
  // preview should reshow.
  AddDefaultCamera();
  EXPECT_FALSE(timer->IsRunning());
  EXPECT_TRUE(camera_controller->camera_preview_widget());
}

// Tests a flaky camera that disconnects before recording begins and reconnects
// during recording within the grace period.
TEST_F(CaptureModeCameraTest, ReconnectDuringGracePeriodAfterRecordingStarts) {
  StartCaptureSession(CaptureModeSource::kFullscreen, CaptureModeType::kVideo);
  auto* camera_controller = AddAndRemoveCameraAndTriggerGracePeriod();
  base::OneShotTimer* timer =
      camera_controller->camera_reconnect_timer_for_test();
  EXPECT_TRUE(timer->IsRunning());
  EXPECT_FALSE(camera_controller->camera_preview_widget());

  // Start recording, and expect that `should_show_preview_` will change to
  // false.
  EXPECT_TRUE(camera_controller->should_show_preview());
  StartVideoRecordingImmediately();
  EXPECT_FALSE(camera_controller->should_show_preview());

  // Re-add the camera during the grace period, the timer should stop, but the
  // preview should not be recreated.
  AddDefaultCamera();
  EXPECT_FALSE(timer->IsRunning());
  EXPECT_FALSE(camera_controller->camera_preview_widget());
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

TEST_F(CaptureModeCameraTest, ManagedByPolicyCameraOptions) {
  GetTestDelegate()->set_is_camera_disabled_by_policy(true);

  StartCaptureSession(CaptureModeSource::kFullscreen, CaptureModeType::kVideo);
  OpenSettingsView();

  // At this moment, there are no camera devices connected. The camera menu
  // group should be hidden.
  CaptureModeSettingsTestApi test_api;
  CaptureModeMenuGroup* camera_menu_group = test_api.GetCameraMenuGroup();
  ASSERT_TRUE(camera_menu_group);
  EXPECT_FALSE(camera_menu_group->GetVisible());

  // Camera addition/removal are still observed even when managed by policy, but
  // once a camera is added, the group becomes visible, but shows only a dimmed
  // "Off" option.
  AddDefaultCamera();
  EXPECT_TRUE(camera_menu_group->GetVisible());
  EXPECT_TRUE(camera_menu_group->IsOptionChecked(kCameraOff));
  EXPECT_FALSE(camera_menu_group->IsOptionEnabled(kCameraOff));
  EXPECT_FALSE(test_api.GetCameraOption(kCameraDevicesBegin));

  // Selecting a camera will be ignored.
  auto* camera_controller = GetCameraController();
  camera_controller->SetSelectedCamera(CameraId(kDefaultCameraModelId, 1));
  EXPECT_FALSE(camera_controller->selected_camera().is_valid());
  EXPECT_TRUE(camera_menu_group->IsOptionChecked(kCameraOff));
  EXPECT_FALSE(camera_controller->camera_preview_widget());

  // Removing the existing camera should hide the camera menu group and remove
  // all its options.
  RemoveDefaultCamera();
  EXPECT_FALSE(camera_menu_group->GetVisible());
  EXPECT_FALSE(test_api.GetCameraOption(kCameraOff));
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
  // Note that `device_id_1` ("/dev/video0") comes before `device_id_2`
  // ("/dev/video1") in sort order, so it will be added first in the menu.
  EXPECT_TRUE(test_api.GetCameraOption(kCameraDevicesBegin));
  EXPECT_TRUE(test_api.GetCameraOption(kCameraDevicesBegin + 1));
  EXPECT_FALSE(camera_menu_group->IsOptionChecked(kCameraOff));
  EXPECT_TRUE(camera_menu_group->IsOptionChecked(kCameraDevicesBegin));
  EXPECT_FALSE(camera_menu_group->IsOptionChecked(kCameraDevicesBegin + 1));
  EXPECT_EQ(base::UTF16ToUTF8(camera_menu_group->GetOptionLabelForTesting(
                kCameraDevicesBegin)),
            display_name_1);
  EXPECT_EQ(base::UTF16ToUTF8(camera_menu_group->GetOptionLabelForTesting(
                kCameraDevicesBegin + 1)),
            display_name_2);
  EXPECT_TRUE(camera_controller->selected_camera().is_valid());
}

TEST_F(CaptureModeCameraTest, CameraPreviewWidgetStackingInFullscreen) {
  StartCaptureSession(CaptureModeSource::kFullscreen, CaptureModeType::kVideo);
  auto* camera_controller = GetCameraController();
  AddDefaultCamera();
  camera_controller->SetSelectedCamera(CameraId(kDefaultCameraModelId, 1));
  base::RunLoop().RunUntilIdle();
  const auto* camera_preview_widget =
      camera_controller->camera_preview_widget();
  EXPECT_TRUE(camera_preview_widget);

  auto* preview_window = camera_preview_widget->GetNativeWindow();
  const auto* menu_container = preview_window->GetRootWindow()->GetChildById(
      kShellWindowId_MenuContainer);
  auto* parent = preview_window->parent();
  // Parent of the preview should be the MenuContainer when capture mode
  // session is active with `kFullscreen` type. And the preview window should
  // be the bottom-most child of it.
  EXPECT_EQ(parent, menu_container);
  EXPECT_EQ(menu_container->children().front(), preview_window);

  StartRecordingFromSource(CaptureModeSource::kFullscreen);
  // Parent of the preview should be the MenuContainer when video recording
  // in progress with `kFullscreen` type. And the preview window should be the
  // top-most child of it.
  preview_window = camera_preview_widget->GetNativeWindow();
  parent = preview_window->parent();
  EXPECT_EQ(parent, menu_container);
  EXPECT_EQ(menu_container->children().back(), preview_window);
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
  const auto* menu_container = preview_window->GetRootWindow()->GetChildById(
      kShellWindowId_MenuContainer);
  // Parent of the preview should be the MenuContainer when video recording
  // in progress with `kRegion` type. And the preview window should be the
  // top-most child of it.
  ASSERT_EQ(preview_window->parent(), menu_container);
  EXPECT_EQ(menu_container->children().back(), preview_window);
}

// Tests that camera preview widget is shown, hidden and parented correctly
// while moving, dragging and updating the user selection region.
TEST_F(CaptureModeCameraTest, CameraPreviewWhileUpdatingCaptureRegion) {
  UpdateDisplay("800x700");
  auto* controller =
      StartCaptureSession(CaptureModeSource::kRegion, CaptureModeType::kVideo);
  auto* camera_controller = GetCameraController();
  auto* capture_session = controller->capture_mode_session();
  AddDefaultCamera();
  camera_controller->SetSelectedCamera(CameraId(kDefaultCameraModelId, 1));
  const auto* camera_preview_widget =
      camera_controller->camera_preview_widget();
  EXPECT_TRUE(camera_preview_widget);
  auto* preview_window = camera_preview_widget->GetNativeWindow();

  const int min_region_length =
      capture_mode::kMinCaptureSurfaceShortSideLengthForVisibleCamera;
  const gfx::Rect capture_region(10, 20, min_region_length, min_region_length);
  controller->SetUserCaptureRegion(capture_region, /*by_user=*/true);

  // After user capture region is set, parent of the preview should be the
  // MenuContainer.
  const auto* menu_container = preview_window->GetRootWindow()->GetChildById(
      kShellWindowId_MenuContainer);
  ASSERT_EQ(preview_window->parent(), menu_container);
  EXPECT_TRUE(camera_preview_widget->IsVisible());
  EXPECT_TRUE(preview_window->IsVisible());

  // Press the bottom right of selection region. Verify preview is hidden and
  // it's still parented to `menu_container`.
  auto* event_generator = GetEventGenerator();
  event_generator->set_current_screen_location(capture_region.bottom_right());
  event_generator->PressLeftButton();
  EXPECT_FALSE(camera_preview_widget->IsVisible());
  EXPECT_FALSE(preview_window->IsVisible());
  ASSERT_EQ(preview_window->parent(), menu_container);

  // Move mouse to update the selection region. Verify preview is still
  // hidden.
  const gfx::Vector2d delta(15, 20);
  event_generator->MoveMouseTo(capture_region.bottom_right() + delta);
  EXPECT_TRUE(capture_session->is_drag_in_progress());
  EXPECT_FALSE(camera_preview_widget->IsVisible());
  EXPECT_FALSE(preview_window->IsVisible());
  ASSERT_EQ(preview_window->parent(), menu_container);

  // Now release the drag to end selection region update. Verify preview is
  // shown and parent of the preview should be MenuContainer.
  event_generator->ReleaseLeftButton();
  EXPECT_FALSE(capture_session->is_drag_in_progress());
  EXPECT_TRUE(camera_preview_widget->IsVisible());
  EXPECT_TRUE(preview_window->IsVisible());
  EXPECT_EQ(preview_window->parent(), menu_container);

  // Press in the selection region to move it around. Since in the
  // use case, selection region is not updated, preview should not be hidden.
  const gfx::Point current_position(capture_region.origin() + delta);
  event_generator->set_current_screen_location(current_position);
  event_generator->PressLeftButton();
  EXPECT_TRUE(camera_preview_widget->IsVisible());
  EXPECT_EQ(preview_window->parent(), menu_container);

  // Move mouse to move selection region around. Verify preview is shown.
  event_generator->MoveMouseTo(current_position + delta);
  EXPECT_TRUE(camera_preview_widget->IsVisible());
  EXPECT_EQ(preview_window->parent(), menu_container);

  // Now release the move to end moving selection region. Verify preview is
  // shown.
  event_generator->ReleaseLeftButton();
  EXPECT_TRUE(camera_preview_widget->IsVisible());
  EXPECT_EQ(preview_window->parent(), menu_container);
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
  auto* window_being_recorded =
      controller->video_recording_watcher_for_testing()
          ->window_being_recorded();
  DCHECK(window_being_recorded);
  auto window_confine_bounds =
      capture_mode_util::GetCaptureWindowConfineBounds(window_being_recorded);
  wm::ConvertRectToScreen(window_being_recorded, &window_confine_bounds);
  VerifyPreviewAlignment(window_confine_bounds);
}

TEST_F(CaptureModeCameraTest, MultiDisplayCameraPreviewWidgetBounds) {
  UpdateDisplay("800x700,801+0-800x700");

  const gfx::Point point_in_second_display = gfx::Point(1000, 500);
  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(point_in_second_display);

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
  event_generator->MoveMouseTo(gfx::Point(10, 20));
  EXPECT_TRUE(gfx::Rect(0, 0, 800, 700)
                  .Contains(preview_widget->GetWindowBoundsInScreen()));

  // Move back to the second display, switch to `kRegion` and set the capture
  // region. The camera preview should be moved back to the second display and
  // inside the capture region.
  event_generator->MoveMouseTo(point_in_second_display);
  controller->SetSource(CaptureModeSource::kRegion);
  // The capture region set through `controller` is in root coordinate.
  const gfx::Rect capture_region(100, 0, 400, 550);
  controller->SetUserCaptureRegion(capture_region, /*by_user=*/true);
  const gfx::Rect capture_region_in_screen(901, 0, 400, 550);
  const gfx::Rect preview_bounds = preview_widget->GetWindowBoundsInScreen();
  EXPECT_TRUE(second_display_bounds.Contains(preview_bounds));
  EXPECT_TRUE(capture_region_in_screen.Contains(preview_bounds));

  // Start the window recording inside the second display, the camera preview
  // should be inside the window that is being recorded inside the second
  // display.
  window()->SetBoundsInScreen(
      gfx::Rect(900, 0, 600, 500),
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
  auto* camera_preview_window = camera_preview_widget->GetNativeWindow();
  const auto* selected_window =
      controller->capture_mode_session()->GetSelectedWindow();
  ASSERT_EQ(camera_preview_window->parent(), selected_window);

  // Verify that camera preview is at the bottom right corner of the window.
  VerifyPreviewAlignment(selected_window->GetBoundsInScreen());
  // `camera_preview_window` should not have a transient parent.
  EXPECT_FALSE(wm::GetTransientParent(camera_preview_window));
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

// Tests that capture label's opacity changes accordingly when it's overlapped
// or it's not overlapped with camera preview. Also tests that when located
// events is or is not on capture label, its opacity is updated accordingly.
TEST_F(CaptureModeCameraTest,
       CaptureLabelOpacityChangeWhenOverlappingWithCameraPreview) {
  UpdateDisplay("900x800");
  auto* controller =
      StartCaptureSession(CaptureModeSource::kRegion, CaptureModeType::kVideo);
  auto* capture_session =
      static_cast<CaptureModeSession*>(controller->capture_mode_session());
  ASSERT_EQ(capture_session->session_type(), SessionType::kReal);
  auto* camera_controller = GetCameraController();
  AddDefaultCamera();
  camera_controller->SetSelectedCamera(CameraId(kDefaultCameraModelId, 1));
  const auto* camera_preview_widget =
      camera_controller->camera_preview_widget();
  const auto* capture_label_widget = capture_session->capture_label_widget();
  const ui::Layer* capture_label_layer = capture_label_widget->GetLayer();

  // Set capture region big enough to make capture label not overlapping with
  // camera preview. Verify capture label is fully opaque.
  const gfx::Rect capture_region(100, 100, 700, 700);
  SelectCaptureRegion(capture_region);
  EXPECT_FALSE(capture_label_widget->GetWindowBoundsInScreen().Intersects(
      camera_preview_widget->GetWindowBoundsInScreen()));
  EXPECT_EQ(capture_label_layer->GetTargetOpacity(), 1.f);

  // Update capture region smaller to make capture label overlap with camera
  // preview. Verify capture label is `kOverlapOpacity`.
  // Make sure to resize the region to a value that won't cause the camera to be
  // hidden according to the camera size specs.
  const int delta_x =
      kMinRegionLengthForCameraToIntersectLabelButton - capture_region.width();
  const int delta_y =
      kMinRegionLengthForCameraToIntersectLabelButton - capture_region.height();
  const gfx::Vector2d delta(delta_x, delta_y);
  auto* event_generator = GetEventGenerator();
  event_generator->set_current_screen_location(capture_region.bottom_right());
  event_generator->PressLeftButton();
  event_generator->MoveMouseTo(capture_region.bottom_right() + delta);
  event_generator->ReleaseLeftButton();
  EXPECT_TRUE(capture_label_widget->GetWindowBoundsInScreen().Intersects(
      camera_preview_widget->GetWindowBoundsInScreen()));
  EXPECT_EQ(capture_label_layer->GetTargetOpacity(),
            capture_mode::kCaptureUiOverlapOpacity);

  // Move mouse on top of capture label, verify it's updated to fully opaque
  // even it's still overlapped with camera preview.
  const gfx::Rect capture_lable_bounds =
      capture_label_widget->GetWindowBoundsInScreen();
  event_generator->MoveMouseTo(capture_lable_bounds.CenterPoint());
  EXPECT_TRUE(capture_lable_bounds.Intersects(
      camera_preview_widget->GetWindowBoundsInScreen()));
  EXPECT_EQ(capture_label_layer->GetTargetOpacity(), 1.0f);

  // Move mouse to the outside of capture label, verify it's updated to
  // `kOverlapOpacity`.
  const gfx::Vector2d delta1(50, 50);
  event_generator->MoveMouseTo(capture_lable_bounds.bottom_right() + delta1);
  EXPECT_EQ(capture_label_layer->GetTargetOpacity(),
            capture_mode::kCaptureUiOverlapOpacity);

  // Click on the outside of the capture region to reset it, verify capture
  // label is updated to fully opaque.
  const gfx::Rect current_capture_region = controller->user_capture_region();
  event_generator->MoveMouseTo(current_capture_region.bottom_right() + delta1);
  event_generator->ClickLeftButton();
  EXPECT_EQ(capture_label_layer->GetTargetOpacity(), 1.0f);
}

TEST_F(CaptureModeCameraTest,
       CaptureBarOpacityChangeWhenOverlappingWithCameraPreview) {
  // Update display size and update window with customized size to make sure
  // camera preview overlap with capture bar with capture source `kWindow`.
  UpdateDisplay("1366x768");
  window()->SetBounds({0, 195, 903, 492});

  auto* controller =
      StartCaptureSession(CaptureModeSource::kWindow, CaptureModeType::kVideo);
  auto* capture_session = controller->capture_mode_session();
  auto* camera_controller = GetCameraController();
  AddDefaultCamera();
  camera_controller->SetSelectedCamera(CameraId(kDefaultCameraModelId, 1));
  const auto* camera_preview_widget =
      camera_controller->camera_preview_widget();
  const auto* capture_bar_widget = capture_session->GetCaptureModeBarWidget();
  const ui::Layer* capture_bar_layer = capture_bar_widget->GetLayer();

  // Move mouse on top of `window` to set the selected window. Verify capture
  // bar is `kOverlapOpacity`.
  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(window()->GetBoundsInScreen().CenterPoint());
  EXPECT_EQ(capture_session->GetSelectedWindow(), window());
  EXPECT_TRUE(capture_bar_widget->GetWindowBoundsInScreen().Intersects(
      camera_preview_widget->GetWindowBoundsInScreen()));
  EXPECT_EQ(capture_bar_layer->GetTargetOpacity(),
            capture_mode::kCaptureUiOverlapOpacity);

  // Move mouse on top of capture bar. Verify capture bar is updated to fully
  // opaque.
  event_generator->MoveMouseTo(
      capture_bar_widget->GetWindowBoundsInScreen().CenterPoint());
  EXPECT_TRUE(capture_bar_widget->GetWindowBoundsInScreen().Intersects(
      camera_preview_widget->GetWindowBoundsInScreen()));
  EXPECT_EQ(capture_bar_layer->GetTargetOpacity(), 1.0f);

  // Mouse mouse to the outside of capture bar, verify it's updated to
  // `kOverlapOpacity`.
  const gfx::Point capture_bar_origin =
      capture_bar_widget->GetWindowBoundsInScreen().origin();
  event_generator->MoveMouseTo(capture_bar_origin.x() - 10,
                               capture_bar_origin.y() - 10);
  EXPECT_EQ(capture_bar_layer->GetTargetOpacity(),
            capture_mode::kCaptureUiOverlapOpacity);
}

TEST_F(CaptureModeCameraTest, CaptureBarOpacityChangeOnDisplayRotation) {
  // Update display size and update window with customized size to make sure
  // camera preview overlap with capture bar with capture source `kWindow`.
  UpdateDisplay("1366x768");
  window()->SetBounds({0, 195, 903, 492});

  auto* controller =
      StartCaptureSession(CaptureModeSource::kWindow, CaptureModeType::kVideo);
  auto* capture_session = controller->capture_mode_session();
  auto* camera_controller = GetCameraController();
  AddDefaultCamera();
  camera_controller->SetSelectedCamera(CameraId(kDefaultCameraModelId, 1));
  const auto* camera_preview_widget =
      camera_controller->camera_preview_widget();
  const auto* capture_bar_widget = capture_session->GetCaptureModeBarWidget();
  const ui::Layer* capture_bar_layer = capture_bar_widget->GetLayer();

  // Move mouse on top of `window` to set the selected window. Verify capture
  // bar is `kOverlapOpacity`.
  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(window()->GetBoundsInScreen().CenterPoint());
  EXPECT_EQ(capture_session->GetSelectedWindow(), window());
  EXPECT_TRUE(capture_bar_widget->GetWindowBoundsInScreen().Intersects(
      camera_preview_widget->GetWindowBoundsInScreen()));
  EXPECT_EQ(capture_bar_layer->GetTargetOpacity(),
            capture_mode::kCaptureUiOverlapOpacity);

  // Rotate the primary display by 90 degrees. Verify that capture bar no longer
  // overlaps with camera preview and it's updated to fully opaque.
  Shell::Get()->display_manager()->SetDisplayRotation(
      WindowTreeHostManager::GetPrimaryDisplayId(), display::Display::ROTATE_90,
      display::Display::RotationSource::USER);
  EXPECT_FALSE(capture_bar_widget->GetWindowBoundsInScreen().Intersects(
      camera_preview_widget->GetWindowBoundsInScreen()));
  EXPECT_EQ(capture_bar_layer->GetTargetOpacity(), 1.0f);

  // Rotate the primary display by 180 degrees. Verify that capture bar is
  // overlapped with camera preview and it's updated to `kOverlapOpacity`.
  Shell::Get()->display_manager()->SetDisplayRotation(
      WindowTreeHostManager::GetPrimaryDisplayId(),
      display::Display::ROTATE_180, display::Display::RotationSource::USER);
  EXPECT_TRUE(capture_bar_widget->GetWindowBoundsInScreen().Intersects(
      camera_preview_widget->GetWindowBoundsInScreen()));
  EXPECT_EQ(capture_bar_layer->GetTargetOpacity(),
            capture_mode::kCaptureUiOverlapOpacity);
}

TEST_F(CaptureModeCameraTest, CaptureLabelOpacityChangeOnCaptureSourceChange) {
  UpdateDisplay("800x600");
  auto* controller =
      StartCaptureSession(CaptureModeSource::kRegion, CaptureModeType::kVideo);
  auto* capture_session =
      static_cast<CaptureModeSession*>(controller->capture_mode_session());
  ASSERT_EQ(capture_session->session_type(), SessionType::kReal);
  auto* camera_controller = GetCameraController();
  AddDefaultCamera();
  camera_controller->SetSelectedCamera(CameraId(kDefaultCameraModelId, 1));
  auto* camera_preview_widget = camera_controller->camera_preview_widget();
  auto* capture_label_widget = capture_session->capture_label_widget();
  ui::Layer* capture_label_layer = capture_label_widget->GetLayer();

  // Select capture region to make sure capture label is overlapped with
  // camera preview. Verify capture label is `kOverlapOpacity`.
  const int min_region_length = kMinRegionLengthForCameraToIntersectLabelButton;
  SelectCaptureRegion({100, 100, min_region_length, min_region_length});
  EXPECT_TRUE(capture_label_widget->GetWindowBoundsInScreen().Intersects(
      camera_preview_widget->GetWindowBoundsInScreen()));
  EXPECT_EQ(capture_label_layer->GetTargetOpacity(),
            capture_mode::kCaptureUiOverlapOpacity);

  // Change the capture source from `kRegion` to `kFullscreen`, verify capture
  // label is updated to fully opaque.
  controller->SetSource(CaptureModeSource::kFullscreen);
  EXPECT_EQ(capture_label_layer->GetTargetOpacity(), 1.0f);
}

TEST_F(CaptureModeCameraTest,
       CaptureLabelOpacityChangeWhileVideoRecordingInProgress) {
  auto* controller =
      StartCaptureSession(CaptureModeSource::kRegion, CaptureModeType::kVideo);
  auto* camera_controller = GetCameraController();
  AddDefaultCamera();
  camera_controller->SetSelectedCamera(CameraId(kDefaultCameraModelId, 1));
  auto* camera_preview_widget = camera_controller->camera_preview_widget();
  const int min_region_length = kMinRegionLengthForCameraToIntersectLabelButton;
  controller->SetUserCaptureRegion(
      {100, 100, min_region_length, min_region_length}, /*by_user=*/true);

  StartVideoRecordingImmediately();
  EXPECT_FALSE(controller->IsActive());

  // Start a new capture session, verify even capture label is overlapped with
  // camera preview, it's still fully opaque since camera preview does not
  // belong to the new capture session.
  controller->Start(CaptureModeEntryType::kQuickSettings);
  EXPECT_EQ(CaptureModeSource::kRegion, controller->source());
  auto* capture_session =
      static_cast<CaptureModeSession*>(controller->capture_mode_session());
  ASSERT_EQ(capture_session->session_type(), SessionType::kReal);

  const auto* capture_label_widget = capture_session->capture_label_widget();
  EXPECT_TRUE(capture_label_widget->GetWindowBoundsInScreen().Intersects(
      camera_preview_widget->GetWindowBoundsInScreen()));
  EXPECT_EQ(capture_label_widget->GetLayer()->GetTargetOpacity(), 1.0f);
}

TEST_F(CaptureModeCameraTest, FocusableCameraPreviewInFullscreen) {
  UpdateDisplay("800x700");
  auto* controller = StartCaptureSession(CaptureModeSource::kFullscreen,
                                         CaptureModeType::kVideo);
  AddDefaultCamera();
  auto* camera_controller = GetCameraController();
  camera_controller->SetSelectedCamera(CameraId(kDefaultCameraModelId, 1));

  auto* event_generator = GetEventGenerator();
  using FocusGroup = CaptureModeSessionFocusCycler::FocusGroup;
  CaptureModeSessionTestApi test_api(controller->capture_mode_session());

  // Tests that the camera preview is focusable in fullscreen capture.
  auto* camera_preview_view = camera_controller->camera_preview_view();
  auto* resize_button = GetPreviewResizeButton();
  SendKey(ui::VKEY_TAB, event_generator, ui::EF_NONE, /*count=*/6);
  EXPECT_EQ(FocusGroup::kCameraPreview, test_api.GetCurrentFocusGroup());
  EXPECT_EQ(0u, test_api.GetCurrentFocusIndex());
  EXPECT_TRUE(camera_preview_view->has_focus());

  // Press tab again should advance the focus on the resize button. And the
  // resize button should be invisible before and visible after being focused.
  EXPECT_FALSE(resize_button->GetVisible());
  SendKey(ui::VKEY_TAB, event_generator);
  EXPECT_EQ(1u, test_api.GetCurrentFocusIndex());
  EXPECT_TRUE(resize_button->has_focus());
  EXPECT_TRUE(resize_button->GetVisible());

  // Press space when the resize button is focused should collapse the camera
  // preview.
  EXPECT_TRUE(resize_button->has_focus());
  EXPECT_FALSE(camera_controller->is_camera_preview_collapsed());
  SendKey(ui::VKEY_SPACE, event_generator);
  EXPECT_TRUE(camera_controller->is_camera_preview_collapsed());
  // Press space again should expand the camera preview.
  SendKey(ui::VKEY_SPACE, event_generator);
  EXPECT_FALSE(camera_controller->is_camera_preview_collapsed());

  // When the resize button inside the camera preview is focused, press tab
  // should advance to focus on the settings button.
  SendKey(ui::VKEY_TAB, event_generator);
  EXPECT_EQ(FocusGroup::kSettingsClose, test_api.GetCurrentFocusGroup());
  EXPECT_EQ(0u, test_api.GetCurrentFocusIndex());

  // The resize button should fade out and become invisible in
  // `kResizeButtonShowDuration` after removing focus from it.
  base::OneShotTimer* hide_timer =
      camera_preview_view->resize_button_hide_timer_for_test();
  EXPECT_FALSE(resize_button->has_focus());
  EXPECT_TRUE(hide_timer->IsRunning());
  EXPECT_EQ(hide_timer->GetCurrentDelay(),
            capture_mode::kResizeButtonShowDuration);
  {
    ViewVisibilityChangeWaiter waiter(resize_button);
    EXPECT_TRUE(resize_button->GetVisible());
    hide_timer->FireNow();
    waiter.Wait();
    EXPECT_FALSE(resize_button->GetVisible());
  }

  // Shift tab should advance the focus from the settings button back to the
  // resize button inside the camera preview.
  SendKey(ui::VKEY_TAB, event_generator, ui::EF_SHIFT_DOWN);
  EXPECT_EQ(FocusGroup::kCameraPreview, test_api.GetCurrentFocusGroup());
  EXPECT_EQ(1u, test_api.GetCurrentFocusIndex());
  EXPECT_TRUE(resize_button->has_focus());

  // The resize button should keep visible when it is focused, even trigger
  // `resize_button_hide_timer_` to refresh its visibility.
  hide_timer = camera_preview_view->resize_button_hide_timer_for_test();
  EXPECT_TRUE(hide_timer->IsRunning());
  EXPECT_TRUE(resize_button->GetVisible());
  hide_timer->FireNow();
  EXPECT_TRUE(resize_button->GetVisible());

  // Continue shift tab should move the focus from the resize button to the
  // camera preview.
  SendKey(ui::VKEY_TAB, event_generator, ui::EF_SHIFT_DOWN);
  EXPECT_EQ(0u, test_api.GetCurrentFocusIndex());
  EXPECT_TRUE(camera_preview_view->has_focus());

  // The resize button should fade out and become invisible again in
  // `kResizeButtonShowDuration` after removing focus from it.
  hide_timer = camera_preview_view->resize_button_hide_timer_for_test();
  EXPECT_FALSE(resize_button->has_focus());
  EXPECT_TRUE(hide_timer->IsRunning());
  EXPECT_EQ(hide_timer->GetCurrentDelay(),
            capture_mode::kResizeButtonShowDuration);
  {
    ViewVisibilityChangeWaiter waiter(resize_button);
    EXPECT_TRUE(resize_button->GetVisible());
    hide_timer->FireNow();
    waiter.Wait();
    EXPECT_FALSE(resize_button->GetVisible());
  }

  // Tests moving the camera preview through the keyboard when it is focused.
  EXPECT_TRUE(camera_controller->camera_preview_view()->has_focus());
  EXPECT_EQ(CameraPreviewSnapPosition::kBottomRight,
            camera_controller->camera_preview_snap_position());
  // Press control+right should not move the camera preview, as it is currently
  // at the right.
  SendKey(ui::VKEY_RIGHT, event_generator, ui::EF_CONTROL_DOWN);
  EXPECT_EQ(CameraPreviewSnapPosition::kBottomRight,
            camera_controller->camera_preview_snap_position());
  // Press control+down should not move the camera preview either, as it is
  // currently at the bottom.
  SendKey(ui::VKEY_DOWN, event_generator, ui::EF_CONTROL_DOWN);
  EXPECT_EQ(CameraPreviewSnapPosition::kBottomRight,
            camera_controller->camera_preview_snap_position());
  // Press control+left should move the camera preview from bottom right to
  // bottom left.
  SendKey(ui::VKEY_LEFT, event_generator, ui::EF_CONTROL_DOWN);
  EXPECT_EQ(CameraPreviewSnapPosition::kBottomLeft,
            camera_controller->camera_preview_snap_position());
  // Press control+right now should work to move the camera preview from bottom
  // left to bottom right.
  SendKey(ui::VKEY_RIGHT, event_generator, ui::EF_CONTROL_DOWN);
  EXPECT_EQ(CameraPreviewSnapPosition::kBottomRight,
            camera_controller->camera_preview_snap_position());
  // Press control+up should move the camera preview from bottom right to top
  // right.
  SendKey(ui::VKEY_UP, event_generator, ui::EF_CONTROL_DOWN);
  EXPECT_EQ(CameraPreviewSnapPosition::kTopRight,
            camera_controller->camera_preview_snap_position());

  // Shift tab again should advance the focus from the camera preview back to
  // the window capture source button.
  SendKey(ui::VKEY_TAB, event_generator, ui::EF_SHIFT_DOWN);
  EXPECT_EQ(FocusGroup::kTypeSource, test_api.GetCurrentFocusGroup());
  EXPECT_EQ(4u, test_api.GetCurrentFocusIndex());
}

TEST_F(CaptureModeCameraTest, FocusableCameraPreviewInRegion) {
  UpdateDisplay("1366x768");
  auto* controller = CaptureModeController::Get();
  controller->SetUserCaptureRegion(gfx::Rect(10, 10, 800, 700),
                                   /*by_user=*/true);

  StartCaptureSession(CaptureModeSource::kRegion, CaptureModeType::kVideo);
  AddDefaultCamera();
  auto* camera_controller = GetCameraController();
  camera_controller->SetSelectedCamera(CameraId(kDefaultCameraModelId, 1));

  auto* event_generator = GetEventGenerator();
  using FocusGroup = CaptureModeSessionFocusCycler::FocusGroup;
  CaptureModeSessionTestApi test_api(controller->capture_mode_session());

  // Tests that the camera preview is focusable in region capture.
  auto* camera_preview_view = camera_controller->camera_preview_view();
  auto* resize_button = GetPreviewResizeButton();
  SendKey(ui::VKEY_TAB, event_generator, ui::EF_NONE, /*count=*/15);
  EXPECT_EQ(FocusGroup::kCameraPreview, test_api.GetCurrentFocusGroup());
  EXPECT_EQ(0u, test_api.GetCurrentFocusIndex());
  EXPECT_TRUE(camera_preview_view->has_focus());
  SendKey(ui::VKEY_TAB, event_generator);
  EXPECT_EQ(1u, test_api.GetCurrentFocusIndex());
  EXPECT_TRUE(resize_button->has_focus());

  // When the resize button inside camera preview is focused, press tab should
  // advance to focus on the capture button.
  SendKey(ui::VKEY_TAB, event_generator);
  EXPECT_EQ(FocusGroup::kCaptureButton, test_api.GetCurrentFocusGroup());
  EXPECT_EQ(0u, test_api.GetCurrentFocusIndex());

  // Press tab until focus is on the settings button.
  while (FocusGroup::kSettingsClose != test_api.GetCurrentFocusGroup()) {
    SendKey(ui::VKEY_TAB, event_generator);
  }
  EXPECT_EQ(0u, test_api.GetCurrentFocusIndex());

  // Shift tab should move the focus from the settings button back to the
  // capture button.
  SendKey(ui::VKEY_TAB, event_generator, ui::EF_SHIFT_DOWN);
  EXPECT_EQ(FocusGroup::kCaptureButton, test_api.GetCurrentFocusGroup());
  // The index of the focused item depends on whether the recording type drop
  // down button exists or not.
  const size_t expected_index = features::IsGifRecordingEnabled() ? 1u : 0u;
  EXPECT_EQ(expected_index, test_api.GetCurrentFocusIndex());

  // Shift tab again until the focus is moved from the capture button back to
  // the resize button inside the camera preview.
  while (FocusGroup::kCameraPreview != test_api.GetCurrentFocusGroup()) {
    SendKey(ui::VKEY_TAB, event_generator, ui::EF_SHIFT_DOWN);
  }
  EXPECT_EQ(1u, test_api.GetCurrentFocusIndex());
  EXPECT_TRUE(resize_button->has_focus());
  // Continue shift tab should move the focus from the resize button to the
  // camera preview.
  SendKey(ui::VKEY_TAB, event_generator, ui::EF_SHIFT_DOWN);
  EXPECT_EQ(0u, test_api.GetCurrentFocusIndex());
  EXPECT_TRUE(camera_preview_view->has_focus());

  // Shift tab to advance the focus back to the window capture source button.
  SendKey(ui::VKEY_TAB, event_generator, ui::EF_SHIFT_DOWN, /*count=*/10);
  EXPECT_EQ(FocusGroup::kTypeSource, test_api.GetCurrentFocusGroup());
  EXPECT_EQ(4u, test_api.GetCurrentFocusIndex());

  // Update the capture region to test when the resize button is not focusable.
  controller->SetUserCaptureRegion(gfx::Rect(10, 10, 400, 550),
                                   /*by_user=*/true);
  SendKey(ui::VKEY_TAB, event_generator, ui::EF_NONE, /*count=*/10);
  EXPECT_EQ(FocusGroup::kCameraPreview, test_api.GetCurrentFocusGroup());
  EXPECT_EQ(0u, test_api.GetCurrentFocusIndex());
  EXPECT_TRUE(camera_preview_view->has_focus());
  // Press tab should advance the focus on the capture button instead of the
  // resize button. As the resize button is forced to be hidden, which is not
  // focusable in this case.
  EXPECT_FALSE(camera_preview_view->is_collapsible());
  SendKey(ui::VKEY_TAB, event_generator);
  EXPECT_EQ(FocusGroup::kCaptureButton, test_api.GetCurrentFocusGroup());
  EXPECT_EQ(0u, test_api.GetCurrentFocusIndex());
}

TEST_F(CaptureModeCameraTest, FocusableCameraPreviewInWindow) {
  UpdateDisplay("1366x768");
  // Create one more window besides `window_`.
  std::unique_ptr<aura::Window> window2(
      CreateTestWindow(gfx::Rect(150, 50, 800, 700)));
  window()->SetBounds(gfx::Rect(30, 40, 800, 700));

  auto* controller =
      StartCaptureSession(CaptureModeSource::kWindow, CaptureModeType::kVideo);
  AddDefaultCamera();
  auto* camera_controller = GetCameraController();
  camera_controller->SetSelectedCamera(CameraId(kDefaultCameraModelId, 1));

  auto* event_generator = GetEventGenerator();
  using FocusGroup = CaptureModeSessionFocusCycler::FocusGroup;
  auto* capture_mode_session = controller->capture_mode_session();
  CaptureModeSessionTestApi test_api(capture_mode_session);

  const auto* preview_window =
      camera_controller->camera_preview_widget()->GetNativeWindow();

  // Tab to focus on `window2`, which is the most recently used window. It
  // should be set to the current selected window, and the camera preview should
  // be shown inside it.
  auto* camera_preview_view = camera_controller->camera_preview_view();
  auto* resize_button = GetPreviewResizeButton();
  SendKey(ui::VKEY_TAB, event_generator, ui::EF_NONE, /*count=*/6);
  EXPECT_EQ(FocusGroup::kCaptureWindow, test_api.GetCurrentFocusGroup());
  EXPECT_EQ(0u, test_api.GetCurrentFocusIndex());
  EXPECT_TRUE(test_api.GetHighlightableWindow(window2.get())->has_focus());
  EXPECT_EQ(window2.get(), capture_mode_session->GetSelectedWindow());
  EXPECT_EQ(window2.get(), preview_window->parent());

  // Press tab should advance to focus on the camera preview inside. But the
  // FocusGroup should not change, as the camera preview is treated as part of
  // the selected window in this case.
  SendKey(ui::VKEY_TAB, event_generator);
  EXPECT_EQ(FocusGroup::kCaptureWindow, test_api.GetCurrentFocusGroup());
  EXPECT_EQ(1u, test_api.GetCurrentFocusIndex());
  EXPECT_TRUE(camera_preview_view->has_focus());
  // Press tab should focus on the resize button inside the camera preview.
  SendKey(ui::VKEY_TAB, event_generator);
  EXPECT_EQ(2u, test_api.GetCurrentFocusIndex());
  EXPECT_TRUE(resize_button->has_focus());
  EXPECT_FALSE(camera_controller->camera_preview_view()->has_focus());

  // Press tab again should advance focus and set another window `window_` to be
  // the current selected window. The camera preview should be shown inside it
  // now.
  SendKey(ui::VKEY_TAB, event_generator);
  EXPECT_EQ(FocusGroup::kCaptureWindow, test_api.GetCurrentFocusGroup());
  EXPECT_EQ(3u, test_api.GetCurrentFocusIndex());
  EXPECT_TRUE(test_api.GetHighlightableWindow(window())->has_focus());
  EXPECT_EQ(window(), capture_mode_session->GetSelectedWindow());
  EXPECT_EQ(window(), preview_window->parent());

  // Press tab should advance to focus on the camera preview that inside
  // `window_` now.
  SendKey(ui::VKEY_TAB, event_generator);
  EXPECT_EQ(FocusGroup::kCaptureWindow, test_api.GetCurrentFocusGroup());
  EXPECT_EQ(4u, test_api.GetCurrentFocusIndex());
  EXPECT_TRUE(camera_preview_view->has_focus());
  // Press tab to advance the focus on the resize button inside the camera
  // preview.
  SendKey(ui::VKEY_TAB, event_generator);
  EXPECT_EQ(5u, test_api.GetCurrentFocusIndex());
  EXPECT_TRUE(resize_button->has_focus());
  EXPECT_FALSE(camera_preview_view->has_focus());

  // Press tab again should advance to focus on the settings button.
  SendKey(ui::VKEY_TAB, event_generator);
  EXPECT_EQ(FocusGroup::kSettingsClose, test_api.GetCurrentFocusGroup());
  EXPECT_EQ(0u, test_api.GetCurrentFocusIndex());
  EXPECT_FALSE(resize_button->has_focus());
  EXPECT_FALSE(camera_preview_view->has_focus());

  // Shift tab when the settings button is focused should advance back to focus
  // on the resize button inside the camera preview. And the camera preview
  // should now be shown inside `window_`, which is the current selected window.
  SendKey(ui::VKEY_TAB, event_generator, ui::EF_SHIFT_DOWN);
  EXPECT_EQ(FocusGroup::kCaptureWindow, test_api.GetCurrentFocusGroup());
  EXPECT_EQ(5u, test_api.GetCurrentFocusIndex());
  EXPECT_TRUE(resize_button->has_focus());
  EXPECT_FALSE(camera_preview_view->has_focus());
  EXPECT_EQ(window(), capture_mode_session->GetSelectedWindow());
  EXPECT_EQ(window(), preview_window->parent());
  // Shift tab again should move the focus from the resize button to the camera
  // preview.
  SendKey(ui::VKEY_TAB, event_generator, ui::EF_SHIFT_DOWN);
  EXPECT_EQ(FocusGroup::kCaptureWindow, test_api.GetCurrentFocusGroup());
  EXPECT_EQ(4u, test_api.GetCurrentFocusIndex());
  EXPECT_FALSE(resize_button->has_focus());
  EXPECT_TRUE(camera_preview_view->has_focus());

  // Shift tab again should focus on the `window_`
  SendKey(ui::VKEY_TAB, event_generator, ui::EF_SHIFT_DOWN);
  EXPECT_EQ(FocusGroup::kCaptureWindow, test_api.GetCurrentFocusGroup());
  EXPECT_EQ(3u, test_api.GetCurrentFocusIndex());
  EXPECT_TRUE(test_api.GetHighlightableWindow(window())->has_focus());
  EXPECT_FALSE(resize_button->has_focus());
  EXPECT_FALSE(camera_preview_view->has_focus());
  EXPECT_EQ(window(), capture_mode_session->GetSelectedWindow());

  // Continue shift tab should advance back to focus on the resize button inside
  // the camera preview. And the camera preview should now inside `window2`,
  // which is the current selected window.
  SendKey(ui::VKEY_TAB, event_generator, ui::EF_SHIFT_DOWN);
  EXPECT_EQ(FocusGroup::kCaptureWindow, test_api.GetCurrentFocusGroup());
  EXPECT_EQ(2u, test_api.GetCurrentFocusIndex());
  EXPECT_TRUE(resize_button->has_focus());
  EXPECT_FALSE(camera_preview_view->has_focus());
  EXPECT_EQ(window2.get(), capture_mode_session->GetSelectedWindow());
  EXPECT_EQ(window2.get(), preview_window->parent());
  // Continue shift tab should move the focus from the resize button to the
  // camera preview.
  SendKey(ui::VKEY_TAB, event_generator, ui::EF_SHIFT_DOWN);
  EXPECT_EQ(1u, test_api.GetCurrentFocusIndex());
  EXPECT_FALSE(resize_button->has_focus());
  EXPECT_TRUE(camera_preview_view->has_focus());

  // Continue shift tab should focus on the `window2`.
  SendKey(ui::VKEY_TAB, event_generator, ui::EF_SHIFT_DOWN);
  EXPECT_EQ(FocusGroup::kCaptureWindow, test_api.GetCurrentFocusGroup());
  EXPECT_EQ(0u, test_api.GetCurrentFocusIndex());
  EXPECT_TRUE(test_api.GetHighlightableWindow(window2.get())->has_focus());
  EXPECT_FALSE(resize_button->has_focus());
  EXPECT_FALSE(camera_preview_view->has_focus());
  EXPECT_EQ(window2.get(), capture_mode_session->GetSelectedWindow());

  // Continue shift tab should advance back to the window capture source button.
  SendKey(ui::VKEY_TAB, event_generator, ui::EF_SHIFT_DOWN);
  EXPECT_EQ(FocusGroup::kTypeSource, test_api.GetCurrentFocusGroup());
  EXPECT_EQ(4u, test_api.GetCurrentFocusIndex());

  // Destroy a window and test that the destroyed window will not be included in
  // the keyboard focus navigation.
  window2.reset();
  SendKey(ui::VKEY_TAB, event_generator);
  EXPECT_EQ(FocusGroup::kCaptureWindow, test_api.GetCurrentFocusGroup());
  EXPECT_EQ(0u, test_api.GetCurrentFocusIndex());
  EXPECT_TRUE(test_api.GetHighlightableWindow(window())->has_focus());
  SendKey(ui::VKEY_TAB, event_generator);
  EXPECT_EQ(FocusGroup::kCaptureWindow, test_api.GetCurrentFocusGroup());
  EXPECT_EQ(1u, test_api.GetCurrentFocusIndex());
  EXPECT_TRUE(camera_preview_view->has_focus());
  SendKey(ui::VKEY_TAB, event_generator);
  EXPECT_EQ(2u, test_api.GetCurrentFocusIndex());
  EXPECT_EQ(FocusGroup::kCaptureWindow, test_api.GetCurrentFocusGroup());
  EXPECT_TRUE(resize_button->has_focus());
  // Continue tab after focusing on the resize button should advance the focus
  // to settings close button.
  SendKey(ui::VKEY_TAB, event_generator);
  EXPECT_EQ(FocusGroup::kSettingsClose, test_api.GetCurrentFocusGroup());
  EXPECT_EQ(0u, test_api.GetCurrentFocusIndex());
}

TEST_F(CaptureModeCameraTest,
       FocusableCameraPreviewInVideoRecordingWithFullscreenCapture) {
  UpdateDisplay("800x700");
  StartCaptureSession(CaptureModeSource::kFullscreen, CaptureModeType::kVideo);
  AddDefaultCamera();
  auto* camera_controller = GetCameraController();
  camera_controller->SetSelectedCamera(CameraId(kDefaultCameraModelId, 1));

  auto* event_generator = GetEventGenerator();
  auto* camera_preview_view = camera_controller->camera_preview_view();
  auto* resize_button = GetPreviewResizeButton();
  StartVideoRecordingImmediately();

  EXPECT_TRUE(camera_preview_view->is_collapsible());
  EXPECT_FALSE(camera_controller->is_camera_preview_collapsed());

  // Press shortcut "Search+Alt+S" should focus the camera preview.
  SendKey(ui::VKEY_S, event_generator, ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN);
  EXPECT_TRUE(camera_preview_view->has_focus());
  EXPECT_FALSE(resize_button->has_focus());
  EXPECT_FALSE(resize_button->GetVisible());

  // Press tab should fade in the resize button and advance the focus on it.
  SendKey(ui::VKEY_TAB, event_generator);
  EXPECT_FALSE(camera_preview_view->has_focus());
  EXPECT_TRUE(resize_button->has_focus());
  EXPECT_TRUE(resize_button->GetVisible());

  // Shift tab should advance the focus back to the camera preview view. But the
  // resize button will be kept as visible as it will be fade out in a few
  // seconds.
  SendKey(ui::VKEY_TAB, event_generator, ui::EF_SHIFT_DOWN);
  EXPECT_TRUE(camera_preview_view->has_focus());
  EXPECT_FALSE(resize_button->has_focus());
  EXPECT_TRUE(resize_button->GetVisible());

  // Press tab again to focus on the resize button.
  SendKey(ui::VKEY_TAB, event_generator);
  EXPECT_FALSE(camera_preview_view->has_focus());
  EXPECT_TRUE(resize_button->has_focus());
  EXPECT_TRUE(resize_button->GetVisible());

  // Press space key when the resize button is focused should be able to expand
  // or collapse the camera preview.
  SendKey(ui::VKEY_SPACE, event_generator);
  EXPECT_TRUE(camera_controller->is_camera_preview_collapsed());
  EXPECT_TRUE(resize_button->has_focus());
  SendKey(ui::VKEY_SPACE, event_generator);
  EXPECT_FALSE(camera_controller->is_camera_preview_collapsed());
  EXPECT_TRUE(resize_button->has_focus());

  // Press escape key should remove the focus from the camera preview, either
  // the camera preview view or the resize button.
  SendKey(ui::VKEY_ESCAPE, event_generator);
  EXPECT_FALSE(camera_preview_view->has_focus());
  EXPECT_FALSE(resize_button->has_focus());

  // Press shortcut "Search+Alt+S" should not focus the camera preview when
  // capture session is active even though video recording is in progress.
  auto* controller = StartCaptureSession(CaptureModeSource::kFullscreen,
                                         CaptureModeType::kImage);
  EXPECT_TRUE(controller->IsActive());
  EXPECT_TRUE(controller->is_recording_in_progress());
  SendKey(ui::VKEY_S, event_generator, ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN);
  EXPECT_FALSE(camera_preview_view->has_focus());

  // Press tab should still able to focus the camera preview view and the resize
  // button.
  SendKey(ui::VKEY_TAB, event_generator, ui::EF_NONE, /*count=*/5);
  EXPECT_TRUE(camera_preview_view->has_focus());
  EXPECT_FALSE(resize_button->has_focus());
  SendKey(ui::VKEY_TAB, event_generator);
  EXPECT_FALSE(camera_preview_view->has_focus());
  EXPECT_TRUE(resize_button->has_focus());
  // Continue tab should be able to advance the focus on the settings button.
  SendKey(ui::VKEY_TAB, event_generator);
  EXPECT_FALSE(camera_preview_view->has_focus());
  EXPECT_FALSE(resize_button->has_focus());

  CaptureModeSessionTestApi test_api(controller->capture_mode_session());
  EXPECT_TRUE(CaptureModeSessionFocusCycler::HighlightHelper::Get(
                  test_api.GetCaptureModeBarView()->settings_button())
                  ->has_focus());
}

TEST_F(CaptureModeCameraTest,
       FocusableCameraPreviewInVideoRecordingWithRegionCapture) {
  auto* controller = CaptureModeController::Get();
  controller->SetUserCaptureRegion(gfx::Rect(10, 10, 400, 550),
                                   /*by_user=*/true);

  StartCaptureSession(CaptureModeSource::kRegion, CaptureModeType::kVideo);
  AddDefaultCamera();
  auto* camera_controller = GetCameraController();
  camera_controller->SetSelectedCamera(CameraId(kDefaultCameraModelId, 1));

  auto* event_generator = GetEventGenerator();
  auto* camera_preview_view = camera_controller->camera_preview_view();
  auto* resize_button = GetPreviewResizeButton();
  StartVideoRecordingImmediately();

  EXPECT_FALSE(camera_controller->is_camera_preview_collapsed());
  EXPECT_FALSE(camera_preview_view->is_collapsible());

  // Press shortcut "Search+Alt+S" should focus the camera preview.
  SendKey(ui::VKEY_S, event_generator, ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN);
  EXPECT_TRUE(camera_preview_view->has_focus());
  EXPECT_FALSE(resize_button->has_focus());
  EXPECT_FALSE(resize_button->GetVisible());

  // Press tab nothing will happen. As the camera preview is not collapsible,
  // which means the camera preview is the only focusable item. Focus will not
  // be moved to the resize button and it will continue to be hidden.
  SendKey(ui::VKEY_TAB, event_generator);
  EXPECT_TRUE(camera_preview_view->has_focus());
  EXPECT_FALSE(resize_button->has_focus());
  EXPECT_FALSE(resize_button->GetVisible());

  // Mouse pressing outside of the camera preview should remove the focus from
  // the camera preview.
  const gfx::Point origin = camera_preview_view->GetBoundsInScreen().origin();
  const gfx::Vector2d delta(-50, -50);
  event_generator->MoveMouseTo(origin + delta);
  event_generator->ClickLeftButton();
  EXPECT_FALSE(camera_preview_view->has_focus());
  EXPECT_FALSE(resize_button->has_focus());
}

TEST_F(CaptureModeCameraTest,
       FocusableCameraPreviewInVideoRecordingWithWindowCapture) {
  UpdateDisplay("1366x768");
  window()->SetBounds(gfx::Rect(30, 40, 800, 700));
  auto* controller =
      StartCaptureSession(CaptureModeSource::kWindow, CaptureModeType::kVideo);
  AddDefaultCamera();
  auto* camera_controller = GetCameraController();
  camera_controller->SetSelectedCamera(CameraId(kDefaultCameraModelId, 1));
  auto* event_generator = GetEventGenerator();
  auto* camera_preview_view = camera_controller->camera_preview_view();
  auto* resize_button = GetPreviewResizeButton();

  event_generator->MoveMouseTo(window()->GetBoundsInScreen().origin());
  EXPECT_EQ(controller->capture_mode_session()->GetSelectedWindow(), window());
  StartVideoRecordingImmediately();

  EXPECT_FALSE(camera_controller->is_camera_preview_collapsed());
  EXPECT_TRUE(camera_preview_view->is_collapsible());

  // Press shortcut "Search+Alt+S" should focus the camera preview.
  SendKey(ui::VKEY_S, event_generator, ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN);
  EXPECT_TRUE(camera_preview_view->has_focus());
  EXPECT_FALSE(resize_button->has_focus());

  // Press tab should fade in the resize button and advance the focus on it.
  SendKey(ui::VKEY_TAB, event_generator);
  EXPECT_FALSE(camera_preview_view->has_focus());
  EXPECT_TRUE(resize_button->has_focus());
  EXPECT_TRUE(resize_button->GetVisible());

  // Shift tab should advance the focus back to the camera preview view. But the
  // resize button will be kept as visible as it will be fade out in a few
  // seconds.
  SendKey(ui::VKEY_TAB, event_generator, ui::EF_SHIFT_DOWN);
  EXPECT_TRUE(camera_preview_view->has_focus());
  EXPECT_FALSE(resize_button->has_focus());
}

TEST_F(CaptureModeCameraTest, CaptureBarOpacityChangeOnKeyboardNavigation) {
  using FocusGroup = CaptureModeSessionFocusCycler::FocusGroup;

  // Update display size and update window with customized size to make sure
  // camera preview overlap with capture bar with capture source `kWindow`.
  UpdateDisplay("1366x768");
  window()->SetBounds({0, 0, 903, 700});

  auto* controller =
      StartCaptureSession(CaptureModeSource::kWindow, CaptureModeType::kVideo);
  CaptureModeSessionTestApi test_api(controller->capture_mode_session());
  auto* capture_session = controller->capture_mode_session();
  auto* camera_controller = GetCameraController();
  AddDefaultCamera();
  camera_controller->SetSelectedCamera(CameraId(kDefaultCameraModelId, 1));
  const auto* camera_preview_widget =
      camera_controller->camera_preview_widget();
  const auto* capture_bar_widget = capture_session->GetCaptureModeBarWidget();
  const ui::Layer* capture_bar_layer = capture_bar_widget->GetLayer();

  // Move mouse on top of `window` to set the selected window. Verify capture
  // bar is `kOverlapOpacity`.
  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(window()->GetBoundsInScreen().CenterPoint());
  EXPECT_EQ(capture_session->GetSelectedWindow(), window());
  EXPECT_TRUE(capture_bar_widget->GetWindowBoundsInScreen().Intersects(
      camera_preview_widget->GetWindowBoundsInScreen()));
  EXPECT_EQ(capture_bar_layer->GetTargetOpacity(),
            capture_mode::kCaptureUiOverlapOpacity);

  // Now tab through the capture bar, verify that as long as the focus is on
  // capture bar or capture settings menu, capture bar is updated to full
  // opaque.
  SendKey(ui::VKEY_TAB, event_generator);
  EXPECT_EQ(capture_bar_layer->GetTargetOpacity(), 1.0f);

  // Tab four times to focus the last source button (window mode button). Verify
  // capture bar is still fully opaque.
  SendKey(ui::VKEY_TAB, event_generator, ui::EF_NONE, /*count=*/4);
  EXPECT_EQ(FocusGroup::kTypeSource, test_api.GetCurrentFocusGroup());
  EXPECT_EQ(4u, test_api.GetCurrentFocusIndex());
  EXPECT_EQ(capture_bar_layer->GetTargetOpacity(), 1.0f);

  // Tab once to focus on the window to be captured, verify that capture bar is
  // `kOverlapOpacity` since the focus is removed from capture bar.
  SendKey(ui::VKEY_TAB, event_generator);
  EXPECT_EQ(FocusGroup::kCaptureWindow, test_api.GetCurrentFocusGroup());
  EXPECT_EQ(capture_bar_layer->GetTargetOpacity(),
            capture_mode::kCaptureUiOverlapOpacity);

  // Tab three times to focus on the settings button, verify capture bar is
  // updated to fully opaque again.
  SendKey(ui::VKEY_TAB, event_generator, ui::EF_NONE, /*count=*/3);
  EXPECT_EQ(FocusGroup::kSettingsClose, test_api.GetCurrentFocusGroup());
  EXPECT_EQ(0u, test_api.GetCurrentFocusIndex());
  EXPECT_EQ(capture_bar_layer->GetTargetOpacity(), 1.0f);

  // Now enter space to open the settings menu. Verify capture bar is still full
  // opaque.
  SendKey(ui::VKEY_SPACE, event_generator);
  EXPECT_EQ(FocusGroup::kPendingSettings, test_api.GetCurrentFocusGroup());
  EXPECT_EQ(capture_bar_layer->GetTargetOpacity(), 1.0f);

  // Tab once to focus on the settings menu. Verify capture bar is still full
  // opaque.
  SendKey(ui::VKEY_TAB, event_generator);
  EXPECT_EQ(FocusGroup::kSettingsMenu, test_api.GetCurrentFocusGroup());
  EXPECT_EQ(capture_bar_layer->GetTargetOpacity(), 1.0f);
}

TEST_F(CaptureModeCameraTest, CaptureLabelOpacityChangeOnKeyboardNavigation) {
  UpdateDisplay("800x600");
  using FocusGroup = CaptureModeSessionFocusCycler::FocusGroup;

  auto* controller =
      StartCaptureSession(CaptureModeSource::kRegion, CaptureModeType::kVideo);
  CaptureModeSessionTestApi test_api(controller->capture_mode_session());
  auto* capture_session =
      static_cast<CaptureModeSession*>(controller->capture_mode_session());
  ASSERT_EQ(capture_session->session_type(), SessionType::kReal);
  auto* camera_controller = GetCameraController();
  AddDefaultCamera();
  camera_controller->SetSelectedCamera(CameraId(kDefaultCameraModelId, 1));
  auto* camera_preview_widget = camera_controller->camera_preview_widget();
  auto* capture_label_widget = capture_session->capture_label_widget();
  ui::Layer* capture_label_layer = capture_label_widget->GetLayer();

  // Select capture region to make sure capture label is overlapped with
  // camera preview. Verify capture label is `kOverlapOpacity`.
  const int min_region_length = kMinRegionLengthForCameraToIntersectLabelButton;
  SelectCaptureRegion({100, 100, min_region_length, min_region_length});
  EXPECT_TRUE(capture_label_widget->GetWindowBoundsInScreen().Intersects(
      camera_preview_widget->GetWindowBoundsInScreen()));
  EXPECT_EQ(capture_label_layer->GetTargetOpacity(),
            capture_mode::kCaptureUiOverlapOpacity);

  auto* event_generator = GetEventGenerator();
  // Tab four times to focus on the region capture source, verify capture label
  // is still `kOverlapOpacity`.
  SendKey(ui::VKEY_TAB, event_generator, ui::EF_NONE, /*count=*/4);
  EXPECT_EQ(FocusGroup::kTypeSource, test_api.GetCurrentFocusGroup());
  EXPECT_EQ(3u, test_api.GetCurrentFocusIndex());
  EXPECT_EQ(capture_label_layer->GetTargetOpacity(),
            capture_mode::kCaptureUiOverlapOpacity);

  // Tab twice to focus on `kSelection`, verify capture label is still
  // `kOverlapOpacity`.
  SendKey(ui::VKEY_TAB, event_generator, ui::EF_NONE, /*count=*/2);
  EXPECT_EQ(FocusGroup::kSelection, test_api.GetCurrentFocusGroup());
  EXPECT_EQ(capture_label_layer->GetTargetOpacity(),
            capture_mode::kCaptureUiOverlapOpacity);

  // Tab eleven times to focus on cpature label, verify capture label is updated
  // to fully opaque.
  EXPECT_FALSE(camera_controller->camera_preview_view()->is_collapsible());
  SendKey(ui::VKEY_TAB, event_generator, ui::EF_NONE, /*count=*/10);
  EXPECT_EQ(FocusGroup::kCaptureButton, test_api.GetCurrentFocusGroup());
  EXPECT_EQ(capture_label_layer->GetTargetOpacity(), 1.0f);

  // Tab until the settings button on the capture bar is focused, then verify
  // that the capture lable's opacity is updated to `kOverlapOpacity`.
  while (FocusGroup::kSettingsClose != test_api.GetCurrentFocusGroup()) {
    SendKey(ui::VKEY_TAB, event_generator);
  }
  EXPECT_EQ(capture_label_layer->GetTargetOpacity(),
            capture_mode::kCaptureUiOverlapOpacity);
}

// Tests that when switching capture source from `kRegion` to `kFullscreen`,
// camera preview should be shown.
// Regression test for https://crbug.com/1316911.
TEST_F(CaptureModeCameraTest, CameraPreviewVisibilityOnCaptureSourceChanged) {
  StartCaptureSession(CaptureModeSource::kFullscreen, CaptureModeType::kVideo);
  AddDefaultCamera();
  CaptureModeTestApi().SelectCameraAtIndex(0);
  auto* camera_preview_widget = GetCameraController()->camera_preview_widget();
  auto* preview_window = camera_preview_widget->GetNativeWindow();

  // Verify that camera preview is visible.
  EXPECT_EQ(preview_window->parent(),
            preview_window->GetRootWindow()->GetChildById(
                kShellWindowId_MenuContainer));
  EXPECT_TRUE(camera_preview_widget->IsVisible());
  EXPECT_TRUE(preview_window->TargetVisibility());

  // Click on the region source button, verify that camera preview is parented
  // to UnparentedContainer and becomes invisible.
  auto* event_generator = GetEventGenerator();
  ClickOnView(GetRegionToggleButton(), event_generator);
  EXPECT_EQ(preview_window->parent(),
            preview_window->GetRootWindow()->GetChildById(
                kShellWindowId_UnparentedContainer));
  EXPECT_FALSE(camera_preview_widget->IsVisible());

  // Now switch capture source to `kFullscreen`, verify that camera preview is
  // parented to MenuContainer and becomes visible again.
  ClickOnView(GetFullscreenToggleButton(), event_generator);
  EXPECT_EQ(preview_window->parent(),
            preview_window->GetRootWindow()->GetChildById(
                kShellWindowId_MenuContainer));
  EXPECT_TRUE(preview_window->TargetVisibility());
  EXPECT_TRUE(camera_preview_widget->IsVisible());
}

// Tests that the recording starts with camera metrics are recorded correctly
// both in clamshell and tablet mode.
TEST_F(CaptureModeCameraTest, RecordingStartsWithCameraHistogramTest) {
  base::HistogramTester histogram_tester;
  constexpr char kHistogramNameBase[] = "RecordingStartsWithCamera";

  AddDefaultCamera();

  struct {
    bool tablet_enabled;
    bool camera_on;
  } kTestCases[] = {
      {false, false},
      {false, true},
      {true, false},
      {true, true},
  };

  for (const auto test_case : kTestCases) {
    if (test_case.tablet_enabled) {
      SwitchToTabletMode();
      EXPECT_TRUE(Shell::Get()->IsInTabletMode());
    } else {
      EXPECT_FALSE(Shell::Get()->IsInTabletMode());
    }

    const std::string histogram_name =
        BuildHistogramName(kHistogramNameBase, /*behavior=*/nullptr,
                           /*append_ui_mode_suffix=*/true);
    histogram_tester.ExpectBucketCount(histogram_name, test_case.camera_on, 0);

    auto* controller = StartCaptureSession(CaptureModeSource::kFullscreen,
                                           CaptureModeType::kVideo);
    auto* camera_controller = GetCameraController();
    camera_controller->SetSelectedCamera(
        test_case.camera_on ? CameraId(kDefaultCameraModelId, 1) : CameraId());

    StartVideoRecordingImmediately();
    EXPECT_TRUE(controller->is_recording_in_progress());

    controller->EndVideoRecording(EndRecordingReason::kStopRecordingButton);
    WaitForCaptureFileToBeSaved();

    histogram_tester.ExpectBucketCount(histogram_name, test_case.camera_on, 1);
  }
}

// Tests that the number of camera disconnections happens during recording is
// recorded correctly both in clamshell and tablet mode.
TEST_F(CaptureModeCameraTest,
       RecordCameraDisconnectionsDuringRecordingsHistogramTest) {
  constexpr char kHistogramNameBase[] = "CameraDisconnectionsDuringRecordings";
  base::HistogramTester histogram_tester;

  auto* camera_controller = GetCameraController();

  auto disconnect_and_reconnect_camera_n_times = [&](int n) {
    for (int i = 0; i < n; i++) {
      AddAndRemoveCameraAndTriggerGracePeriod();
      camera_controller->camera_reconnect_timer_for_test()->FireNow();
    }
  };

  for (const bool tablet_enabled : {false, true}) {
    if (tablet_enabled) {
      SwitchToTabletMode();
      EXPECT_TRUE(Shell::Get()->IsInTabletMode());
    } else {
      EXPECT_FALSE(Shell::Get()->IsInTabletMode());
    }

    auto* controller = StartCaptureSession(CaptureModeSource::kFullscreen,
                                           CaptureModeType::kVideo);
    controller->StartVideoRecordingImmediatelyForTesting();

    // Disconnect the camera, exhaust the timer and reconnect three times. The
    // metric should record accordingly.
    disconnect_and_reconnect_camera_n_times(3);
    controller->EndVideoRecording(EndRecordingReason::kStopRecordingButton);
    WaitForCaptureFileToBeSaved();
    histogram_tester.ExpectBucketCount(
        BuildHistogramName(kHistogramNameBase, /*behavior=*/nullptr,
                           /*append_ui_mode_suffix=*/true),
        3, 1);
  }
}

// Tests that the number of connected cameras to the device is recorded whenever
// the number changes.
TEST_F(CaptureModeCameraTest, RecordNumberOfConnectedCamerasHistogramTest) {
  constexpr char kHistogramNameBase[] = "NumberOfConnectedCameras";
  const std::string histogram_name =
      BuildHistogramName(kHistogramNameBase, /*behavior=*/nullptr,
                         /*append_ui_mode_suffix=*/false);
  base::HistogramTester histogram_tester;
  // Make sure the device change alert triggered by the SystemMonitor is handled
  // before we connect a camera device.
  {
    base::RunLoop loop;
    GetCameraController()->SetOnCameraListReceivedForTesting(
        loop.QuitClosure());
    base::SystemMonitor::Get()->ProcessDevicesChanged(
        base::SystemMonitor::DEVTYPE_VIDEO_CAPTURE);
    loop.Run();
  }

  // Verify that before we connect any camera device, there's 0 cameras and it
  // has been recorded.
  histogram_tester.ExpectBucketCount(histogram_name, 0, 1);

  // Connect one camera, verify that the number of one camera device has been
  // recorded once.
  AddFakeCamera("/dev/video", "fake cam ", "model 1");
  histogram_tester.ExpectBucketCount(histogram_name, 1, 1);

  // Connect the second camera, verify that the number of two camera devices has
  // been recorded once.
  AddFakeCamera("/dev/video1", "fake cam 2", "model 2");
  histogram_tester.ExpectBucketCount(histogram_name, 2, 1);

  // Disconnect the second camera, now the number of connected cameres drops
  // back to one, verify that the number of one camera device has been recorded
  // twice.
  RemoveFakeCamera("/dev/video1");
  histogram_tester.ExpectBucketCount(histogram_name, 1, 2);

  // Connect the third camera, now the number of connected cameras is two again,
  // verify that the number of two camera devices has been recorded twice.
  AddFakeCamera("/dev/video2", "fake cam 3", "model 3");
  histogram_tester.ExpectBucketCount(histogram_name, 2, 2);
}

// TODO(crbug.com/331316079): Flaky on LSAN / ASAN.
// TODO(crbug.com/350946974): Flaky in general.
// Tests that the duration for disconnected camera to become available again is
// recorded correctly both in clamshell and tablet mode.
TEST_F(CaptureModeCameraTest,
       DISABLED_RecordCameraReconnectDurationHistogramTest) {
  constexpr char kHistogramNameBase[] = "CameraReconnectDuration";
  base::HistogramTester histogram_tester;

  for (const bool tablet_enabled : {false, true}) {
    if (tablet_enabled) {
      SwitchToTabletMode();
      EXPECT_TRUE(Shell::Get()->IsInTabletMode());
    } else {
      EXPECT_FALSE(Shell::Get()->IsInTabletMode());
    }

    AddAndRemoveCameraAndTriggerGracePeriod();
    WaitForSeconds(1);
    AddDefaultCamera();
    histogram_tester.ExpectBucketCount(
        BuildHistogramName(kHistogramNameBase, /*behavior=*/nullptr,
                           /*append_ui_mode_suffix=*/true),
        1, 1);
    RemoveDefaultCamera();
  }
}

// Tests that the camera size on start is recorded correctly in the metrics both
// in clamshell and tablet mode.
TEST_F(CaptureModeCameraTest, RecordingCameraSizeOnStartHistogramTest) {
  UpdateDisplay("1366x768");

  constexpr char kHistogramNameBase[] = "RecordingCameraSizeOnStart";
  base::HistogramTester histogram_tester;

  auto* camera_controller = GetCameraController();
  AddDefaultCamera();
  camera_controller->SetSelectedCamera(CameraId(kDefaultCameraModelId, 1));

  for (const bool tablet_enabled : {false, true}) {
    if (tablet_enabled) {
      SwitchToTabletMode();
      EXPECT_TRUE(Shell::Get()->IsInTabletMode());
    } else {
      EXPECT_FALSE(Shell::Get()->IsInTabletMode());
    }

    const std::string histogram_name =
        BuildHistogramName(kHistogramNameBase, /*behavior=*/nullptr,
                           /*append_ui_mode_suffix=*/true);
    for (const bool collapsed : {false, true}) {
      const auto sample = collapsed ? CaptureModeCameraSize::kCollapsed
                                    : CaptureModeCameraSize::kExpanded;
      histogram_tester.ExpectBucketCount(histogram_name, sample, 0);

      auto* controller = StartCaptureSession(CaptureModeSource::kFullscreen,
                                             CaptureModeType::kVideo);

      auto* event_generator = GetEventGenerator();

      // Resize button is hidden by default, click/tap on the preview to make
      // it visible.
      ClickOrTapView(camera_controller->camera_preview_view(), tablet_enabled,
                     event_generator);

      auto* resize_button = GetPreviewResizeButton();
      DCHECK(resize_button);

      if (collapsed) {
        if (!camera_controller->is_camera_preview_collapsed())
          ClickOrTapView(resize_button, tablet_enabled, event_generator);

        EXPECT_TRUE(camera_controller->is_camera_preview_collapsed());
      } else {
        if (camera_controller->is_camera_preview_collapsed())
          ClickOrTapView(resize_button, tablet_enabled, event_generator);

        EXPECT_FALSE(camera_controller->is_camera_preview_collapsed());
      }

      StartVideoRecordingImmediately();
      controller->EndVideoRecording(EndRecordingReason::kStopRecordingButton);
      WaitForCaptureFileToBeSaved();
      histogram_tester.ExpectBucketCount(histogram_name, sample, 1);
    }
  }
}

// Tests that the camera position on start is recorded correctly in the metrics
// both in clamshell and tablet mode.
TEST_F(CaptureModeCameraTest, RecordingCameraPositionOnStartHistogramTest) {
  constexpr char kHistogramName[] = "RecordingCameraPositionOnStart";
  base::HistogramTester histogram_tester;

  StartCaptureSession(CaptureModeSource::kFullscreen, CaptureModeType::kVideo);
  AddDefaultCamera();
  auto* camera_controller = GetCameraController();
  const CameraId camera_id(kDefaultCameraModelId, 1);
  camera_controller->SetSelectedCamera(camera_id);

  const CameraPreviewSnapPosition kCameraPositionTestCases[]{
      CameraPreviewSnapPosition::kTopLeft,
      CameraPreviewSnapPosition::kBottomLeft,
      CameraPreviewSnapPosition::kTopRight,
      CameraPreviewSnapPosition::kBottomRight};

  for (const bool tablet_enabled : {false, true}) {
    if (tablet_enabled) {
      SwitchToTabletMode();
      EXPECT_TRUE(Shell::Get()->IsInTabletMode());
    } else {
      EXPECT_FALSE(Shell::Get()->IsInTabletMode());
    }

    const std::string histogram_name = BuildHistogramName(
        kHistogramName, /*behavior=*/nullptr, /*append_ui_mode_suffix=*/true);
    for (const auto camera_position : kCameraPositionTestCases) {
      histogram_tester.ExpectBucketCount(histogram_name, camera_position, 0);
      auto* controller = StartCaptureSession(CaptureModeSource::kFullscreen,
                                             CaptureModeType::kVideo);
      DCHECK(camera_controller->camera_preview_widget());
      camera_controller->SetCameraPreviewSnapPosition(camera_position);
      StartVideoRecordingImmediately();
      controller->EndVideoRecording(EndRecordingReason::kStopRecordingButton);
      WaitForCaptureFileToBeSaved();
      histogram_tester.ExpectBucketCount(histogram_name, camera_position, 1);
    }
  }
}

TEST_F(CaptureModeCameraTest, ToastVisibilityChangeOnCaptureRegionUpdated) {
  UpdateDisplay("800x600");

  auto* controller =
      StartCaptureSession(CaptureModeSource::kRegion, CaptureModeType::kVideo);
  auto* capture_session =
      static_cast<CaptureModeSession*>(controller->capture_mode_session());
  ASSERT_EQ(capture_session->session_type(), SessionType::kReal);
  auto* camera_controller = GetCameraController();
  auto* capture_toast_controller = capture_session->capture_toast_controller();
  AddDefaultCamera();
  camera_controller->SetSelectedCamera(CameraId(kDefaultCameraModelId, 1));

  // Set capture region big enough to fit the camera preview. Verify the
  // current capture toast is `kUserNudge`.
  const gfx::Rect capture_region(100, 100, 300, 300);
  SelectCaptureRegion(capture_region);
  auto* capture_toast_widget = capture_toast_controller->capture_toast_widget();
  EXPECT_TRUE(capture_toast_widget);
  ASSERT_TRUE(capture_toast_controller->current_toast_type());
  EXPECT_EQ(*(capture_toast_controller->current_toast_type()),
            CaptureToastType::kUserNudge);

  // Update capture region small enough to not fit the camera preview. Verify
  // that the capture toast is updated to `kCameraPreview` and the user nudge is
  // dismissed forever.
  const int delta_x =
      capture_mode::kMinCaptureSurfaceShortSideLengthForVisibleCamera - 30 -
      capture_region.width();
  const int delta_y =
      capture_mode::kMinCaptureSurfaceShortSideLengthForVisibleCamera - 30 -
      capture_region.height();
  const gfx::Vector2d delta(delta_x, delta_y);
  auto* event_generator = GetEventGenerator();
  event_generator->set_current_screen_location(capture_region.bottom_right());
  event_generator->PressLeftButton();
  // Verify that when drag starts, the capture toast is hidden.
  EXPECT_FALSE(capture_toast_widget->IsVisible());
  event_generator->MoveMouseTo(capture_region.bottom_right() + delta);
  event_generator->ReleaseLeftButton();
  EXPECT_EQ(*(capture_toast_controller->current_toast_type()),
            CaptureToastType::kCameraPreview);
  EXPECT_TRUE(capture_toast_widget->IsVisible());
  EXPECT_FALSE(GetUserNudgeController());

  // Start dragging the capture region again to update it, but make it still
  // small enough to not fit the camera preview. Verify at the beginning of the
  // drag, preview toast is hidden. After the drag is released, preview toast is
  // shown again.
  const gfx::Vector2d delta1(delta_x + 10, delta_y + 10);
  event_generator->set_current_screen_location(capture_region.bottom_right());
  event_generator->PressLeftButton();
  // Verify that when drag starts, the capture toast is hidden.
  EXPECT_FALSE(capture_toast_widget->IsVisible());
  event_generator->MoveMouseTo(capture_region.bottom_right() + delta1);
  event_generator->ReleaseLeftButton();
  ASSERT_TRUE(capture_toast_controller->current_toast_type());
  EXPECT_EQ(*(capture_toast_controller->current_toast_type()),
            CaptureToastType::kCameraPreview);
  EXPECT_TRUE(capture_toast_widget->IsVisible());

  // Update capture region big enough to show the camera preview. Verify the
  // preview toast is hidden.
  event_generator->set_current_screen_location(capture_region.origin());
  event_generator->PressLeftButton();
  // Verify that when drag starts, the capture toast is hidden.
  EXPECT_FALSE(capture_toast_widget->IsVisible());
  event_generator->MoveMouseTo(capture_region.bottom_right());
  event_generator->ReleaseLeftButton();
  // Verify that since the capture toast is dismissed, current toast type is
  // reset.
  EXPECT_FALSE(capture_toast_controller->current_toast_type());
  EXPECT_FALSE(capture_toast_widget->IsVisible());
}

// Tests that the capture toast will be faded out on time out when there are no
// actions taken.
TEST_F(CaptureModeCameraTest, ToastVisibilityChangeOnTimeOut) {
  UpdateDisplay("800x600");

  auto* controller =
      StartCaptureSession(CaptureModeSource::kRegion, CaptureModeType::kVideo);
  auto* capture_session =
      static_cast<CaptureModeSession*>(controller->capture_mode_session());
  ASSERT_EQ(capture_session->session_type(), SessionType::kReal);
  auto* camera_controller = GetCameraController();
  auto* capture_toast_controller = capture_session->capture_toast_controller();
  AddDefaultCamera();
  camera_controller->SetSelectedCamera(CameraId(kDefaultCameraModelId, 1));

  // Set capture region small enough to not fit the camera preview. Verify the
  // current capture toast is `kCameraPreview`.
  const gfx::Rect capture_region = GetTooSmallToFitCameraRegion();
  SelectCaptureRegion(capture_region);
  auto* capture_toast_widget = capture_toast_controller->capture_toast_widget();
  EXPECT_TRUE(capture_toast_widget->IsVisible());
  ASSERT_TRUE(capture_toast_controller->current_toast_type());
  EXPECT_EQ(*(capture_toast_controller->current_toast_type()),
            CaptureToastType::kCameraPreview);

  // Verify the timer is running after the toast is shown and when the timer is
  // fired, the capture toast is hidden.
  base::OneShotTimer* timer =
      capture_toast_controller->capture_toast_dismiss_timer_for_test();
  EXPECT_TRUE(timer->IsRunning());
  timer->FireNow();
  EXPECT_FALSE(capture_toast_widget->IsVisible());
}

TEST_F(CaptureModeCameraTest, ToastVisibilityChangeOnSettingsMenuOpen) {
  UpdateDisplay("800x600");

  auto* controller =
      StartCaptureSession(CaptureModeSource::kRegion, CaptureModeType::kVideo);
  auto* capture_session =
      static_cast<CaptureModeSession*>(controller->capture_mode_session());
  ASSERT_EQ(capture_session->session_type(), SessionType::kReal);
  auto* camera_controller = GetCameraController();
  auto* capture_toast_controller = capture_session->capture_toast_controller();
  AddDefaultCamera();
  camera_controller->SetSelectedCamera(CameraId(kDefaultCameraModelId, 1));

  // Set capture region small enough to not fit the camera preview. Verify the
  // current capture toast is `kCameraPreview`.
  const gfx::Rect capture_region = GetTooSmallToFitCameraRegion();
  SelectCaptureRegion(capture_region);
  auto* capture_toast_widget = capture_toast_controller->capture_toast_widget();
  EXPECT_TRUE(capture_toast_widget->IsVisible());
  ASSERT_TRUE(capture_toast_controller->current_toast_type());
  EXPECT_EQ(*(capture_toast_controller->current_toast_type()),
            CaptureToastType::kCameraPreview);

  // Now open settings menu, verify that preview toast is dismissed immediately
  // on settings menu open.
  OpenSettingsView();
  EXPECT_FALSE(capture_toast_widget->IsVisible());
}

TEST_F(CaptureModeCameraTest, ToastVisibilityChangeOnCaptureRegionMoved) {
  UpdateDisplay("800x600");

  auto* controller =
      StartCaptureSession(CaptureModeSource::kRegion, CaptureModeType::kVideo);
  auto* capture_session =
      static_cast<CaptureModeSession*>(controller->capture_mode_session());
  ASSERT_EQ(capture_session->session_type(), SessionType::kReal);
  auto* camera_controller = GetCameraController();
  auto* capture_toast_controller = capture_session->capture_toast_controller();
  AddDefaultCamera();
  camera_controller->SetSelectedCamera(CameraId(kDefaultCameraModelId, 1));

  // Set capture region small enough to not fit the camera preview. Verify the
  // current capture toast is `kCameraPreview`.
  const gfx::Rect capture_region = GetTooSmallToFitCameraRegion();
  SelectCaptureRegion(capture_region);
  auto* capture_toast_widget = capture_toast_controller->capture_toast_widget();
  EXPECT_TRUE(capture_toast_widget->IsVisible());
  ASSERT_TRUE(capture_toast_controller->current_toast_type());
  EXPECT_EQ(*(capture_toast_controller->current_toast_type()),
            CaptureToastType::kCameraPreview);

  // Start moving the capture region, verify the preview toast is hidden at the
  // beginning of the move and is shown once the move is done.
  const gfx::Vector2d delta(20, 20);
  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(capture_region.origin() + delta);
  event_generator->PressLeftButton();
  EXPECT_FALSE(capture_toast_widget->IsVisible());
  event_generator->MoveMouseTo(capture_region.CenterPoint());
  event_generator->ReleaseLeftButton();
  EXPECT_TRUE(capture_toast_widget->IsVisible());
}

// Tests that the preview toast shows correctly when capture mode is turned on
// through quick settings which keeps the configurations) from the previous
// session.
TEST_F(CaptureModeCameraTest, ToastVisibilityChangeOnCaptureModeTurnedOn) {
  UpdateDisplay("800x600");

  auto* controller =
      StartCaptureSession(CaptureModeSource::kRegion, CaptureModeType::kVideo);
  auto* capture_session =
      static_cast<CaptureModeSession*>(controller->capture_mode_session());
  ASSERT_EQ(capture_session->session_type(), SessionType::kReal);
  auto* camera_controller = GetCameraController();
  auto* capture_toast_controller = capture_session->capture_toast_controller();
  AddDefaultCamera();
  camera_controller->SetSelectedCamera(CameraId(kDefaultCameraModelId, 1));

  // Set capture region small enough to not fit the camera preview. Verify the
  // current capture toast is `kCameraPreview`.
  const gfx::Rect capture_region = GetTooSmallToFitCameraRegion();
  SelectCaptureRegion(capture_region);
  auto* capture_toast_widget = capture_toast_controller->capture_toast_widget();
  EXPECT_TRUE(capture_toast_widget->IsVisible());
  ASSERT_TRUE(capture_toast_controller->current_toast_type());
  EXPECT_EQ(*(capture_toast_controller->current_toast_type()),
            CaptureToastType::kCameraPreview);

  // Close capture mode.
  controller->Stop();

  // Turn on capture mode again through the quick settings, verify that toast
  // preview is visible.
  controller->Start(CaptureModeEntryType::kQuickSettings);
  capture_session =
      static_cast<CaptureModeSession*>(controller->capture_mode_session());
  capture_toast_controller = capture_session->capture_toast_controller();
  EXPECT_TRUE(capture_toast_controller->capture_toast_widget()->IsVisible());
  ASSERT_TRUE(capture_toast_controller->current_toast_type());
  EXPECT_EQ(*(capture_toast_controller->current_toast_type()),
            CaptureToastType::kCameraPreview);
}

TEST_F(CaptureModeCameraTest, ToastStackingOrderChangeOnCaptureModeTurnedOn) {
  auto* controller =
      StartCaptureSession(CaptureModeSource::kRegion, CaptureModeType::kVideo);
  auto* camera_controller = GetCameraController();
  AddDefaultCamera();
  camera_controller->SetSelectedCamera(CameraId(kDefaultCameraModelId, 1));

  // Set capture region small enough to make capture toast shown.
  const gfx::Rect capture_region = GetTooSmallToFitCameraRegion();
  SelectCaptureRegion(capture_region);

  // Close capture mode.
  controller->Stop();

  // Turn on capture mode again through the quick settings, verify that the
  // stacking order for capture toast relative to other capture UIs is correct.
  controller->Start(CaptureModeEntryType::kQuickSettings);
  base::RunLoop().RunUntilIdle();
  auto* capture_session =
      static_cast<CaptureModeSession*>(controller->capture_mode_session());
  ASSERT_EQ(capture_session->session_type(), SessionType::kReal);
  auto* capture_toast_controller = capture_session->capture_toast_controller();
  auto* capture_toast_widget = capture_toast_controller->capture_toast_widget();
  auto* capture_toast_window = capture_toast_widget->GetNativeWindow();
  auto* capture_label_window =
      capture_session->capture_label_widget()->GetNativeWindow();
  auto* capture_bar_window =
      capture_session->GetCaptureModeBarWidget()->GetNativeWindow();
  auto* camera_preview_window =
      camera_controller->camera_preview_widget()->GetNativeWindow();

  EXPECT_TRUE(
      IsWindowStackedRightBelow(capture_label_window, capture_bar_window));
  EXPECT_TRUE(
      IsWindowStackedRightBelow(capture_toast_window, capture_label_window));
  EXPECT_TRUE(
      IsWindowStackedRightBelow(camera_preview_window, capture_toast_window));
  EXPECT_TRUE(IsLayerStackedRightBelow(capture_session->layer(),
                                       camera_preview_window->layer()));
}

TEST_F(CaptureModeCameraTest, ToastVisibilityChangeOnPerformingCapture) {
  UpdateDisplay("800x600");

  auto* controller =
      StartCaptureSession(CaptureModeSource::kRegion, CaptureModeType::kVideo);
  auto* capture_session =
      static_cast<CaptureModeSession*>(controller->capture_mode_session());
  ASSERT_EQ(capture_session->session_type(), SessionType::kReal);
  auto* camera_controller = GetCameraController();
  auto* capture_toast_controller = capture_session->capture_toast_controller();
  AddDefaultCamera();
  camera_controller->SetSelectedCamera(CameraId(kDefaultCameraModelId, 1));

  // Set capture region small enough to not fit the camera preview. Verify the
  // current capture toast is `kCameraPreview`.
  const gfx::Rect capture_region = GetTooSmallToFitCameraRegion();
  SelectCaptureRegion(capture_region);
  auto* capture_toast_widget = capture_toast_controller->capture_toast_widget();
  EXPECT_TRUE(capture_toast_widget->IsVisible());
  ASSERT_TRUE(capture_toast_controller->current_toast_type());
  EXPECT_EQ(*(capture_toast_controller->current_toast_type()),
            CaptureToastType::kCameraPreview);

  // Perform the screen recording, verify that the capture toast is going to be
  // faded out.
  controller->PerformCapture();
  EXPECT_EQ(capture_toast_widget->GetLayer()->GetTargetOpacity(), 0.f);
}

TEST_F(CaptureModeCameraTest, ToastVisibilityChangeOnMultiDisplays) {
  UpdateDisplay("800x700,801+0-800x700");
  const gfx::Rect first_display_bounds(0, 0, 800, 700);
  const gfx::Rect second_display_bounds(801, 0, 800, 700);

  // Set the window's bounds small enough to not fit the camera preview.
  window()->SetBoundsInScreen(
      gfx::Rect(600, 500, 100, 100),
      display::Screen::GetScreen()->GetDisplayNearestWindow(
          Shell::GetAllRootWindows()[0]));

  // Create a window in the second display and set its bounds small enough to
  // not fit the camera preview.
  std::unique_ptr<aura::Window> window1(CreateTestWindow());
  window1->SetBoundsInScreen(
      gfx::Rect(1400, 500, 100, 100),
      display::Screen::GetScreen()->GetDisplayNearestWindow(
          Shell::GetAllRootWindows()[1]));

  // Start the capture session.
  auto* controller =
      StartCaptureSession(CaptureModeSource::kWindow, CaptureModeType::kVideo);
  auto* camera_controller = GetCameraController();
  auto* capture_session =
      static_cast<CaptureModeSession*>(controller->capture_mode_session());
  ASSERT_EQ(capture_session->session_type(), SessionType::kReal);
  auto* capture_toast_controller = capture_session->capture_toast_controller();
  AddDefaultCamera();
  camera_controller->SetSelectedCamera(CameraId(kDefaultCameraModelId, 1));

  // Move the mouse on top of `window` to select it, since it's too small to fit
  // the camera preview, verify the preview toast shows and it's on the first
  // display.
  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(window()->GetBoundsInScreen().CenterPoint());
  EXPECT_EQ(capture_session->GetSelectedWindow(), window());
  auto* capture_toast_widget = capture_toast_controller->capture_toast_widget();
  EXPECT_TRUE(capture_toast_widget->IsVisible());
  ASSERT_TRUE(capture_toast_controller->current_toast_type());
  EXPECT_EQ(*(capture_toast_controller->current_toast_type()),
            CaptureToastType::kCameraPreview);
  first_display_bounds.Contains(
      capture_toast_widget->GetWindowBoundsInScreen());

  // Now move the mouse to the top of `window1`, since it's also too small to
  // fit the camera preview, verify the preview toast still shows. Since
  // `window1` is on the second display, verify the preview toast also shows up
  // on the second display.
  event_generator->MoveMouseTo(window1->GetBoundsInScreen().CenterPoint());
  EXPECT_EQ(capture_session->GetSelectedWindow(), window1.get());
  EXPECT_TRUE(capture_toast_widget->IsVisible());
  ASSERT_TRUE(capture_toast_controller->current_toast_type());
  EXPECT_EQ(*(capture_toast_controller->current_toast_type()),
            CaptureToastType::kCameraPreview);
  second_display_bounds.Contains(
      capture_toast_widget->GetWindowBoundsInScreen());

  // Move mouse to the outside of `window1`, verify that preview toast is
  // dismissed since there's no window selected for now.
  event_generator->MoveMouseTo({1300, 500});
  EXPECT_FALSE(capture_toast_widget->IsVisible());

  // Update the bounds of `window` big enough to fit the camera preview.
  window()->SetBoundsInScreen(
      gfx::Rect(100, 200, 300, 300),
      display::Screen::GetScreen()->GetDisplayNearestWindow(
          Shell::GetAllRootWindows()[0]));

  // Now move the mouse to the top of `window` again, verify that preview toast
  // is not shown, since the window is big enough to show the camera preview.
  event_generator->MoveMouseTo(window()->GetBoundsInScreen().CenterPoint());
  EXPECT_EQ(capture_session->GetSelectedWindow(), window());
  EXPECT_FALSE(capture_toast_widget->IsVisible());
}

class CaptureModeCameraPreviewTest
    : public CaptureModeCameraTest,
      public testing::WithParamInterface<CaptureModeSource> {
 public:
  enum class CameraPreviewState {
    // The camera preview is shown inside an area that makes its expanded size
    // big enough so it can collapse to a smaller size.
    kCollapsible,

    // The camera preview is shown inside an area that is small enough to
    // disable its collapsability without causing it to hide.
    kNotCollapsible,

    // The camera preview is shown inside an area that is too small for it to
    // show at all.
    kHidden,
  };

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

  gfx::Size GetMinSurfaceSizeForCollapsibleCamera() const {
    const int min_length = capture_mode::kMinCollapsibleCameraPreviewDiameter *
                           capture_mode::kCaptureSurfaceShortSideDivider;
    return gfx::Size(min_length, min_length);
  }

  gfx::Size GetMinSurfaceSizeSoCameraBecomes(
      CameraPreviewState preview_state) const {
    gfx::Size min_size = GetMinSurfaceSizeForCollapsibleCamera();
    switch (preview_state) {
      case CameraPreviewState::kCollapsible:
        min_size.Enlarge(10, 20);
        break;
      case CameraPreviewState::kNotCollapsible:
        min_size.Enlarge(-10, -20);
        break;
      case CameraPreviewState::kHidden:
        const int length_for_hidden =
            capture_mode::kMinCaptureSurfaceShortSideLengthForVisibleCamera - 5;
        min_size.SetSize(length_for_hidden, length_for_hidden - 5);
        break;
    }
    return min_size;
  }

  void ResizeDisplaySoCameraPreviewBecomes(CameraPreviewState preview_state) {
    gfx::Size min_size = GetMinSurfaceSizeSoCameraBecomes(preview_state);
    const int shelf_size = ShelfConfig::Get()->shelf_size();
    min_size.Enlarge(shelf_size, shelf_size);
    UpdateDisplay(min_size.ToString());
  }

  void ResizeRegionSoCameraPreviewBecomes(CameraPreviewState preview_state) {
    CaptureModeController::Get()->SetUserCaptureRegion(
        gfx::Rect(GetMinSurfaceSizeSoCameraBecomes(preview_state)),
        /*by_user=*/true);
  }

  void ResizeWindowSoCameraPreviewBecomes(CameraPreviewState preview_state) {
    auto size = GetMinSurfaceSizeSoCameraBecomes(preview_state);
    if (auto* frame_header =
            capture_mode_util::GetWindowFrameHeader(window())) {
      size.Enlarge(0, frame_header->GetHeaderHeight());
    }
    window()->SetBounds(gfx::Rect(size));
  }

  void ResizeSurfaceSoCameraPreviewBecomes(CameraPreviewState preview_state) {
    switch (GetParam()) {
      case CaptureModeSource::kFullscreen:
        ResizeDisplaySoCameraPreviewBecomes(preview_state);
        break;
      case CaptureModeSource::kRegion:
        ResizeRegionSoCameraPreviewBecomes(preview_state);
        break;
      case CaptureModeSource::kWindow:
        ResizeWindowSoCameraPreviewBecomes(preview_state);
        break;
    }
  }

  // Based on the `CaptureModeSource`, it returns the current capture region's
  // bounds in screen.
  gfx::Rect GetCaptureBoundsInScreen() const {
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
        auto bounds =
            capture_mode_util::GetCaptureWindowConfineBounds(window());
        wm::ConvertRectToScreen(window(), &bounds);
        return bounds;
    }
  }

  gfx::Size GetExpectedPreviewSize(bool collapsed) const {
    return capture_mode_util::CalculateCameraPreviewSizeSpecs(
               GetCaptureBoundsInScreen().size(), collapsed)
        .size;
  }

  // Returns the cursor type when cursor is on top of the current capture
  // surface.
  ui::mojom::CursorType GetCursorTypeOnCaptureSurface() const {
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

TEST_P(CaptureModeCameraPreviewTest, PreviewVisibilityWhileFolderSelection) {
  AddDefaultCamera();
  StartCaptureSessionWithParam();
  CaptureModeTestApi().SelectCameraAtIndex(0);

  // The camera preview should be initially visible.
  auto* controller = CaptureModeController::Get();
  ASSERT_TRUE(controller->IsActive());
  auto* preview_widget = GetCameraController()->camera_preview_widget();
  ASSERT_TRUE(preview_widget);
  EXPECT_TRUE(preview_widget->IsVisible());

  // Click on the settings button, the settings menu should open, and the camera
  // preview should remain visible.
  CaptureModeSessionTestApi session_test_api(
      controller->capture_mode_session());
  auto* settings_button =
      session_test_api.GetCaptureModeBarView()->settings_button();
  auto* event_generator = GetEventGenerator();
  ClickOnView(settings_button, event_generator);
  ASSERT_TRUE(session_test_api.GetCaptureModeSettingsWidget());
  EXPECT_TRUE(preview_widget->IsVisible());

  // Click on the "Select folder ..." option, the folder selection dialog should
  // open, all capture UIs should hide, including the camera preview.
  CaptureModeSettingsTestApi settings_test_api;
  ClickOnView(settings_test_api.GetSelectFolderMenuItem(), event_generator);
  EXPECT_TRUE(session_test_api.IsFolderSelectionDialogShown());
  EXPECT_FALSE(session_test_api.AreAllUisVisible());
  EXPECT_FALSE(preview_widget->IsVisible());

  // Dismiss the folder selection dialog, all capture UIs should show again,
  // including the camera preview.
  FakeFolderSelectionDialogFactory::Get()->CancelDialog();
  EXPECT_FALSE(session_test_api.IsFolderSelectionDialogShown());
  EXPECT_TRUE(session_test_api.AreAllUisVisible());
  EXPECT_TRUE(preview_widget->IsVisible());
}

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

  // Rotate the primary display by 90 degrees. Verify that the camera preview
  // is still at the bottom right corner of capture bounds.
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
// should be snapped to the correct snap position. It tests two use cases,
// when capture session is active and when there's a video recording in
// progress including drag to snap by mouse and by touch.
TEST_P(CaptureModeCameraPreviewTest, CameraPreviewDragToSnap) {
  UpdateDisplay("1600x800");
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

  // Drag the camera preview for a small distance. Tests that even though the
  // snap position does not change, the preview should be snapped back to its
  // previous position.
  DragPreviewToPoint(preview_widget, {capture_bounds_center_point.x() + 20,
                                      capture_bounds_center_point.y() + 20});
  EXPECT_EQ(CameraPreviewSnapPosition::kBottomRight,
            camera_controller->camera_preview_snap_position());
  VerifyPreviewAlignment(GetCaptureBoundsInScreen());

  // Drag and drop camera preview by mouse to the top right of the
  // `capture_bounds_center_point`, verify that camera preview is snapped to
  // the top right with correct position.
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

  // Start video recording, verify camera preview is snapped to the correct
  // snap position at the end of drag when there's a video recording in
  // progress.
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

  // Now drag and drop camera preview by touch to the bottom right of the
  // center point, verify that camera preview is snapped to the bottom right
  // with correct position.
  DragPreviewToPoint(preview_widget,
                     {capture_bounds_center_point.x() + 20,
                      capture_bounds_center_point.y() + 20},
                     /*by_touch_gestures=*/true);
  EXPECT_EQ(CameraPreviewSnapPosition::kBottomRight,
            camera_controller->camera_preview_snap_position());
  VerifyPreviewAlignment(GetCaptureBoundsInScreen());
}

// Tests the use case after pressing on the resize button on camera preview and
// releasing the press outside of camera preview, camera preview is still
// draggable. Regression test for https://crbug.com/1308885.
TEST_P(CaptureModeCameraPreviewTest,
       CameraPreviewDragToSnapAfterPressOnResizeButton) {
  StartCaptureSessionWithParam();
  auto* camera_controller = GetCameraController();
  AddDefaultCamera();
  camera_controller->SetSelectedCamera(CameraId(kDefaultCameraModelId, 1));
  auto* preview_widget = camera_controller->camera_preview_widget();
  auto* resize_button = GetPreviewResizeButton();
  const int camera_previw_width =
      preview_widget->GetWindowBoundsInScreen().width();
  const gfx::Point capture_bounds_center_point =
      GetCaptureBoundsInScreen().CenterPoint();
  const gfx::Point center_point_of_resize_button =
      resize_button->GetBoundsInScreen().CenterPoint();

  // By default the snap position of preview widget should be `kBottomRight`.
  EXPECT_EQ(CameraPreviewSnapPosition::kBottomRight,
            camera_controller->camera_preview_snap_position());

  auto* event_generator = GetEventGenerator();
  event_generator->set_current_screen_location(center_point_of_resize_button);
  event_generator->PressLeftButton();

  const gfx::Vector2d delta(-camera_previw_width, -camera_previw_width);
  // Now move mouse to the outside of the camera preview and then release.
  event_generator->MoveMouseTo(center_point_of_resize_button + delta);
  event_generator->ReleaseLeftButton();

  // Now try to drag the camera preview to the top left, after camera preview is
  // snapped, the current snap position should be `kTopLeft`.
  DragPreviewToPoint(preview_widget, capture_bounds_center_point + delta);
  EXPECT_EQ(CameraPreviewSnapPosition::kTopLeft,
            camera_controller->camera_preview_snap_position());
}

TEST_P(CaptureModeCameraPreviewTest, CaptureUisVisibilityChangeOnDragAndDrop) {
  StartCaptureSessionWithParam();
  auto* camera_controller = GetCameraController();
  auto* capture_session = static_cast<CaptureModeSession*>(
      CaptureModeController::Get()->capture_mode_session());
  ASSERT_EQ(capture_session->session_type(), SessionType::kReal);
  AddDefaultCamera();
  camera_controller->SetSelectedCamera(CameraId(kDefaultCameraModelId, 1));
  auto* preview_widget = camera_controller->camera_preview_widget();
  const gfx::Point center_point_of_preview_widget =
      preview_widget->GetWindowBoundsInScreen().CenterPoint();

  const auto* capture_bar_widget = capture_session->GetCaptureModeBarWidget();
  const auto* capture_label_widget = capture_session->capture_label_widget();

  // Press on top of the preview widget. Verify capture bar and capture label
  // are hidden.
  auto* event_generator = GetEventGenerator();
  event_generator->set_current_screen_location(center_point_of_preview_widget);
  event_generator->PressLeftButton();
  EXPECT_FALSE(capture_bar_widget->IsVisible());
  EXPECT_FALSE(capture_label_widget->IsVisible());

  // Now drag and move the preview widget. Verify capture bar and capture
  // label are still hidden.
  const gfx::Vector2d delta(-50, -60);
  event_generator->MoveMouseTo(center_point_of_preview_widget + delta);
  EXPECT_FALSE(capture_bar_widget->IsVisible());
  EXPECT_FALSE(capture_label_widget->IsVisible());

  // Release the drag. Verify capture bar and capture label are shown again.
  event_generator->ReleaseLeftButton();
  EXPECT_TRUE(capture_bar_widget->IsVisible());
  EXPECT_TRUE(capture_label_widget->IsVisible());
}

TEST_P(CaptureModeCameraPreviewTest, CameraPreviewDragToSnapOnMultipleDisplay) {
  UpdateDisplay("800x700,801+0-800x700");

  const gfx::Point point_in_second_display = gfx::Point(1000, 500);
  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(point_in_second_display);

  // Start capture mode on the second display.
  StartCaptureSessionWithParam();
  auto* camera_controller = GetCameraController();
  AddDefaultCamera();
  camera_controller->SetSelectedCamera(CameraId(kDefaultCameraModelId, 1));
  auto* preview_widget = camera_controller->camera_preview_widget();
  const gfx::Point capture_bounds_center_point =
      GetCaptureBoundsInScreen().CenterPoint();

  // Drag and drop camera preview by mouse to the top right of the
  // `capture_bounds_center_point`, verify that camera preview is snapped to
  // the top right with correct position.
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

  // Try to drag and drop camera preview by touch to the top left of the
  // current capture bounds' center point, verity it's not moved. Also verify
  // the snap position is not updated.
  DragPreviewToPoint(preview_widget,
                     {capture_bounds_center_point.x() - 20,
                      capture_bounds_center_point.y() - 20},
                     /*by_touch_gestures=*/true);
  EXPECT_EQ(preview_widget->GetWindowBoundsInScreen(),
            preview_bounds_in_screen_before_drag);
  EXPECT_EQ(camera_controller->camera_preview_snap_position(),
            snap_position_before_drag);
}

// Tests that the bounds of the camera preview widget should always be
// constrained by the capture mode confine bounds.
TEST_P(CaptureModeCameraPreviewTest,
       PreviewWidgetIsConstrainedByConfineBounds) {
  StartCaptureSessionWithParam();
  auto* camera_controller = GetCameraController();
  AddDefaultCamera();
  camera_controller->SetSelectedCamera(CameraId(kDefaultCameraModelId, 1));
  auto* preview_widget = camera_controller->camera_preview_widget();
  ASSERT_TRUE(preview_widget);

  const auto confine_bounds = GetCaptureBoundsInScreen();

  // Create an outsetted bounds to generate locations outside of the confine
  // bounds.
  gfx::Rect outer_rect = confine_bounds;
  outer_rect.Inset(-20);

  for (const auto& release_point :
       {outer_rect.origin(), outer_rect.top_right(), outer_rect.bottom_left(),
        outer_rect.bottom_right()}) {
    DragPreviewToPoint(preview_widget, release_point);
    EXPECT_TRUE(
        confine_bounds.Contains(preview_widget->GetWindowBoundsInScreen()));
  }
}

// Tests that dragging camera preview outside of the preview circle shouldn't
// work even if the drag events are contained in the preview bounds.
TEST_P(CaptureModeCameraPreviewTest, DragPreviewOutsidePreviewCircle) {
  StartCaptureSessionWithParam();
  auto* camera_controller = GetCameraController();
  AddDefaultCamera();
  camera_controller->SetSelectedCamera(CameraId(kDefaultCameraModelId, 1));
  auto* preview_widget = camera_controller->camera_preview_widget();
  const gfx::Point capture_bounds_center_point =
      GetCaptureBoundsInScreen().CenterPoint();
  const gfx::Rect preview_bounds_in_screen_before_drag =
      preview_widget->GetWindowBoundsInScreen();

  // Try to drag camera preview at its origin point, verify camera
  // preview is not draggable and its position is not changed.
  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(preview_bounds_in_screen_before_drag.origin());
  event_generator->PressLeftButton();
  event_generator->MoveMouseTo(capture_bounds_center_point);
  EXPECT_EQ(preview_widget->GetWindowBoundsInScreen(),
            preview_bounds_in_screen_before_drag);
}

// Tests that dragging camera preview outside of the preview circle doesn't
// work when video recording is in progress.
TEST_P(CaptureModeCameraPreviewTest,
       DragPreviewOutsidePreviewCircleWhileVideoRecordingInProgress) {
  StartCaptureSessionWithParam();
  auto* camera_controller = GetCameraController();
  AddDefaultCamera();
  camera_controller->SetSelectedCamera(CameraId(kDefaultCameraModelId, 1));
  auto* preview_widget = camera_controller->camera_preview_widget();
  const gfx::Point capture_bounds_center_point =
      GetCaptureBoundsInScreen().CenterPoint();

  const gfx::Rect preview_bounds_in_screen_before_drag =
      preview_widget->GetWindowBoundsInScreen();
  const auto snap_position_before_drag =
      camera_controller->camera_preview_snap_position();
  // Verify by default snap position is `kBottomRight`.
  EXPECT_EQ(snap_position_before_drag, CameraPreviewSnapPosition::kBottomRight);

  // Try to drag camera preview at its origin point to the top left of current
  // capture bounds' center point, verity it's not moved.
  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(preview_bounds_in_screen_before_drag.origin());
  event_generator->PressLeftButton();
  event_generator->MoveMouseTo(capture_bounds_center_point);
  EXPECT_EQ(preview_widget->GetWindowBoundsInScreen(),
            preview_bounds_in_screen_before_drag);

  // Release drag, verify snap position is not changed.
  event_generator->ReleaseLeftButton();
  EXPECT_EQ(camera_controller->camera_preview_snap_position(),
            snap_position_before_drag);
}

// Tests that when mouse event is on top of camera preview circle, cursor type
// should be updated accordingly.
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

  auto* cursor_manager = Shell::Get()->cursor_manager();
  // Verify that moving mouse to the origin point on camera preview won't
  // update the cursor type to `kPointer`.
  event_generator->MoveMouseTo(preview_bounds_in_screen.origin());
  EXPECT_NE(cursor_manager->GetCursor(), ui::mojom::CursorType::kPointer);

  // Verify that moving mouse on camera preview will update the cursor type
  // to `kPointer`.
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

  // Continue dragging and then drop camera preview, make sure cursor's
  // position is outside of camera preview after it's snapped. Verify cursor
  // type is updated to the correct type of the current capture source.
  DragPreviewToPoint(preview_widget, {camera_preview_origin_point.x() - 20,
                                      camera_preview_origin_point.y() - 20});
  EXPECT_EQ(cursor_manager->GetCursor(), GetCursorTypeOnCaptureSurface());
}

// Tests the functionality of resize button on changing the size of the camera
// preview widget, updating the icon image and tooltip text after clicking on
// it. It also tests the ability to restore to previous resize button settings
// if any when initiating a new capture mode session.
TEST_P(CaptureModeCameraPreviewTest, ResizePreviewWidget) {
  UpdateDisplay("800x700");
  StartCaptureSessionWithParam();
  auto* controller = CaptureModeController::Get();
  auto* camera_controller = GetCameraController();
  AddDefaultCamera();
  camera_controller->SetSelectedCamera(CameraId(kDefaultCameraModelId, 1));

  views::Widget* preview_widget = camera_controller->camera_preview_widget();
  DCHECK(preview_widget);
  const auto default_preview_bounds = preview_widget->GetWindowBoundsInScreen();
  EXPECT_EQ(default_preview_bounds.size(),
            GetExpectedPreviewSize(/*collapsed=*/false));

  auto* resize_button = GetPreviewResizeButton();
  auto* event_generator = GetEventGenerator();

  // Tests the default settings of the resize button.
  VerifyResizeButton(camera_controller->is_camera_preview_collapsed(),
                     resize_button);

  // First time click on resize button will make the preview widget collapse
  // to half of the default size with tooltip text and resize button icon
  // changed to expanded related contents accordingly.
  ClickOnView(resize_button, event_generator);
  EXPECT_EQ(preview_widget->GetWindowBoundsInScreen().size(),
            GetExpectedPreviewSize(/*collapsed=*/true));
  VerifyResizeButton(camera_controller->is_camera_preview_collapsed(),
                     resize_button);

  // Second time click on resize button will make the preview widget expand
  // back to the default size with tooltip text and resize button icon changed
  // to the collapsed related contents accordingly.
  ClickOnView(resize_button, event_generator);
  EXPECT_EQ(preview_widget->GetWindowBoundsInScreen(), default_preview_bounds);
  VerifyResizeButton(camera_controller->is_camera_preview_collapsed(),
                     resize_button);

  // Click on the resize button again will collapse the preview widget. Exit the
  // session and start a new session, the settings for preview widget bounds and
  // resize button will be restored.
  ClickOnView(resize_button, event_generator);
  EXPECT_EQ(preview_widget->GetWindowBoundsInScreen().size(),
            GetExpectedPreviewSize(/*collapsed=*/true));
  VerifyResizeButton(camera_controller->is_camera_preview_collapsed(),
                     resize_button);
  const auto collapsed_preview_bounds =
      preview_widget->GetWindowBoundsInScreen();
  controller->Stop();

  StartCaptureSessionWithParam();
  preview_widget = camera_controller->camera_preview_widget();
  EXPECT_TRUE(preview_widget);
  EXPECT_EQ(preview_widget->GetWindowBoundsInScreen(),
            collapsed_preview_bounds);

  resize_button = GetPreviewResizeButton();
  EXPECT_TRUE(resize_button);
  VerifyResizeButton(camera_controller->is_camera_preview_collapsed(),
                     resize_button);
}

// Tests that resizing the camera preview using the resize button, which uses
// the bounds animation, works correctly on a secondary display. Regression test
// for https://crbug.com/1313247.
TEST_P(CaptureModeCameraPreviewTest, MultiDisplayResize) {
  UpdateDisplay("800x700,801+0-800x700");
  ASSERT_EQ(2u, Shell::GetAllRootWindows().size());

  auto* camera_controller = GetCameraController();
  AddDefaultCamera();
  camera_controller->SetSelectedCamera(CameraId(kDefaultCameraModelId, 1));

  // Put the cursor in the secondary display, and expect the session root to be
  // there.
  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(gfx::Point(900, 500));
  StartCaptureSessionWithParam();
  auto* controller = CaptureModeController::Get();
  auto* session = controller->capture_mode_session();
  auto* display_2_root = Shell::GetAllRootWindows()[1].get();

  // When capturing a window, set its bounds such that it is placed on the
  // secondary display.
  if (GetParam() == CaptureModeSource::kWindow) {
    views::Widget::GetWidgetForNativeWindow(window())->SetBounds(
        {900, 10, 700, 650});
    EXPECT_EQ(display_2_root, window()->GetRootWindow());
    event_generator->MoveMouseToCenterOf(window());
  }

  EXPECT_EQ(display_2_root, session->current_root());

  VerifyPreviewAlignment(GetCaptureBoundsInScreen());

  auto* resize_button = GetPreviewResizeButton();
  ClickOnView(resize_button, event_generator);

  VerifyPreviewAlignment(GetCaptureBoundsInScreen());
}

// Tests the visibility of the resize button on mouse events.
TEST_P(CaptureModeCameraPreviewTest, ResizeButtonVisibilityOnMouseEvents) {
  UpdateDisplay("1366x768");

  StartCaptureSessionWithParam();
  CaptureModeCameraController* camera_controller = GetCameraController();
  AddDefaultCamera();
  camera_controller->SetSelectedCamera(CameraId(kDefaultCameraModelId, 1));
  views::Widget* preview_widget = camera_controller->camera_preview_widget();
  DCHECK(preview_widget);
  const gfx::Rect default_preview_bounds =
      preview_widget->GetWindowBoundsInScreen();

  CameraPreviewResizeButton* resize_button = GetPreviewResizeButton();
  auto* event_generator = GetEventGenerator();

  // Tests that the resize button is hidden by default.
  EXPECT_FALSE(resize_button->GetVisible());

  // Tests that the resize button will show up when the mouse is entering the
  // bounds of the preview widget.
  event_generator->MoveMouseTo(default_preview_bounds.CenterPoint());
  EXPECT_TRUE(resize_button->GetVisible());

  // Tests that the resize button will stay visible while moving the mouse
  // within the bounds of the preview widget.
  event_generator->MoveMouseTo(default_preview_bounds.top_center());
  EXPECT_TRUE(resize_button->GetVisible());

  // Tests that when the mouse is exiting the bounds of the preview widget, the
  // resize button will disappear after the predefined duration.
  auto outside_point = default_preview_bounds.origin();
  outside_point.Offset(-1, -1);
  event_generator->MoveMouseTo(outside_point);

  base::OneShotTimer* timer = camera_controller->camera_preview_view()
                                  ->resize_button_hide_timer_for_test();
  EXPECT_TRUE(timer->IsRunning());
  EXPECT_EQ(timer->GetCurrentDelay(), capture_mode::kResizeButtonShowDuration);

  {
    ViewVisibilityChangeWaiter waiter(resize_button);
    EXPECT_TRUE(resize_button->GetVisible());
    timer->FireNow();
    waiter.Wait();
    EXPECT_FALSE(resize_button->GetVisible());
  }
}

// Tests the visibility of the resize button on tap events.
TEST_P(CaptureModeCameraPreviewTest, ResizeButtonVisibilityOnTapEvents) {
  UpdateDisplay("800x700");
  StartCaptureSessionWithParam();
  CaptureModeCameraController* camera_controller = GetCameraController();
  AddDefaultCamera();
  camera_controller->SetSelectedCamera(CameraId(kDefaultCameraModelId, 1));
  views::Widget* preview_widget = camera_controller->camera_preview_widget();
  DCHECK(preview_widget);
  const gfx::Rect default_preview_bounds =
      preview_widget->GetWindowBoundsInScreen();

  CameraPreviewResizeButton* resize_button = GetPreviewResizeButton();
  auto* event_generator = GetEventGenerator();

  // Tests that the resize button is hidden by default.
  EXPECT_FALSE(resize_button->GetVisible());

  // Tests that resize button shows up when tapping within the bounds of the
  // preview widget and will fade out after the predefined duration.
  event_generator->GestureTapAt(default_preview_bounds.CenterPoint());
  EXPECT_TRUE(resize_button->GetVisible());
  base::OneShotTimer* timer = camera_controller->camera_preview_view()
                                  ->resize_button_hide_timer_for_test();
  EXPECT_TRUE(timer->IsRunning());
  EXPECT_EQ(timer->GetCurrentDelay(), capture_mode::kResizeButtonShowDuration);

  {
    ViewVisibilityChangeWaiter waiter(resize_button);
    timer->FireNow();
    waiter.Wait();
    EXPECT_FALSE(resize_button->GetVisible());
  }
}

// Tests the visibility of the resize button on camera preview drag to snap.
TEST_P(CaptureModeCameraPreviewTest,
       ResizeButtonVisibilityOnCameraPreviewDragToSnap) {
  UpdateDisplay("1366x768");
  StartCaptureSessionWithParam();
  CaptureModeCameraController* camera_controller = GetCameraController();
  AddDefaultCamera();
  camera_controller->SetSelectedCamera(CameraId(kDefaultCameraModelId, 1));
  views::Widget* preview_widget = camera_controller->camera_preview_widget();
  const gfx::Rect preview_bounds = preview_widget->GetWindowBoundsInScreen();

  CameraPreviewResizeButton* resize_button = GetPreviewResizeButton();
  auto* event_generator = GetEventGenerator();

  // Tests that the resize button is hidden by default.
  EXPECT_FALSE(resize_button->GetVisible());

  // Tests that the resize button will show up when the mouse is entering the
  // bounds of the preview widget.
  event_generator->MoveMouseTo(preview_bounds.CenterPoint());
  EXPECT_TRUE(resize_button->GetVisible());

  // Tests that when start dragging camera preview, resize button will be
  // hidden.
  event_generator->PressLeftButton();
  EXPECT_FALSE(resize_button->GetVisible());

  // Drag camera preview, test that resize button is still hidden.
  event_generator->MoveMouseBy(-300, -300);
  EXPECT_FALSE(resize_button->GetVisible());

  // Now release drag, verify that resize button is still hidden since cursor is
  // not on top of camera preview after it's snapped.
  event_generator->ReleaseLeftButton();
  EXPECT_FALSE(resize_button->GetVisible());

  // Now drag camera preview with a small distance, make sure when it's snapped
  // cursor is still on top of it. Verify that resize button is shown after
  // camera preview is snapped.
  const gfx::Vector2d delta(-30, -30);
  DragPreviewToPoint(preview_widget, preview_bounds.CenterPoint() + delta);
  EXPECT_TRUE(resize_button->GetVisible());
}

TEST_P(CaptureModeCameraPreviewTest, CameraPreviewDeintersectsWithSystemTray) {
  UpdateDisplay("1366x768");

  // Open system tray.
  ui::test::EventGenerator* event_generator = GetEventGenerator();

  auto* system_tray = GetPrimaryUnifiedSystemTray();
  event_generator->MoveMouseTo(system_tray->GetBoundsInScreen().CenterPoint());
  event_generator->ClickLeftButton();
  EXPECT_TRUE(system_tray->IsBubbleShown());

  StartCaptureSessionWithParam();
  auto* camera_controller = GetCameraController();
  // Verify current default snap position is the `kBottomRight` before we select
  // a camera device.
  EXPECT_EQ(camera_controller->camera_preview_snap_position(),
            CameraPreviewSnapPosition::kBottomRight);

  AddDefaultCamera();
  camera_controller->SetSelectedCamera(CameraId(kDefaultCameraModelId, 1));

  auto* preview_widget = camera_controller->camera_preview_widget();
  EXPECT_TRUE(system_tray->IsBubbleShown());

  // Verify that camera preview doesn't overlap with system tray when it's
  // shown. Also verify current snap position is updated and not `kBottomRight`
  // anymore.
  EXPECT_FALSE(system_tray->GetBubbleBoundsInScreen().Intersects(
      preview_widget->GetWindowBoundsInScreen()));
  EXPECT_NE(camera_controller->camera_preview_snap_position(),
            CameraPreviewSnapPosition::kBottomRight);
}

TEST_P(CaptureModeCameraPreviewTest,
       CameraPreviewDeintersectsWithSystemTrayWhileVideoRecordingInProgress) {
  // Update display size big enough to make sure when capture source is
  // `kWindow`, the selected window is not system tray.
  UpdateDisplay("1366x768");

  StartCaptureSessionWithParam();
  auto* camera_controller = GetCameraController();
  AddDefaultCamera();
  camera_controller->SetSelectedCamera(CameraId(kDefaultCameraModelId, 1));
  auto* preview_widget = camera_controller->camera_preview_widget();
  const gfx::Point capture_bounds_center_point =
      GetCaptureBoundsInScreen().CenterPoint();

  StartVideoRecordingImmediately();
  // Verify current snap position is `kBottomRight`;
  EXPECT_EQ(camera_controller->camera_preview_snap_position(),
            CameraPreviewSnapPosition::kBottomRight);
  // Now open system tray.
  ui::test::EventGenerator* event_generator = GetEventGenerator();
  auto* system_tray = GetPrimaryUnifiedSystemTray();
  event_generator->MoveMouseTo(system_tray->GetBoundsInScreen().CenterPoint());
  event_generator->ClickLeftButton();
  EXPECT_TRUE(system_tray->IsBubbleShown());

  // Verify that after system tray is open, camera preview is snapped and
  // doesn't overlap with system tray.
  EXPECT_NE(camera_controller->camera_preview_snap_position(),
            CameraPreviewSnapPosition::kBottomRight);
  EXPECT_FALSE(system_tray->GetBubbleBoundsInScreen().Intersects(
      preview_widget->GetWindowBoundsInScreen()));

  // Now try to drag camera preview to the bottom right corner, verify that
  // since system tray is still open, when drag is released, camera preview is
  // not snapped to the bottom right corner even it's the nearest corner to the
  // release position if system tray is still shown.
  const gfx::Vector2d delta(20, 20);
  DragPreviewToPoint(preview_widget, capture_bounds_center_point + delta);
  // Please notice, when capture source is `kWindow`, once the drag starts,
  // system tray will be closed, in this use case we just need to verify camera
  // preview is snapped to the bottom right corner.
  if (system_tray->IsBubbleShown()) {
    EXPECT_NE(camera_controller->camera_preview_snap_position(),
              CameraPreviewSnapPosition::kBottomRight);
    EXPECT_FALSE(system_tray->GetBubbleBoundsInScreen().Intersects(
        preview_widget->GetWindowBoundsInScreen()));
  } else {
    EXPECT_EQ(camera_controller->camera_preview_snap_position(),
              CameraPreviewSnapPosition::kBottomRight);
  }
}

TEST_P(CaptureModeCameraPreviewTest, CameraPreviewDeintersectsWithPipWindow) {
  // Create a window at the bottom right of the display, then convert it to a
  // PIP window.
  std::unique_ptr<aura::Window> pip_window(
      CreateTestWindow(gfx::Rect(700, 450, 104, 100)));
  ConvertToPipWindow(pip_window.get());
  const gfx::Rect origin_pip_window_bounds = pip_window->GetBoundsInScreen();

  StartCaptureSessionWithParam();
  auto* camera_controller = GetCameraController();
  AddDefaultCamera();
  camera_controller->SetSelectedCamera(CameraId(kDefaultCameraModelId, 1));
  auto* preview_widget = camera_controller->camera_preview_widget();

  // Verify that after preview widget is enabled, pip window is repositioned to
  // avoid the overlap with camera preview.
  EXPECT_EQ(camera_controller->camera_preview_snap_position(),
            CameraPreviewSnapPosition::kBottomRight);
  const gfx::Rect current_pip_window_bounds = pip_window->GetBoundsInScreen();
  EXPECT_NE(origin_pip_window_bounds, current_pip_window_bounds);
  EXPECT_FALSE(current_pip_window_bounds.Intersects(
      preview_widget->GetWindowBoundsInScreen()));
}

TEST_P(CaptureModeCameraPreviewTest,
       CameraPreviewDeintersectsWithPipWindowDuringRecording) {
  // Create a window at the top left of the display, then convert it to a PIP
  // window.
  std::unique_ptr<aura::Window> pip_window(
      CreateTestWindow(gfx::Rect(0, 0, 104, 100)));
  ConvertToPipWindow(pip_window.get());
  const gfx::Rect origin_pip_window_bounds = pip_window->GetBoundsInScreen();

  StartCaptureSessionWithParam();
  auto* camera_controller = GetCameraController();
  AddDefaultCamera();
  camera_controller->SetSelectedCamera(CameraId(kDefaultCameraModelId, 1));
  auto* preview_widget = camera_controller->camera_preview_widget();
  const gfx::Point capture_bounds_center_point =
      GetCaptureBoundsInScreen().CenterPoint();

  // Verify camera preview is enabled, current snap position is `kBottomRight`
  // and pip window is not repositioned since there's no overlap.
  EXPECT_TRUE(preview_widget);
  EXPECT_EQ(camera_controller->camera_preview_snap_position(),
            CameraPreviewSnapPosition::kBottomRight);
  EXPECT_EQ(origin_pip_window_bounds, pip_window->GetBoundsInScreen());

  StartVideoRecordingImmediately();
  // Now drag camera preview to the top left corner, verify pip window is
  // repositioned to avoid overlap with camera preview.
  const gfx::Vector2d delta(-20, -20);
  DragPreviewToPoint(preview_widget, capture_bounds_center_point + delta);
  EXPECT_EQ(camera_controller->camera_preview_snap_position(),
            CameraPreviewSnapPosition::kTopLeft);
  EXPECT_NE(origin_pip_window_bounds, pip_window->GetBoundsInScreen());
  EXPECT_FALSE(preview_widget->GetWindowBoundsInScreen().Intersects(
      pip_window->GetBoundsInScreen()));
}

TEST_P(CaptureModeCameraPreviewTest,
       CameraPreviewDeintersectsWithAutoclickBar) {
  // Update display size big enough to make sure when capture source is
  // `kWindow`, the selected window is not system tray.
  UpdateDisplay("1366x768");

  views::Widget* autoclick_bubble_widget = EnableAndGetAutoClickBubbleWidget();
  EXPECT_TRUE(autoclick_bubble_widget->IsVisible());
  const gfx::Rect origin_autoclick_bar_bounds =
      autoclick_bubble_widget->GetWindowBoundsInScreen();

  StartCaptureSessionWithParam();
  auto* camera_controller = GetCameraController();
  AddDefaultCamera();
  camera_controller->SetSelectedCamera(CameraId(kDefaultCameraModelId, 1));
  auto* preview_widget = camera_controller->camera_preview_widget();

  // Verify that after preview widget is enabled, autoclick bar is repositioned
  // to avoid the overlap with camera preview.
  EXPECT_EQ(camera_controller->camera_preview_snap_position(),
            CameraPreviewSnapPosition::kBottomRight);
  const gfx::Rect current_autoclick_bar_bounds =
      autoclick_bubble_widget->GetWindowBoundsInScreen();
  EXPECT_NE(origin_autoclick_bar_bounds, current_autoclick_bar_bounds);
  EXPECT_FALSE(current_autoclick_bar_bounds.Intersects(
      preview_widget->GetWindowBoundsInScreen()));
}

TEST_P(CaptureModeCameraPreviewTest,
       CameraPreviewDeintersectsWithSystemTrayOnSizeChanged) {
  // Update display size to make sure when system tray is open, camera preview
  // can stay in the same side with it when camera preview is collapsed,
  // otherwise, camera preview should be snapped to the other side of the
  // display.
  UpdateDisplay("1366x950");

  StartCaptureSessionWithParam();
  auto* camera_controller = GetCameraController();
  AddDefaultCamera();
  camera_controller->SetSelectedCamera(CameraId(kDefaultCameraModelId, 1));
  auto* preview_widget = camera_controller->camera_preview_widget();

  // Verify camera preview is enabled, and by default, the current snap position
  // should be `kBottomRight`.
  EXPECT_TRUE(preview_widget);
  EXPECT_EQ(camera_controller->camera_preview_snap_position(),
            CameraPreviewSnapPosition::kBottomRight);

  // Click on resize button to collapse camera preview.
  auto* resize_button = GetPreviewResizeButton();
  auto* event_generator = GetEventGenerator();
  ClickOnView(resize_button, event_generator);
  EXPECT_TRUE(camera_controller->is_camera_preview_collapsed());

  StartVideoRecordingImmediately();
  // Open system tray.
  auto* system_tray = GetPrimaryUnifiedSystemTray();
  event_generator->MoveMouseTo(system_tray->GetBoundsInScreen().CenterPoint());
  event_generator->ClickLeftButton();
  EXPECT_TRUE(system_tray->IsBubbleShown());

  // After system tray is shown, verify that camera preview is snapped to the
  // top right corner, and there's no overlap between camera preview and system
  // tray.
  EXPECT_TRUE(system_tray->IsBubbleShown());
  EXPECT_EQ(camera_controller->camera_preview_snap_position(),
            CameraPreviewSnapPosition::kTopRight);
  EXPECT_FALSE(preview_widget->GetWindowBoundsInScreen().Intersects(
      system_tray->GetBoundsInScreen()));

  // Click on the resize button to enlarge camera preview. Verify that camera
  // preview remains snapped to the top right corner, since there's no overlap.
  ClickOnView(resize_button, event_generator);
  EXPECT_FALSE(preview_widget->GetWindowBoundsInScreen().Intersects(
      system_tray->GetBoundsInScreen()));
  EXPECT_EQ(camera_controller->camera_preview_snap_position(),
            CameraPreviewSnapPosition::kTopRight);
}

TEST_P(CaptureModeCameraPreviewTest, CameraPreviewSpecs) {
  AddDefaultCamera();
  CaptureModeTestApi().SelectCameraAtIndex(0);
  auto* camera_controller = GetCameraController();

  struct {
    CameraPreviewState preview_state;
    std::string scope_trace;
  } kTestCases[] = {
      {CameraPreviewState::kCollapsible, "Collapsible Preview"},
      {CameraPreviewState::kNotCollapsible, "Not Collapsible Preview"},
      {CameraPreviewState::kHidden, "Hidden Preview"},
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.scope_trace);

    UpdateDisplay("1366x700");
    StartCaptureSessionWithParam();
    auto* camera_preview_widget = camera_controller->camera_preview_widget();
    auto* camera_preview_view = camera_controller->camera_preview_view();
    EXPECT_TRUE(camera_preview_widget);
    EXPECT_TRUE(camera_preview_widget->IsVisible());
    EXPECT_TRUE(camera_preview_view->is_collapsible());

    ResizeSurfaceSoCameraPreviewBecomes(test_case.preview_state);
    const auto preview_screen_bounds =
        camera_preview_widget->GetWindowBoundsInScreen();
    switch (test_case.preview_state) {
      case CameraPreviewState::kCollapsible:
        EXPECT_TRUE(camera_preview_widget->IsVisible());
        EXPECT_TRUE(camera_preview_view->is_collapsible());
        EXPECT_EQ(preview_screen_bounds.size(),
                  GetExpectedPreviewSize(/*collapsed=*/false));
        break;

      case CameraPreviewState::kNotCollapsible:
        EXPECT_TRUE(camera_preview_widget->IsVisible());
        EXPECT_FALSE(camera_preview_view->is_collapsible());
        EXPECT_EQ(preview_screen_bounds.size(),
                  GetExpectedPreviewSize(/*collapsed=*/false));
        break;

      case CameraPreviewState::kHidden:
        EXPECT_FALSE(camera_preview_widget->IsVisible());
        EXPECT_FALSE(camera_preview_view->is_collapsible());
        break;
    }
  }
}

// Tests that the resize button will stay visible after mouse exiting the
// preview and time exceeding the predefined duration on mouse event when switch
// access is enabled. And the resize button will behave in a default way if
// switch access is not enabled.
TEST_P(CaptureModeCameraPreviewTest,
       ResizeButtonSwitchAccessVisibilityTestOnMouseEvent) {
  UpdateDisplay("1366x768");

  CaptureModeCameraController* camera_controller = GetCameraController();
  AddDefaultCamera();
  camera_controller->SetSelectedCamera(CameraId(kDefaultCameraModelId, 1));
  auto* event_generator = GetEventGenerator();

  for (const bool switch_access_enabled : {false, true}) {
    AccessibilityController* a11y_controller =
        Shell::Get()->accessibility_controller();
    a11y_controller->switch_access().SetEnabled(switch_access_enabled);
    EXPECT_EQ(switch_access_enabled, a11y_controller->IsSwitchAccessRunning());

    StartCaptureSessionWithParam();
    views::Widget* preview_widget = camera_controller->camera_preview_widget();
    DCHECK(preview_widget);
    gfx::Rect preview_bounds = preview_widget->GetWindowBoundsInScreen();
    CameraPreviewResizeButton* resize_button = GetPreviewResizeButton();

    // Tests the default visibility of the resize button based on whether switch
    // access is enabled or not.
    EXPECT_EQ(resize_button->GetVisible(),
              switch_access_enabled ? true : false);

    event_generator->MoveMouseTo(preview_bounds.CenterPoint());
    EXPECT_TRUE(resize_button->GetVisible());

    auto outside_point = preview_bounds.origin();
    outside_point.Offset(-1, -1);
    event_generator->MoveMouseTo(outside_point);
    base::OneShotTimer* timer = camera_controller->camera_preview_view()
                                    ->resize_button_hide_timer_for_test();
    timer->FireNow();
    EXPECT_EQ(resize_button->GetVisible(),
              switch_access_enabled ? true : false);

    // Tests that the resize button will be hidden when start dragging the
    // camera preview regardless of whether the switch access is enabled or not.
    event_generator->MoveMouseTo(preview_bounds.CenterPoint());
    EXPECT_TRUE(resize_button->GetVisible());
    event_generator->PressLeftButton();
    EXPECT_FALSE(resize_button->GetVisible());
    event_generator->MoveMouseBy(-100, -100);
    EXPECT_FALSE(resize_button->GetVisible());

    // Tests that the resize button will be visible if the switch access is
    // enabled after releasing the drag and not visible otherwise.
    event_generator->ReleaseLeftButton();
    EXPECT_EQ(resize_button->GetVisible(),
              switch_access_enabled ? true : false);

    CaptureModeController::Get()->Stop();
  }
}

// Tests that the resize button will stay visible after tapping on the preview
// and time exceeding the predefined duration on tap event when switch access is
// enabled. And the resize button will behave in a default way if switch
// access is not enabled.
TEST_P(CaptureModeCameraPreviewTest,
       ResizeButtonSwitchAccessVisibilityTestOnTapEvent) {
  UpdateDisplay("1366x768");

  SwitchToTabletMode();
  EXPECT_TRUE(Shell::Get()->IsInTabletMode());

  CaptureModeCameraController* camera_controller = GetCameraController();
  AddDefaultCamera();
  camera_controller->SetSelectedCamera(CameraId(kDefaultCameraModelId, 1));
  auto* event_generator = GetEventGenerator();

  for (const bool switch_access_enabled : {false, true}) {
    AccessibilityController* a11y_controller =
        Shell::Get()->accessibility_controller();
    a11y_controller->switch_access().SetEnabled(switch_access_enabled);
    EXPECT_EQ(switch_access_enabled, a11y_controller->IsSwitchAccessRunning());

    StartCaptureSessionWithParam();
    views::Widget* preview_widget = camera_controller->camera_preview_widget();
    DCHECK(preview_widget);
    gfx::Rect preview_bounds = preview_widget->GetWindowBoundsInScreen();
    CameraPreviewResizeButton* resize_button = GetPreviewResizeButton();

    // Tests the default visibility of the resize button based on whether switch
    // access is enabled or not.
    EXPECT_EQ(resize_button->GetVisible(),
              switch_access_enabled ? true : false);

    event_generator->GestureTapAt(preview_bounds.CenterPoint());
    EXPECT_TRUE(resize_button->GetVisible());

    base::OneShotTimer* timer = camera_controller->camera_preview_view()
                                    ->resize_button_hide_timer_for_test();
    if (timer->IsRunning())
      timer->FireNow();

    EXPECT_EQ(resize_button->GetVisible(),
              switch_access_enabled ? true : false);
    CaptureModeController::Get()->Stop();
  }
}

INSTANTIATE_TEST_SUITE_P(All,
                         CaptureModeCameraPreviewTest,
                         testing::Values(CaptureModeSource::kFullscreen,
                                         CaptureModeSource::kRegion,
                                         CaptureModeSource::kWindow));

// -----------------------------------------------------------------------------
// CameraPreviewWithNotificationTest:

class CameraPreviewWithNotificationTest : public CaptureModeCameraTest {
 public:
  CameraPreviewWithNotificationTest() = default;
  CameraPreviewWithNotificationTest(const CameraPreviewWithNotificationTest&) =
      delete;
  CameraPreviewWithNotificationTest& operator=(
      const CameraPreviewWithNotificationTest&) = delete;
  ~CameraPreviewWithNotificationTest() override = default;

  // CaptureModeCameraTest:
  void SetUp() override {
    CaptureModeCameraTest::SetUp();

    auto test_api = std::make_unique<NotificationCenterTestApi>();
    // Add a notification to show the notification center tray in the shelf.
    test_api->AddNotification();
    ASSERT_TRUE(test_api->IsTrayShown());
  }
};

TEST_F(CameraPreviewWithNotificationTest,
       AvoidCollisionWithNotificationBubbleShownFirst) {
  NotificationCenterTray* notification_center_tray =
      GetPrimaryNotificationCenterTray();

  // Click the notification center tray to open the corresponding bubble.
  LeftClickOn(notification_center_tray);
  EXPECT_TRUE(notification_center_tray->IsBubbleShown());

  StartCaptureSession(CaptureModeSource::kFullscreen, CaptureModeType::kVideo);
  auto* camera_controller = GetCameraController();
  // Verify current default snap position is `kBottomRight` before we select a
  // camera device.
  EXPECT_EQ(camera_controller->camera_preview_snap_position(),
            CameraPreviewSnapPosition::kBottomRight);

  AddDefaultCamera();
  camera_controller->SetSelectedCamera(CameraId(kDefaultCameraModelId, 1));
  EXPECT_TRUE(notification_center_tray->IsBubbleShown());

  auto* preview_widget = camera_controller->camera_preview_widget();
  // The camera preview should not intersect with the notification bubble when
  // it is shown. The snap position should be updated to avoid this.
  EXPECT_FALSE(
      notification_center_tray->GetBubbleView()->GetBoundsInScreen().Intersects(
          preview_widget->GetWindowBoundsInScreen()));
  EXPECT_NE(camera_controller->camera_preview_snap_position(),
            CameraPreviewSnapPosition::kBottomRight);
}

TEST_F(CameraPreviewWithNotificationTest,
       AvoidCollisionWithCameraPreviewShownFirst) {
  StartCaptureSession(CaptureModeSource::kFullscreen, CaptureModeType::kVideo);
  auto* camera_controller = GetCameraController();
  AddDefaultCamera();
  camera_controller->SetSelectedCamera(CameraId(kDefaultCameraModelId, 1));
  StartVideoRecordingImmediately();

  NotificationCenterTray* notification_center_tray =
      GetPrimaryNotificationCenterTray();

  // The camera preview should be snapped to the bottom right when the
  // notification bubble is not shown.
  EXPECT_FALSE(notification_center_tray->IsBubbleShown());
  EXPECT_EQ(camera_controller->camera_preview_snap_position(),
            CameraPreviewSnapPosition::kBottomRight);

  // Click the notification center tray to open the corresponding bubble. The
  // camera preview snap position should be updated to avoid the collision.
  LeftClickOn(notification_center_tray);
  EXPECT_TRUE(notification_center_tray->IsBubbleShown());
  auto* preview_widget = camera_controller->camera_preview_widget();
  EXPECT_FALSE(
      notification_center_tray->GetBubbleView()->GetBoundsInScreen().Intersects(
          preview_widget->GetWindowBoundsInScreen()));
  EXPECT_NE(camera_controller->camera_preview_snap_position(),
            CameraPreviewSnapPosition::kBottomRight);
}

// -----------------------------------------------------------------------------
// CameraPreviewWithHoldingSpaceTest:

class CameraPreviewWithHoldingSpaceTest : public CaptureModeCameraTest {
 public:
  CameraPreviewWithHoldingSpaceTest() = default;
  CameraPreviewWithHoldingSpaceTest(const CameraPreviewWithHoldingSpaceTest&) =
      delete;
  CameraPreviewWithHoldingSpaceTest& operator=(
      const CameraPreviewWithHoldingSpaceTest&) = delete;
  ~CameraPreviewWithHoldingSpaceTest() override = default;

  HoldingSpaceModel* model() { return &model_; }

  testing::NiceMock<MockHoldingSpaceClient>* client() {
    return &holding_space_client_;
  }

  HoldingSpaceTestApi* holding_space_test_api() {
    return holding_space_test_api_.get();
  }

  // CaptureModeCameraTest:
  void SetUp() override {
    CaptureModeCameraTest::SetUp();

    holding_space_test_api_ = std::make_unique<HoldingSpaceTestApi>();
    AccountId user_account = AccountId::FromUserEmail(kTestUser);
    HoldingSpaceController::Get()->RegisterClientAndModelForUser(
        user_account, client(), model());

    TestSessionControllerClient* session = GetSessionControllerClient();
    session->AddUserSession(kTestUser);
    holding_space_prefs::MarkTimeOfFirstAvailability(
        session->GetUserPrefService(user_account));
    holding_space_prefs::MarkTimeOfFirstAdd(
        session->GetUserPrefService(user_account));
    session->SwitchActiveUser(user_account);
  }

  void TearDown() override {
    holding_space_test_api_.reset();
    CaptureModeCameraTest::TearDown();
  }

 private:
  std::unique_ptr<HoldingSpaceTestApi> holding_space_test_api_;
  testing::NiceMock<MockHoldingSpaceClient> holding_space_client_;
  HoldingSpaceModel model_;
};

TEST_F(CameraPreviewWithHoldingSpaceTest,
       AvoidCollisionWithHoldingSpaceBubbleShownFirst) {
  EXPECT_TRUE(holding_space_test_api()->IsShowingInShelf());
  // Tap on the holding space tray to show the corresponding bubble.
  holding_space_test_api()->Show();
  EXPECT_TRUE(holding_space_test_api()->IsShowing());

  StartCaptureSession(CaptureModeSource::kFullscreen, CaptureModeType::kVideo);
  auto* camera_controller = GetCameraController();
  // Verify current default snap position is `kBottomRight` before we select a
  // camera device.
  EXPECT_EQ(camera_controller->camera_preview_snap_position(),
            CameraPreviewSnapPosition::kBottomRight);

  AddDefaultCamera();
  camera_controller->SetSelectedCamera(CameraId(kDefaultCameraModelId, 1));
  EXPECT_TRUE(holding_space_test_api()->IsShowing());

  auto* preview_widget = camera_controller->camera_preview_widget();
  // The camera preview should not intersect with the holding space bubble when
  // it is shown. The snap position should be updated to avoid this.
  EXPECT_FALSE(
      holding_space_test_api()->GetBubble()->GetBoundsInScreen().Intersects(
          preview_widget->GetWindowBoundsInScreen()));
  EXPECT_NE(camera_controller->camera_preview_snap_position(),
            CameraPreviewSnapPosition::kBottomRight);
}

TEST_F(CameraPreviewWithHoldingSpaceTest,
       AvoidCollisionWithCameraPreviewShownFirst) {
  StartCaptureSession(CaptureModeSource::kFullscreen, CaptureModeType::kVideo);
  auto* camera_controller = GetCameraController();
  AddDefaultCamera();
  camera_controller->SetSelectedCamera(CameraId(kDefaultCameraModelId, 1));
  StartVideoRecordingImmediately();

  EXPECT_TRUE(holding_space_test_api()->IsShowingInShelf());
  // The camera preview should be snapped to the bottom right when the holding
  // space bubble is not shown.
  EXPECT_FALSE(holding_space_test_api()->IsShowing());
  EXPECT_EQ(camera_controller->camera_preview_snap_position(),
            CameraPreviewSnapPosition::kBottomRight);

  // Tap on the holding space tray to show the corresponding bubble. The camera
  // preview snap position should be updated to avoid the collision.
  holding_space_test_api()->Show();
  EXPECT_TRUE(holding_space_test_api()->IsShowing());
  auto* preview_widget = camera_controller->camera_preview_widget();
  EXPECT_FALSE(
      holding_space_test_api()->GetBubble()->GetBoundsInScreen().Intersects(
          preview_widget->GetWindowBoundsInScreen()));
  EXPECT_NE(camera_controller->camera_preview_snap_position(),
            CameraPreviewSnapPosition::kBottomRight);
}

// -----------------------------------------------------------------------------
// ProjectorCaptureModeCameraTest:

class ProjectorCaptureModeCameraTest : public CaptureModeCameraTest {
 public:
  ProjectorCaptureModeCameraTest() = default;
  ~ProjectorCaptureModeCameraTest() override = default;

  // CaptureModeCameraTest:
  void SetUp() override {
    CaptureModeCameraTest::SetUp();
    projector_helper_.SetUp();
  }

  void StartProjectorModeSession() {
    projector_helper_.StartProjectorModeSession();
  }

 private:
  ProjectorCaptureModeIntegrationHelper projector_helper_;
};

TEST_F(ProjectorCaptureModeCameraTest, NoAvailableCameras) {
  // Initially no camera should be selected.
  auto* camera_controller = GetCameraController();
  EXPECT_FALSE(camera_controller->selected_camera().is_valid());

  // Starting a projector session should not result in showing any cameras, or
  // any crashes.
  StartProjectorModeSession();
  EXPECT_FALSE(camera_controller->selected_camera().is_valid());
  EXPECT_FALSE(camera_controller->camera_preview_widget());
}

TEST_F(ProjectorCaptureModeCameraTest, FirstCamSelectedByDefault) {
  AddDefaultCamera();

  // Initially no camera should be selected.
  auto* camera_controller = GetCameraController();
  EXPECT_FALSE(camera_controller->selected_camera().is_valid());

  // Starting a projector session should result in selecting the first available
  // camera by default, and its preview should be visible.
  StartProjectorModeSession();
  EXPECT_TRUE(camera_controller->selected_camera().is_valid());
  EXPECT_TRUE(camera_controller->camera_preview_widget());
}

// Regression test for http://b/353883311. Tests that starting a default capture
// mode session and dismissing it during an active Projector recording should
// not revert the automatically selected camera for the on-going recording.
TEST_F(ProjectorCaptureModeCameraTest,
       DefaultCaptureSessionWhileProjectorRecording) {
  AddDefaultCamera();

  // Start a Projector-initiated session and start recording. The first
  // available camera will be selected by default.
  StartProjectorModeSession();
  auto* camera_controller = GetCameraController();
  EXPECT_TRUE(camera_controller->selected_camera().is_valid());
  EXPECT_TRUE(camera_controller->camera_preview_widget());
  CaptureModeTestApi test_api;
  test_api.PerformCapture();
  WaitForRecordingToStart();
  auto* controller = CaptureModeController::Get();
  EXPECT_TRUE(controller->is_recording_in_progress());
  EXPECT_TRUE(camera_controller->camera_preview_widget());

  // Start a new default screenshot session while the projector recording is in
  // progress. Ending this session should not revert the auto-selected camera.
  test_api.StartForFullscreen(/*for_video=*/false);
  controller->Stop();
  EXPECT_TRUE(controller->is_recording_in_progress());
  EXPECT_TRUE(camera_controller->selected_camera().is_valid());
  EXPECT_TRUE(camera_controller->camera_preview_widget());
}

TEST_F(ProjectorCaptureModeCameraTest,
       SessionStartsWithAnAlreadySelectedCamera) {
  const std::string model_id_1 = "model1";
  const std::string model_id_2 = "model2";
  AddFakeCamera("/dev/video0", "fake cam 1", model_id_1);
  AddFakeCamera("/dev/video1", "fake cam 2", model_id_2);

  // Initially there's a camera already selected before we start the session,
  // and it's the second camera in the list.
  auto* camera_controller = GetCameraController();
  CameraId cam_id_1(model_id_1, 1);
  CameraId cam_id_2(model_id_2, 1);
  EXPECT_EQ(cam_id_1, camera_controller->available_cameras()[0].camera_id);
  EXPECT_EQ(cam_id_2, camera_controller->available_cameras()[1].camera_id);
  camera_controller->SetSelectedCamera(cam_id_2);
  EXPECT_TRUE(camera_controller->selected_camera().is_valid());

  // Starting a projector session should not result in selecting the first
  // camera. The already selected camera should remain as is.
  StartProjectorModeSession();
  EXPECT_TRUE(camera_controller->selected_camera().is_valid());
  EXPECT_EQ(cam_id_2, camera_controller->selected_camera());
  EXPECT_TRUE(camera_controller->camera_preview_widget());
  CaptureModeController::Get()->Stop();

  // Starting a normal screen capture session and the previously selected
  // `cam_id_2` should remain being selected.
  StartCaptureSession(CaptureModeSource::kFullscreen, CaptureModeType::kVideo);
  EXPECT_TRUE(camera_controller->selected_camera().is_valid());
  EXPECT_EQ(cam_id_2, camera_controller->selected_camera());
  EXPECT_TRUE(camera_controller->camera_preview_widget());
}

// Tests that the recording starts with camera metrics are recorded correctly in
// a projector-initiated recording.
TEST_F(ProjectorCaptureModeCameraTest,
       ProjectorRecordingStartsWithCameraHistogramTest) {
  base::HistogramTester histogram_tester;
  constexpr char kHistogramNameBase[] = "RecordingStartsWithCamera";

  AddDefaultCamera();

  struct {
    bool tablet_enabled;
    bool camera_on;
  } kTestCases[] = {
      {false, false},
      {false, true},
      {true, false},
      {true, true},
  };

  for (const auto test_case : kTestCases) {
    if (test_case.tablet_enabled) {
      SwitchToTabletMode();
      EXPECT_TRUE(Shell::Get()->IsInTabletMode());
    } else {
      EXPECT_FALSE(Shell::Get()->IsInTabletMode());
    }

    const std::string histogram_name = BuildHistogramName(
        kHistogramNameBase,
        CaptureModeTestApi().GetBehavior(BehaviorType::kProjector),
        /*append_ui_mode_suffix=*/true);
    histogram_tester.ExpectBucketCount(histogram_name, test_case.camera_on, 0);

    auto* controller = CaptureModeController::Get();
    controller->SetType(CaptureModeType::kVideo);
    controller->SetSource(CaptureModeSource::kFullscreen);

    StartProjectorModeSession();
    EXPECT_TRUE(controller->IsActive());
    auto* session = controller->capture_mode_session();
    ASSERT_TRUE(session);

    GetCameraController()->SetSelectedCamera(
        test_case.camera_on ? CameraId(kDefaultCameraModelId, 1) : CameraId());

    StartVideoRecordingImmediately();
    EXPECT_TRUE(controller->is_recording_in_progress());

    WaitForSeconds(1);

    controller->EndVideoRecording(EndRecordingReason::kStopRecordingButton);
    WaitForCaptureFileToBeSaved();

    histogram_tester.ExpectBucketCount(histogram_name, test_case.camera_on, 1);
  }
}

// Tests that the auto-selected camera in the projector-initiated capture mode
// session will not be carried over to the normal capture mode session before
// the video recording starts.
TEST_F(ProjectorCaptureModeCameraTest,
       DoNotRememberProjectorCameraSelectionBeforeVideoRecording) {
  AddDefaultCamera();

  // Initially no camera should be selected for the normal capture mode session.
  auto* controller = CaptureModeController::Get();
  StartCaptureSession(CaptureModeSource::kFullscreen, CaptureModeType::kVideo);
  auto* camera_controller = GetCameraController();
  EXPECT_FALSE(camera_controller->selected_camera().is_valid());
  controller->Stop();
  EXPECT_FALSE(camera_controller->selected_camera().is_valid());

  // Starts a projector-initiated capture mode session, the camera will be
  // auto-selected and reset to previous settings after the session.
  StartProjectorModeSession();
  EXPECT_TRUE(camera_controller->selected_camera().is_valid());
  controller->Stop();

  // Starts the capture mode session again and the camera selection settings
  // will be restored.
  StartCaptureSession(CaptureModeSource::kFullscreen, CaptureModeType::kVideo);
  EXPECT_FALSE(camera_controller->selected_camera().is_valid());
}

// Tests that the auto-selected camera in the projector-initiated capture mode
// session will not be carried over to the normal capture mode session after
// completing a video recording.
TEST_F(ProjectorCaptureModeCameraTest,
       DoNotRememberProjectorCameraSelectionAfterVideoRecording) {
  AddDefaultCamera();

  auto* controller = CaptureModeController::Get();
  StartCaptureSession(CaptureModeSource::kFullscreen, CaptureModeType::kVideo);
  auto* camera_controller = GetCameraController();
  EXPECT_FALSE(camera_controller->selected_camera().is_valid());
  controller->Stop();

  // Starts a projector-initiated capture mode session and begin video
  // recording, the camera will be auto-selected and reset to previous settings
  // after the session.
  StartProjectorModeSession();
  EXPECT_TRUE(camera_controller->selected_camera().is_valid());
  StartVideoRecordingImmediately();
  controller->EndVideoRecording(EndRecordingReason::kStopRecordingButton);
  WaitForCaptureFileToBeSaved();

  // Starts the capture mode session again and the camera selection settings
  // will be restored.
  StartCaptureSession(CaptureModeSource::kFullscreen, CaptureModeType::kVideo);
  EXPECT_FALSE(camera_controller->selected_camera().is_valid());
}

// A test fixture for testing the rendered video frames. The boolean parameter
// determines the type of the buffer that backs the video frames. `true` means
// the `kGpuMemoryBuffer` is used, `false` means the `kSharedMemory` buffer type
// is used.
class CaptureModeCameraFramesTest : public CaptureModeCameraTest,
                                    public testing::WithParamInterface<bool> {
 public:
  CaptureModeCameraFramesTest() {
    // Ensure pixels get drawn since they are verified by tests.
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        ::switches::kEnablePixelOutputInTests);
  }

  CaptureModeCameraFramesTest(const CaptureModeCameraFramesTest&) = delete;
  CaptureModeCameraFramesTest& operator=(const CaptureModeCameraFramesTest&) =
      delete;
  ~CaptureModeCameraFramesTest() override = default;

  bool ShouldUseGpuMemoryBuffers() const { return GetParam(); }

  // CaptureModeCameraFramesTest:
  void SetUp() override {
    CaptureModeCameraTest::SetUp();
    CaptureModeTestApi test_api;
    test_api.SetForceUseGpuMemoryBufferForCameraFrames(
        ShouldUseGpuMemoryBuffers());
    AddDefaultCamera();
    ASSERT_EQ(1u, test_api.GetNumberOfAvailableCameras());
    test_api.SelectCameraAtIndex(0);
    const CameraId camera_id(kDefaultCameraModelId, 1);
    EXPECT_EQ(camera_id, GetCameraController()->selected_camera());
  }

  void TearDown() override {
    CaptureModeTestApi().SetForceUseGpuMemoryBufferForCameraFrames(false);
    CaptureModeCameraTest::TearDown();
  }
};

namespace {

// Waits for several rendered frames and verifies that the content of the
// received video frames are the same as that of the produced video frames.
void WaitForAndVerifyRenderedVideoFrame() {
  // PaintCanvasVideoRenderer needs a context provider that is capable of GPU
  // raster to copy the video frame to a bitmap.
  auto context_provider =
      base::MakeRefCounted<viz::TestInProcessContextProvider>(
          viz::TestContextType::kGpuRaster, /*support_locking=*/false);
  auto result = context_provider->BindToCurrentSequence();
  CHECK_EQ(result, gpu::ContextResult::kSuccess);

  // Render a number of frames that are 3 times the size of the buffer pool.
  // This allows us to exercise calls to `OnNewBuffer()` and potentially
  // `OnFrameDropped()`.
  for (size_t i = 0; i < 3 * FakeCameraDevice::kMaxBufferCount; ++i) {
    base::RunLoop loop;
    CaptureModeTestApi().SetOnCameraVideoFrameRendered(
        base::BindLambdaForTesting([&loop, &context_provider](
                                       scoped_refptr<media::VideoFrame> frame) {
          ASSERT_TRUE(frame);
          const gfx::Size frame_size = frame->visible_rect().size();
          const auto produced_frame_bitmap =
              FakeCameraDevice::GetProducedFrameAsBitmap(frame_size);

          media::PaintCanvasVideoRenderer renderer;
          SkBitmap received_frame_bitmap;
          received_frame_bitmap.allocN32Pixels(frame_size.width(),
                                               frame_size.height());
          cc::SkiaPaintCanvas canvas(received_frame_bitmap);
          renderer.Copy(frame, &canvas, context_provider.get());

          EXPECT_TRUE(gfx::test::AreBitmapsEqual(produced_frame_bitmap,
                                                 received_frame_bitmap));

          loop.Quit();
        }));
    loop.Run();
  }
}

}  // namespace

TEST_P(CaptureModeCameraFramesTest, VerifyFrames) {
  CaptureModeTestApi().StartForFullscreen(/*for_video=*/true);
  EXPECT_TRUE(GetCameraController()->camera_preview_widget());
  WaitForAndVerifyRenderedVideoFrame();
}

TEST_P(CaptureModeCameraFramesTest, TurnOffCameraWhileRendering) {
  CaptureModeTestApi test_api;
  test_api.StartForFullscreen(/*for_video=*/true);
  auto* camera_controller = GetCameraController();
  EXPECT_TRUE(camera_controller->camera_preview_widget());
  WaitForAndVerifyRenderedVideoFrame();
  test_api.TurnCameraOff();
  EXPECT_FALSE(camera_controller->camera_preview_widget());
}

TEST_P(CaptureModeCameraFramesTest, DisconnectCameraWhileRendering) {
  CaptureModeTestApi test_api;
  test_api.StartForFullscreen(/*for_video=*/true);
  auto* camera_controller = GetCameraController();
  EXPECT_TRUE(camera_controller->camera_preview_widget());
  WaitForAndVerifyRenderedVideoFrame();
  RemoveDefaultCamera();
  EXPECT_FALSE(camera_controller->camera_preview_widget());
}

TEST_P(CaptureModeCameraFramesTest, SelectAnotherCameraWhileRendering) {
  CaptureModeTestApi test_api;
  test_api.StartForFullscreen(/*for_video=*/true);
  auto* camera_controller = GetCameraController();
  EXPECT_TRUE(camera_controller->camera_preview_widget());
  auto* preview_view = camera_controller->camera_preview_view();
  ASSERT_TRUE(preview_view);
  EXPECT_EQ(preview_view->camera_id(), camera_controller->selected_camera());
  WaitForAndVerifyRenderedVideoFrame();

  // Adding a new camera while rendering an existing one should not affect
  // anything since the new one is not selected yet.
  const std::string device_id = "/dev/video0";
  const std::string display_name = "Integrated Webcam";
  const std::string model_id = "0123:4567";
  AddFakeCamera(device_id, display_name, model_id);
  EXPECT_EQ(preview_view, camera_controller->camera_preview_view());

  // Now select the new camera, a new widget should be created immediately for
  // the new camera.
  const CameraId second_camera_id(model_id, 1);
  camera_controller->SetSelectedCamera(second_camera_id);
  EXPECT_TRUE(camera_controller->camera_preview_widget());
  EXPECT_NE(preview_view, camera_controller->camera_preview_view());
  preview_view = camera_controller->camera_preview_view();
  EXPECT_EQ(preview_view->camera_id(), second_camera_id);
  WaitForAndVerifyRenderedVideoFrame();
}

// Regression test for https://crbug.com/1316230.
TEST_P(CaptureModeCameraFramesTest, CameraFatalErrors) {
  CaptureModeTestApi().StartForFullscreen(/*for_video=*/true);
  auto* camera_controller = GetCameraController();
  EXPECT_TRUE(camera_controller->selected_camera().is_valid());
  EXPECT_TRUE(camera_controller->camera_preview_widget());
  WaitForAndVerifyRenderedVideoFrame();

  // When a camera fatal error happens during rendering, we detect that an
  // consider it as a camera disconnection, which will result in the temporary
  // removal of the preview, before it gets re-added again when we refresh the
  // list of cameras.
  auto* video_source_provider = GetTestDelegate()->video_source_provider();
  video_source_provider->TriggerFatalErrorOnCamera(kDefaultCameraDeviceId);
  CameraDevicesChangeWaiter().Wait();
  EXPECT_FALSE(camera_controller->camera_preview_widget());
  EXPECT_TRUE(camera_controller->selected_camera().is_valid());

  // Now wait for the camera to be re-added again.
  CameraDevicesChangeWaiter().Wait();
  EXPECT_TRUE(camera_controller->camera_preview_widget());
  WaitForAndVerifyRenderedVideoFrame();
}

INSTANTIATE_TEST_SUITE_P(All, CaptureModeCameraFramesTest, testing::Bool());

// The test fixture for starting test without active session.
using NoSessionCaptureModeCameraTest = NoSessionAshTestBase;

// Tests that camera info is requested after the user logs in instead of on
// Chrome startup.
TEST_F(NoSessionCaptureModeCameraTest, RequestCameraInfoAfterUserLogsIn) {
  auto* camera_controller = GetCameraController();
  GetTestDelegate()->video_source_provider()->AddFakeCameraWithoutNotifying(
      "/dev/video0", "Integrated Webcam", "0123:4567",
      media::MEDIA_VIDEO_FACING_NONE);

  // Verify that the camera devices info is not updated yet.
  EXPECT_TRUE(camera_controller->available_cameras().empty());

  // Simulate the user login process and wait for the camera info to be updated.
  {
    base::RunLoop loop;
    camera_controller->SetOnCameraListReceivedForTesting(loop.QuitClosure());
    SimulateUserLogin("example@gmail.com", user_manager::UserType::kRegular);
    loop.Run();
  }

  // Verify that after the user logs in, the camera info is up-to-date.
  EXPECT_EQ(camera_controller->available_cameras().size(), 1u);
}

TEST_F(CaptureModeCameraTest, CameraPrivacyIndicators) {
  ui::ScopedAnimationDurationScaleMode animation_scale(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);

  auto* message_center = message_center::MessageCenter::Get();
  auto capture_mode_privacy_notification_id =
      GetPrivacyIndicatorsNotificationId(kCaptureModePrivacyIndicatorId);

  // Initially the session doesn't show any camera preview since the camera
  // hasn't connected yet. There should be no privacy indicators.
  StartCaptureSession(CaptureModeSource::kFullscreen, CaptureModeType::kVideo);
  auto* camera_controller = GetCameraController();
  camera_controller->SetSelectedCamera(CameraId(kDefaultCameraModelId, 1));
  EXPECT_FALSE(camera_controller->camera_preview_widget());
  EXPECT_FALSE(IsCameraIndicatorIconVisible());
  EXPECT_FALSE(IsMicrophoneIndicatorIconVisible());
  EXPECT_FALSE(message_center->FindNotificationById(
      capture_mode_privacy_notification_id));

  // Once the camera gets connected, the camera privacy indicator
  // icon/notification should show. No microphone yet (not until recording
  // starts with audio).
  AddDefaultCamera();
  EXPECT_TRUE(camera_controller->camera_preview_widget());
  EXPECT_TRUE(IsCameraIndicatorIconVisible());
  EXPECT_FALSE(IsMicrophoneIndicatorIconVisible());
  EXPECT_TRUE(message_center->FindNotificationById(
      capture_mode_privacy_notification_id));

  // If the camera gets disconnected for some reason, the indicator should go
  // away, and come back once it reconnects again.
  RemoveDefaultCamera();
  EXPECT_FALSE(camera_controller->camera_preview_widget());
  // The widget closes its window asynchronously, run a loop to finish that.
  base::RunLoop().RunUntilIdle();
  // Fast forward by the minimum duration the privacy indicator should be held.
  task_environment()->FastForwardBy(
      PrivacyIndicatorsController::kPrivacyIndicatorsMinimumHoldDuration);
  EXPECT_FALSE(IsCameraIndicatorIconVisible());
  EXPECT_FALSE(IsMicrophoneIndicatorIconVisible());
  EXPECT_FALSE(message_center->FindNotificationById(
      capture_mode_privacy_notification_id));

  AddDefaultCamera();
  EXPECT_TRUE(camera_controller->camera_preview_widget());
  EXPECT_TRUE(IsCameraIndicatorIconVisible());
  EXPECT_FALSE(IsMicrophoneIndicatorIconVisible());
  EXPECT_TRUE(message_center->FindNotificationById(
      capture_mode_privacy_notification_id));
}

TEST_F(CaptureModeCameraTest, DuringRecordingPrivacyIndicators) {
  ui::ScopedAnimationDurationScaleMode animation_scale(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);

  auto* message_center = message_center::MessageCenter::Get();
  auto capture_mode_privacy_notification_id =
      GetPrivacyIndicatorsNotificationId(kCaptureModePrivacyIndicatorId);

  // Even with the selected camera present, no indicators will show until the
  // capture session starts.
  auto* camera_controller = GetCameraController();
  camera_controller->SetSelectedCamera(CameraId(kDefaultCameraModelId, 1));
  AddDefaultCamera();
  EXPECT_FALSE(camera_controller->camera_preview_widget());
  EXPECT_FALSE(IsCameraIndicatorIconVisible());
  EXPECT_FALSE(IsMicrophoneIndicatorIconVisible());
  EXPECT_FALSE(message_center->FindNotificationById(
      capture_mode_privacy_notification_id));

  auto* capture_controller = StartCaptureSession(CaptureModeSource::kFullscreen,
                                                 CaptureModeType::kVideo);
  EXPECT_TRUE(camera_controller->camera_preview_widget());
  EXPECT_TRUE(IsCameraIndicatorIconVisible());
  EXPECT_FALSE(IsMicrophoneIndicatorIconVisible());
  EXPECT_TRUE(message_center->FindNotificationById(
      capture_mode_privacy_notification_id));

  // When the user selects audio recording, the idicators won't change.
  // Recording has to start first.
  capture_controller->SetAudioRecordingMode(AudioRecordingMode::kMicrophone);
  EXPECT_FALSE(IsMicrophoneIndicatorIconVisible());

  StartRecordingFromSource(CaptureModeSource::kFullscreen);
  EXPECT_TRUE(IsMicrophoneIndicatorIconVisible());
  EXPECT_TRUE(message_center->FindNotificationById(
      capture_mode_privacy_notification_id));

  // Once recording ends, both indicators should disappear.
  capture_controller->EndVideoRecording(
      EndRecordingReason::kStopRecordingButton);
  WaitForCaptureFileToBeSaved();
  // Fast forward by the minimum duration the privacy indicator should be held.
  task_environment()->FastForwardBy(
      PrivacyIndicatorsController::kPrivacyIndicatorsMinimumHoldDuration);
  EXPECT_FALSE(IsCameraIndicatorIconVisible());
  EXPECT_FALSE(IsMicrophoneIndicatorIconVisible());
  EXPECT_FALSE(message_center->FindNotificationById(
      capture_mode_privacy_notification_id));
}

TEST_F(CaptureModeCameraTest, CameraPreviewViewAccessibleProperties) {
  StartCaptureSession(CaptureModeSource::kRegion, CaptureModeType::kVideo);
  AddDefaultCamera();
  GetCameraController()->SetSelectedCamera(CameraId(kDefaultCameraModelId, 1));
  auto* camera_preview_view = GetCameraController()->camera_preview_view();

  ui::AXNodeData data;
  camera_preview_view->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.role, ax::mojom::Role::kVideo);
  EXPECT_EQ(
      data.GetString16Attribute(ax::mojom::StringAttribute::kName),
      l10n_util::GetStringUTF16(IDS_ASH_SCREEN_CAPTURE_CAMERA_PREVIEW_FOCUSED));
}

TEST_F(CaptureModeCameraTest, CaptureModeMenuHeaderAccessibleProperties) {
  StartCaptureSession(CaptureModeSource::kFullscreen, CaptureModeType::kVideo);
  OpenSettingsView();
  CaptureModeSettingsTestApi test_api;
  AddDefaultCamera();
  auto* menu_header = test_api.GetCameraMenuHeader();
  ui::AXNodeData data;

  menu_header->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.role, ax::mojom::Role::kHeader);
}

}  // namespace ash
