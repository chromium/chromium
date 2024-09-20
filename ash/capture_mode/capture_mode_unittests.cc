// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "ash/accessibility/magnifier/docked_magnifier_controller.h"
#include "ash/accessibility/magnifier/magnifier_glass.h"
#include "ash/annotator/annotation_tray.h"
#include "ash/annotator/annotations_overlay_controller.h"
#include "ash/annotator/annotator_controller.h"
#include "ash/app_list/app_list_controller_impl.h"
#include "ash/capture_mode/capture_mode_bar_view.h"
#include "ash/capture_mode/capture_mode_behavior.h"
#include "ash/capture_mode/capture_mode_constants.h"
#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/capture_mode/capture_mode_menu_group.h"
#include "ash/capture_mode/capture_mode_menu_toggle_button.h"
#include "ash/capture_mode/capture_mode_metrics.h"
#include "ash/capture_mode/capture_mode_session.h"
#include "ash/capture_mode/capture_mode_session_focus_cycler.h"
#include "ash/capture_mode/capture_mode_session_test_api.h"
#include "ash/capture_mode/capture_mode_settings_test_api.h"
#include "ash/capture_mode/capture_mode_settings_view.h"
#include "ash/capture_mode/capture_mode_source_view.h"
#include "ash/capture_mode/capture_mode_test_util.h"
#include "ash/capture_mode/capture_mode_type_view.h"
#include "ash/capture_mode/capture_mode_types.h"
#include "ash/capture_mode/capture_mode_util.h"
#include "ash/capture_mode/fake_folder_selection_dialog_factory.h"
#include "ash/capture_mode/stop_recording_button_tray.h"
#include "ash/capture_mode/test_capture_mode_delegate.h"
#include "ash/capture_mode/user_nudge_controller.h"
#include "ash/capture_mode/video_recording_watcher.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/display/cursor_window_controller.h"
#include "ash/display/output_protection_delegate.h"
#include "ash/display/screen_orientation_controller_test_api.h"
#include "ash/display/window_tree_host_manager.h"
#include "ash/projector/projector_controller_impl.h"
#include "ash/projector/projector_metrics.h"
#include "ash/public/cpp/capture_mode/capture_mode_test_api.h"
#include "ash/public/cpp/holding_space/holding_space_test_api.h"
#include "ash/public/cpp/projector/projector_new_screencast_precondition.h"
#include "ash/public/cpp/projector/projector_session.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/test/mock_projector_client.h"
#include "ash/public/cpp/test/shell_test_api.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/icon_button.h"
#include "ash/style/tab_slider_button.h"
#include "ash/system/status_area_widget.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_util.h"
#include "ash/test/test_widget_builder.h"
#include "ash/wm/desks/desk.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/desks/desks_test_util.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_item.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller_test_api.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "base/containers/contains.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/safe_base_name.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "chromeos/ash/services/recording/recording_service_test_api.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "chromeos/dbus/power_manager/suspend.pb.h"
#include "chromeos/ui/frame/caption_buttons/frame_caption_button_container_view.h"
#include "chromeos/ui/frame/frame_header.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user_type.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/viz/privileged/mojom/compositing/frame_sink_video_capture.mojom.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/capture_client.h"
#include "ui/aura/client/capture_client_observer.h"
#include "ui/aura/client/cursor_shape_client.h"
#include "ui/aura/client/window_parenting_client.h"
#include "ui/aura/window_tracker.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/clipboard_buffer.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/cursor/cursor_factory.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/base/mojom/window_show_state.mojom.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/display/screen.h"
#include "ui/display/types/display_constants.h"
#include "ui/display/util/display_util.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_handler.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/coordinate_conversion.h"
#include "ui/wm/core/window_modality_controller.h"
#include "ui/wm/core/window_util.h"

namespace ash {

namespace {

using ::ui::mojom::CursorType;

constexpr char kEndRecordingReasonInClamshellHistogramName[] =
    "Ash.CaptureModeController.EndRecordingReason.ClamshellMode";

// Returns true if the software-composited cursor is enabled.
bool IsCursorCompositingEnabled() {
  return Shell::Get()
      ->window_tree_host_manager()
      ->cursor_window_controller()
      ->is_cursor_compositing_enabled();
}

// Sets up a callback that will be triggered when a capture file (image or
// video) is deleted as a result of a user action. The callback will verify the
// successful deletion of the file, and will quit the given `loop`.
void SetUpFileDeletionVerifier(base::RunLoop* loop) {
  DCHECK(loop);
  CaptureModeTestApi().SetOnCaptureFileDeletedCallback(
      base::BindLambdaForTesting(
          [loop](const base::FilePath& path, bool delete_successful) {
            EXPECT_TRUE(delete_successful);
            base::ScopedAllowBlockingForTesting allow_blocking;
            EXPECT_FALSE(base::PathExists(path));
            loop->Quit();
          }));
}

// Defines a capture client observer, that sets the input capture to the window
// given to the constructor, and destroys it once capture is lost.
class TestCaptureClientObserver : public aura::client::CaptureClientObserver {
 public:
  explicit TestCaptureClientObserver(std::unique_ptr<aura::Window> window)
      : window_(std::move(window)) {
    DCHECK(window_);
    auto* capture_client =
        aura::client::GetCaptureClient(window_->GetRootWindow());
    capture_client->SetCapture(window_.get());
    capture_client->AddObserver(this);
  }

  ~TestCaptureClientObserver() override { StopObserving(); }

  // aura::client::CaptureClientObserver:
  void OnCaptureChanged(aura::Window* lost_capture,
                        aura::Window* gained_capture) override {
    if (lost_capture != window_.get())
      return;

    StopObserving();
    window_.reset();
  }

 private:
  void StopObserving() {
    if (!window_)
      return;

    auto* capture_client =
        aura::client::GetCaptureClient(window_->GetRootWindow());
    capture_client->RemoveObserver(this);
  }

  std::unique_ptr<aura::Window> window_;
};

}  // namespace

class CaptureModeTest : public AshTestBase {
 public:
  CaptureModeTest() = default;
  explicit CaptureModeTest(base::test::TaskEnvironment::TimeSource time)
      : AshTestBase(time) {}
  CaptureModeTest(const CaptureModeTest&) = delete;
  CaptureModeTest& operator=(const CaptureModeTest&) = delete;
  ~CaptureModeTest() override = default;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();
  }

  views::Widget* GetCaptureModeLabelWidget() const {
    auto* session = CaptureModeController::Get()->capture_mode_session();
    DCHECK(session);
    return CaptureModeSessionTestApi(session).GetCaptureLabelWidget();
  }

  CaptureModeSettingsView* GetCaptureModeSettingsView() const {
    auto* session = CaptureModeController::Get()->capture_mode_session();
    DCHECK(session);
    return CaptureModeSessionTestApi(session).GetCaptureModeSettingsView();
  }

  views::Widget* GetCaptureModeSettingsWidget() const {
    auto* session = CaptureModeController::Get()->capture_mode_session();
    DCHECK(session);
    return CaptureModeSessionTestApi(session).GetCaptureModeSettingsWidget();
  }

  bool IsFolderSelectionDialogShown() const {
    auto* session = CaptureModeController::Get()->capture_mode_session();
    DCHECK(session);
    return CaptureModeSessionTestApi(session).IsFolderSelectionDialogShown();
  }

  bool AreAllCaptureSessionUisVisible() const {
    auto* session = CaptureModeController::Get()->capture_mode_session();
    DCHECK(session);
    return CaptureModeSessionTestApi(session).AreAllUisVisible();
  }

  aura::Window* GetDimensionsLabelWindow() const {
    auto* controller = CaptureModeController::Get();
    DCHECK(controller->IsActive());
    auto* widget = CaptureModeSessionTestApi(controller->capture_mode_session())
                       .GetDimensionsLabelWidget();
    return widget ? widget->GetNativeWindow() : nullptr;
  }

  std::optional<gfx::Point> GetMagnifierGlassCenterPoint() const {
    auto* controller = CaptureModeController::Get();
    DCHECK(controller->IsActive());
    auto& magnifier =
        CaptureModeSessionTestApi(controller->capture_mode_session())
            .GetMagnifierGlass();
    if (magnifier.host_widget_for_testing()) {
      return magnifier.host_widget_for_testing()
          ->GetWindowBoundsInScreen()
          .CenterPoint();
    }
    return std::nullopt;
  }

  // Start Capture Mode with source region and type image.
  CaptureModeController* StartImageRegionCapture() {
    return StartCaptureSession(CaptureModeSource::kRegion,
                               CaptureModeType::kImage);
  }

  CaptureModeController* StartSessionAndRecordWindow(aura::Window* window) {
    auto* controller = StartCaptureSession(CaptureModeSource::kWindow,
                                           CaptureModeType::kVideo);
    GetEventGenerator()->MoveMouseToCenterOf(window);
    StartVideoRecordingImmediately();
    EXPECT_TRUE(controller->is_recording_in_progress());
    return controller;
  }

  // Select a region by pressing and dragging the mouse.
  void SelectRegion(const gfx::Rect& region_in_screen,
                    bool release_mouse = true) {
    SelectCaptureModeRegion(GetEventGenerator(), region_in_screen,
                            release_mouse);
  }

  void WaitForSessionToEnd() {
    auto* controller = CaptureModeController::Get();
    if (!controller->IsActive())
      return;

    auto* test_delegate = static_cast<TestCaptureModeDelegate*>(
        controller->delegate_for_testing());
    ASSERT_TRUE(test_delegate);
    base::RunLoop run_loop;
    test_delegate->set_on_session_state_changed_callback(
        run_loop.QuitClosure());
    run_loop.Run();
    ASSERT_FALSE(controller->IsActive());
  }

  void RemoveSecondaryDisplay() {
    const int64_t primary_id = WindowTreeHostManager::GetPrimaryDisplayId();
    display::ManagedDisplayInfo primary_info =
        display_manager()->GetDisplayInfo(primary_id);
    std::vector<display::ManagedDisplayInfo> display_info_list;
    display_info_list.push_back(primary_info);
    display_manager()->OnNativeDisplaysChanged(display_info_list);

    // Spin the run loop so that we get a signal that the associated root window
    // of the removed display is destroyed.
    base::RunLoop().RunUntilIdle();
  }

  void SwitchToUser2() {
    auto* session_controller = GetSessionControllerClient();
    constexpr char kUserEmail[] = "user2@capture_mode";
    session_controller->AddUserSession(kUserEmail);
    session_controller->SwitchActiveUser(AccountId::FromUserEmail(kUserEmail));
  }

  void OpenSettingsView() {
    auto* session = static_cast<CaptureModeSession*>(
        CaptureModeController::Get()->capture_mode_session());
    DCHECK(session);
    ASSERT_EQ(session->session_type(), SessionType::kReal);
    ClickOnView(CaptureModeSessionTestApi(session)
                    .GetCaptureModeBarView()
                    ->settings_button(),
                GetEventGenerator());
  }

  std::unique_ptr<aura::Window> CreateTransientModalChildWindow(
      gfx::Rect child_window_bounds,
      aura::Window* transient_parent) {
    auto child = CreateTestWindow(child_window_bounds);
    wm::AddTransientChild(transient_parent, child.get());
    child->Show();

    child->SetProperty(aura::client::kModalKey, ui::mojom::ModalType::kWindow);
    wm::SetModalParent(child.get(), transient_parent);
    return child;
  }

  void VerifyOverlayWindow(aura::Window* overlay_window,
                           CaptureModeSource source,
                           const gfx::Rect user_region) {
    VerifyOverlayWindowForCaptureMode(overlay_window, GetWindowBeingRecorded(),
                                      source, user_region);
  }

  aura::Window* GetWindowBeingRecorded() const {
    auto* controller = CaptureModeController::Get();
    DCHECK(controller->is_recording_in_progress());
    return controller->video_recording_watcher_for_testing()
        ->window_being_recorded();
  }
};

class CaptureSessionWidgetClosed {
 public:
  explicit CaptureSessionWidgetClosed(views::Widget* widget) {
    DCHECK(widget);
    widget_ = widget->GetWeakPtr();
  }
  CaptureSessionWidgetClosed(const CaptureSessionWidgetClosed&) = delete;
  CaptureSessionWidgetClosed& operator=(const CaptureSessionWidgetClosed&) =
      delete;
  ~CaptureSessionWidgetClosed() = default;

  bool GetWidgetClosed() const { return !widget_ || widget_->IsClosed(); }

 private:
  base::WeakPtr<views::Widget> widget_;
};

TEST_F(CaptureModeTest, StartStop) {
  auto* controller = CaptureModeController::Get();
  controller->Start(CaptureModeEntryType::kQuickSettings);
  EXPECT_TRUE(controller->IsActive());
  // Calling start again is a no-op.
  controller->Start(CaptureModeEntryType::kQuickSettings);
  EXPECT_TRUE(controller->IsActive());

  // Closing the session should close the native window of capture mode bar
  // immediately.
  auto* bar_window = GetCaptureModeBarWidget()->GetNativeWindow();
  aura::WindowTracker tracker({bar_window});
  controller->Stop();
  EXPECT_TRUE(tracker.windows().empty());
  EXPECT_FALSE(controller->IsActive());
}

TEST_F(CaptureModeTest, CheckCursorVisibility) {
  // Hide cursor before entering capture mode.
  auto* cursor_manager = Shell::Get()->cursor_manager();
  cursor_manager->SetCursor(CursorType::kPointer);
  cursor_manager->HideCursor();
  cursor_manager->DisableMouseEvents();
  EXPECT_FALSE(cursor_manager->IsCursorVisible());

  auto* controller = CaptureModeController::Get();
  controller->Start(CaptureModeEntryType::kQuickSettings);
  // After capture mode initialization, cursor should be visible.
  EXPECT_TRUE(cursor_manager->IsCursorVisible());
  EXPECT_TRUE(cursor_manager->IsMouseEventsEnabled());

  // Enter tablet mode.
  SwitchToTabletMode();
  // After entering tablet mode, cursor should be invisible and locked.
  EXPECT_FALSE(cursor_manager->IsCursorVisible());
  EXPECT_TRUE(cursor_manager->IsCursorLocked());

  // Leave tablet mode, cursor should be visible again.
  LeaveTabletMode();
  EXPECT_TRUE(cursor_manager->IsCursorVisible());
}

TEST_F(CaptureModeTest, CheckCursorVisibilityOnTabletMode) {
  auto* cursor_manager = Shell::Get()->cursor_manager();

  // Enter tablet mode.
  SwitchToTabletMode();
  // After entering tablet mode, cursor should be invisible.
  EXPECT_FALSE(cursor_manager->IsCursorVisible());

  // Open capture mode.
  auto* controller = CaptureModeController::Get();
  controller->Start(CaptureModeEntryType::kQuickSettings);
  // Cursor should be invisible since it's still in tablet mode.
  EXPECT_FALSE(cursor_manager->IsCursorVisible());

  // Leave tablet mode, cursor should be visible now.
  LeaveTabletMode();
  EXPECT_TRUE(cursor_manager->IsCursorVisible());
}

// Regression test for https://crbug.com/1172425.
TEST_F(CaptureModeTest, NoCrashOnClearingCapture) {
  TestCaptureClientObserver observer(CreateTestWindow(gfx::Rect(200, 200)));
  auto* controller = StartImageRegionCapture();
  EXPECT_TRUE(controller->IsActive());
}

TEST_F(CaptureModeTest, CheckWidgetClosed) {
  auto* controller = CaptureModeController::Get();
  controller->Start(CaptureModeEntryType::kQuickSettings);
  EXPECT_TRUE(controller->IsActive());
  EXPECT_TRUE(GetCaptureModeBarWidget());
  CaptureSessionWidgetClosed observer(GetCaptureModeBarWidget());
  EXPECT_FALSE(observer.GetWidgetClosed());
  controller->Stop();
  EXPECT_FALSE(controller->IsActive());
  EXPECT_FALSE(controller->capture_mode_session());
  // The Widget should have been destroyed by now.
  EXPECT_TRUE(observer.GetWidgetClosed());
}

TEST_F(CaptureModeTest, StartWithMostRecentTypeAndSource) {
  auto* controller = CaptureModeController::Get();
  controller->SetSource(CaptureModeSource::kFullscreen);
  controller->SetType(CaptureModeType::kVideo);
  controller->Start(CaptureModeEntryType::kQuickSettings);
  EXPECT_TRUE(controller->IsActive());

  EXPECT_FALSE(GetImageToggleButton()->selected());
  EXPECT_TRUE(GetVideoToggleButton()->selected());
  EXPECT_TRUE(GetFullscreenToggleButton()->selected());
  EXPECT_FALSE(GetRegionToggleButton()->selected());
  EXPECT_FALSE(GetWindowToggleButton()->selected());

  ClickOnView(GetCloseButton(), GetEventGenerator());
  EXPECT_FALSE(controller->IsActive());
}

TEST_F(CaptureModeTest, AccessibleCheckedState) {
  auto* controller = CaptureModeController::Get();
  controller->Start(CaptureModeEntryType::kQuickSettings);
  ui::AXNodeData data;
  GetImageToggleButton()->SetSelected(true);
  GetImageToggleButton()->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.GetCheckedState(), ax::mojom::CheckedState::kTrue);

  data = ui::AXNodeData();
  GetImageToggleButton()->SetSelected(false);
  GetImageToggleButton()->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.GetCheckedState(), ax::mojom::CheckedState::kFalse);
}

TEST_F(CaptureModeTest, ChangeTypeAndSourceFromUI) {
  auto* controller = CaptureModeController::Get();
  controller->Start(CaptureModeEntryType::kQuickSettings);
  EXPECT_TRUE(controller->IsActive());

  EXPECT_TRUE(GetImageToggleButton()->selected());
  EXPECT_FALSE(GetVideoToggleButton()->selected());
  auto* event_generator = GetEventGenerator();
  ClickOnView(GetVideoToggleButton(), event_generator);
  EXPECT_FALSE(GetImageToggleButton()->selected());
  EXPECT_TRUE(GetVideoToggleButton()->selected());
  EXPECT_EQ(controller->type(), CaptureModeType::kVideo);

  ClickOnView(GetWindowToggleButton(), event_generator);
  EXPECT_FALSE(GetFullscreenToggleButton()->selected());
  EXPECT_FALSE(GetRegionToggleButton()->selected());
  EXPECT_TRUE(GetWindowToggleButton()->selected());
  EXPECT_EQ(controller->source(), CaptureModeSource::kWindow);

  ClickOnView(GetFullscreenToggleButton(), event_generator);
  EXPECT_TRUE(GetFullscreenToggleButton()->selected());
  EXPECT_FALSE(GetRegionToggleButton()->selected());
  EXPECT_FALSE(GetWindowToggleButton()->selected());
  EXPECT_EQ(controller->source(), CaptureModeSource::kFullscreen);
}

TEST_F(CaptureModeTest, VideoRecordingUiBehavior) {
  // Start Capture Mode in a fullscreen video recording mode.
  CaptureModeController* controller = StartCaptureSession(
      CaptureModeSource::kFullscreen, CaptureModeType::kVideo);
  EXPECT_TRUE(controller->IsActive());
  EXPECT_FALSE(controller->is_recording_in_progress());
  EXPECT_FALSE(IsCursorCompositingEnabled());

  // Hit Enter to begin recording.
  auto* event_generator = GetEventGenerator();
  SendKey(ui::VKEY_RETURN, event_generator);
  EXPECT_EQ(CursorType::kPointer,
            Shell::Get()->cursor_manager()->GetCursor().type());
  WaitForRecordingToStart();
  EXPECT_FALSE(controller->IsActive());
  EXPECT_TRUE(controller->is_recording_in_progress());

  // The composited cursor should remain disabled now that we're using the
  // cursor overlay on the capturer. The stop-recording button should show up in
  // the status area widget.
  EXPECT_FALSE(IsCursorCompositingEnabled());
  auto* stop_recording_button = Shell::GetPrimaryRootWindowController()
                                    ->GetStatusAreaWidget()
                                    ->stop_recording_button_tray();
  EXPECT_TRUE(stop_recording_button->visible_preferred());

  // End recording via the stop-recording button. Expect that it's now hidden.
  base::HistogramTester histogram_tester;
  ClickOnView(stop_recording_button, event_generator);
  EXPECT_FALSE(stop_recording_button->visible_preferred());
  EXPECT_FALSE(controller->is_recording_in_progress());
  EXPECT_FALSE(IsCursorCompositingEnabled());
  histogram_tester.ExpectBucketCount(
      kEndRecordingReasonInClamshellHistogramName,
      EndRecordingReason::kStopRecordingButton, 1);
}

TEST_F(CaptureModeTest, NoCrashOnMultipleClicksOnStopRecordingButton) {
  ash::CaptureModeTestApi test_api;
  test_api.StartForFullscreen(/*for_video=*/true);
  test_api.PerformCapture();
  test_api.FlushRecordingServiceForTesting();

  auto* stop_recording_button = Shell::GetPrimaryRootWindowController()
                                    ->GetStatusAreaWidget()
                                    ->stop_recording_button_tray();
  EXPECT_TRUE(stop_recording_button->visible_preferred());

  // Use slow animations so that the stop recording button takes much longer to
  // hide, so it's easier to repro the crash at http://b/270625738.
  ui::ScopedAnimationDurationScaleMode animation_scale(
      ui::ScopedAnimationDurationScaleMode::SLOW_DURATION);

  LeftClickOn(stop_recording_button);
  test_api.FlushRecordingServiceForTesting();

  // There should be no crash on the second click.
  LeftClickOn(stop_recording_button);
}

// Tests the behavior of repositioning a region with capture mode.
TEST_F(CaptureModeTest, CaptureRegionRepositionBehavior) {
  // Use a set display size as we will be choosing points in this test.
  UpdateDisplay("800x700");

  auto* controller = StartImageRegionCapture();

  // The first time selecting a region, the region is a default rect.
  EXPECT_EQ(gfx::Rect(), controller->user_capture_region());

  // Press down and drag to select a region.
  SelectRegion(gfx::Rect(100, 100, 600, 600));

  // Click somewhere in the center on the region and drag. The whole region
  // should move. Note that the point cannot be in the capture button bounds,
  // which is located in the center of the region.
  auto* event_generator = GetEventGenerator();
  event_generator->set_current_screen_location(gfx::Point(200, 200));
  event_generator->DragMouseBy(-50, -50);
  EXPECT_EQ(gfx::Rect(50, 50, 600, 600), controller->user_capture_region());

  // Try to drag the region offscreen. The region should be bound by the display
  // size.
  event_generator->set_current_screen_location(gfx::Point(100, 100));
  event_generator->DragMouseBy(-150, -150);
  EXPECT_EQ(gfx::Rect(600, 600), controller->user_capture_region());
}

// Tests the behavior of resizing a region with capture mode using the corner
// drag affordances.
TEST_F(CaptureModeTest, CaptureRegionCornerResizeBehavior) {
  // Use a set display size as we will be choosing points in this test.
  UpdateDisplay("800x700");

  auto* controller = StartImageRegionCapture();
  // Create the initial region.
  const gfx::Rect target_region(gfx::Rect(200, 200, 400, 400));
  SelectRegion(target_region);

  // For each corner point try dragging to several points and verify that the
  // capture region is as expected.
  struct {
    std::string trace;
    gfx::Point drag_point;
    // The point that stays the same while dragging. It is the opposite vertex
    // to |drag_point| on |target_region|.
    gfx::Point anchor_point;
  } kDragCornerCases[] = {
      {"origin", target_region.origin(), target_region.bottom_right()},
      {"top_right", target_region.top_right(), target_region.bottom_left()},
      {"bottom_right", target_region.bottom_right(), target_region.origin()},
      {"bottom_left", target_region.bottom_left(), target_region.top_right()},
  };

  // The test corner points are one in each corner outside |target_region| and
  // one point inside |target_region|.
  auto drag_test_points = {gfx::Point(100, 100), gfx::Point(700, 100),
                           gfx::Point(700, 700), gfx::Point(100, 700),
                           gfx::Point(400, 400)};
  auto* event_generator = GetEventGenerator();
  for (auto test_case : kDragCornerCases) {
    SCOPED_TRACE(test_case.trace);
    event_generator->set_current_screen_location(test_case.drag_point);
    event_generator->PressLeftButton();

    // At each drag test point, the region rect should be the rect created by
    // the given |corner_point| and the drag test point. That is, the width
    // should match the x distance between the two points, the height should
    // match the y distance between the two points and that both points are
    // contained in the region.
    for (auto drag_test_point : drag_test_points) {
      event_generator->MoveMouseTo(drag_test_point);
      gfx::Rect region = controller->user_capture_region();
      const gfx::Vector2d distance = test_case.anchor_point - drag_test_point;
      EXPECT_EQ(std::abs(distance.x()), region.width());
      EXPECT_EQ(std::abs(distance.y()), region.height());

      // gfx::Rect::Contains returns the point (x+width, y+height) as false, so
      // make the region one unit bigger to account for this.
      region.Inset(gfx::Insets(-1));
      EXPECT_TRUE(region.Contains(drag_test_point));
      EXPECT_TRUE(region.Contains(test_case.anchor_point));
    }

    // Make sure the region is reset for the next iteration.
    event_generator->MoveMouseTo(test_case.drag_point);
    event_generator->ReleaseLeftButton();
    ASSERT_EQ(target_region, controller->user_capture_region());
  }
}

// Tests the behavior of resizing a region with capture mode using the edge drag
// affordances.
TEST_F(CaptureModeTest, CaptureRegionEdgeResizeBehavior) {
  // Use a set display size as we will be choosing points in this test.
  UpdateDisplay("800x700");

  auto* controller = StartImageRegionCapture();
  // Create the initial region.
  const gfx::Rect target_region(gfx::Rect(200, 200, 200, 200));
  SelectRegion(target_region);

  // For each edge point try dragging to several points and verify that the
  // capture region is as expected.
  struct DragEdgeCase {
    std::string trace;
    gfx::Point drag_point;
    // True if horizontal direction (left, right). Height stays the same while
    // dragging if true, width stays the same while dragging if false.
    bool horizontal;
    // The edge that stays the same while dragging. It is the opposite edge to
    // |drag_point|. For example, if |drag_point| is the left center of
    // |target_region|, then |anchor_edge| is the right edge.
    int anchor_edge;
  };

  // Cases where the drag starts in the center of the edge, i.e., at the
  // indicator circles.
  std::vector<DragEdgeCase> drag_edge_cases = {
      {"left", target_region.left_center(), true, target_region.right()},
      {"top", target_region.top_center(), false, target_region.bottom()},
      {"right", target_region.right_center(), true, target_region.x()},
      {"bottom", target_region.bottom_center(), false, target_region.y()},
  };

  // Append cases where the drag starts along the edge but not at the circles.
  std::vector<DragEdgeCase> offset_cases = {};
  for (auto center_case : drag_edge_cases) {
    DragEdgeCase new_case(center_case);
    center_case.horizontal ? new_case.drag_point.Offset(0, 25)
                           : new_case.drag_point.Offset(25, 0);
    offset_cases.push_back(new_case);
  }
  drag_edge_cases.insert(drag_edge_cases.end(), offset_cases.begin(),
                         offset_cases.end());

  // Drag to a couple of points that change both x and y. In all these cases,
  // only the width or height should change.
  auto drag_test_points = {gfx::Point(150, 150), gfx::Point(350, 350),
                           gfx::Point(450, 450)};
  auto* event_generator = GetEventGenerator();
  for (auto test_case : drag_edge_cases) {
    SCOPED_TRACE(test_case.trace);
    event_generator->set_current_screen_location(test_case.drag_point);
    event_generator->PressLeftButton();

    for (auto drag_test_point : drag_test_points) {
      event_generator->MoveMouseTo(drag_test_point);
      const gfx::Rect region = controller->user_capture_region();

      // One of width/height will always be the same as |target_region|'s
      // initial width/height, depending on the edge affordance. The other
      // dimension will be the distance from |drag_test_point| to the anchor
      // edge.
      const int variable_length = std::abs(
          (test_case.horizontal ? drag_test_point.x() : drag_test_point.y()) -
          test_case.anchor_edge);
      const int expected_width =
          test_case.horizontal ? variable_length : target_region.width();
      const int expected_height =
          test_case.horizontal ? target_region.height() : variable_length;

      EXPECT_EQ(expected_width, region.width());
      EXPECT_EQ(expected_height, region.height());
    }

    // Make sure the region is reset for the next iteration.
    event_generator->MoveMouseTo(test_case.drag_point);
    event_generator->ReleaseLeftButton();
    ASSERT_EQ(target_region, controller->user_capture_region());
  }
}

// Tests that the capture region persists after exiting and reentering capture
// mode.
TEST_F(CaptureModeTest, CaptureRegionPersistsAfterExit) {
  auto* controller = StartImageRegionCapture();
  const gfx::Rect region(100, 100, 200, 200);
  SelectRegion(region);

  controller->Stop();
  controller->Start(CaptureModeEntryType::kQuickSettings);
  EXPECT_EQ(region, controller->user_capture_region());
}

// Tests that the capture region resets when clicking outside the current
// capture regions bounds.
TEST_F(CaptureModeTest, CaptureRegionResetsOnClickOutside) {
  auto* controller = StartImageRegionCapture();
  SelectRegion(gfx::Rect(100, 100, 200, 200));

  // Click on an area outside of the current capture region. The capture region
  // should reset to default rect.
  auto* event_generator = GetEventGenerator();
  event_generator->set_current_screen_location(gfx::Point(400, 400));
  event_generator->ClickLeftButton();
  EXPECT_EQ(gfx::Rect(), controller->user_capture_region());
}

// Tests that buttons on the capture mode bar still work when a region is
// "covering" them.
TEST_F(CaptureModeTest, CaptureRegionCoversCaptureModeBar) {
  UpdateDisplay("800x700");

  auto* controller = StartImageRegionCapture();

  // Select a region such that the capture mode bar is covered.
  SelectRegion(gfx::Rect(5, 5, 795, 695));
  EXPECT_TRUE(controller->user_capture_region().Contains(
      GetCaptureModeBarView()->GetBoundsInScreen()));

  // Click on the fullscreen toggle button to verify that we enter fullscreen
  // capture mode. Then click on the region toggle button to verify that we
  // reenter region capture mode and that the region is still covering the
  // capture mode bar.
  auto* event_generator = GetEventGenerator();
  ClickOnView(GetFullscreenToggleButton(), event_generator);
  EXPECT_EQ(CaptureModeSource::kFullscreen, controller->source());
  ClickOnView(GetRegionToggleButton(), GetEventGenerator());
  ASSERT_EQ(CaptureModeSource::kRegion, controller->source());
  ASSERT_TRUE(controller->user_capture_region().Contains(
      GetCaptureModeBarView()->GetBoundsInScreen()));

  ClickOnView(GetCloseButton(), event_generator);
  EXPECT_FALSE(controller->IsActive());
}

// Tests that the magnifying glass appears while fine tuning the capture region,
// and that the cursor is hidden if the magnifying glass is present.
TEST_F(CaptureModeTest, CaptureRegionMagnifierWhenFineTuning) {
  const gfx::Vector2d kDragDelta(50, 50);
  UpdateDisplay("800x700");

  // Start Capture Mode in a region in image mode.
  StartImageRegionCapture();

  // Press down and drag to select a region. The magnifier should not be
  // visible yet.
  gfx::Rect capture_region{200, 200, 400, 400};
  SelectRegion(capture_region);
  EXPECT_EQ(std::nullopt, GetMagnifierGlassCenterPoint());

  auto check_magnifier_shows_properly = [this](const gfx::Point& origin,
                                               const gfx::Point& destination,
                                               bool should_show_magnifier) {
    // If |should_show_magnifier|, check that the magnifying glass is centered
    // on the mouse after press and during drag, and that the cursor is hidden.
    // If not |should_show_magnifier|, check that the magnifying glass never
    // shows. Should always be not visible when mouse button is released.
    auto* event_generator = GetEventGenerator();
    std::optional<gfx::Point> expected_origin =
        should_show_magnifier ? std::make_optional(origin) : std::nullopt;
    std::optional<gfx::Point> expected_destination =
        should_show_magnifier ? std::make_optional(destination) : std::nullopt;

    auto* cursor_manager = Shell::Get()->cursor_manager();
    EXPECT_TRUE(cursor_manager->IsCursorVisible());

    // Move cursor to |origin| and click.
    event_generator->set_current_screen_location(origin);
    event_generator->PressLeftButton();
    EXPECT_EQ(expected_origin, GetMagnifierGlassCenterPoint());
    EXPECT_NE(should_show_magnifier, cursor_manager->IsCursorVisible());

    // Drag to |destination| while holding left button.
    event_generator->MoveMouseTo(destination);
    EXPECT_EQ(expected_destination, GetMagnifierGlassCenterPoint());
    EXPECT_NE(should_show_magnifier, cursor_manager->IsCursorVisible());

    // Drag back to |origin| while still holding left button.
    event_generator->MoveMouseTo(origin);
    EXPECT_EQ(expected_origin, GetMagnifierGlassCenterPoint());
    EXPECT_NE(should_show_magnifier, cursor_manager->IsCursorVisible());

    // Release left button.
    event_generator->ReleaseLeftButton();
    EXPECT_EQ(std::nullopt, GetMagnifierGlassCenterPoint());
    EXPECT_TRUE(cursor_manager->IsCursorVisible());
  };

  // Drag the capture region from within the existing selected region. The
  // magnifier should not be visible at any point.
  check_magnifier_shows_properly(gfx::Point(400, 250), gfx::Point(500, 350),
                                 /*should_show_magnifier=*/false);

  // Check that each corner fine tune position shows the magnifier when
  // dragging.
  struct {
    std::string trace;
    FineTunePosition position;
  } kFineTunePositions[] = {
      {"top_left_vertex", FineTunePosition::kTopLeftVertex},
      {"top_right_vertex", FineTunePosition::kTopRightVertex},
      {"bottom_right_vertex", FineTunePosition::kBottomRightVertex},
      {"bottom_left_vertex", FineTunePosition::kBottomLeftVertex}};
  for (const auto& fine_tune_position : kFineTunePositions) {
    SCOPED_TRACE(fine_tune_position.trace);
    const gfx::Point drag_affordance_location =
        capture_mode_util::GetLocationForFineTunePosition(
            capture_region, fine_tune_position.position);
    check_magnifier_shows_properly(drag_affordance_location,
                                   drag_affordance_location + kDragDelta,
                                   /*should_show_magnifier=*/true);
  }
}

// Tests that the dimensions label properly renders for capture regions.
TEST_F(CaptureModeTest, CaptureRegionDimensionsLabelLocation) {
  UpdateDisplay("900x800");

  // Start Capture Mode in a region in image mode.
  StartImageRegionCapture();

  // Press down and don't move the mouse. Label shouldn't display for empty
  // capture regions.
  auto* generator = GetEventGenerator();
  generator->set_current_screen_location(gfx::Point(0, 0));
  generator->PressLeftButton();
  auto* controller = CaptureModeController::Get();
  EXPECT_TRUE(controller->IsActive());
  EXPECT_TRUE(controller->user_capture_region().IsEmpty());
  EXPECT_EQ(nullptr, GetDimensionsLabelWindow());
  generator->ReleaseLeftButton();

  // Press down and drag to select a large region. Verify that the dimensions
  // label is centered and that the label is below the capture region.
  gfx::Rect capture_region{100, 100, 600, 200};
  SelectRegion(capture_region, /*release_mouse=*/false);
  EXPECT_EQ(capture_region.CenterPoint().x(),
            GetDimensionsLabelWindow()->bounds().CenterPoint().x());
  EXPECT_EQ(capture_region.bottom() +
                CaptureModeSession::kSizeLabelYDistanceFromRegionDp,
            GetDimensionsLabelWindow()->bounds().y());
  generator->ReleaseLeftButton();
  EXPECT_EQ(nullptr, GetDimensionsLabelWindow());

  // Create a new capture region close to the left side of the screen such that
  // if the label was centered it would extend out of the screen.
  // The x value of the label should be the left edge of the screen (0).
  capture_region.SetRect(2, 100, 2, 100);
  SelectRegion(capture_region, /*release_mouse=*/false);
  EXPECT_EQ(0, GetDimensionsLabelWindow()->bounds().x());
  generator->ReleaseLeftButton();
  EXPECT_EQ(nullptr, GetDimensionsLabelWindow());

  // Create a new capture region close to the right side of the screen such that
  // if the label was centered it would extend out of the screen.
  // The right (x + width) of the label should be the right edge of the screen
  // (900).
  capture_region.SetRect(896, 100, 2, 100);
  SelectRegion(capture_region, /*release_mouse=*/false);
  EXPECT_EQ(900, GetDimensionsLabelWindow()->bounds().right());
  generator->ReleaseLeftButton();
  EXPECT_EQ(nullptr, GetDimensionsLabelWindow());

  // Create a new capture region close to the bottom side of the screen.
  // The label should now appear inside the capture region, just above the
  // bottom edge. It should be above the bottom of the screen as well.
  capture_region.SetRect(100, 700, 600, 100);
  SelectRegion(capture_region, /*release_mouse=*/false);
  EXPECT_EQ(800 - CaptureModeSession::kSizeLabelYDistanceFromRegionDp,
            GetDimensionsLabelWindow()->bounds().bottom());
  generator->ReleaseLeftButton();
  EXPECT_EQ(nullptr, GetDimensionsLabelWindow());
}

TEST_F(CaptureModeTest, CaptureRegionCaptureButtonLocation) {
  UpdateDisplay("900x800");

  auto* controller = StartImageRegionCapture();

  // Select a large region. Verify that the capture button widget is centered.
  SelectRegion(gfx::Rect(100, 100, 600, 600));

  views::Widget* capture_button_widget = GetCaptureModeLabelWidget();
  ASSERT_TRUE(capture_button_widget);
  aura::Window* capture_button_window =
      capture_button_widget->GetNativeWindow();
  EXPECT_EQ(gfx::Point(400, 400),
            capture_button_window->bounds().CenterPoint());

  // Drag the bottom corner so that the region is too small to fit the capture
  // button. Verify that the button is aligned horizontally and placed below the
  // region.
  auto* event_generator = GetEventGenerator();
  event_generator->DragMouseTo(gfx::Point(120, 120));
  EXPECT_EQ(gfx::Rect(100, 100, 20, 20), controller->user_capture_region());
  EXPECT_EQ(110, capture_button_window->bounds().CenterPoint().x());
  const int distance_from_region =
      CaptureModeSession::kCaptureButtonDistanceFromRegionDp;
  EXPECT_EQ(120 + distance_from_region, capture_button_window->bounds().y());

  // Click inside the region to drag the entire region to the bottom of the
  // screen. Verify that the button is aligned horizontally and placed above the
  // region.
  event_generator->set_current_screen_location(gfx::Point(110, 110));
  event_generator->DragMouseTo(gfx::Point(110, 790));
  EXPECT_EQ(gfx::Rect(100, 780, 20, 20), controller->user_capture_region());
  EXPECT_EQ(110, capture_button_window->bounds().CenterPoint().x());
  EXPECT_EQ(780 - distance_from_region,
            capture_button_window->bounds().bottom());
}

// Tests some edge cases to ensure the capture button does not intersect the
// capture bar and end up unclickable since it is stacked below the capture bar.
// Regression test for https://crbug.com/1186462.
TEST_F(CaptureModeTest, CaptureRegionCaptureButtonDoesNotIntersectCaptureBar) {
  UpdateDisplay("800x700");

  StartImageRegionCapture();

  // Create a region that would cover the capture mode bar. Add some insets to
  // ensure that the capture button could fit inside. Verify that the two
  // widgets do not overlap.
  const gfx::Rect capture_bar_bounds =
      GetCaptureModeBarWidget()->GetWindowBoundsInScreen();
  gfx::Rect region_bounds = capture_bar_bounds;
  region_bounds.Inset(-20);
  SelectRegion(region_bounds);
  EXPECT_FALSE(capture_bar_bounds.Intersects(
      GetCaptureModeLabelWidget()->GetWindowBoundsInScreen()));

  // Create a thin region above the capture mode bar. The algorithm would
  // normally place the capture label under the region, but should adjust to
  // avoid intersecting.
  auto* event_generator = GetEventGenerator();
  event_generator->set_current_screen_location(gfx::Point());
  event_generator->ClickLeftButton();
  const int capture_bar_midpoint_x = capture_bar_bounds.CenterPoint().x();
  SelectRegion(
      gfx::Rect(capture_bar_midpoint_x, capture_bar_bounds.y() - 10, 20, 10));
  EXPECT_FALSE(capture_bar_bounds.Intersects(
      GetCaptureModeLabelWidget()->GetWindowBoundsInScreen()));

  // Create a thin region below the capture mode bar which reaches the bottom of
  // the display. The algorithm would  normally place the capture label above
  // the region, but should adjust to avoid intersecting.
  event_generator->set_current_screen_location(gfx::Point());
  event_generator->ClickLeftButton();
  SelectRegion(gfx::Rect(capture_bar_midpoint_x, capture_bar_bounds.bottom(),
                         20, 700 - capture_bar_bounds.bottom()));
  EXPECT_FALSE(capture_bar_bounds.Intersects(
      GetCaptureModeLabelWidget()->GetWindowBoundsInScreen()));

  // Create a thin region that is vertical as tall as the display, and at the
  // left edge of the display. The capture label button should be right of the
  // region.
  event_generator->set_current_screen_location(gfx::Point());
  event_generator->ClickLeftButton();
  SelectRegion(gfx::Rect(20, 700));
  EXPECT_GT(GetCaptureModeLabelWidget()->GetWindowBoundsInScreen().x(), 20);
}

// Tests that pressing on the capture bar and releasing the press outside of the
// capture bar, the capture region could still be draggable and set. Regression
// test for https://crbug.com/1325028.
TEST_F(CaptureModeTest, SetCaptureRegionAfterPressOnCaptureBar) {
  UpdateDisplay("800x600");

  auto* controller =
      StartCaptureSession(CaptureModeSource::kRegion, CaptureModeType::kVideo);
  auto* settings_button = GetSettingsButton();

  // Press on the settings button without release.
  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(
      settings_button->GetBoundsInScreen().CenterPoint());
  event_generator->PressLeftButton();
  // Move mouse to the outside of the capture bar and then release the press.
  event_generator->MoveMouseTo({300, 300});
  event_generator->ReleaseLeftButton();

  // Set the capture region, and verify it's set successfully.
  const gfx::Rect region_bounds(100, 100, 200, 200);
  SelectRegion(region_bounds);
  EXPECT_EQ(controller->user_capture_region(), region_bounds);
}

TEST_F(CaptureModeTest, WindowCapture) {
  // Create 2 windows that overlap with each other.
  const gfx::Rect bounds1(0, 0, 200, 200);
  std::unique_ptr<aura::Window> window1(CreateTestWindow(bounds1));
  const gfx::Rect bounds2(150, 150, 200, 200);
  std::unique_ptr<aura::Window> window2(CreateTestWindow(bounds2));

  auto* controller = CaptureModeController::Get();
  controller->SetSource(CaptureModeSource::kWindow);
  controller->SetType(CaptureModeType::kImage);
  controller->Start(CaptureModeEntryType::kAccelTakeWindowScreenshot);
  EXPECT_TRUE(controller->IsActive());

  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseToCenterOf(window1.get());
  auto* capture_mode_session = controller->capture_mode_session();
  EXPECT_EQ(capture_mode_session->GetSelectedWindow(), window1.get());
  event_generator->MoveMouseToCenterOf(window2.get());
  EXPECT_EQ(capture_mode_session->GetSelectedWindow(), window2.get());

  // Now move the mouse to the overlapped area.
  event_generator->MoveMouseTo(gfx::Point(175, 175));
  EXPECT_EQ(capture_mode_session->GetSelectedWindow(), window2.get());
  // Close the current selected window should automatically focus to next one.
  window2.reset();
  EXPECT_EQ(capture_mode_session->GetSelectedWindow(), window1.get());
  // Open another one on top also change the selected window.
  std::unique_ptr<aura::Window> window3(CreateTestWindow(bounds2));
  EXPECT_EQ(capture_mode_session->GetSelectedWindow(), window3.get());
  // Minimize the window should also automatically change the selected window.
  WindowState::Get(window3.get())->Minimize();
  EXPECT_EQ(capture_mode_session->GetSelectedWindow(), window1.get());

  // Stop the capture session to avoid CaptureModeSession from receiving more
  // events during test tearing down.
  controller->Stop();
}

TEST_F(CaptureModeTest, WindowCaptureConfineBoundsDoNotOverlapWindowCaption) {
  std::unique_ptr<aura::Window> window(CreateTestWindow(gfx::Rect(200, 200)));
  auto* controller =
      StartCaptureSession(CaptureModeSource::kWindow, CaptureModeType::kVideo);
  GetEventGenerator()->MoveMouseToCenterOf(window.get());
  auto* capture_mode_session = controller->capture_mode_session();
  EXPECT_EQ(capture_mode_session->GetSelectedWindow(), window.get());

  auto* frame_header = capture_mode_util::GetWindowFrameHeader(window.get());
  auto* caption_button_container = frame_header->caption_button_container();

  // While the session is still active, the calculated confine bounds should not
  // overlap with the frame caption.
  EXPECT_FALSE(controller->GetCaptureSurfaceConfineBounds().Intersects(
      caption_button_container->bounds()));

  // Start recording and expect that the confine bounds calculated during
  // recording still do not overlap with the frame caption.
  StartVideoRecordingImmediately();
  WaitForRecordingToStart();
  EXPECT_FALSE(controller->GetCaptureSurfaceConfineBounds().Intersects(
      caption_button_container->bounds()));
}

// Tests that the capture bar is located on the root with the cursor when
// starting capture mode.
TEST_F(CaptureModeTest, MultiDisplayCaptureBarInitialLocation) {
  UpdateDisplay("800x700,801+0-800x700");

  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(gfx::Point(1000, 500));

  auto* controller = StartImageRegionCapture();
  EXPECT_TRUE(gfx::Rect(801, 0, 800, 800)
                  .Contains(GetCaptureModeBarView()->GetBoundsInScreen()));
  controller->Stop();

  event_generator->MoveMouseTo(gfx::Point(100, 500));
  StartImageRegionCapture();
  EXPECT_TRUE(gfx::Rect(800, 800).Contains(
      GetCaptureModeBarView()->GetBoundsInScreen()));
}

// Tests behavior of a capture mode session if the active display is removed.
TEST_F(CaptureModeTest, DisplayRemoval) {
  UpdateDisplay("1200x700,1201+0-800x700");

  // Start capture mode on the secondary display.
  GetEventGenerator()->MoveMouseTo(gfx::Point(1300, 500));
  auto* controller = StartImageRegionCapture();
  auto* session = controller->capture_mode_session();
  EXPECT_TRUE(gfx::Rect(1201, 0, 800, 700)
                  .Contains(GetCaptureModeBarView()->GetBoundsInScreen()));
  ASSERT_EQ(Shell::GetAllRootWindows()[1], session->current_root());

  RemoveSecondaryDisplay();

  // Tests that the capture mode bar is now on the primary display.
  const gfx::Rect bar_bounds_in_screen =
      GetCaptureModeBarView()->GetBoundsInScreen();
  EXPECT_TRUE(gfx::Rect(1200, 700).Contains(bar_bounds_in_screen));
  ASSERT_EQ(Shell::GetAllRootWindows()[0], session->current_root());

  // Tests that the capture mode bar is centered on the primary display.
  // Regression test for http://b/303094552.
  EXPECT_EQ(600, bar_bounds_in_screen.CenterPoint().x());
}

// Tests behavior of a capture mode session if the active display is removed
// and countdown running.
TEST_F(CaptureModeTest, DisplayRemovalWithCountdownVisible) {
  UpdateDisplay("800x700,801+0-800x700");

  // Start capture mode on the secondary display.
  auto recorded_window = CreateTestWindow(gfx::Rect(1000, 200, 400, 400));
  auto* controller =
      StartCaptureSession(CaptureModeSource::kWindow, CaptureModeType::kVideo);
  GetEventGenerator()->MoveMouseToCenterOf(recorded_window.get());

  auto* session = controller->capture_mode_session();

  RemoveSecondaryDisplay();

  ASSERT_EQ(Shell::GetAllRootWindows()[0], session->current_root());

  // Test passes if no crash.
}

// Tests behavior of a capture mode session if the active display is removed,
// countdown running, fullscreen window, and in overview mode.
TEST_F(CaptureModeTest,
       DisplayRemovalWithCountdownVisibleFullscreenWindowAndInOverview) {
  UpdateDisplay("800x700,801+0-800x700");

  // Start capture mode on the secondary display.
  auto recorded_window = CreateTestWindow(gfx::Rect(1000, 200, 400, 400));
  auto* controller =
      StartCaptureSession(CaptureModeSource::kWindow, CaptureModeType::kVideo);
  GetEventGenerator()->MoveMouseToCenterOf(recorded_window.get());
  // Make the window fullscreen. This is important as the corner case is
  // moving a fullscreen window triggers the shelf to occur, which changes
  // display metrics.
  recorded_window->SetProperty(aura::client::kShowStateKey,
                               ui::mojom::WindowShowState::kFullscreen);

  auto* session = controller->capture_mode_session();

  EnterOverview();
  RemoveSecondaryDisplay();

  ASSERT_EQ(Shell::GetAllRootWindows()[0], session->current_root());

  // Test passes if no crash.
}

// Tests that using fullscreen or window source, moving the mouse across
// displays will change the root window of the capture session.
TEST_F(CaptureModeTest, MultiDisplayFullscreenOrWindowSourceRootWindow) {
  UpdateDisplay("800x700,801+0-800x700");
  ASSERT_EQ(2u, Shell::GetAllRootWindows().size());

  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(gfx::Point(100, 500));

  for (auto source :
       {CaptureModeSource::kFullscreen, CaptureModeSource::kWindow}) {
    SCOPED_TRACE(source == CaptureModeSource::kFullscreen ? "Fullscreen source"
                                                          : "Window source");

    auto* controller = StartCaptureSession(source, CaptureModeType::kImage);
    auto* session = controller->capture_mode_session();
    EXPECT_EQ(Shell::GetAllRootWindows()[0], session->current_root());

    event_generator->MoveMouseTo(gfx::Point(1000, 500));
    EXPECT_EQ(Shell::GetAllRootWindows()[1], session->current_root());

    event_generator->MoveMouseTo(gfx::Point(100, 500));
    EXPECT_EQ(Shell::GetAllRootWindows()[0], session->current_root());

    controller->Stop();
  }
}

// Tests that in region mode, moving the mouse across displays will not change
// the root window of the capture session, but clicking on a new display will.
TEST_F(CaptureModeTest, MultiDisplayRegionSourceRootWindow) {
  UpdateDisplay("800x700,801+0-800x700");
  ASSERT_EQ(2u, Shell::GetAllRootWindows().size());

  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(gfx::Point(100, 500));

  auto* controller = StartImageRegionCapture();
  auto* session = controller->capture_mode_session();
  EXPECT_EQ(Shell::GetAllRootWindows()[0], session->current_root());

  // Tests that moving the mouse to the secondary display does not change the
  // root.
  event_generator->MoveMouseTo(gfx::Point(1000, 500));
  EXPECT_EQ(Shell::GetAllRootWindows()[0], session->current_root());

  // Tests that pressing the mouse changes the root. The capture bar stays on
  // the primary display until the mouse is released.
  event_generator->PressLeftButton();
  EXPECT_EQ(Shell::GetAllRootWindows()[1], session->current_root());
  EXPECT_TRUE(gfx::Rect(800, 800).Contains(
      GetCaptureModeBarView()->GetBoundsInScreen()));

  event_generator->ReleaseLeftButton();
  EXPECT_EQ(Shell::GetAllRootWindows()[1], session->current_root());
  EXPECT_TRUE(gfx::Rect(801, 0, 800, 800)
                  .Contains(GetCaptureModeBarView()->GetBoundsInScreen()));
}

// Tests that using touch on multi display setups works as intended. Regression
// test for https://crbug.com/1159512.
TEST_F(CaptureModeTest, MultiDisplayTouch) {
  UpdateDisplay("800x700,801+0-800x700");
  ASSERT_EQ(2u, Shell::GetAllRootWindows().size());

  auto* controller = StartImageRegionCapture();
  auto* session = controller->capture_mode_session();
  ASSERT_EQ(Shell::GetAllRootWindows()[0], session->current_root());

  // Touch and move your finger on the secondary display. We should switch roots
  // and the region size should be as expected.
  auto* event_generator = GetEventGenerator();
  event_generator->PressTouch(gfx::Point(1000, 200));
  event_generator->MoveTouch(gfx::Point(1200, 400));
  event_generator->ReleaseTouch();
  EXPECT_EQ(Shell::GetAllRootWindows()[1], session->current_root());
  EXPECT_EQ(gfx::Size(200, 200), controller->user_capture_region().size());
}

TEST_F(CaptureModeTest, RegionCursorStates) {
  UpdateDisplay("800x700,801+0-800x700");

  auto* cursor_manager = Shell::Get()->cursor_manager();
  auto* event_generator = GetEventGenerator();

  struct {
    std::string scoped_trace;
    gfx::Rect display_rect;
    gfx::Point point;
    gfx::Rect capture_region;
  } kRegionTestCases[] = {
      {"primary_display", gfx::Rect(0, 0, 800, 700), gfx::Point(250, 250),
       gfx::Rect(200, 200, 200, 200)},
      {"external_display", gfx::Rect(801, 0, 800, 700), gfx::Point(1050, 250),
       gfx::Rect(1000, 200, 200, 200)},
  };

  for (auto test_case : kRegionTestCases) {
    SCOPED_TRACE(test_case.scoped_trace);
    event_generator->MoveMouseTo(test_case.point);
    const CursorType original_cursor_type = cursor_manager->GetCursor().type();
    EXPECT_FALSE(cursor_manager->IsCursorLocked());
    auto* controller = StartImageRegionCapture();
    EXPECT_TRUE(test_case.display_rect.Contains(
        GetCaptureModeBarView()->GetBoundsInScreen()));
    auto outside_point = test_case.capture_region.origin();
    outside_point.Offset(-10, -10);
    // Clear the previous region if any.
    event_generator->MoveMouseTo(outside_point);
    event_generator->ClickLeftButton();
    EXPECT_TRUE(cursor_manager->IsCursorVisible());
    EXPECT_EQ(CursorType::kCell, cursor_manager->GetCursor().type());
    EXPECT_TRUE(cursor_manager->IsCursorLocked());

    // Makes sure that the cursor is updated when the user releases the region
    // select and is still hovering in the same location.
    SelectRegion(test_case.capture_region);
    EXPECT_EQ(CursorType::kSouthEastResize, cursor_manager->GetCursor().type());

    // Verify that all of the `FineTunePosition` locations have the correct
    // cursor when hovered over both in primary display and external display.
    event_generator->MoveMouseTo(test_case.capture_region.origin());
    EXPECT_EQ(CursorType::kNorthWestResize, cursor_manager->GetCursor().type());
    event_generator->MoveMouseTo(test_case.capture_region.top_center());
    EXPECT_EQ(CursorType::kNorthSouthResize,
              cursor_manager->GetCursor().type());
    event_generator->MoveMouseTo(test_case.capture_region.top_right());
    EXPECT_EQ(CursorType::kNorthEastResize, cursor_manager->GetCursor().type());
    event_generator->MoveMouseTo(test_case.capture_region.right_center());
    EXPECT_EQ(CursorType::kEastWestResize, cursor_manager->GetCursor().type());
    event_generator->MoveMouseTo(test_case.capture_region.bottom_right());
    EXPECT_EQ(CursorType::kSouthEastResize, cursor_manager->GetCursor().type());
    event_generator->MoveMouseTo(test_case.capture_region.bottom_center());
    EXPECT_EQ(CursorType::kNorthSouthResize,
              cursor_manager->GetCursor().type());
    event_generator->MoveMouseTo(test_case.capture_region.bottom_left());
    EXPECT_EQ(CursorType::kSouthWestResize, cursor_manager->GetCursor().type());
    event_generator->MoveMouseTo(test_case.capture_region.left_center());
    EXPECT_EQ(CursorType::kEastWestResize, cursor_manager->GetCursor().type());

    // Tests that within the bounds of the selected region, the cursor is a hand
    // when hovering over the capture button, otherwise it is a
    // multi-directional move cursor.
    event_generator->MoveMouseTo(test_case.point);
    EXPECT_EQ(CursorType::kMove, cursor_manager->GetCursor().type());
    event_generator->MoveMouseTo(test_case.capture_region.CenterPoint());
    EXPECT_EQ(CursorType::kHand, cursor_manager->GetCursor().type());

    // Tests that the cursor changes to a cell type when hovering over the
    // unselected region.
    event_generator->MoveMouseTo(outside_point);
    EXPECT_EQ(CursorType::kCell, cursor_manager->GetCursor().type());

    // Check that cursor is unlocked when changing sources, and that the cursor
    // changes to a pointer when hovering over the capture mode bar.
    event_generator->MoveMouseTo(
        GetRegionToggleButton()->GetBoundsInScreen().CenterPoint());
    EXPECT_EQ(CursorType::kPointer, cursor_manager->GetCursor().type());
    event_generator->MoveMouseTo(
        GetWindowToggleButton()->GetBoundsInScreen().CenterPoint());
    EXPECT_EQ(CursorType::kPointer, cursor_manager->GetCursor().type());
    event_generator->ClickLeftButton();
    ASSERT_EQ(CaptureModeSource::kWindow, controller->source());

    // The event on the capture bar to change capture source will still keep the
    // cursor locked.
    EXPECT_TRUE(cursor_manager->IsCursorLocked());
    EXPECT_EQ(CursorType::kPointer, cursor_manager->GetCursor().type());

    // Tests that on changing back to region capture mode, the cursor becomes
    // locked, and is still a pointer type over the bar, whilst a cell cursor
    // otherwise (not over the selected region).
    event_generator->MoveMouseTo(
        GetRegionToggleButton()->GetBoundsInScreen().CenterPoint());
    event_generator->ClickLeftButton();
    EXPECT_TRUE(cursor_manager->IsCursorLocked());
    EXPECT_EQ(CursorType::kPointer, cursor_manager->GetCursor().type());

    // Tests that clicking on the button again doesn't change the cursor.
    event_generator->ClickLeftButton();
    EXPECT_EQ(CursorType::kPointer, cursor_manager->GetCursor().type());
    event_generator->MoveMouseTo(outside_point);
    EXPECT_EQ(CursorType::kCell, cursor_manager->GetCursor().type());

    // Tests that when exiting capture mode that the cursor is restored to its
    // original state.
    controller->Stop();
    EXPECT_FALSE(controller->IsActive());
    EXPECT_FALSE(cursor_manager->IsCursorLocked());
    EXPECT_EQ(original_cursor_type, cursor_manager->GetCursor().type());
  }

  // Tests the cursor state in tablet mode.
  auto* controller = StartImageRegionCapture();

  // Enter tablet mode, the cursor should be hidden.
  SwitchToTabletMode();
  EXPECT_FALSE(cursor_manager->IsCursorVisible());
  EXPECT_TRUE(cursor_manager->IsCursorLocked());

  // Move mouse but it should still be invisible.
  event_generator->MoveMouseTo(gfx::Point(100, 100));
  EXPECT_FALSE(cursor_manager->IsCursorVisible());
  EXPECT_TRUE(cursor_manager->IsCursorLocked());

  // Return to clamshell mode, mouse should appear again.
  LeaveTabletMode();
  EXPECT_TRUE(cursor_manager->IsCursorVisible());
  EXPECT_EQ(CursorType::kCell, cursor_manager->GetCursor().type());
  controller->Stop();
}

// Regression testing for https://crbug.com/1334824.
TEST_F(CaptureModeTest, CursorShouldNotChangeWhileAdjustingRegion) {
  UpdateDisplay("800x600");

  auto* cursor_manager = Shell::Get()->cursor_manager();
  auto* event_generator = GetEventGenerator();

  EXPECT_FALSE(cursor_manager->IsCursorLocked());
  StartImageRegionCapture();
  event_generator->MoveMouseTo(gfx::Point(200, 200));
  EXPECT_EQ(CursorType::kCell, cursor_manager->GetCursor().type());
  event_generator->PressLeftButton();
  event_generator->MoveMouseTo(gfx::Point(300, 300));
  EXPECT_EQ(CursorType::kSouthEastResize, cursor_manager->GetCursor().type());

  // Drag the region by moving the cursor to the center point of the capture bar
  // and expect that it doesn't change back to a pointer.
  const auto capture_bar_center =
      GetCaptureModeBarView()->GetBoundsInScreen().CenterPoint();
  event_generator->MoveMouseTo(capture_bar_center);
  EXPECT_EQ(CursorType::kSouthEastResize, cursor_manager->GetCursor().type());
}

TEST_F(CaptureModeTest, FullscreenCursorStates) {
  auto* cursor_manager = Shell::Get()->cursor_manager();
  CursorType original_cursor_type = cursor_manager->GetCursor().type();
  EXPECT_FALSE(cursor_manager->IsCursorLocked());
  EXPECT_EQ(CursorType::kPointer, original_cursor_type);

  auto* event_generator = GetEventGenerator();
  CaptureModeController* controller = StartCaptureSession(
      CaptureModeSource::kFullscreen, CaptureModeType::kImage);
  EXPECT_EQ(controller->type(), CaptureModeType::kImage);
  EXPECT_TRUE(cursor_manager->IsCursorLocked());
  event_generator->MoveMouseTo(gfx::Point(175, 175));
  EXPECT_TRUE(cursor_manager->IsCursorVisible());

  // Use image capture icon as the mouse cursor icon in image capture mode.
  EXPECT_EQ(CursorType::kCustom, cursor_manager->GetCursor().type());
  CaptureModeSessionTestApi test_api(controller->capture_mode_session());
  EXPECT_TRUE(test_api.IsUsingCustomCursor(CaptureModeType::kImage));

  // Move the mouse over to capture label widget won't change the cursor since
  // it's a label not a label button.
  event_generator->MoveMouseTo(test_api.GetCaptureLabelWidget()
                                   ->GetWindowBoundsInScreen()
                                   .CenterPoint());
  EXPECT_EQ(CursorType::kCustom, cursor_manager->GetCursor().type());
  EXPECT_TRUE(test_api.IsUsingCustomCursor(CaptureModeType::kImage));

  // Use pointer mouse if the event is on the capture bar.
  ClickOnView(GetVideoToggleButton(), event_generator);
  EXPECT_EQ(controller->type(), CaptureModeType::kVideo);
  EXPECT_EQ(CursorType::kPointer, cursor_manager->GetCursor().type());
  EXPECT_TRUE(cursor_manager->IsCursorLocked());
  EXPECT_TRUE(cursor_manager->IsCursorVisible());

  // Use video record icon as the mouse cursor icon in video recording mode.
  event_generator->MoveMouseTo(gfx::Point(175, 175));
  EXPECT_EQ(CursorType::kCustom, cursor_manager->GetCursor().type());
  EXPECT_TRUE(test_api.IsUsingCustomCursor(CaptureModeType::kVideo));
  EXPECT_TRUE(cursor_manager->IsCursorLocked());
  EXPECT_TRUE(cursor_manager->IsCursorVisible());

  // Enter tablet mode, the cursor should be hidden.
  // To avoid flaky failures due to mouse devices blocking entering tablet mode,
  // we detach all mouse devices. This shouldn't affect testing the cursor
  // status.
  SwitchToTabletMode();
  EXPECT_FALSE(cursor_manager->IsCursorVisible());
  EXPECT_TRUE(cursor_manager->IsCursorLocked());

  // Exit tablet mode, the cursor should appear again.
  LeaveTabletMode();
  EXPECT_TRUE(cursor_manager->IsCursorVisible());
  EXPECT_EQ(CursorType::kCustom, cursor_manager->GetCursor().type());
  EXPECT_TRUE(test_api.IsUsingCustomCursor(CaptureModeType::kVideo));
  EXPECT_TRUE(cursor_manager->IsCursorLocked());

  // Stop capture mode, the cursor should be restored to its original state.
  controller->Stop();
  EXPECT_FALSE(controller->IsActive());
  EXPECT_FALSE(cursor_manager->IsCursorLocked());
  EXPECT_EQ(original_cursor_type, cursor_manager->GetCursor().type());

  // Test that if we're in tablet mode for dev purpose, the cursor should still
  // be visible.
  Shell::Get()->tablet_mode_controller()->SetEnabledForDev(true);
  StartCaptureSession(CaptureModeSource::kFullscreen, CaptureModeType::kImage);
  EXPECT_EQ(controller->type(), CaptureModeType::kImage);
  EXPECT_TRUE(cursor_manager->IsCursorLocked());
  event_generator->MoveMouseTo(gfx::Point(175, 175));
  EXPECT_TRUE(cursor_manager->IsCursorVisible());
}

TEST_F(CaptureModeTest, WindowCursorStates) {
  std::unique_ptr<aura::Window> window(CreateTestWindow(gfx::Rect(200, 200)));

  auto* cursor_manager = Shell::Get()->cursor_manager();
  CursorType original_cursor_type = cursor_manager->GetCursor().type();
  EXPECT_FALSE(cursor_manager->IsCursorLocked());
  EXPECT_EQ(CursorType::kPointer, original_cursor_type);

  auto* event_generator = GetEventGenerator();
  CaptureModeController* controller =
      StartCaptureSession(CaptureModeSource::kWindow, CaptureModeType::kImage);
  EXPECT_EQ(controller->type(), CaptureModeType::kImage);

  // If the mouse is above the window, use the image capture icon.
  event_generator->MoveMouseTo(gfx::Point(150, 150));
  EXPECT_TRUE(cursor_manager->IsCursorLocked());
  EXPECT_TRUE(cursor_manager->IsCursorVisible());
  EXPECT_EQ(CursorType::kCustom, cursor_manager->GetCursor().type());
  CaptureModeSessionTestApi test_api(controller->capture_mode_session());
  EXPECT_TRUE(test_api.IsUsingCustomCursor(CaptureModeType::kImage));

  // If the mouse is not above the window, use a pointer.
  event_generator->MoveMouseTo(gfx::Point(300, 300));
  EXPECT_TRUE(cursor_manager->IsCursorLocked());
  EXPECT_TRUE(cursor_manager->IsCursorVisible());
  EXPECT_EQ(CursorType::kPointer, cursor_manager->GetCursor().type());

  // Use pointer mouse if the event is on the capture bar.
  ClickOnView(GetVideoToggleButton(), event_generator);
  EXPECT_EQ(controller->type(), CaptureModeType::kVideo);
  EXPECT_EQ(CursorType::kPointer, cursor_manager->GetCursor().type());
  EXPECT_TRUE(cursor_manager->IsCursorLocked());
  EXPECT_TRUE(cursor_manager->IsCursorVisible());

  // Use video record icon as the mouse cursor icon in video recording mode.
  event_generator->MoveMouseTo(gfx::Point(150, 150));
  EXPECT_TRUE(cursor_manager->IsCursorLocked());
  EXPECT_TRUE(cursor_manager->IsCursorVisible());
  EXPECT_EQ(CursorType::kCustom, cursor_manager->GetCursor().type());
  EXPECT_TRUE(test_api.IsUsingCustomCursor(CaptureModeType::kVideo));

  // If the mouse is not above the window, use the original mouse cursor.
  event_generator->MoveMouseTo(gfx::Point(300, 300));
  EXPECT_TRUE(cursor_manager->IsCursorLocked());
  EXPECT_TRUE(cursor_manager->IsCursorVisible());
  EXPECT_EQ(CursorType::kPointer, cursor_manager->GetCursor().type());

  // Move above the window again, the cursor should change back to the video
  // record icon.
  event_generator->MoveMouseTo(gfx::Point(150, 150));
  EXPECT_TRUE(cursor_manager->IsCursorLocked());
  EXPECT_TRUE(cursor_manager->IsCursorVisible());
  EXPECT_EQ(CursorType::kCustom, cursor_manager->GetCursor().type());
  EXPECT_TRUE(test_api.IsUsingCustomCursor(CaptureModeType::kVideo));

  // Enter tablet mode, the cursor should be hidden.
  // To avoid flaky failures due to mouse devices blocking entering tablet mode,
  // we detach all mouse devices. This shouldn't affect testing the cursor
  // status.
  SwitchToTabletMode();
  EXPECT_FALSE(cursor_manager->IsCursorVisible());
  EXPECT_TRUE(cursor_manager->IsCursorLocked());

  // Exit tablet mode, the cursor should appear again.
  LeaveTabletMode();
  EXPECT_TRUE(cursor_manager->IsCursorVisible());
  EXPECT_EQ(CursorType::kCustom, cursor_manager->GetCursor().type());
  EXPECT_TRUE(test_api.IsUsingCustomCursor(CaptureModeType::kVideo));
  EXPECT_TRUE(cursor_manager->IsCursorLocked());

  // Stop capture mode, the cursor should be restored to its original state.
  controller->Stop();
  EXPECT_FALSE(controller->IsActive());
  EXPECT_FALSE(cursor_manager->IsCursorLocked());
  EXPECT_EQ(original_cursor_type, cursor_manager->GetCursor().type());
}

// Tests that nothing crashes when windows are destroyed while being observed.
TEST_F(CaptureModeTest, WindowDestruction) {
  // Create 2 windows that overlap with each other.
  const gfx::Rect bounds1(0, 0, 200, 200);
  const gfx::Rect bounds2(150, 150, 200, 200);
  const gfx::Rect bounds3(50, 50, 200, 200);
  std::unique_ptr<aura::Window> window1(CreateTestWindow(bounds1));
  std::unique_ptr<aura::Window> window2(CreateTestWindow(bounds2));

  auto* cursor_manager = Shell::Get()->cursor_manager();
  CursorType original_cursor_type = cursor_manager->GetCursor().type();
  EXPECT_FALSE(cursor_manager->IsCursorLocked());
  EXPECT_EQ(CursorType::kPointer, original_cursor_type);

  // Start capture session with Image type, so we have a custom cursor.
  auto* event_generator = GetEventGenerator();
  CaptureModeController* controller =
      StartCaptureSession(CaptureModeSource::kWindow, CaptureModeType::kImage);
  EXPECT_EQ(controller->type(), CaptureModeType::kImage);

  // If the mouse is above the window, use the image capture icon.
  event_generator->MoveMouseToCenterOf(window2.get());
  EXPECT_TRUE(cursor_manager->IsCursorLocked());
  EXPECT_TRUE(cursor_manager->IsCursorVisible());
  EXPECT_EQ(CursorType::kCustom, cursor_manager->GetCursor().type());
  auto* capture_mode_session = controller->capture_mode_session();
  CaptureModeSessionTestApi test_api(capture_mode_session);
  EXPECT_TRUE(test_api.IsUsingCustomCursor(CaptureModeType::kImage));

  // Destroy the window while hovering. There is no window underneath, so it
  // should revert back to a pointer.
  window2.reset();
  EXPECT_TRUE(cursor_manager->IsCursorLocked());
  EXPECT_TRUE(cursor_manager->IsCursorVisible());
  EXPECT_EQ(CursorType::kPointer, cursor_manager->GetCursor().type());

  // Destroy the window while mouse is in a pressed state. Cursor should revert
  // back to the original cursor.
  std::unique_ptr<aura::Window> window3(CreateTestWindow(bounds2));
  EXPECT_EQ(CursorType::kCustom, cursor_manager->GetCursor().type());
  EXPECT_TRUE(test_api.IsUsingCustomCursor(CaptureModeType::kImage));
  event_generator->PressLeftButton();
  EXPECT_EQ(CursorType::kCustom, cursor_manager->GetCursor().type());
  EXPECT_TRUE(test_api.IsUsingCustomCursor(CaptureModeType::kImage));
  window3.reset();
  event_generator->ReleaseLeftButton();
  EXPECT_EQ(original_cursor_type, cursor_manager->GetCursor().type());

  // When hovering over a window, if it is destroyed and there is another window
  // under the cursor location in screen, then the selected window is
  // automatically updated.
  std::unique_ptr<aura::Window> window4(CreateTestWindow(bounds3));
  event_generator->MoveMouseToCenterOf(window4.get());
  EXPECT_EQ(CursorType::kCustom, cursor_manager->GetCursor().type());
  EXPECT_TRUE(test_api.IsUsingCustomCursor(CaptureModeType::kImage));
  EXPECT_EQ(capture_mode_session->GetSelectedWindow(), window4.get());
  window4.reset();
  EXPECT_EQ(CursorType::kCustom, cursor_manager->GetCursor().type());
  EXPECT_TRUE(test_api.IsUsingCustomCursor(CaptureModeType::kImage));
  // Check to see it's observing window1.
  EXPECT_EQ(capture_mode_session->GetSelectedWindow(), window1.get());

  // Cursor is over a window in the mouse pressed state. If the window is
  // destroyed and there is another window under the cursor, the selected window
  // is updated and the new selected window is captured.
  std::unique_ptr<aura::Window> window5(CreateTestWindow(bounds3));
  EXPECT_EQ(capture_mode_session->GetSelectedWindow(), window5.get());
  event_generator->PressLeftButton();
  window5.reset();
  EXPECT_EQ(CursorType::kCustom, cursor_manager->GetCursor().type());
  EXPECT_TRUE(test_api.IsUsingCustomCursor(CaptureModeType::kImage));
  EXPECT_EQ(capture_mode_session->GetSelectedWindow(), window1.get());
  event_generator->ReleaseLeftButton();
  EXPECT_FALSE(controller->IsActive());
}

TEST_F(CaptureModeTest, CursorUpdatedOnDisplayRotation) {
  UpdateDisplay("600x400");
  const int64_t display_id =
      display::Screen::GetScreen()->GetPrimaryDisplay().id();
  display::SetInternalDisplayIds({display_id});
  ScreenOrientationControllerTestApi orientation_test_api(
      Shell::Get()->screen_orientation_controller());

  auto* event_generator = GetEventGenerator();
  auto* cursor_manager = Shell::Get()->cursor_manager();
  CaptureModeController* controller = StartCaptureSession(
      CaptureModeSource::kFullscreen, CaptureModeType::kImage);
  event_generator->MoveMouseTo(gfx::Point(175, 175));
  EXPECT_TRUE(cursor_manager->IsCursorVisible());

  // Use image capture icon as the mouse cursor icon in image capture mode.
  const ui::Cursor landscape_cursor = cursor_manager->GetCursor();
  EXPECT_EQ(CursorType::kCustom, landscape_cursor.type());
  CaptureModeSessionTestApi session_test_api(
      controller->capture_mode_session());
  EXPECT_TRUE(session_test_api.IsUsingCustomCursor(CaptureModeType::kImage));

  // Rotate the screen.
  orientation_test_api.SetDisplayRotation(
      display::Display::ROTATE_270, display::Display::RotationSource::ACTIVE);
  const ui::Cursor portrait_cursor = cursor_manager->GetCursor();
  EXPECT_TRUE(session_test_api.IsUsingCustomCursor(CaptureModeType::kImage));
  EXPECT_NE(landscape_cursor, portrait_cursor);
}

// Tests that in Region mode, cursor compositing is used instead of the system
// cursor when the cursor is being dragged.
TEST_F(CaptureModeTest, RegionDragCursorCompositing) {
  auto* event_generator = GetEventGenerator();
  auto* session = StartImageRegionCapture()->capture_mode_session();
  auto* cursor_manager = Shell::Get()->cursor_manager();

  // Initially cursor should be visible and cursor compositing is not enabled.
  EXPECT_FALSE(session->is_drag_in_progress());
  EXPECT_FALSE(IsCursorCompositingEnabled());
  EXPECT_TRUE(cursor_manager->IsCursorVisible());

  const gfx::Rect target_region(gfx::Rect(200, 200, 200, 200));

  // For each start and end point try dragging and verify that cursor
  // compositing is functioning as expected.
  struct {
    std::string trace;
    gfx::Point start_point;
    gfx::Point end_point;
  } kDragCases[] = {
      {"initial_region", target_region.origin(), target_region.bottom_right()},
      {"edge_resize", target_region.right_center(),
       gfx::Point(target_region.right_center() + gfx::Vector2d(50, 0))},
      {"corner_resize", target_region.origin(), gfx::Point(175, 175)},
      {"move", gfx::Point(250, 250), gfx::Point(300, 300)},
  };

  for (auto test_case : kDragCases) {
    SCOPED_TRACE(test_case.trace);

    event_generator->MoveMouseTo(test_case.start_point);
    event_generator->PressLeftButton();
    EXPECT_TRUE(session->is_drag_in_progress());
    EXPECT_TRUE(IsCursorCompositingEnabled());

    event_generator->MoveMouseTo(test_case.end_point);
    EXPECT_TRUE(session->is_drag_in_progress());
    EXPECT_TRUE(IsCursorCompositingEnabled());

    event_generator->ReleaseLeftButton();
    EXPECT_FALSE(session->is_drag_in_progress());
    EXPECT_FALSE(IsCursorCompositingEnabled());
  }
}

// Test that during countdown, capture mode session should not handle any
// incoming input events.
TEST_F(CaptureModeTest, DoNotHandleEventDuringCountDown) {
  // We need a non-zero duration to avoid infinite loop on countdown.
  ui::ScopedAnimationDurationScaleMode animation_scale(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  // Create 2 windows that overlap with each other.
  std::unique_ptr<aura::Window> window1(CreateTestWindow(gfx::Rect(200, 200)));
  std::unique_ptr<aura::Window> window2(
      CreateTestWindow(gfx::Rect(150, 150, 200, 200)));

  auto* controller = CaptureModeController::Get();
  controller->SetSource(CaptureModeSource::kWindow);
  controller->SetType(CaptureModeType::kVideo);
  controller->Start(CaptureModeEntryType::kQuickSettings);
  EXPECT_TRUE(controller->IsActive());

  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseToCenterOf(window1.get());
  auto* capture_mode_session = controller->capture_mode_session();
  EXPECT_EQ(capture_mode_session->GetSelectedWindow(), window1.get());

  // Start video recording. Countdown should start at this moment.
  event_generator->ClickLeftButton();

  // Now move the mouse onto the other window, we should not change the captured
  // window during countdown.
  event_generator->MoveMouseToCenterOf(window2.get());
  EXPECT_EQ(capture_mode_session->GetSelectedWindow(), window1.get());
  EXPECT_NE(capture_mode_session->GetSelectedWindow(), window2.get());

  WaitForRecordingToStart();
}

// Test that during countdown, window changes or crashes are handled.
TEST_F(CaptureModeTest, WindowChangesDuringCountdown) {
  // We need a non-zero duration to avoid infinite loop on countdown.
  ui::ScopedAnimationDurationScaleMode animation_scale(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  std::unique_ptr<aura::Window> window;

  auto* controller = CaptureModeController::Get();
  controller->SetSource(CaptureModeSource::kWindow);
  controller->SetType(CaptureModeType::kVideo);

  auto start_countdown = [this, &window, controller]() {
    window = CreateTestWindow(gfx::Rect(200, 200));
    controller->Start(CaptureModeEntryType::kQuickSettings);

    auto* event_generator = GetEventGenerator();
    event_generator->MoveMouseToCenterOf(window.get());
    event_generator->ClickLeftButton();

    EXPECT_TRUE(controller->IsActive());
    EXPECT_FALSE(controller->is_recording_in_progress());
  };

  // Destroying or minimizing the observed window terminates the countdown and
  // exits capture mode.
  start_countdown();
  window.reset();
  EXPECT_FALSE(controller->IsActive());

  start_countdown();
  WindowState::Get(window.get())->Minimize();
  EXPECT_FALSE(controller->IsActive());

  // Activation changes (such as opening overview) should not terminate the
  // countdown.
  start_countdown();
  EnterOverview();
  EXPECT_TRUE(controller->IsActive());
  EXPECT_FALSE(controller->is_recording_in_progress());

  // Wait for countdown to finish and check that recording starts.
  WaitForRecordingToStart();
  EXPECT_FALSE(controller->IsActive());
  EXPECT_TRUE(controller->is_recording_in_progress());
}

// Verifies that the video notification will show the same thumbnail image as
// sent by recording service.
TEST_F(CaptureModeTest, VideoNotificationThumbnail) {
  auto* controller = StartCaptureSession(CaptureModeSource::kFullscreen,
                                         CaptureModeType::kVideo);
  StartVideoRecordingImmediately();
  EXPECT_TRUE(controller->is_recording_in_progress());
  CaptureModeTestApi().FlushRecordingServiceForTesting();

  auto* test_delegate =
      static_cast<TestCaptureModeDelegate*>(controller->delegate_for_testing());

  // Request and wait for a video frame so that the recording service can use it
  // to create a video thumbnail.
  test_delegate->RequestAndWaitForVideoFrame();
  SkBitmap service_thumbnail =
      gfx::Image(test_delegate->GetVideoThumbnail()).AsBitmap();
  EXPECT_FALSE(service_thumbnail.drawsNothing());

  CaptureNotificationWaiter waiter;
  controller->EndVideoRecording(EndRecordingReason::kStopRecordingButton);
  EXPECT_FALSE(controller->is_recording_in_progress());
  waiter.Wait();

  // Verify that the service's thumbnail is the same image shown in the
  // notification shown when recording ends.
  const message_center::Notification* notification = GetPreviewNotification();
  EXPECT_TRUE(notification);
  EXPECT_FALSE(notification->image().IsEmpty());
  const SkBitmap notification_thumbnail = notification->image().AsBitmap();
  EXPECT_TRUE(
      gfx::test::AreBitmapsEqual(notification_thumbnail, service_thumbnail));
}

TEST_F(CaptureModeTest, LowDriveFsSpace) {
  auto* controller = StartCaptureSession(CaptureModeSource::kFullscreen,
                                         CaptureModeType::kVideo);
  const base::FilePath drive_fs_folder = CreateFolderOnDriveFS("test");
  controller->SetCustomCaptureFolder(drive_fs_folder);
  auto* test_delegate =
      static_cast<TestCaptureModeDelegate*>(controller->delegate_for_testing());

  // Simulate low DriveFS free space by setting it to e.g. 200 MB.
  test_delegate->set_fake_drive_fs_free_bytes(200 * 1024 * 1024);

  base::HistogramTester histogram_tester;
  StartVideoRecordingImmediately();
  EXPECT_TRUE(controller->is_recording_in_progress());
  CaptureModeTestApi().FlushRecordingServiceForTesting();
  test_delegate->RequestAndWaitForVideoFrame();

  // Recording should end immediately due to a low Drive FS free space.
  WaitForCaptureFileToBeSaved();
  histogram_tester.ExpectBucketCount(
      kEndRecordingReasonInClamshellHistogramName,
      EndRecordingReason::kLowDriveFsQuota, 1);
}

TEST_F(CaptureModeTest, WindowRecordingCaptureId) {
  auto window = CreateTestWindow(gfx::Rect(200, 200));
  StartCaptureSession(CaptureModeSource::kWindow, CaptureModeType::kVideo);

  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseToCenterOf(window.get());
  auto* controller = CaptureModeController::Get();
  StartVideoRecordingImmediately();
  EXPECT_TRUE(controller->is_recording_in_progress());

  // The window should have a valid capture ID.
  EXPECT_TRUE(window->subtree_capture_id().is_valid());

  // Once recording ends, the window should no longer be marked as capturable.
  controller->EndVideoRecording(EndRecordingReason::kStopRecordingButton);
  EXPECT_FALSE(controller->is_recording_in_progress());
  EXPECT_FALSE(window->subtree_capture_id().is_valid());
}

TEST_F(CaptureModeTest, ClosingDimmedWidgetAboveRecordedWindow) {
  views::Widget* widget = TestWidgetBuilder().BuildOwnedByNativeWidget();
  auto* window = widget->GetNativeWindow();
  auto recorded_window = CreateTestWindow(gfx::Rect(200, 200));

  auto* controller = StartSessionAndRecordWindow(recorded_window.get());
  EXPECT_TRUE(controller->is_recording_in_progress());
  auto* recording_watcher = controller->video_recording_watcher_for_testing();

  // Activate the window so that it becomes on top of the recorded window, and
  // expect it gets dimmed.
  wm::ActivateWindow(window);
  EXPECT_TRUE(recording_watcher->IsWindowDimmedForTesting(window));

  // Close the widget, this should not lead to any use-after-free. See
  // https://crbug.com/1273197.
  widget->Close();
}

TEST_F(CaptureModeTest, DimmingOfUnRecordedWindows) {
  auto win1 = CreateTestWindow(gfx::Rect(200, 200));
  auto win2 = CreateTestWindow(gfx::Rect(200, 200));
  auto recorded_window = CreateTestWindow(gfx::Rect(200, 200));

  auto* controller = StartSessionAndRecordWindow(recorded_window.get());
  auto* recording_watcher = controller->video_recording_watcher_for_testing();
  auto* shield_layer = recording_watcher->layer();
  // Since the recorded window is the top most, no windows should be
  // individually dimmed.
  EXPECT_TRUE(recording_watcher->should_paint_layer());
  EXPECT_TRUE(IsLayerStackedRightBelow(shield_layer, recorded_window->layer()));
  EXPECT_FALSE(
      recording_watcher->IsWindowDimmedForTesting(recorded_window.get()));
  EXPECT_FALSE(recording_watcher->IsWindowDimmedForTesting(win1.get()));
  EXPECT_FALSE(recording_watcher->IsWindowDimmedForTesting(win2.get()));

  // Activating |win1| brings it to the front of the shield, so it should be
  // dimmed separately.
  wm::ActivateWindow(win1.get());
  EXPECT_TRUE(recording_watcher->should_paint_layer());
  EXPECT_TRUE(IsLayerStackedRightBelow(shield_layer, recorded_window->layer()));
  EXPECT_FALSE(
      recording_watcher->IsWindowDimmedForTesting(recorded_window.get()));
  EXPECT_TRUE(recording_watcher->IsWindowDimmedForTesting(win1.get()));
  EXPECT_FALSE(recording_watcher->IsWindowDimmedForTesting(win2.get()));
  // Similarly for |win2|.
  wm::ActivateWindow(win2.get());
  EXPECT_TRUE(recording_watcher->should_paint_layer());
  EXPECT_TRUE(IsLayerStackedRightBelow(shield_layer, recorded_window->layer()));
  EXPECT_FALSE(
      recording_watcher->IsWindowDimmedForTesting(recorded_window.get()));
  EXPECT_TRUE(recording_watcher->IsWindowDimmedForTesting(win1.get()));
  EXPECT_TRUE(recording_watcher->IsWindowDimmedForTesting(win2.get()));

  // Minimizing the recorded window should stop painting the shield, and the
  // dimmers should be removed.
  WindowState::Get(recorded_window.get())->Minimize();
  EXPECT_FALSE(recording_watcher->should_paint_layer());
  EXPECT_FALSE(recording_watcher->IsWindowDimmedForTesting(win1.get()));
  EXPECT_FALSE(recording_watcher->IsWindowDimmedForTesting(win2.get()));

  // Activating the recorded window again unminimizes the window, which will
  // reenable painting the shield.
  wm::ActivateWindow(recorded_window.get());
  EXPECT_FALSE(recording_watcher->IsWindowDimmedForTesting(win1.get()));
  EXPECT_FALSE(recording_watcher->IsWindowDimmedForTesting(win2.get()));
  EXPECT_FALSE(WindowState::Get(recorded_window.get())->IsMinimized());
  EXPECT_TRUE(recording_watcher->should_paint_layer());

  // Destroying a dimmed window is correctly tracked.
  wm::ActivateWindow(win2.get());
  EXPECT_TRUE(recording_watcher->IsWindowDimmedForTesting(win2.get()));
  win2.reset();
  EXPECT_FALSE(recording_watcher->IsWindowDimmedForTesting(win2.get()));
}

TEST_F(CaptureModeTest, DimmingWithDesks) {
  auto recorded_window = CreateAppWindow(gfx::Rect(250, 100));
  auto* controller = StartSessionAndRecordWindow(recorded_window.get());
  auto* recording_watcher = controller->video_recording_watcher_for_testing();
  EXPECT_TRUE(recording_watcher->should_paint_layer());

  auto* desks_controller = DesksController::Get();
  desks_controller->NewDesk(DesksCreationRemovalSource::kKeyboard);
  Desk* desk_2 = desks_controller->desks()[1].get();
  ActivateDesk(desk_2);

  // A window on a different desk than that of the recorded window should not be
  // dimmed.
  auto win1 = CreateAppWindow(gfx::Rect(200, 200));
  EXPECT_FALSE(recording_watcher->IsWindowDimmedForTesting(win1.get()));

  // However, moving it to the desk of the recorded window should give it a
  // dimmer, since it's a more recently-used window (i.e. above the recorded
  // window).
  Desk* desk_1 = desks_controller->desks()[0].get();
  desks_controller->MoveWindowFromActiveDeskTo(
      win1.get(), desk_1, win1->GetRootWindow(),
      DesksMoveWindowFromActiveDeskSource::kShortcut);
  EXPECT_TRUE(recording_watcher->IsWindowDimmedForTesting(win1.get()));

  // Moving the recorded window out of the active desk should destroy the
  // dimmer.
  ActivateDesk(desk_1);
  desks_controller->MoveWindowFromActiveDeskTo(
      recorded_window.get(), desk_2, recorded_window->GetRootWindow(),
      DesksMoveWindowFromActiveDeskSource::kShortcut);
  EXPECT_FALSE(recording_watcher->IsWindowDimmedForTesting(win1.get()));
}

TEST_F(CaptureModeTest, DimmingWithDisplays) {
  UpdateDisplay("500x400,401+0-800x700");
  auto recorded_window = CreateAppWindow(gfx::Rect(250, 100));
  auto* controller = StartSessionAndRecordWindow(recorded_window.get());
  auto* recording_watcher = controller->video_recording_watcher_for_testing();
  EXPECT_TRUE(recording_watcher->should_paint_layer());

  // Create a new window on the second display. It should not be dimmed.
  auto window = CreateTestWindow(gfx::Rect(420, 10, 200, 200));
  auto roots = Shell::GetAllRootWindows();
  EXPECT_EQ(roots[1], window->GetRootWindow());
  EXPECT_FALSE(recording_watcher->IsWindowDimmedForTesting(window.get()));

  // However when moved to the first display, it gets dimmed.
  window_util::MoveWindowToDisplay(window.get(),
                                   roots[0]->GetHost()->GetDisplayId());
  EXPECT_TRUE(recording_watcher->IsWindowDimmedForTesting(window.get()));

  // Moving the recorded window to the second display will remove the dimming of
  // |window|.
  window_util::MoveWindowToDisplay(recorded_window.get(),
                                   roots[1]->GetHost()->GetDisplayId());
  EXPECT_FALSE(recording_watcher->IsWindowDimmedForTesting(window.get()));
}

TEST_F(CaptureModeTest, MultiDisplayWindowRecording) {
  UpdateDisplay("500x400,401+0-800x700");
  auto roots = Shell::GetAllRootWindows();
  ASSERT_EQ(2u, roots.size());

  auto window = CreateTestWindow(gfx::Rect(200, 200));
  auto* controller =
      StartCaptureSession(CaptureModeSource::kWindow, CaptureModeType::kVideo);

  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseToCenterOf(window.get());
  auto* session_layer = controller->capture_mode_session()->layer();
  StartVideoRecordingImmediately();
  EXPECT_TRUE(controller->is_recording_in_progress());
  // The session layer is reused to paint the recording shield.
  auto* shield_layer =
      controller->video_recording_watcher_for_testing()->layer();
  EXPECT_EQ(session_layer, shield_layer);
  EXPECT_EQ(shield_layer->parent(), window->layer()->parent());
  EXPECT_TRUE(IsLayerStackedRightBelow(shield_layer, window->layer()));
  EXPECT_EQ(shield_layer->bounds(), roots[0]->bounds());

  // The capturer should capture from the frame sink of the first display.
  // The video size should match the window's size.
  CaptureModeTestApi test_api;
  test_api.FlushRecordingServiceForTesting();
  auto* test_delegate =
      static_cast<TestCaptureModeDelegate*>(controller->delegate_for_testing());
  EXPECT_EQ(roots[0]->GetFrameSinkId(), test_delegate->GetCurrentFrameSinkId());
  EXPECT_EQ(roots[0]->bounds().size(),
            test_delegate->GetCurrentFrameSinkSizeInPixels());
  EXPECT_EQ(window->bounds().size(), test_delegate->GetCurrentVideoSize());

  // Moving a window to a different display should be propagated to the service,
  // with the new root's frame sink ID, and the new root's size.
  window_util::MoveWindowToDisplay(window.get(),
                                   roots[1]->GetHost()->GetDisplayId());
  test_api.FlushRecordingServiceForTesting();
  ASSERT_EQ(window->GetRootWindow(), roots[1]);
  EXPECT_EQ(roots[1]->GetFrameSinkId(), test_delegate->GetCurrentFrameSinkId());
  EXPECT_EQ(roots[1]->bounds().size(),
            test_delegate->GetCurrentFrameSinkSizeInPixels());
  EXPECT_EQ(window->bounds().size(), test_delegate->GetCurrentVideoSize());

  // The shield layer should move with the window, and maintain the stacking
  // below the window's layer.
  EXPECT_EQ(shield_layer->parent(), window->layer()->parent());
  EXPECT_TRUE(IsLayerStackedRightBelow(shield_layer, window->layer()));
  EXPECT_EQ(shield_layer->bounds(), roots[1]->bounds());
}

// Flaky especially on MSan: https://crbug.com/1293188
TEST_F(CaptureModeTest, DISABLED_WindowResizing) {
  UpdateDisplay("700x600");
  auto window = CreateTestWindow(gfx::Rect(200, 200));
  auto* controller =
      StartCaptureSession(CaptureModeSource::kWindow, CaptureModeType::kVideo);

  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseToCenterOf(window.get());
  StartVideoRecordingImmediately();
  EXPECT_TRUE(controller->is_recording_in_progress());
  auto* test_delegate =
      static_cast<TestCaptureModeDelegate*>(controller->delegate_for_testing());

  CaptureModeTestApi test_api;
  test_api.FlushRecordingServiceForTesting();
  EXPECT_EQ(gfx::Size(200, 200), test_delegate->GetCurrentVideoSize());
  EXPECT_EQ(gfx::Size(700, 600),
            test_delegate->GetCurrentFrameSinkSizeInPixels());

  // Multiple resize events should be throttled.
  window->SetBounds(gfx::Rect(250, 250));
  test_api.FlushRecordingServiceForTesting();
  EXPECT_EQ(gfx::Size(200, 200), test_delegate->GetCurrentVideoSize());

  window->SetBounds(gfx::Rect(250, 300));
  test_api.FlushRecordingServiceForTesting();
  EXPECT_EQ(gfx::Size(200, 200), test_delegate->GetCurrentVideoSize());

  window->SetBounds(gfx::Rect(300, 300));
  test_api.FlushRecordingServiceForTesting();
  EXPECT_EQ(gfx::Size(200, 200), test_delegate->GetCurrentVideoSize());

  // Once throttling ends, the current size is pushed.
  auto* recording_watcher = controller->video_recording_watcher_for_testing();
  recording_watcher->SendThrottledWindowSizeChangedNowForTesting();
  test_api.FlushRecordingServiceForTesting();
  EXPECT_EQ(gfx::Size(300, 300), test_delegate->GetCurrentVideoSize());
  EXPECT_EQ(gfx::Size(700, 600),
            test_delegate->GetCurrentFrameSinkSizeInPixels());

  // Maximizing a window changes its size, and is pushed to the service with
  // throttling.
  WindowState::Get(window.get())->Maximize();
  test_api.FlushRecordingServiceForTesting();
  EXPECT_EQ(gfx::Size(300, 300), test_delegate->GetCurrentVideoSize());

  recording_watcher->SendThrottledWindowSizeChangedNowForTesting();
  test_api.FlushRecordingServiceForTesting();
  EXPECT_NE(gfx::Size(300, 300), test_delegate->GetCurrentVideoSize());
  EXPECT_EQ(window->bounds().size(), test_delegate->GetCurrentVideoSize());
}

TEST_F(CaptureModeTest, RotateDisplayWhileRecording) {
  UpdateDisplay("600x800");

  auto* controller =
      StartCaptureSession(CaptureModeSource::kRegion, CaptureModeType::kVideo);
  SelectRegion(gfx::Rect(20, 40, 100, 200));
  StartVideoRecordingImmediately();
  EXPECT_TRUE(controller->is_recording_in_progress());

  // Initially the frame sink size matches the un-rotated display size in DIPs,
  // but the video size matches the size of the crop region.
  CaptureModeTestApi test_api;
  test_api.FlushRecordingServiceForTesting();
  auto* test_delegate =
      static_cast<TestCaptureModeDelegate*>(controller->delegate_for_testing());
  EXPECT_EQ(gfx::Size(600, 800),
            test_delegate->GetCurrentFrameSinkSizeInPixels());
  EXPECT_EQ(gfx::Size(100, 200), test_delegate->GetCurrentVideoSize());

  // Rotate by 90 degree, the frame sink size should be updated to match that.
  // The video size should remain unaffected.
  Shell::Get()->display_manager()->SetDisplayRotation(
      WindowTreeHostManager::GetPrimaryDisplayId(), display::Display::ROTATE_90,
      display::Display::RotationSource::USER);
  test_api.FlushRecordingServiceForTesting();
  EXPECT_EQ(gfx::Size(800, 600),
            test_delegate->GetCurrentFrameSinkSizeInPixels());
  EXPECT_EQ(gfx::Size(100, 200), test_delegate->GetCurrentVideoSize());
}

// Regression test for https://crbug.com/1331095.
// This is disabled due to flakiness: b/318349807
TEST_F(CaptureModeTest, DISABLED_CornerRegionWithScreenRotation) {
  UpdateDisplay("800x600");

  // Pick a region at the bottom right corner of the landscape screen, so that
  // when the screen is rotated to portrait, the unadjusted region becomes
  // outside the new portrait bounds.
  auto* controller =
      StartCaptureSession(CaptureModeSource::kRegion, CaptureModeType::kVideo);
  SelectRegion(gfx::Rect(700, 400, 100, 200));
  StartVideoRecordingImmediately();
  EXPECT_TRUE(controller->is_recording_in_progress());

  CaptureModeTestApi test_api;
  test_api.FlushRecordingServiceForTesting();
  auto* test_delegate =
      static_cast<TestCaptureModeDelegate*>(controller->delegate_for_testing());

  EXPECT_EQ(gfx::Size(100, 200), test_delegate->GetCurrentVideoSize());
  auto* root_window = Shell::GetPrimaryRootWindow();
  auto* recording_watcher = controller->video_recording_watcher_for_testing();
  gfx::Rect effective_region_bounds =
      recording_watcher->GetEffectivePartialRegionBounds();
  EXPECT_FALSE(effective_region_bounds.IsEmpty());
  EXPECT_TRUE(root_window->bounds().Contains(effective_region_bounds));

  // Verifies that the bounds of the visible rect of the frame is within the
  // bounds of the root window.
  auto verify_video_frame = [&](const media::VideoFrame& frame,
                                const gfx::Rect& content_rect) {
    EXPECT_TRUE(root_window->bounds().Contains(frame.visible_rect()));
  };

  test_delegate->recording_service()->RequestAndWaitForVideoFrame(
      base::BindLambdaForTesting(verify_video_frame));

  // Rotate by 90 degree, the adjusted region bounds should not be empty and
  // should remain within the bounds of the new portrait root window bounds.
  Shell::Get()->display_manager()->SetDisplayRotation(
      WindowTreeHostManager::GetPrimaryDisplayId(), display::Display::ROTATE_90,
      display::Display::RotationSource::USER);
  test_api.FlushRecordingServiceForTesting();
  EXPECT_EQ(gfx::Size(100, 200), test_delegate->GetCurrentVideoSize());

  effective_region_bounds =
      recording_watcher->GetEffectivePartialRegionBounds();
  EXPECT_FALSE(effective_region_bounds.IsEmpty());
  EXPECT_TRUE(root_window->bounds().Contains(effective_region_bounds));
  test_delegate->recording_service()->RequestAndWaitForVideoFrame(
      base::BindLambdaForTesting(verify_video_frame));
}

// Tests that the video frames delivered to the service for recorded windows are
// valid (i.e. they have the correct size, and suffer from no letterboxing, even
// when the window gets resized).
// This is a regression test for https://crbug.com/1214023.
//
// TODO(crbug.com/1439950): This test is flaky.
TEST_F(CaptureModeTest, DISABLED_VerifyWindowRecordingVideoFrames) {
  auto window = CreateTestWindow(gfx::Rect(100, 50, 200, 200));
  StartCaptureSession(CaptureModeSource::kWindow, CaptureModeType::kVideo);

  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseToCenterOf(window.get());
  auto* controller = CaptureModeController::Get();
  StartVideoRecordingImmediately();
  EXPECT_TRUE(controller->is_recording_in_progress());
  CaptureModeTestApi test_api;
  test_api.FlushRecordingServiceForTesting();

  bool is_video_frame_valid = false;
  std::string failures;
  auto verify_video_frame = [&](const media::VideoFrame& frame,
                                const gfx::Rect& content_rect) {
    is_video_frame_valid = true;
    failures.clear();

    // Having the content positioned at (0,0) with a size that matches the
    // current window's size means that there is no letterboxing.
    if (gfx::Point() != content_rect.origin()) {
      is_video_frame_valid = false;
      failures =
          base::StringPrintf("content_rect is not at (0,0), instead at: %s\n",
                             content_rect.origin().ToString().c_str());
    }

    const gfx::Size window_size = window->bounds().size();
    if (window_size != content_rect.size()) {
      is_video_frame_valid = false;
      failures += base::StringPrintf(
          "content_rect doesn't match the window size:\n"
          "  content_rect.size(): %s\n"
          "  window_size: %s\n",
          content_rect.size().ToString().c_str(),
          window_size.ToString().c_str());
    }

    // The video frame contents should match the bounds of the video frame.
    if (frame.visible_rect() != content_rect) {
      is_video_frame_valid = false;
      failures += base::StringPrintf(
          "content_rect doesn't match the frame's visible_rect:\n"
          "  content_rect: %s\n"
          "  visible_rect: %s\n",
          content_rect.ToString().c_str(),
          frame.visible_rect().ToString().c_str());
    }

    if (frame.coded_size() != window_size) {
      is_video_frame_valid = false;
      failures += base::StringPrintf(
          "the frame's coded size doesn't match the window size:\n"
          "  frame.coded_size(): %s\n"
          "  window_size: %s\n",
          frame.coded_size().ToString().c_str(),
          window_size.ToString().c_str());
    }
  };

  auto* test_delegate =
      static_cast<TestCaptureModeDelegate*>(controller->delegate_for_testing());
  ASSERT_TRUE(test_delegate->recording_service());
  {
    SCOPED_TRACE("Initial window size");
    test_delegate->recording_service()->RequestAndWaitForVideoFrame(
        base::BindLambdaForTesting(verify_video_frame));
    EXPECT_TRUE(is_video_frame_valid) << failures;
  }

  // Even when the window is resized and the throttled size reaches the service,
  // new video frames should still be valid.
  window->SetBounds(gfx::Rect(120, 60, 600, 500));
  auto* recording_watcher = controller->video_recording_watcher_for_testing();
  recording_watcher->SendThrottledWindowSizeChangedNowForTesting();
  test_api.FlushRecordingServiceForTesting();
  {
    SCOPED_TRACE("After window resizing");
    // A video frame is produced on the Viz side when a CopyOutputRequest is
    // fulfilled. Those CopyOutputRequests could have been placed before the
    // window's layer resize results in a new resized render pass in Viz. But
    // eventually this must happen, and a valid frame must be delivered.
    int remaining_attempts = 2;
    do {
      --remaining_attempts;
      test_delegate->recording_service()->RequestAndWaitForVideoFrame(
          base::BindLambdaForTesting(verify_video_frame));
    } while (!is_video_frame_valid && remaining_attempts);
    EXPECT_TRUE(is_video_frame_valid) << failures;
  }

  controller->EndVideoRecording(EndRecordingReason::kStopRecordingButton);
  EXPECT_FALSE(controller->is_recording_in_progress());
}

// Tests that the focus should be on the `Settings` button after closing the
// settings menu.
TEST_F(CaptureModeTest, ReturnFocusToSettingsButtonAfterSettingsMenuIsClosed) {
  auto* controller = StartCaptureSession(CaptureModeSource::kFullscreen,
                                         CaptureModeType::kImage);
  auto* capture_mode_session =
      static_cast<CaptureModeSession*>(controller->capture_mode_session());
  ASSERT_EQ(capture_mode_session->session_type(), SessionType::kReal);
  CaptureModeSessionTestApi test_api(capture_mode_session);

  using FocusGroup = CaptureModeSessionFocusCycler::FocusGroup;
  auto* event_generator = GetEventGenerator();

  // Check the initial focus of the focus ring.
  EXPECT_EQ(FocusGroup::kNone, test_api.GetCurrentFocusGroup());

  // Tab six times, `Settings` button should be focused.
  SendKey(ui::VKEY_TAB, event_generator, ui::EF_NONE, /*count=*/6);
  EXPECT_EQ(FocusGroup::kSettingsClose, test_api.GetCurrentFocusGroup());
  EXPECT_TRUE(CaptureModeSessionFocusCycler::HighlightHelper::Get(
                  test_api.GetCaptureModeBarView()->settings_button())
                  ->has_focus());

  // Press the space key and the settings menu will be opened.
  SendKey(ui::VKEY_SPACE, event_generator, ui::EF_NONE);
  EXPECT_TRUE(test_api.GetCaptureModeSettingsView());
  EXPECT_EQ(FocusGroup::kPendingSettings, test_api.GetCurrentFocusGroup());

  // Close the settings menu, the focus ring should be on the `Settings` button.
  SendKey(ui::VKEY_ESCAPE, event_generator, ui::EF_NONE);
  EXPECT_FALSE(test_api.GetCaptureModeSettingsView());
  EXPECT_EQ(FocusGroup::kSettingsClose, test_api.GetCurrentFocusGroup());
  EXPECT_TRUE(CaptureModeSessionFocusCycler::HighlightHelper::Get(
                  test_api.GetCaptureModeBarView()->settings_button())
                  ->has_focus());

  // Tab the space key to open the settings menu again and tab to focus on the
  // settings menu item.
  SendKey(ui::VKEY_SPACE, event_generator, ui::EF_NONE);
  EXPECT_TRUE(test_api.GetCaptureModeSettingsView());
  SendKey(ui::VKEY_TAB, event_generator, ui::EF_NONE, /*count=*/3);
  EXPECT_EQ(FocusGroup::kSettingsMenu, test_api.GetCurrentFocusGroup());

  // Close the settings menu, the focus ring should be on the `Settings` button.
  SendKey(ui::VKEY_ESCAPE, event_generator, ui::EF_NONE);
  EXPECT_FALSE(test_api.GetCaptureModeSettingsView());
  EXPECT_EQ(FocusGroup::kSettingsClose, test_api.GetCurrentFocusGroup());
  EXPECT_TRUE(CaptureModeSessionFocusCycler::HighlightHelper::Get(
                  test_api.GetCaptureModeBarView()->settings_button())
                  ->has_focus());
}

// Tests that minimized window(s) will be ignored whereas four corners occluded
// but overall partially occluded window will be focusable while tabbing through
// in `kWindow` mode.
TEST_F(CaptureModeTest, IgnoreMinimizeWindowsInKWindow) {
  // Layout of three windows: four corners of `window3` are occluded by
  // `window1` and `window2`.
  //
  //   +------+
  //   |      |       +-----------+
  //   |  1   |-------|           |
  //   |      |   3   |     2     |
  //   |      |       |           |
  //   |      |       |           |
  //   |      |-------|           |
  //   |      |       +-----------+
  //   +------+
  std::unique_ptr<aura::Window> window3 =
      CreateTestWindow(gfx::Rect(100, 45, 150, 200));
  std::unique_ptr<aura::Window> window2 =
      CreateTestWindow(gfx::Rect(150, 50, 150, 250));
  std::unique_ptr<aura::Window> window1 =
      CreateTestWindow(gfx::Rect(20, 30, 100, 300));
  std::unique_ptr<aura::Window> window4(
      CreateTestWindow(gfx::Rect(0, 0, 50, 90)));
  WindowState::Get(window4.get())->Minimize();

  auto* controller =
      StartCaptureSession(CaptureModeSource::kWindow, CaptureModeType::kImage);
  auto* capture_mode_session =
      static_cast<CaptureModeSession*>(controller->capture_mode_session());
  ASSERT_EQ(capture_mode_session->session_type(), SessionType::kReal);
  CaptureModeSessionTestApi test_api(capture_mode_session);
  using FocusGroup = CaptureModeSessionFocusCycler::FocusGroup;
  auto* event_generator = GetEventGenerator();

  EXPECT_EQ(FocusGroup::kNone, test_api.GetCurrentFocusGroup());
  EXPECT_EQ(0u, test_api.GetCurrentFocusIndex());

  // Tab six times, `window1` should be focused. Tab another time, `window2`
  // should be focused. Tab again, `window3` will be focused.
  SendKey(ui::VKEY_TAB, event_generator, ui::EF_NONE, /*count=*/6);
  EXPECT_EQ(FocusGroup::kCaptureWindow, test_api.GetCurrentFocusGroup());
  EXPECT_EQ(window1.get(), capture_mode_session->GetSelectedWindow());

  SendKey(ui::VKEY_TAB, event_generator);
  EXPECT_EQ(window2.get(), capture_mode_session->GetSelectedWindow());

  SendKey(ui::VKEY_TAB, event_generator);
  EXPECT_EQ(window3.get(), capture_mode_session->GetSelectedWindow());

  // Tab once, the `settings` button should be focused. The minimized `window4`
  // will be ignored during the tabbing process.
  SendKey(ui::VKEY_TAB, event_generator);
  EXPECT_EQ(FocusGroup::kSettingsClose, test_api.GetCurrentFocusGroup());
  controller->Stop();
}

// Tests that partially occluded window(s) will be focusable even when four
// edges are occluded by other windows while tabbing through in `kWindow` mode.
TEST_F(CaptureModeTest, PartiallyOccludedWindowIsFocusableInKWindow) {
  // Layout of five windows: four edges of `window3` is occluded by `window1`,
  // `window2`, `window4` and `window5` respectively, but the middle part is not
  // occluded.
  //        +-----------+
  //        |           |
  //   +----|     4     |
  //   |    |           |---------+
  //   |    |           |         |
  //   |    +-|-------|-+         |
  //   |  1   |   3   |     2     |
  //   |      |       |           |
  //   |    +-|-------|--+        |
  //   |    |            |--------+
  //   +----|     5      |
  //        |            |
  //        +------------+
  std::unique_ptr<aura::Window> window3 =
      CreateTestWindow(gfx::Rect(100, 45, 150, 200));
  std::unique_ptr<aura::Window> window2 =
      CreateTestWindow(gfx::Rect(150, 50, 150, 250));
  std::unique_ptr<aura::Window> window1 =
      CreateTestWindow(gfx::Rect(20, 30, 100, 300));
  std::unique_ptr<aura::Window> window4 =
      CreateTestWindow(gfx::Rect(50, 5, 150, 55));
  std::unique_ptr<aura::Window> window5 =
      CreateTestWindow(gfx::Rect(60, 225, 210, 45));

  auto* controller =
      StartCaptureSession(CaptureModeSource::kWindow, CaptureModeType::kImage);
  auto* capture_mode_session =
      static_cast<CaptureModeSession*>(controller->capture_mode_session());
  ASSERT_EQ(capture_mode_session->session_type(), SessionType::kReal);
  CaptureModeSessionTestApi test_api(capture_mode_session);
  using FocusGroup = CaptureModeSessionFocusCycler::FocusGroup;
  auto* event_generator = GetEventGenerator();

  EXPECT_EQ(FocusGroup::kNone, test_api.GetCurrentFocusGroup());
  EXPECT_EQ(0u, test_api.GetCurrentFocusIndex());

  // Tab six times, `window5` should be focused. Then `window4`, `window1`,
  // `window2` and `window3` will be focused after each tab.
  SendKey(ui::VKEY_TAB, event_generator, ui::EF_NONE, /*count=*/6);
  EXPECT_EQ(FocusGroup::kCaptureWindow, test_api.GetCurrentFocusGroup());
  EXPECT_EQ(window5.get(), capture_mode_session->GetSelectedWindow());
  SendKey(ui::VKEY_TAB, event_generator);
  EXPECT_EQ(window4.get(), capture_mode_session->GetSelectedWindow());
  SendKey(ui::VKEY_TAB, event_generator);
  EXPECT_EQ(window1.get(), capture_mode_session->GetSelectedWindow());
  SendKey(ui::VKEY_TAB, event_generator);
  EXPECT_EQ(window2.get(), capture_mode_session->GetSelectedWindow());
  SendKey(ui::VKEY_TAB, event_generator);
  EXPECT_EQ(window3.get(), capture_mode_session->GetSelectedWindow());

  // Tab once, the `settings` button should be focused.
  SendKey(ui::VKEY_TAB, event_generator);
  EXPECT_EQ(FocusGroup::kSettingsClose, test_api.GetCurrentFocusGroup());
  controller->Stop();
}

// Tests that fully occluded window(s) will be ignored while tabbing in
// `kWindow`.
TEST_F(CaptureModeTest, IgnoreFullyOccludedWindowWhileTabbingInKWindow) {
  // Layout of six windows: `window3` is fully occluded by `window1`, `window2`,
  // `window4`, `window5` and `window6`.
  //        +-----------+
  //        |           |
  //   +----|     4     |
  //   | 1  |           |---------+
  //   |  +-----------------+     |
  //   |  |                 |     |
  //   |  |       6         |  2  |
  //   |  |                 |     |
  //   |  +-----------------+     |
  //   |    |            |--------+
  //   +----|     5      |
  //        |            |
  //        +------------+
  std::unique_ptr<aura::Window> window3 =
      CreateTestWindow(gfx::Rect(100, 45, 150, 200));
  std::unique_ptr<aura::Window> window2 =
      CreateTestWindow(gfx::Rect(150, 50, 150, 250));
  std::unique_ptr<aura::Window> window1 =
      CreateTestWindow(gfx::Rect(20, 30, 100, 300));
  std::unique_ptr<aura::Window> window4 =
      CreateTestWindow(gfx::Rect(50, 5, 150, 55));
  std::unique_ptr<aura::Window> window5 =
      CreateTestWindow(gfx::Rect(60, 225, 210, 45));
  std::unique_ptr<aura::Window> window6 =
      CreateTestWindow(gfx::Rect(30, 55, 175, 185));

  auto* controller =
      StartCaptureSession(CaptureModeSource::kWindow, CaptureModeType::kImage);
  auto* capture_mode_session =
      static_cast<CaptureModeSession*>(controller->capture_mode_session());
  ASSERT_EQ(capture_mode_session->session_type(), SessionType::kReal);
  CaptureModeSessionTestApi test_api(capture_mode_session);
  using FocusGroup = CaptureModeSessionFocusCycler::FocusGroup;
  auto* event_generator = GetEventGenerator();

  EXPECT_EQ(FocusGroup::kNone, test_api.GetCurrentFocusGroup());
  EXPECT_EQ(0u, test_api.GetCurrentFocusIndex());

  // Tab six times, `window6` should be focused. Then `window5`, `window4`,
  // `window1` and `window2` will be focused after each tab.
  SendKey(ui::VKEY_TAB, event_generator, ui::EF_NONE, /*count=*/6);
  EXPECT_EQ(FocusGroup::kCaptureWindow, test_api.GetCurrentFocusGroup());
  EXPECT_EQ(window6.get(), capture_mode_session->GetSelectedWindow());
  SendKey(ui::VKEY_TAB, event_generator);
  EXPECT_EQ(window5.get(), capture_mode_session->GetSelectedWindow());
  SendKey(ui::VKEY_TAB, event_generator);
  EXPECT_EQ(window4.get(), capture_mode_session->GetSelectedWindow());
  SendKey(ui::VKEY_TAB, event_generator);
  EXPECT_EQ(window1.get(), capture_mode_session->GetSelectedWindow());
  SendKey(ui::VKEY_TAB, event_generator);
  EXPECT_EQ(window2.get(), capture_mode_session->GetSelectedWindow());

  // Tab once, the `settings` button should be focused. The fully occluded
  // `window3` will be ignored during the tabbing process.
  SendKey(ui::VKEY_TAB, event_generator);
  EXPECT_EQ(FocusGroup::kSettingsClose, test_api.GetCurrentFocusGroup());
}

// Tests that only Tab and Shift + Tab events advance/reverse focus and stop
// event propagation. Other events like Alt + Tab should still behave as
// intended.
TEST_F(CaptureModeTest, OnlyAdvanceFocusWhenTabShiftPressed) {
  auto window1 = CreateTestWindow();
  auto window2 = CreateTestWindow();

  auto* controller =
      StartCaptureSession(CaptureModeSource::kWindow, CaptureModeType::kVideo);
  auto* capture_mode_session =
      static_cast<CaptureModeSession*>(controller->capture_mode_session());
  ASSERT_EQ(capture_mode_session->session_type(), SessionType::kReal);
  CaptureModeSessionTestApi test_api(capture_mode_session);
  using FocusGroup = CaptureModeSessionFocusCycler::FocusGroup;
  auto* event_generator = GetEventGenerator();

  EXPECT_EQ(FocusGroup::kNone, test_api.GetCurrentFocusGroup());
  EXPECT_EQ(0u, test_api.GetCurrentFocusIndex());

  // Tab should advance focus forward.
  SendKey(ui::VKEY_TAB, event_generator, ui::EF_NONE, /*count=*/6);
  EXPECT_EQ(window2.get(), capture_mode_session->GetSelectedWindow());

  // Shift + Tab should advance focus backwards (reverse focus).
  SendKey(ui::VKEY_TAB, event_generator, ui::EF_SHIFT_DOWN, /*count=*/5);
  EXPECT_EQ(FocusGroup::kTypeSource, test_api.GetCurrentFocusGroup());

  // Non-shortcut modifiers like Caps Lock should not count towards the flags we
  // are concerned with, so Tab and Shift + Tab should still work normally.
  SendKey(ui::VKEY_TAB, event_generator, ui::EF_CAPS_LOCK_ON, /*count=*/5);
  EXPECT_EQ(window2.get(), capture_mode_session->GetSelectedWindow());
  SendKey(ui::VKEY_TAB, event_generator, ui::EF_NUM_LOCK_ON | ui::EF_SHIFT_DOWN,
          /*count=*/5);
  EXPECT_EQ(FocusGroup::kTypeSource, test_api.GetCurrentFocusGroup());

  // Alt + Tab should cycle the active window, and the focus should not change.
  ASSERT_EQ(window_util::GetActiveWindow(), window2.get());
  // We need to wait synchronously until the event has been fully processed to
  // check if the activation has been changed.
  ui::test::EmulateFullKeyPressReleaseSequence(event_generator, ui::VKEY_TAB,
                                               false, false, true, false);
  EXPECT_EQ(window_util::GetActiveWindow(), window1.get());
  EXPECT_EQ(FocusGroup::kTypeSource, test_api.GetCurrentFocusGroup());
  ui::test::EmulateFullKeyPressReleaseSequence(event_generator, ui::VKEY_TAB,
                                               false, false, true, false);
  EXPECT_EQ(window_util::GetActiveWindow(), window2.get());
  EXPECT_EQ(FocusGroup::kTypeSource, test_api.GetCurrentFocusGroup());

  // Alt + Shift + Tab should also cycle the active window in the reverse
  // direction.
  ui::test::EmulateFullKeyPressReleaseSequence(event_generator, ui::VKEY_TAB,
                                               false, true, true, false);
  EXPECT_EQ(window_util::GetActiveWindow(), window1.get());
  EXPECT_EQ(FocusGroup::kTypeSource, test_api.GetCurrentFocusGroup());

  // Ctrl + Tab and Ctrl + Shift + Tab should not do anything.
  SendKey(ui::VKEY_TAB, event_generator, ui::EF_CONTROL_DOWN);
  EXPECT_EQ(window_util::GetActiveWindow(), window1.get());
  EXPECT_EQ(FocusGroup::kTypeSource, test_api.GetCurrentFocusGroup());
  SendKey(ui::VKEY_TAB, event_generator,
          ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN);
  EXPECT_EQ(window_util::GetActiveWindow(), window1.get());
  EXPECT_EQ(FocusGroup::kTypeSource, test_api.GetCurrentFocusGroup());
}

// Tests that the capture region will be refreshed if in overview to reflect the
// bounds of the overview item for this window in `kWindow` mode.
TEST_F(CaptureModeTest, RefreshCaptureRegionInOverviewForKWindow) {
  auto window = CreateAppWindow(gfx::Rect(100, 50, 200, 200));
  auto* controller =
      StartCaptureSession(CaptureModeSource::kWindow, CaptureModeType::kImage);
  auto* session = controller->capture_mode_session();

  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseToCenterOf(window.get());
  EXPECT_EQ(window.get(), session->GetSelectedWindow());

  // Start overview and verify that the capture region is refreshed correctly.
  auto* overview_controller = OverviewController::Get();
  overview_controller->StartOverview(OverviewStartAction::kTests);
  ASSERT_TRUE(overview_controller->InOverviewSession());
  auto* overview_item =
      overview_controller->overview_session()->GetOverviewItemForWindow(
          window.get());
  const auto target_bounds = overview_item->target_bounds();
  event_generator->MoveMouseTo(
      gfx::ToRoundedPoint(target_bounds.CenterPoint()));
  auto capture_region_in_overview =
      CaptureModeSessionTestApi(session).GetSelectedWindowTargetBounds();
  wm::ConvertRectToScreen(window->GetRootWindow(), &capture_region_in_overview);
  EXPECT_EQ(capture_region_in_overview, gfx::ToRoundedRect(target_bounds));
}

class CaptureModeSaveFileTest
    : public CaptureModeTest,
      public testing::WithParamInterface<CaptureModeType> {
 public:
  CaptureModeSaveFileTest() = default;
  CaptureModeSaveFileTest(
      const CaptureModeSaveFileTest& capture_mode_save_file_test) = delete;
  CaptureModeSaveFileTest& operator=(const CaptureModeSaveFileTest&) = delete;
  ~CaptureModeSaveFileTest() override = default;

  void StartCaptureSessionWithParam() {
    StartCaptureSession(CaptureModeSource::kFullscreen, GetParam());
  }

  // Based on the `CaptureModeType`, it performs the capture and then returns
  // the path of the saved image or video files.
  base::FilePath PerformCapture() {
    auto* controller = CaptureModeController::Get();
    switch (GetParam()) {
      case CaptureModeType::kImage:
        controller->PerformCapture();
        return WaitForCaptureFileToBeSaved();

      case CaptureModeType::kVideo:
        StartVideoRecordingImmediately();
        controller->EndVideoRecording(EndRecordingReason::kStopRecordingButton);
        return WaitForCaptureFileToBeSaved();
    }
  }
};

// Tests that if the custom folder becomes unavailable, the captured file should
// be saved into the default folder. Otherwise, it's saved into custom folder.
TEST_P(CaptureModeSaveFileTest, SaveCapturedFileWithCustomFolder) {
  auto* controller = CaptureModeController::Get();
  const base::FilePath default_folder =
      controller->delegate_for_testing()->GetUserDefaultDownloadsFolder();
  const base::FilePath custom_folder((FILE_PATH_LITERAL("/home/tests")));
  controller->SetCustomCaptureFolder(custom_folder);

  // Make sure the current folder is the custom folder here and then perform
  // capture.
  auto capture_folder = controller->GetCurrentCaptureFolder();
  EXPECT_FALSE(capture_folder.is_default_downloads_folder);
  StartCaptureSessionWithParam();
  base::FilePath file_saved_path = PerformCapture();

  // Since `custom_folder` is not available, the captured files will be saved
  // into default folder;
  EXPECT_EQ(file_saved_path.DirName(), default_folder);

  // Now create an available custom folder and set it for custom capture folder.
  const base::FilePath available_custom_folder =
      CreateCustomFolderInUserDownloadsPath("test");
  controller->SetCustomCaptureFolder(available_custom_folder);

  capture_folder = controller->GetCurrentCaptureFolder();
  EXPECT_FALSE(capture_folder.is_default_downloads_folder);
  StartCaptureSessionWithParam();
  file_saved_path = PerformCapture();

  // Since `available_custom_folder` is now available, the captured files will
  // be saved into the custom folder;
  EXPECT_EQ(file_saved_path.DirName(), available_custom_folder);
}

TEST_P(CaptureModeSaveFileTest, CaptureModeSaveToLocationMetric) {
  constexpr char kHistogramBase[] = "SaveLocation";
  const std::string histogram_name = BuildHistogramName(
      kHistogramBase, /*behavior=*/nullptr, /*append_ui_mode_suffix=*/true);
  base::HistogramTester histogram_tester;
  auto* controller = CaptureModeController::Get();
  auto* test_delegate = controller->delegate_for_testing();

  // Initialize four different save-to locations for screen capture that
  // includes default downloads folder, local customized folder, root drive and
  // a specific folder on drive.
  const auto downloads_folder = test_delegate->GetUserDefaultDownloadsFolder();
  const base::FilePath custom_folder =
      CreateCustomFolderInUserDownloadsPath("test");
  base::FilePath mount_point_path;
  test_delegate->GetDriveFsMountPointPath(&mount_point_path);
  const auto root_drive_folder = mount_point_path.Append("root");
  const base::FilePath non_root_drive_folder = CreateFolderOnDriveFS("test");
  const base::FilePath onedrive_root =
      test_delegate->GetOneDriveMountPointPath();
  const base::FilePath onedrive_folder = onedrive_root.Append("test");
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_TRUE(base::CreateDirectory(onedrive_folder));
  }
  struct {
    base::FilePath set_save_file_folder;
    CaptureModeSaveToLocation save_location;
  } kTestCases[] = {
      {downloads_folder, CaptureModeSaveToLocation::kDefault},
      {custom_folder, CaptureModeSaveToLocation::kCustomizedFolder},
      {root_drive_folder, CaptureModeSaveToLocation::kDrive},
      {non_root_drive_folder, CaptureModeSaveToLocation::kDriveFolder},
      {onedrive_root, CaptureModeSaveToLocation::kOneDrive},
      {onedrive_folder, CaptureModeSaveToLocation::kOneDriveFolder},
  };
  for (auto test_case : kTestCases) {
    histogram_tester.ExpectBucketCount(histogram_name, test_case.save_location,
                                       0);
  }
  // Set four different save-to locations in clamshell mode and check the
  // histogram results.
  EXPECT_FALSE(Shell::Get()->IsInTabletMode());
  for (auto test_case : kTestCases) {
    StartCaptureSessionWithParam();
    controller->SetCustomCaptureFolder(test_case.set_save_file_folder);
    auto file_saved_path = PerformCapture();
    histogram_tester.ExpectBucketCount(histogram_name, test_case.save_location,
                                       1);
  }

  // Set four different save-to locations in tablet mode and check the histogram
  // results.
  SwitchToTabletMode();
  EXPECT_TRUE(Shell::Get()->IsInTabletMode());
  for (auto test_case : kTestCases) {
    StartCaptureSessionWithParam();
    controller->SetCustomCaptureFolder(test_case.set_save_file_folder);
    auto file_saved_path = PerformCapture();
    histogram_tester.ExpectBucketCount(histogram_name, test_case.save_location,
                                       1);
  }
}

INSTANTIATE_TEST_SUITE_P(All,
                         CaptureModeSaveFileTest,
                         testing::Values(CaptureModeType::kImage,
                                         CaptureModeType::kVideo));

// Test fixture for verifying that the videos are recorded at the pixel size of
// the targets being captured in all recording modes. This avoids having the
// scaling in CopyOutputRequests when performing the capture at a different size
// than that of the render pass (which is in pixels). This scaling causes a loss
// of quality, and a blurry video frames. https://crbug.com/1215185.
class CaptureModeRecordingSizeTest : public CaptureModeTest {
 public:
  CaptureModeRecordingSizeTest() = default;
  ~CaptureModeRecordingSizeTest() override = default;

  // CaptureModeTest:
  void SetUp() override {
    CaptureModeTest::SetUp();
    window_ = CreateTestWindow(gfx::Rect(100, 50, 200, 200));
    CaptureModeController::Get()->SetUserCaptureRegion(user_region_,
                                                       /*by_user=*/true);
    UpdateDisplay("800x600");
  }

  void TearDown() override {
    window_.reset();
    CaptureModeTest::TearDown();
  }

  // Converts the given |size| from DIPs to pixels based on the given value of
  // |dsf|.
  gfx::Size ToPixels(const gfx::Size& size, float dsf) const {
    return gfx::ToFlooredSize(gfx::ConvertSizeToPixels(size, dsf));
  }

 protected:
  // Verifies the size of the received video frame.
  static void VerifyVideoFrame(const gfx::Size& expected_video_size,
                               const media::VideoFrame& frame,
                               const gfx::Rect& content_rect) {
    // The I420 pixel format does not like odd dimensions, so the size of the
    // visible rect in the video frame will be adjusted to be an even value.
    const gfx::Size adjusted_size(expected_video_size.width() & ~1,
                                  expected_video_size.height() & ~1);
    EXPECT_EQ(adjusted_size, frame.visible_rect().size());
  }

  CaptureModeController* StartVideoRecording(CaptureModeSource source) {
    auto* controller = StartCaptureSession(source, CaptureModeType::kVideo);
    if (source == CaptureModeSource::kWindow)
      GetEventGenerator()->MoveMouseToCenterOf(window_.get());
    StartVideoRecordingImmediately();
    EXPECT_TRUE(controller->is_recording_in_progress());
    CaptureModeTestApi().FlushRecordingServiceForTesting();
    return controller;
  }

 protected:
  const gfx::Rect user_region_{20, 50};
  std::unique_ptr<aura::Window> window_;
};

// TODO(crbug.com/1291073): Flaky on ChromeOS.
#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_CaptureAtPixelsFullscreen DISABLED_CaptureAtPixelsFullscreen
#else
#define MAYBE_CaptureAtPixelsFullscreen CaptureAtPixelsFullscreen
#endif
TEST_F(CaptureModeRecordingSizeTest, MAYBE_CaptureAtPixelsFullscreen) {
  float dsf = 1.6f;
  SetDeviceScaleFactor(dsf);
  EXPECT_EQ(dsf, window_->GetHost()->device_scale_factor());
  auto* controller = StartVideoRecording(CaptureModeSource::kFullscreen);
  auto* root = window_->GetRootWindow();
  gfx::Size initial_root_window_size_pixels =
      ToPixels(root->bounds().size(), dsf);
  auto* test_delegate =
      static_cast<TestCaptureModeDelegate*>(controller->delegate_for_testing());
  ASSERT_TRUE(test_delegate->recording_service());
  {
    SCOPED_TRACE("Testing @ 1.6 device scale factor");
    EXPECT_EQ(initial_root_window_size_pixels,
              test_delegate->GetCurrentVideoSize());

    EXPECT_EQ(
        dsf, test_delegate->recording_service()->GetCurrentDeviceScaleFactor());

    EXPECT_EQ(initial_root_window_size_pixels,
              test_delegate->GetCurrentFrameSinkSizeInPixels());

    test_delegate->recording_service()->RequestAndWaitForVideoFrame(
        base::BindOnce(&CaptureModeRecordingSizeTest::VerifyVideoFrame,
                       initial_root_window_size_pixels));
  }

  // Change the DSF and expect the video size will remain at the initial pixel
  // size of the fullscreen.
  dsf = 2.f;
  SetDeviceScaleFactor(dsf);
  EXPECT_EQ(dsf, window_->GetHost()->device_scale_factor());
  {
    SCOPED_TRACE("Testing @ 2.0 device scale factor");
    EXPECT_EQ(initial_root_window_size_pixels,
              test_delegate->GetCurrentVideoSize());

    // The recording service still tracks the up-to-date DSF and frame sink
    // pixel size even though it doesn't change the video size from its initial
    // value.
    EXPECT_EQ(
        dsf, test_delegate->recording_service()->GetCurrentDeviceScaleFactor());

    EXPECT_EQ(ToPixels(root->bounds().size(), dsf),
              test_delegate->GetCurrentFrameSinkSizeInPixels());

    test_delegate->recording_service()->RequestAndWaitForVideoFrame(
        base::BindOnce(&CaptureModeRecordingSizeTest::VerifyVideoFrame,
                       initial_root_window_size_pixels));
  }

  // When recording the fullscreen, the video size never changes, and remains at
  // the initial pixel size of the recording. Hence, there should be no
  // reconfigures.
  EXPECT_EQ(0, test_delegate->recording_service()
                   ->GetNumberOfVideoEncoderReconfigures());
}

// The test is flaky. https://crbug.com/1287724.
TEST_F(CaptureModeRecordingSizeTest, DISABLED_CaptureAtPixelsRegion) {
  float dsf = 1.6f;
  SetDeviceScaleFactor(dsf);
  EXPECT_EQ(dsf, window_->GetHost()->device_scale_factor());
  auto* controller = StartVideoRecording(CaptureModeSource::kRegion);
  auto* test_delegate =
      static_cast<TestCaptureModeDelegate*>(controller->delegate_for_testing());
  ASSERT_TRUE(test_delegate->recording_service());

  {
    SCOPED_TRACE("Testing @ 1.6 device scale factor");
    const gfx::Size expected_video_size = ToPixels(user_region_.size(), dsf);
    EXPECT_EQ(expected_video_size, test_delegate->GetCurrentVideoSize());

    EXPECT_EQ(
        dsf, test_delegate->recording_service()->GetCurrentDeviceScaleFactor());

    test_delegate->recording_service()->RequestAndWaitForVideoFrame(
        base::BindOnce(&CaptureModeRecordingSizeTest::VerifyVideoFrame,
                       expected_video_size));
  }

  // Change the DSF and expect the video size to change to match the new pixel
  // size of the recorded target.
  dsf = 2.f;
  SetDeviceScaleFactor(dsf);
  EXPECT_EQ(dsf, window_->GetHost()->device_scale_factor());
  {
    SCOPED_TRACE("Testing @ 2.0 device scale factor");
    const gfx::Size expected_video_size = ToPixels(user_region_.size(), dsf);
    EXPECT_EQ(expected_video_size, test_delegate->GetCurrentVideoSize());

    EXPECT_EQ(
        dsf, test_delegate->recording_service()->GetCurrentDeviceScaleFactor());

    test_delegate->recording_service()->RequestAndWaitForVideoFrame(
        base::BindOnce(&CaptureModeRecordingSizeTest::VerifyVideoFrame,
                       expected_video_size));
  }

  // Since the user chooses the capture region in DIPs, its corresponding pixel
  // size will change when changing the device scale factor. Therefore, the
  // encoder is expected to reconfigure once.
  EXPECT_EQ(1, test_delegate->recording_service()
                   ->GetNumberOfVideoEncoderReconfigures());
}

// The test is flaky. https://crbug.com/1287724.
TEST_F(CaptureModeRecordingSizeTest, DISABLED_CaptureAtPixelsWindow) {
  float dsf = 1.6f;
  SetDeviceScaleFactor(dsf);
  EXPECT_EQ(dsf, window_->GetHost()->device_scale_factor());
  auto* controller = StartVideoRecording(CaptureModeSource::kWindow);
  auto* test_delegate =
      static_cast<TestCaptureModeDelegate*>(controller->delegate_for_testing());
  ASSERT_TRUE(test_delegate->recording_service());

  {
    SCOPED_TRACE("Testing @ 1.6 device scale factor");
    const gfx::Size expected_video_size =
        ToPixels(window_->GetBoundsInRootWindow().size(), dsf);
    EXPECT_EQ(expected_video_size, test_delegate->GetCurrentVideoSize());

    EXPECT_EQ(
        dsf, test_delegate->recording_service()->GetCurrentDeviceScaleFactor());

    test_delegate->recording_service()->RequestAndWaitForVideoFrame(
        base::BindOnce(&CaptureModeRecordingSizeTest::VerifyVideoFrame,
                       expected_video_size));
  }

  // Change the DSF and expect the video size to change to match the new pixel
  // size of the recorded target.
  dsf = 2.f;
  SetDeviceScaleFactor(dsf);
  EXPECT_EQ(dsf, window_->GetHost()->device_scale_factor());
  {
    SCOPED_TRACE("Testing @ 2.0 device scale factor");
    const gfx::Size expected_video_size =
        ToPixels(window_->GetBoundsInRootWindow().size(), dsf);
    EXPECT_EQ(expected_video_size, test_delegate->GetCurrentVideoSize());

    EXPECT_EQ(
        dsf, test_delegate->recording_service()->GetCurrentDeviceScaleFactor());

    test_delegate->recording_service()->RequestAndWaitForVideoFrame(
        base::BindOnce(&CaptureModeRecordingSizeTest::VerifyVideoFrame,
                       expected_video_size));
  }

  // When changing the device scale factor, the DIPs size of the window doesn't
  // change, but (like |kRegion|) its pixel size will. Hence, the
  // reconfiguration.
  EXPECT_EQ(1, test_delegate->recording_service()
                   ->GetNumberOfVideoEncoderReconfigures());
}

// Tests the behavior of screen recording with the presence of HDCP secure
// content on the screen in all capture mode sources (fullscreen, region, and
// window) depending on the test param.
class CaptureModeHdcpTest
    : public CaptureModeTest,
      public ::testing::WithParamInterface<CaptureModeSource> {
 public:
  CaptureModeHdcpTest() = default;
  ~CaptureModeHdcpTest() override = default;

  // CaptureModeTest:
  void SetUp() override {
    CaptureModeTest::SetUp();
    window_ = CreateTestWindow(gfx::Rect(200, 200));
    // Create a child window with protected content. This simulates the real
    // behavior of a browser window hosting a page with protected content, where
    // the window that has a protection mask is the RenderWidgetHostViewAura,
    // which is a descendant of the BrowserFrame window which can get recorded.
    protected_content_window_ = CreateTestWindow(gfx::Rect(150, 150));
    window_->AddChild(protected_content_window_.get());
    protection_delegate_ = std::make_unique<OutputProtectionDelegate>(
        protected_content_window_.get());
    CaptureModeController::Get()->SetUserCaptureRegion(gfx::Rect(20, 50),
                                                       /*by_user=*/true);
  }

  void TearDown() override {
    protection_delegate_.reset();
    protected_content_window_.reset();
    window_.reset();
    CaptureModeTest::TearDown();
  }

  // Enters the capture mode session.
  void StartSessionForVideo() {
    StartCaptureSession(GetParam(), CaptureModeType::kVideo);
  }

  // Attempts video recording from the capture mode source set by the test
  // param.
  void AttemptRecording() {
    auto* controller = CaptureModeController::Get();
    ASSERT_TRUE(controller->IsActive());

    switch (GetParam()) {
      case CaptureModeSource::kFullscreen:
      case CaptureModeSource::kRegion:
        controller->StartVideoRecordingImmediatelyForTesting();
        break;

      case CaptureModeSource::kWindow:
        // Window capture mode selects the window under the cursor as the
        // capture source.
        auto* event_generator = GetEventGenerator();
        event_generator->MoveMouseToCenterOf(window_.get());
        controller->StartVideoRecordingImmediatelyForTesting();
        break;
    }
  }

 protected:
  std::unique_ptr<aura::Window> window_;
  std::unique_ptr<aura::Window> protected_content_window_;
  std::unique_ptr<OutputProtectionDelegate> protection_delegate_;
};

TEST_P(CaptureModeHdcpTest, WindowBecomesProtectedWhileRecording) {
  StartSessionForVideo();
  AttemptRecording();
  WaitForRecordingToStart();

  auto* controller = CaptureModeController::Get();
  EXPECT_TRUE(controller->is_recording_in_progress());

  // The window becomes HDCP protected, which should end video recording.
  base::HistogramTester histogram_tester;
  protection_delegate_->SetProtection(display::CONTENT_PROTECTION_METHOD_HDCP,
                                      base::DoNothing());

  EXPECT_FALSE(controller->is_recording_in_progress());
  histogram_tester.ExpectBucketCount(
      kEndRecordingReasonInClamshellHistogramName,
      EndRecordingReason::kHdcpInterruption, 1);
}

TEST_F(CaptureModeHdcpTest, ProtectedTabBecomesActiveAfterRecordingStarts) {
  // Simulate protected content being on an inactive tab.
  protection_delegate_->SetProtection(display::CONTENT_PROTECTION_METHOD_HDCP,
                                      base::DoNothing());
  Shell::GetPrimaryRootWindow()
      ->GetChildById(kShellWindowId_UnparentedContainer)
      ->AddChild(protected_content_window_.get());

  // Recording should start normally, since the protected window is not a
  // descendant of the window being recorded.
  auto* controller =
      StartCaptureSession(CaptureModeSource::kWindow, CaptureModeType::kVideo);
  GetEventGenerator()->MoveMouseToCenterOf(window_.get());
  controller->StartVideoRecordingImmediatelyForTesting();
  WaitForRecordingToStart();
  EXPECT_TRUE(controller->is_recording_in_progress());

  // Simulate activating the tab that has protected content by parenting it back
  // to the window being recorded. Recording should stop immediately.
  window_->AddChild(protected_content_window_.get());

  EXPECT_FALSE(controller->is_recording_in_progress());
}

TEST_P(CaptureModeHdcpTest, ProtectedWindowDestruction) {
  auto window_2 = CreateTestWindow(gfx::Rect(100, 50));
  OutputProtectionDelegate protection_delegate_2(window_2.get());
  protection_delegate_2.SetProtection(display::CONTENT_PROTECTION_METHOD_HDCP,
                                      base::DoNothing());

  StartSessionForVideo();
  AttemptRecording();

  // Recording cannot start because of another protected window on the screen,
  // except when we're capturing a different |window_|.
  auto* controller = CaptureModeController::Get();
  EXPECT_FALSE(controller->IsActive());
  if (GetParam() == CaptureModeSource::kWindow) {
    WaitForRecordingToStart();
    EXPECT_TRUE(controller->is_recording_in_progress());
    controller->EndVideoRecording(EndRecordingReason::kStopRecordingButton);
    EXPECT_FALSE(controller->is_recording_in_progress());
    // Wait for the video file to be saved so that we can start a new recording.
    WaitForCaptureFileToBeSaved();
  } else {
    EXPECT_FALSE(controller->is_recording_in_progress());
  }

  // When the protected window is destroyed, it's possbile now to record from
  // all capture sources.
  window_2.reset();
  StartSessionForVideo();
  AttemptRecording();
  WaitForRecordingToStart();

  EXPECT_FALSE(controller->IsActive());
  EXPECT_TRUE(controller->is_recording_in_progress());
}

TEST_P(CaptureModeHdcpTest, WindowBecomesProtectedBeforeRecording) {
  protection_delegate_->SetProtection(display::CONTENT_PROTECTION_METHOD_HDCP,
                                      base::DoNothing());
  StartSessionForVideo();
  AttemptRecording();

  // Recording cannot even start.
  auto* controller = CaptureModeController::Get();
  EXPECT_FALSE(controller->is_recording_in_progress());
  EXPECT_FALSE(controller->IsActive());
}

TEST_P(CaptureModeHdcpTest, ProtectedWindowInMultiDisplay) {
  UpdateDisplay("500x400,401+0-500x400");
  auto roots = Shell::GetAllRootWindows();
  ASSERT_EQ(2u, roots.size());
  protection_delegate_->SetProtection(display::CONTENT_PROTECTION_METHOD_HDCP,
                                      base::DoNothing());

  // Move the cursor to the secondary display before starting the session to
  // make sure the session starts on that display.
  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(roots[1]->GetBoundsInScreen().CenterPoint());
  StartSessionForVideo();
  // Also, make sure the selected region is in the secondary display.
  auto* controller = CaptureModeController::Get();
  EXPECT_EQ(controller->capture_mode_session()->current_root(), roots[1]);
  AttemptRecording();

  // Recording should be able to start (since the protected window is on the
  // first display) unless the protected window itself is the one being
  // recorded.
  if (GetParam() == CaptureModeSource::kWindow) {
    EXPECT_FALSE(controller->is_recording_in_progress());
  } else {
    WaitForRecordingToStart();
    EXPECT_TRUE(controller->is_recording_in_progress());

    // Moving the protected window to the display being recorded should
    // terminate the recording.
    base::HistogramTester histogram_tester;
    window_util::MoveWindowToDisplay(window_.get(),
                                     roots[1]->GetHost()->GetDisplayId());
    ASSERT_EQ(window_->GetRootWindow(), roots[1]);
    ASSERT_EQ(protected_content_window_->GetRootWindow(), roots[1]);
    EXPECT_FALSE(controller->is_recording_in_progress());
    histogram_tester.ExpectBucketCount(
        kEndRecordingReasonInClamshellHistogramName,
        EndRecordingReason::kHdcpInterruption, 1);
  }
}

INSTANTIATE_TEST_SUITE_P(All,
                         CaptureModeHdcpTest,
                         testing::Values(CaptureModeSource::kFullscreen,
                                         CaptureModeSource::kRegion,
                                         CaptureModeSource::kWindow));

TEST_F(CaptureModeTest, ClosingWindowBeingRecorded) {
  auto window = CreateTestWindow(gfx::Rect(200, 200));
  StartCaptureSession(CaptureModeSource::kWindow, CaptureModeType::kVideo);

  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseToCenterOf(window.get());
  auto* controller = CaptureModeController::Get();
  StartVideoRecordingImmediately();
  EXPECT_TRUE(controller->is_recording_in_progress());

  // The window should have a valid capture ID.
  EXPECT_TRUE(window->subtree_capture_id().is_valid());

  // Generate a couple of mouse moves, so that the second one gets throttled
  // using the `VideoRecordingWatcher::cursor_events_throttle_timer_`. This is
  // needed for a regression testing of https://crbug.com/1273609.
  event_generator->MoveMouseBy(20, 30);
  event_generator->MoveMouseBy(-10, -20);

  // Closing the window being recorded should end video recording.
  base::HistogramTester histogram_tester;
  window.reset();

  auto* stop_recording_button = Shell::GetPrimaryRootWindowController()
                                    ->GetStatusAreaWidget()
                                    ->stop_recording_button_tray();
  EXPECT_FALSE(stop_recording_button->visible_preferred());
  EXPECT_FALSE(controller->is_recording_in_progress());
  WaitForCaptureFileToBeSaved();
  EXPECT_FALSE(controller->video_recording_watcher_for_testing());
  histogram_tester.ExpectBucketCount(
      kEndRecordingReasonInClamshellHistogramName,
      EndRecordingReason::kDisplayOrWindowClosing, 1);
}

TEST_F(CaptureModeTest, DetachDisplayWhileWindowRecording) {
  UpdateDisplay("500x400,401+0-500x400");
  // Create a window on the second display.
  auto window = CreateTestWindow(gfx::Rect(450, 20, 200, 200));
  auto roots = Shell::GetAllRootWindows();
  ASSERT_EQ(2u, roots.size());
  EXPECT_EQ(window->GetRootWindow(), roots[1]);
  StartCaptureSession(CaptureModeSource::kWindow, CaptureModeType::kVideo);

  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(window->GetBoundsInScreen().CenterPoint());
  auto* controller = CaptureModeController::Get();
  StartVideoRecordingImmediately();
  EXPECT_TRUE(controller->is_recording_in_progress());

  auto* stop_recording_button = RootWindowController::ForWindow(roots[1])
                                    ->GetStatusAreaWidget()
                                    ->stop_recording_button_tray();
  EXPECT_TRUE(stop_recording_button->visible_preferred());

  // Disconnecting the display, on which the window being recorded is located,
  // should not end the recording. The window should be reparented to another
  // display, and the stop-recording button should move with to that display.
  RemoveSecondaryDisplay();
  roots = Shell::GetAllRootWindows();
  ASSERT_EQ(1u, roots.size());

  EXPECT_TRUE(controller->is_recording_in_progress());
  stop_recording_button = RootWindowController::ForWindow(roots[0])
                              ->GetStatusAreaWidget()
                              ->stop_recording_button_tray();
  EXPECT_TRUE(stop_recording_button->visible_preferred());
}

TEST_F(CaptureModeTest, SuspendWhileSessionIsActive) {
  auto* controller = StartCaptureSession(CaptureModeSource::kFullscreen,
                                         CaptureModeType::kVideo);
  EXPECT_TRUE(controller->IsActive());
  power_manager_client()->SendSuspendImminent(
      power_manager::SuspendImminent::IDLE);
  EXPECT_FALSE(controller->IsActive());
}

TEST_F(CaptureModeTest, SuspendAfterCountdownStarts) {
  // User NORMAL_DURATION for the countdown animation so we can have predictable
  // timings.
  ui::ScopedAnimationDurationScaleMode animation_scale(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);
  auto* controller = StartCaptureSession(CaptureModeSource::kFullscreen,
                                         CaptureModeType::kVideo);
  // Hit Enter to begin recording, wait for 1 second, then suspend the device.
  auto* event_generator = GetEventGenerator();
  SendKey(ui::VKEY_RETURN, event_generator);
  WaitForSeconds(1);
  power_manager_client()->SendSuspendImminent(
      power_manager::SuspendImminent::IDLE);
  EXPECT_FALSE(controller->IsActive());
  EXPECT_FALSE(controller->is_recording_in_progress());
}

TEST_F(CaptureModeTest, SuspendAfterRecordingStarts) {
  auto* controller = StartCaptureSession(CaptureModeSource::kFullscreen,
                                         CaptureModeType::kVideo);
  StartVideoRecordingImmediately();
  EXPECT_TRUE(controller->is_recording_in_progress());
  base::HistogramTester histogram_tester;
  power_manager_client()->SendSuspendImminent(
      power_manager::SuspendImminent::IDLE);
  EXPECT_FALSE(controller->is_recording_in_progress());
  histogram_tester.ExpectBucketCount(
      kEndRecordingReasonInClamshellHistogramName,
      EndRecordingReason::kImminentSuspend, 1);
}

TEST_F(CaptureModeTest, SwitchUsersWhileRecording) {
  auto* controller = StartCaptureSession(CaptureModeSource::kFullscreen,
                                         CaptureModeType::kVideo);
  StartVideoRecordingImmediately();
  base::HistogramTester histogram_tester;
  EXPECT_TRUE(controller->is_recording_in_progress());
  SwitchToUser2();
  EXPECT_FALSE(controller->is_recording_in_progress());
  histogram_tester.ExpectBucketCount(
      kEndRecordingReasonInClamshellHistogramName,
      EndRecordingReason::kActiveUserChange, 1);
}

TEST_F(CaptureModeTest, SwitchUsersAfterCountdownStarts) {
  // User NORMAL_DURATION for the countdown animation so we can have predictable
  // timings.
  ui::ScopedAnimationDurationScaleMode animation_scale(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);
  auto* controller = StartCaptureSession(CaptureModeSource::kFullscreen,
                                         CaptureModeType::kVideo);
  // Hit Enter to begin recording, wait for 1 second, then switch users.
  auto* event_generator = GetEventGenerator();
  SendKey(ui::VKEY_RETURN, event_generator);
  WaitForSeconds(1);
  SwitchToUser2();
  EXPECT_FALSE(controller->IsActive());
  EXPECT_FALSE(controller->is_recording_in_progress());
}

TEST_F(CaptureModeTest, ClosingDisplayBeingFullscreenRecorded) {
  UpdateDisplay("500x400,401+0-500x400");
  auto roots = Shell::GetAllRootWindows();
  ASSERT_EQ(2u, roots.size());
  StartCaptureSession(CaptureModeSource::kFullscreen, CaptureModeType::kVideo);

  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(roots[1]->GetBoundsInScreen().CenterPoint());
  auto* controller = CaptureModeController::Get();
  StartVideoRecordingImmediately();
  EXPECT_TRUE(controller->is_recording_in_progress());

  auto* stop_recording_button = RootWindowController::ForWindow(roots[1])
                                    ->GetStatusAreaWidget()
                                    ->stop_recording_button_tray();
  EXPECT_TRUE(stop_recording_button->visible_preferred());

  // Disconnecting the display being fullscreen recorded should end the
  // recording and remove the stop recording button.
  base::HistogramTester histogram_tester;
  RemoveSecondaryDisplay();
  roots = Shell::GetAllRootWindows();
  ASSERT_EQ(1u, roots.size());

  EXPECT_FALSE(controller->is_recording_in_progress());
  stop_recording_button = RootWindowController::ForWindow(roots[0])
                              ->GetStatusAreaWidget()
                              ->stop_recording_button_tray();
  EXPECT_FALSE(stop_recording_button->visible_preferred());
  histogram_tester.ExpectBucketCount(
      kEndRecordingReasonInClamshellHistogramName,
      EndRecordingReason::kDisplayOrWindowClosing, 1);
}

TEST_F(CaptureModeTest, ShuttingDownWhileRecording) {
  StartCaptureSession(CaptureModeSource::kFullscreen, CaptureModeType::kVideo);

  auto* controller = CaptureModeController::Get();
  StartVideoRecordingImmediately();
  EXPECT_TRUE(controller->is_recording_in_progress());

  // Exiting the test now will shut down ash while recording is in progress,
  // there should be no crashes when
  // VideoRecordingWatcher::OnChromeTerminating() terminates the recording.
}

// Tests that metrics are recorded properly for capture mode bar buttons.
TEST_F(CaptureModeTest, CaptureModeBarButtonTypeHistograms) {
  constexpr char kClamshellHistogram[] =
      "Ash.CaptureModeController.BarButtons.ClamshellMode";
  constexpr char kTabletHistogram[] =
      "Ash.CaptureModeController.BarButtons.TabletMode";
  base::HistogramTester histogram_tester;

  CaptureModeController::Get()->Start(CaptureModeEntryType::kQuickSettings);
  auto* event_generator = GetEventGenerator();

  // Tests each bar button in clamshell mode.
  ClickOnView(GetImageToggleButton(), event_generator);
  histogram_tester.ExpectBucketCount(
      kClamshellHistogram, CaptureModeBarButtonType::kScreenCapture, 1);

  ClickOnView(GetVideoToggleButton(), event_generator);
  histogram_tester.ExpectBucketCount(
      kClamshellHistogram, CaptureModeBarButtonType::kScreenRecord, 1);

  ClickOnView(GetFullscreenToggleButton(), event_generator);
  histogram_tester.ExpectBucketCount(kClamshellHistogram,
                                     CaptureModeBarButtonType::kFull, 1);

  ClickOnView(GetRegionToggleButton(), event_generator);
  histogram_tester.ExpectBucketCount(kClamshellHistogram,
                                     CaptureModeBarButtonType::kRegion, 1);

  ClickOnView(GetWindowToggleButton(), event_generator);
  histogram_tester.ExpectBucketCount(kClamshellHistogram,
                                     CaptureModeBarButtonType::kWindow, 1);

  // Enter tablet mode and test the bar buttons.
  ash::TabletModeControllerTestApi().EnterTabletMode();
  ASSERT_TRUE(display::Screen::GetScreen()->InTabletMode());

  ClickOnView(GetImageToggleButton(), event_generator);
  histogram_tester.ExpectBucketCount(
      kTabletHistogram, CaptureModeBarButtonType::kScreenCapture, 1);

  ClickOnView(GetVideoToggleButton(), event_generator);
  histogram_tester.ExpectBucketCount(
      kTabletHistogram, CaptureModeBarButtonType::kScreenRecord, 1);

  ClickOnView(GetFullscreenToggleButton(), event_generator);
  histogram_tester.ExpectBucketCount(kTabletHistogram,
                                     CaptureModeBarButtonType::kFull, 1);

  ClickOnView(GetRegionToggleButton(), event_generator);
  histogram_tester.ExpectBucketCount(kTabletHistogram,
                                     CaptureModeBarButtonType::kRegion, 1);

  ClickOnView(GetWindowToggleButton(), event_generator);
  histogram_tester.ExpectBucketCount(kTabletHistogram,
                                     CaptureModeBarButtonType::kWindow, 1);
}

TEST_F(CaptureModeTest, CaptureSessionSwitchedModeMetric) {
  constexpr char kHistogramName[] =
      "Ash.CaptureModeController.SwitchesFromInitialCaptureMode";
  base::HistogramTester histogram_tester;
  histogram_tester.ExpectBucketCount(kHistogramName, false, 0);
  histogram_tester.ExpectBucketCount(kHistogramName, true, 0);

  // Perform a capture without switching modes. A false should be recorded.
  auto* controller = StartImageRegionCapture();
  SelectRegion(gfx::Rect(100, 100));
  auto* event_generator = GetEventGenerator();
  SendKey(ui::VKEY_RETURN, event_generator);
  histogram_tester.ExpectBucketCount(kHistogramName, false, 1);
  histogram_tester.ExpectBucketCount(kHistogramName, true, 0);

  // Perform a capture after switching to fullscreen mode. A true should be
  // recorded.
  controller->Start(CaptureModeEntryType::kQuickSettings);
  ClickOnView(GetFullscreenToggleButton(), event_generator);
  SendKey(ui::VKEY_RETURN, event_generator);
  histogram_tester.ExpectBucketCount(kHistogramName, false, 1);
  histogram_tester.ExpectBucketCount(kHistogramName, true, 1);

  // Perform a capture after switching to another mode and back to the original
  // mode. A true should still be recorded as there was some switching done.
  controller->Start(CaptureModeEntryType::kQuickSettings);
  ClickOnView(GetRegionToggleButton(), event_generator);
  ClickOnView(GetFullscreenToggleButton(), event_generator);
  SendKey(ui::VKEY_RETURN, event_generator);
  histogram_tester.ExpectBucketCount(kHistogramName, false, 1);
  histogram_tester.ExpectBucketCount(kHistogramName, true, 2);
}

// Test that cancel recording during countdown won't cause crash.
TEST_F(CaptureModeTest, CancelCaptureDuringCountDown) {
  ui::ScopedAnimationDurationScaleMode animation_scale(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);
  StartCaptureSession(CaptureModeSource::kFullscreen, CaptureModeType::kVideo);
  // Hit Enter to begin recording, Wait for 1 second, then press ESC while count
  // down is in progress.
  auto* event_generator = GetEventGenerator();
  SendKey(ui::VKEY_RETURN, event_generator);
  WaitForSeconds(1);
  CaptureModeTestApi test_api;
  EXPECT_TRUE(test_api.IsInCountDownAnimation());
  SendKey(ui::VKEY_ESCAPE, event_generator);
  EXPECT_FALSE(test_api.IsInCountDownAnimation());
  EXPECT_FALSE(test_api.IsSessionActive());
  EXPECT_FALSE(test_api.IsVideoRecordingInProgress());
}

TEST_F(CaptureModeTest, EscDuringCountDownWhileSettingsOpen) {
  ui::ScopedAnimationDurationScaleMode animation_scale(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);
  StartCaptureSession(CaptureModeSource::kFullscreen, CaptureModeType::kVideo);

  // Hitting Esc while the settings menu is open and the count down is in
  // progress should end the session directly.
  auto* event_generator = GetEventGenerator();
  ClickOnView(GetSettingsButton(), event_generator);
  EXPECT_TRUE(GetCaptureModeSettingsWidget());
  SendKey(ui::VKEY_RETURN, event_generator);
  WaitForSeconds(1);
  CaptureModeTestApi test_api;
  EXPECT_TRUE(test_api.IsInCountDownAnimation());
  SendKey(ui::VKEY_ESCAPE, event_generator);
  EXPECT_FALSE(test_api.IsInCountDownAnimation());
  EXPECT_FALSE(test_api.IsSessionActive());
  EXPECT_FALSE(test_api.IsVideoRecordingInProgress());
}

// Tests that metrics are recorded properly for capture region adjustments.
TEST_F(CaptureModeTest, NumberOfCaptureRegionAdjustmentsHistogram) {
  constexpr char kClamshellHistogram[] =
      "Ash.CaptureModeController.CaptureRegionAdjusted.ClamshellMode";
  constexpr char kTabletHistogram[] =
      "Ash.CaptureModeController.CaptureRegionAdjusted.TabletMode";
  base::HistogramTester histogram_tester;
  UpdateDisplay("800x700");

  auto* controller = StartImageRegionCapture();
  // Create the initial region.
  const gfx::Rect target_region(gfx::Rect(200, 200, 400, 400));
  SelectRegion(target_region);

  auto resize_and_reset_region = [](ui::test::EventGenerator* event_generator,
                                    const gfx::Point& top_right) {
    // Enlarges the region and then resize it back to its original size.
    event_generator->set_current_screen_location(top_right);
    event_generator->DragMouseTo(top_right + gfx::Vector2d(50, 50));
    event_generator->DragMouseTo(top_right);
  };

  auto move_and_reset_region = [](ui::test::EventGenerator* event_generator,
                                  const gfx::Point& drag_point) {
    // Moves the region and then moves it back to its original position.
    event_generator->set_current_screen_location(drag_point);
    event_generator->DragMouseTo(drag_point + gfx::Vector2d(-50, -50));
    event_generator->DragMouseTo(drag_point);
  };

  // Resize the region twice by dragging the top right of the region out and
  // then back again.
  auto* event_generator = GetEventGenerator();
  auto top_right = target_region.top_right();
  resize_and_reset_region(event_generator, top_right);

  // Move the region twice by dragging within the region.
  const gfx::Point drag_point(300, 300);
  move_and_reset_region(event_generator, drag_point);

  // Perform a capture to record the count.
  controller->PerformCapture();
  histogram_tester.ExpectBucketCount(kClamshellHistogram, 4, 1);

  // Create a new image region capture. Move the region twice then change
  // sources to fullscreen and back to region. This toggle should reset the
  // count. Perform a capture to record the count.
  StartImageRegionCapture();
  move_and_reset_region(event_generator, drag_point);
  controller->SetSource(CaptureModeSource::kFullscreen);
  controller->SetSource(CaptureModeSource::kRegion);
  controller->PerformCapture();
  histogram_tester.ExpectBucketCount(kClamshellHistogram, 0, 1);

  // Enter tablet mode and restart the capture session. The capture region
  // should be remembered.
  ash::TabletModeControllerTestApi().EnterTabletMode();
  ASSERT_TRUE(display::Screen::GetScreen()->InTabletMode());
  StartImageRegionCapture();
  ASSERT_EQ(target_region, controller->user_capture_region());

  // Resize the region twice by dragging the top right of the region out and
  // then back again.
  resize_and_reset_region(event_generator, top_right);

  // Move the region twice by dragging within the region.
  move_and_reset_region(event_generator, drag_point);

  // Perform a capture to record the count.
  controller->PerformCapture();
  histogram_tester.ExpectBucketCount(kTabletHistogram, 4, 1);

  // Restart the region capture and resize it. Then create a new region by
  // dragging outside of the existing capture region. This should reset the
  // counter. Change source to record a sample.
  StartImageRegionCapture();
  resize_and_reset_region(event_generator, top_right);
  SelectRegion(gfx::Rect(0, 0, 100, 100));
  controller->PerformCapture();
  histogram_tester.ExpectBucketCount(kTabletHistogram, 0, 1);
}

TEST_F(CaptureModeTest, ResizeRegionBoundedByDisplay) {
  UpdateDisplay("800x700");

  auto* controller = StartImageRegionCapture();
  ASSERT_TRUE(controller->IsActive());
  ASSERT_EQ(CaptureModeSource::kRegion, controller->source());

  // Attempt to create a new region that goes outside of the display bounds.
  gfx::Rect target_region(gfx::Rect(200, 200, 800, 800));
  auto* event_generator = GetEventGenerator();
  event_generator->set_current_screen_location(target_region.origin());
  event_generator->PressLeftButton();
  event_generator->MoveMouseTo(target_region.bottom_right());
  event_generator->ReleaseLeftButton();

  // The region should stay within the display bounds.
  ASSERT_TRUE(
      controller->capture_mode_session()->current_root()->bounds().Contains(
          controller->user_capture_region()));
  EXPECT_EQ(gfx::Rect(200, 200, 600, 500), controller->user_capture_region());

  // Attempt to adjust the existing region outside of the display bounds.
  event_generator->set_current_screen_location(target_region.origin());
  event_generator->PressLeftButton();
  event_generator->MoveMouseTo(gfx::Point(-100, -100));
  event_generator->ReleaseLeftButton();

  // The region should stay within the display bounds.
  ASSERT_TRUE(
      controller->capture_mode_session()->current_root()->bounds().Contains(
          controller->user_capture_region()));
  EXPECT_EQ(gfx::Rect(0, 0, 800, 700), controller->user_capture_region());
}

TEST_F(CaptureModeTest, FullscreenCapture) {
  ui::ScopedAnimationDurationScaleMode animation_scale(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  CaptureModeController* controller = StartCaptureSession(
      CaptureModeSource::kFullscreen, CaptureModeType::kImage);
  EXPECT_TRUE(controller->IsActive());
  // Press anywhere to capture image.
  auto* event_generator = GetEventGenerator();
  event_generator->ClickLeftButton();
  EXPECT_FALSE(controller->IsActive());

  controller = StartCaptureSession(CaptureModeSource::kFullscreen,
                                   CaptureModeType::kVideo);
  EXPECT_TRUE(controller->IsActive());
  // Press anywhere to capture video.
  event_generator->ClickLeftButton();
  WaitForRecordingToStart();
  EXPECT_FALSE(controller->IsActive());
}

// Tests that there is no crash when touching the capture label widget in tablet
// mode when capturing a window. Regression test for https://crbug.com/1152938.
TEST_F(CaptureModeTest, TabletTouchCaptureLabelWidgetWindowMode) {
  SwitchToTabletMode();

  // Enter capture window mode.
  CaptureModeController* controller =
      StartCaptureSession(CaptureModeSource::kWindow, CaptureModeType::kImage);
  ASSERT_TRUE(controller->IsActive());

  // Press and release on where the capture label widget would be.
  auto* event_generator = GetEventGenerator();
  DCHECK(GetCaptureModeLabelWidget());
  event_generator->set_current_screen_location(
      GetCaptureModeLabelWidget()->GetWindowBoundsInScreen().CenterPoint());
  event_generator->PressTouch();
  event_generator->ReleaseTouch();

  // There are no windows and home screen window is excluded from window capture
  // mode, so capture mode will still remain active.
  EXPECT_TRUE(Shell::Get()->app_list_controller()->IsHomeScreenVisible());
  EXPECT_TRUE(controller->IsActive());
}

// Tests that after rotating a display, the capture session widgets are updated
// and the capture region is reset.
TEST_F(CaptureModeTest, DisplayRotation) {
  UpdateDisplay("1200x600");

  auto* controller = StartImageRegionCapture();
  SelectRegion(gfx::Rect(1200, 400));
  OpenSettingsView();

  // Rotate the primary display by 90 degrees. Test that the region, capture
  // bar and capture settings fit within the rotated bounds, and the capture
  // label widget is still centered in the region.
  Shell::Get()->display_manager()->SetDisplayRotation(
      WindowTreeHostManager::GetPrimaryDisplayId(), display::Display::ROTATE_90,
      display::Display::RotationSource::USER);
  const gfx::Rect rotated_root_bounds(600, 1200);
  EXPECT_TRUE(rotated_root_bounds.Contains(controller->user_capture_region()));
  const gfx::Rect capture_bar_bounds =
      GetCaptureModeBarView()->GetBoundsInScreen();
  const gfx::Rect settings_bounds =
      CaptureModeSettingsTestApi().GetSettingsView()->GetBoundsInScreen();
  EXPECT_TRUE(rotated_root_bounds.Contains(capture_bar_bounds));
  EXPECT_TRUE(rotated_root_bounds.Contains(settings_bounds));
  // Verify that the space between the bottom of the settings and the top
  // of the capture bar is `kSpaceBetweenCaptureBarAndSettingsMenu`.
  EXPECT_EQ(capture_bar_bounds.y() - settings_bounds.bottom(),
            capture_mode::kSpaceBetweenCaptureBarAndSettingsMenu);
  views::Widget* capture_label_widget = GetCaptureModeLabelWidget();
  ASSERT_TRUE(capture_label_widget);
  EXPECT_EQ(controller->user_capture_region().CenterPoint(),
            capture_label_widget->GetWindowBoundsInScreen().CenterPoint());
}

TEST_F(CaptureModeTest, DisplayBoundsChange) {
  UpdateDisplay("1200x600");

  auto* controller = StartImageRegionCapture();
  SelectRegion(gfx::Rect(1200, 400));

  // Shrink the display. The capture region should shrink, and the capture bar
  // should be adjusted to be centered.
  UpdateDisplay("700x600");
  EXPECT_EQ(gfx::Rect(700, 400), controller->user_capture_region());
  EXPECT_EQ(350,
            GetCaptureModeBarView()->GetBoundsInScreen().CenterPoint().x());
}

TEST_F(CaptureModeTest, ReenterOnSmallerDisplay) {
  UpdateDisplay("1200x600,1201+0-700x600");

  // Start off with the primary display as the targeted display. Create a region
  // that fits the primary display but would be too big for the secondary
  // display.
  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(gfx::Point(700, 300));
  auto* controller = StartImageRegionCapture();
  SelectRegion(gfx::Rect(1200, 400));
  EXPECT_EQ(gfx::Rect(1200, 400), controller->user_capture_region());
  controller->Stop();

  // Make the secondary display the targeted display. Test that the region has
  // shrunk to fit the display.
  event_generator->MoveMouseTo(gfx::Point(1500, 300));
  StartImageRegionCapture();
  EXPECT_EQ(gfx::Rect(700, 400), controller->user_capture_region());
}

// Tests tabbing when in capture window mode.
TEST_F(CaptureModeTest, KeyboardNavigationBasic) {
  auto* controller =
      StartCaptureSession(CaptureModeSource::kWindow, CaptureModeType::kImage);

  using FocusGroup = CaptureModeSessionFocusCycler::FocusGroup;
  CaptureModeSessionTestApi test_api(controller->capture_mode_session());

  // Initially nothing is focused.
  EXPECT_EQ(FocusGroup::kNone, test_api.GetCurrentFocusGroup());
  EXPECT_EQ(0u, test_api.GetCurrentFocusIndex());

  // Tab once, we are now focusing the type and source buttons group on the
  // capture bar.
  auto* event_generator = GetEventGenerator();
  SendKey(ui::VKEY_TAB, event_generator);
  EXPECT_EQ(FocusGroup::kTypeSource, test_api.GetCurrentFocusGroup());
  EXPECT_EQ(0u, test_api.GetCurrentFocusIndex());

  // Tab four times to focus the last source button (window mode button).
  SendKey(ui::VKEY_TAB, event_generator, ui::EF_NONE, /*count=*/4);
  EXPECT_EQ(FocusGroup::kTypeSource, test_api.GetCurrentFocusGroup());
  EXPECT_EQ(4u, test_api.GetCurrentFocusIndex());

  // Tab once to focus the settings and close buttons group on the capture bar.
  SendKey(ui::VKEY_TAB, event_generator);
  EXPECT_EQ(FocusGroup::kSettingsClose, test_api.GetCurrentFocusGroup());
  EXPECT_EQ(0u, test_api.GetCurrentFocusIndex());

  // Shift tab to focus the last source button again.
  SendKey(ui::VKEY_TAB, event_generator, ui::EF_SHIFT_DOWN);
  EXPECT_EQ(FocusGroup::kTypeSource, test_api.GetCurrentFocusGroup());
  EXPECT_EQ(4u, test_api.GetCurrentFocusIndex());

  // Press esc to clear focus, but remain in capture mode.
  SendKey(ui::VKEY_ESCAPE, event_generator);
  EXPECT_EQ(FocusGroup::kNone, test_api.GetCurrentFocusGroup());
  EXPECT_TRUE(controller->IsActive());

  // Tests that pressing esc when there is no focus will exit capture mode.
  SendKey(ui::VKEY_ESCAPE, event_generator);
  EXPECT_FALSE(controller->IsActive());
}

// Tests tabbing through windows on multiple displays when in capture window
// mode.
TEST_F(CaptureModeTest, KeyboardNavigationTabThroughWindowsOnMultipleDisplays) {
  UpdateDisplay("800x700,801+0-800x700");
  std::vector<raw_ptr<aura::Window, VectorExperimental>> root_windows =
      Shell::GetAllRootWindows();
  ASSERT_EQ(2u, root_windows.size());

  // Create three windows, one of them is a modal transient child.
  std::unique_ptr<aura::Window> window1(
      CreateTestWindow(gfx::Rect(0, 0, 200, 200)));
  auto window1_transient = CreateTransientModalChildWindow(
      gfx::Rect(20, 30, 200, 150), window1.get());
  std::unique_ptr<aura::Window> window2(
      CreateTestWindow(gfx::Rect(900, 0, 200, 200)));

  auto* controller =
      StartCaptureSession(CaptureModeSource::kWindow, CaptureModeType::kImage);
  auto* capture_mode_session =
      static_cast<CaptureModeSession*>(controller->capture_mode_session());
  ASSERT_EQ(capture_mode_session->session_type(), SessionType::kReal);

  using FocusGroup = CaptureModeSessionFocusCycler::FocusGroup;
  CaptureModeSessionTestApi test_api(capture_mode_session);

  // Initially nothing is focused.
  EXPECT_EQ(FocusGroup::kNone, test_api.GetCurrentFocusGroup());
  EXPECT_EQ(0u, test_api.GetCurrentFocusIndex());

  // Tab five times, we are now focusing the window mode button on the
  // capture bar.
  auto* event_generator = GetEventGenerator();
  SendKey(ui::VKEY_TAB, event_generator, ui::EF_NONE, /*count=*/5);
  EXPECT_EQ(FocusGroup::kTypeSource, test_api.GetCurrentFocusGroup());
  EXPECT_EQ(4u, test_api.GetCurrentFocusIndex());

  // Enter space to select window mode.
  SendKey(ui::VKEY_SPACE, event_generator);
  EXPECT_EQ(FocusGroup::kTypeSource, test_api.GetCurrentFocusGroup());
  EXPECT_EQ(CaptureModeSource::kWindow, controller->source());

  // Tab once, we are now focusing |window2| and capture mode bar is on
  // display2.
  SendKey(ui::VKEY_TAB, event_generator);
  EXPECT_EQ(FocusGroup::kCaptureWindow, test_api.GetCurrentFocusGroup());
  EXPECT_EQ(0u, test_api.GetCurrentFocusIndex());
  EXPECT_EQ(window2.get(), capture_mode_session->GetSelectedWindow());
  EXPECT_EQ(root_windows[1], capture_mode_session->current_root());

  // Tab once, we are now focusing |window1_transient|. Since
  // |window1_transient| is on display1, capture mode bar will be moved to
  // display1 as well.
  SendKey(ui::VKEY_TAB, event_generator);
  EXPECT_EQ(FocusGroup::kCaptureWindow, test_api.GetCurrentFocusGroup());
  EXPECT_EQ(1u, test_api.GetCurrentFocusIndex());
  EXPECT_EQ(window1_transient.get(), capture_mode_session->GetSelectedWindow());
  EXPECT_EQ(root_windows[0], capture_mode_session->current_root());

  // Tab once, we are now focusing |window1|. Capture mode bar still stays on
  // display1.
  SendKey(ui::VKEY_TAB, event_generator);
  EXPECT_EQ(FocusGroup::kCaptureWindow, test_api.GetCurrentFocusGroup());
  EXPECT_EQ(2u, test_api.GetCurrentFocusIndex());
  EXPECT_EQ(window1.get(), capture_mode_session->GetSelectedWindow());
  EXPECT_EQ(root_windows[0], capture_mode_session->current_root());

  // Press space, make sure nothing is changed and no crash.
  SendKey(ui::VKEY_SPACE, event_generator);
  EXPECT_TRUE(controller->IsActive());
  EXPECT_EQ(window1.get(), capture_mode_session->GetSelectedWindow());
  EXPECT_EQ(root_windows[0], capture_mode_session->current_root());

  // Tab once to focus the settings and close buttons group on the capture bar.
  SendKey(ui::VKEY_TAB, event_generator);
  EXPECT_EQ(FocusGroup::kSettingsClose, test_api.GetCurrentFocusGroup());
  EXPECT_EQ(0u, test_api.GetCurrentFocusIndex());

  // Shift tab to focus |window1| again.
  SendKey(ui::VKEY_TAB, event_generator, ui::EF_SHIFT_DOWN);
  EXPECT_EQ(FocusGroup::kCaptureWindow, test_api.GetCurrentFocusGroup());
  EXPECT_EQ(2u, test_api.GetCurrentFocusIndex());
  EXPECT_EQ(window1.get(), capture_mode_session->GetSelectedWindow());

  // Shift tab to focus |window1_transient|.
  SendKey(ui::VKEY_TAB, event_generator, ui::EF_SHIFT_DOWN);
  EXPECT_EQ(FocusGroup::kCaptureWindow, test_api.GetCurrentFocusGroup());
  EXPECT_EQ(1u, test_api.GetCurrentFocusIndex());
  EXPECT_EQ(window1_transient.get(), capture_mode_session->GetSelectedWindow());
  EXPECT_EQ(root_windows[0], capture_mode_session->current_root());

  // Shift tab to focus |window2|. Capture mode bar will be moved to display2 as
  // well.
  SendKey(ui::VKEY_TAB, event_generator, ui::EF_SHIFT_DOWN);
  EXPECT_EQ(FocusGroup::kCaptureWindow, test_api.GetCurrentFocusGroup());
  EXPECT_EQ(0u, test_api.GetCurrentFocusIndex());
  EXPECT_EQ(window2.get(), capture_mode_session->GetSelectedWindow());
  EXPECT_EQ(root_windows[1], capture_mode_session->current_root());

  // Press esc to clear focus, but remain in capture mode with |window2|
  // selected.
  SendKey(ui::VKEY_ESCAPE, event_generator);
  EXPECT_EQ(FocusGroup::kNone, test_api.GetCurrentFocusGroup());
  EXPECT_TRUE(controller->IsActive());
  EXPECT_EQ(window2.get(), capture_mode_session->GetSelectedWindow());
  EXPECT_EQ(root_windows[1], capture_mode_session->current_root());

  // Press return. Since there's a selected window, capture mode will
  // be ended after capturing the selected window.
  SendKey(ui::VKEY_RETURN, event_generator);
  EXPECT_FALSE(controller->IsActive());
}

// Tests that a click will remove focus.
TEST_F(CaptureModeTest, KeyboardNavigationClicksRemoveFocus) {
  auto* controller = StartImageRegionCapture();
  auto* event_generator = GetEventGenerator();

  CaptureModeSessionTestApi test_api(controller->capture_mode_session());
  SendKey(ui::VKEY_TAB, event_generator);
  EXPECT_TRUE(test_api.HasFocus());

  event_generator->ClickLeftButton();
  EXPECT_FALSE(test_api.HasFocus());
}

// Tests that pressing space on a focused button will activate it.
TEST_F(CaptureModeTest, KeyboardNavigationSpaceToActivateButton) {
  auto* controller = StartImageRegionCapture();
  SelectRegion(gfx::Rect(200, 200));

  auto* event_generator = GetEventGenerator();

  // Tab to the button which changes the capture type to video and hit space.
  SendKey(ui::VKEY_TAB, event_generator, ui::EF_NONE, /*count=*/2);
  SendKey(ui::VKEY_SPACE, event_generator);
  EXPECT_EQ(CaptureModeType::kVideo, controller->type());

  // Shift tab and space to change the capture type back to image.
  SendKey(ui::VKEY_TAB, event_generator, ui::EF_SHIFT_DOWN);
  SendKey(ui::VKEY_SPACE, event_generator);
  EXPECT_EQ(CaptureModeType::kImage, controller->type());

  // Tab to the fullscreen button and hit space.
  SendKey(ui::VKEY_TAB, event_generator, ui::EF_NONE, /*count=*/2);
  SendKey(ui::VKEY_SPACE, event_generator);
  EXPECT_EQ(CaptureModeSource::kFullscreen, controller->source());

  // Tab to the region button and hit space to return to region capture mode.
  SendKey(ui::VKEY_TAB, event_generator);
  SendKey(ui::VKEY_SPACE, event_generator);
  EXPECT_EQ(CaptureModeSource::kRegion, controller->source());

  // Tab to the capture button and hit space to perform a capture, which exits
  // capture mode.
  SendKey(ui::VKEY_TAB, event_generator, ui::EF_NONE, /*count=*/11);
  SendKey(ui::VKEY_SPACE, event_generator);
  EXPECT_FALSE(controller->IsActive());
}

// Tests that functionality to create and adjust a region with keyboard
// shortcuts works as intended.
TEST_F(CaptureModeTest, KeyboardNavigationSelectRegion) {
  auto* controller = StartImageRegionCapture();
  auto* event_generator = GetEventGenerator();
  ASSERT_TRUE(controller->user_capture_region().IsEmpty());

  // Test that hitting space will create a default region.
  SendKey(ui::VKEY_SPACE, event_generator);
  gfx::Rect capture_region = controller->user_capture_region();
  EXPECT_FALSE(capture_region.IsEmpty());

  // Test that hitting an arrow key will do nothing as the selection region is
  // not focused initially.
  SendKey(ui::VKEY_RIGHT, event_generator);
  EXPECT_EQ(capture_region, controller->user_capture_region());

  const int arrow_shift = capture_mode::kArrowKeyboardRegionChangeDp;

  // Hit tab until the whole region is focused.
  SendKey(ui::VKEY_TAB, event_generator, ui::EF_NONE, /*count=*/6);
  CaptureModeSessionTestApi test_api(controller->capture_mode_session());
  EXPECT_EQ(CaptureModeSessionFocusCycler::FocusGroup::kSelection,
            test_api.GetCurrentFocusGroup());
  EXPECT_EQ(0u, test_api.GetCurrentFocusIndex());

  // Arrow keys should shift the whole region.
  SendKey(ui::VKEY_RIGHT, event_generator);
  EXPECT_EQ(capture_region.origin() + gfx::Vector2d(arrow_shift, 0),
            controller->user_capture_region().origin());
  EXPECT_EQ(capture_region.size(), controller->user_capture_region().size());
  SendKey(ui::VKEY_RIGHT, event_generator, ui::EF_SHIFT_DOWN);
  EXPECT_EQ(
      capture_region.origin() +
          gfx::Vector2d(
              arrow_shift + capture_mode::kShiftArrowKeyboardRegionChangeDp, 0),
      controller->user_capture_region().origin());
  EXPECT_EQ(capture_region.size(), controller->user_capture_region().size());

  // Hit tab so that the top left affordance circle is focused. Left and up keys
  // should enlarge the region, right and bottom keys should shrink the region.
  capture_region = controller->user_capture_region();
  SendKey(ui::VKEY_TAB, event_generator);
  SendKey(ui::VKEY_LEFT, event_generator);
  SendKey(ui::VKEY_UP, event_generator);
  EXPECT_EQ(capture_region.size() + gfx::Size(arrow_shift, arrow_shift),
            controller->user_capture_region().size());
  SendKey(ui::VKEY_RIGHT, event_generator);
  SendKey(ui::VKEY_DOWN, event_generator);
  EXPECT_EQ(capture_region.size(), controller->user_capture_region().size());

  // Tab until we focus the bottom right affordance circle. Left and up keys
  // should shrink the region, right and bottom keys should enlarge the region.
  SendKey(ui::VKEY_TAB, event_generator, ui::EF_NONE, /*count=*/4);

  SendKey(ui::VKEY_LEFT, event_generator);
  SendKey(ui::VKEY_UP, event_generator);
  EXPECT_EQ(capture_region.size() - gfx::Size(arrow_shift, arrow_shift),
            controller->user_capture_region().size());
  SendKey(ui::VKEY_RIGHT, event_generator);
  SendKey(ui::VKEY_DOWN, event_generator);
  EXPECT_EQ(capture_region.size(), controller->user_capture_region().size());
}

// Tests behavior regarding the default region when using keyboard navigation.
TEST_F(CaptureModeTest, KeyboardNavigationDefaultRegion) {
  auto* controller = StartImageRegionCapture();
  auto* event_generator = GetEventGenerator();
  ASSERT_TRUE(controller->user_capture_region().IsEmpty());

  // Hit space when nothing is focused to get the expected default capture
  // region.
  SendKey(ui::VKEY_SPACE, event_generator);
  const gfx::Rect expected_default_region = controller->user_capture_region();
  SelectRegion(gfx::Rect(20, 20, 200, 200));

  // Hit space when there is an existing region. Tests that the region remains
  // unchanged.
  SendKey(ui::VKEY_SPACE, event_generator);
  EXPECT_EQ(gfx::Rect(20, 20, 200, 200), controller->user_capture_region());

  // Tab to the image toggle button. Tests that hitting space does not change
  // the region size.
  SelectRegion(gfx::Rect());
  SendKey(ui::VKEY_TAB, event_generator);
  CaptureModeSessionTestApi test_api(controller->capture_mode_session());
  ASSERT_EQ(CaptureModeSessionFocusCycler::FocusGroup::kTypeSource,
            test_api.GetCurrentFocusGroup());
  ASSERT_EQ(0u, test_api.GetCurrentFocusIndex());

  SendKey(ui::VKEY_SPACE, event_generator);
  EXPECT_EQ(gfx::Rect(), controller->user_capture_region());

  // Tests that hitting space while focusing the region toggle button when in
  // region capture mode will make the capture region the default size.
  // SelectRegion removes focus since it uses mouse clicks.
  SelectRegion(gfx::Rect());
  SendKey(ui::VKEY_TAB, event_generator, ui::EF_NONE,
          /*count=*/4);
  SendKey(ui::VKEY_SPACE, event_generator);
  EXPECT_EQ(expected_default_region, controller->user_capture_region());

  // Tests that hitting space while focusing the region toggle button when not
  // in region capture mode does nothing to the capture region.
  SelectRegion(gfx::Rect());
  ClickOnView(GetWindowToggleButton(), event_generator);
  SendKey(ui::VKEY_TAB, event_generator, ui::EF_NONE,
          /*count=*/4);
  ASSERT_EQ(CaptureModeSessionFocusCycler::FocusGroup::kTypeSource,
            test_api.GetCurrentFocusGroup());
  ASSERT_EQ(3u, test_api.GetCurrentFocusIndex());
  SendKey(ui::VKEY_SPACE, event_generator);
  EXPECT_EQ(gfx::Rect(), controller->user_capture_region());
}

TEST_F(CaptureModeTest, A11yEnterWithNoFocus) {
  auto* controller = StartImageRegionCapture();
  SelectRegion(gfx::Rect(20, 20, 200, 200));
  CaptureModeSessionTestApi test_api(controller->capture_mode_session());
  ASSERT_EQ(CaptureModeSessionFocusCycler::FocusGroup::kNone,
            test_api.GetCurrentFocusGroup());

  // When nothing is focused, the `Enter` key should perform the capture.
  auto* event_generator = GetEventGenerator();
  SendKey(ui::VKEY_RETURN, event_generator);
  EXPECT_FALSE(controller->IsActive());
}

TEST_F(CaptureModeTest, A11yEnterWithFocusOnRegionKnob) {
  auto* controller = StartImageRegionCapture();
  SelectRegion(gfx::Rect(20, 20, 200, 200));
  CaptureModeSessionTestApi test_api(controller->capture_mode_session());

  // Tab until you reach the region adjustment focus group.
  auto* event_generator = GetEventGenerator();
  while (test_api.GetCurrentFocusGroup() !=
         CaptureModeSessionFocusCycler::FocusGroup::kSelection) {
    SendKey(ui::VKEY_TAB, event_generator, ui::EF_NONE);
  }

  // Tab twice more to be on one of the knobs.
  SendKey(ui::VKEY_TAB, event_generator, ui::EF_NONE, /*count=*/2);
  ASSERT_EQ(CaptureModeSessionFocusCycler::FocusGroup::kSelection,
            test_api.GetCurrentFocusGroup());

  // Enter should perform capture.
  SendKey(ui::VKEY_RETURN, event_generator);
  EXPECT_FALSE(controller->IsActive());
}

TEST_F(CaptureModeTest, A11yEnterWithFocusOnWindow) {
  auto window = CreateTestWindow(gfx::Rect(200, 200));
  auto* controller =
      StartCaptureSession(CaptureModeSource::kWindow, CaptureModeType::kImage);

  CaptureModeSessionTestApi test_api(controller->capture_mode_session());

  // Tab until you reach the window to be captured.
  auto* event_generator = GetEventGenerator();
  while (test_api.GetCurrentFocusGroup() !=
         CaptureModeSessionFocusCycler::FocusGroup::kCaptureWindow) {
    SendKey(ui::VKEY_TAB, event_generator, ui::EF_NONE);
  }

  // Enter should perform capture.
  SendKey(ui::VKEY_RETURN, event_generator);
  EXPECT_FALSE(controller->IsActive());
}

TEST_F(CaptureModeTest, A11yEnterWithFocusOnFullscreenButton) {
  auto* controller = StartImageRegionCapture();
  EXPECT_EQ(controller->source(), CaptureModeSource::kRegion);
  CaptureModeSessionTestApi test_api(controller->capture_mode_session());

  // Tab once to enter focus into the bar.
  auto* event_generator = GetEventGenerator();
  SendKey(ui::VKEY_TAB, event_generator, ui::EF_NONE);
  ASSERT_EQ(CaptureModeSessionFocusCycler::FocusGroup::kTypeSource,
            test_api.GetCurrentFocusGroup());

  // Tab until you reach the fullscreen toggle button.
  auto* fullscreen_toggle_button = test_api.GetCaptureModeBarView()
                                       ->GetCaptureSourceView()
                                       ->fullscreen_toggle_button();
  while (test_api.GetCurrentFocusedView()->GetView() !=
         fullscreen_toggle_button) {
    SendKey(ui::VKEY_TAB, event_generator, ui::EF_NONE);
  }

  // The first Enter will switch the source to `kFullscreen`.
  SendKey(ui::VKEY_RETURN, event_generator);
  EXPECT_TRUE(controller->IsActive());
  EXPECT_EQ(controller->source(), CaptureModeSource::kFullscreen);

  // The focus should not change.
  EXPECT_EQ(test_api.GetCurrentFocusedView()->GetView(),
            fullscreen_toggle_button);

  // The second Enter should perform capture.
  SendKey(ui::VKEY_RETURN, event_generator);
  EXPECT_FALSE(controller->IsActive());
}

// Tests that the UAF issue caused by `NotifyAccessibilityEvent` after the
// button been destroyed has been handled without leading to a crash.
TEST_F(CaptureModeTest, KeyboardNavigationButtonDestroyedAfterBeenActivated) {
  auto* controller = StartImageRegionCapture();
  SelectRegion(gfx::Rect(200, 300));

  using FocusGroup = CaptureModeSessionFocusCycler::FocusGroup;
  auto* event_generator = GetEventGenerator();

  // Tab 15 times to reach the capture button and press space key to activate
  // the button.
  SendKey(ui::VKEY_TAB, event_generator, ui::EF_NONE, /*count=*/15);
  EXPECT_EQ(FocusGroup::kCaptureButton,
            CaptureModeSessionTestApi(controller->capture_mode_session())
                .GetCurrentFocusGroup());
  SendKey(ui::VKEY_SPACE, event_generator);
  EXPECT_FALSE(controller->IsActive());

  controller = StartCaptureSession(CaptureModeSource::kFullscreen,
                                   CaptureModeType::kImage);

  // Tab 7 times to reach the close button and press space key to activate the
  // button.
  SendKey(ui::VKEY_TAB, event_generator, ui::EF_NONE, /*count=*/7);
  EXPECT_EQ(FocusGroup::kSettingsClose,
            CaptureModeSessionTestApi(controller->capture_mode_session())
                .GetCurrentFocusGroup());
  SendKey(ui::VKEY_SPACE, event_generator);
  EXPECT_FALSE(controller->IsActive());
}

// Tests that accessibility overrides are set as expected on capture mode
// widgets.
TEST_F(CaptureModeTest, AccessibilityFocusAnnotator) {
  StartImageRegionCapture();

  // Helper that takes in a current widget and checks if the accessibility next
  // and previous focus widgets match the given.
  auto check_a11y_overrides = [](const std::string& id, views::Widget* widget,
                                 views::Widget* expected_previous,
                                 views::Widget* expected_next) -> void {
    SCOPED_TRACE(id);
    views::View* contents_view = widget->GetContentsView();
    views::ViewAccessibility& view_accessibility =
        contents_view->GetViewAccessibility();
    EXPECT_EQ(expected_previous, view_accessibility.GetPreviousWindowFocus());
    EXPECT_EQ(expected_next, view_accessibility.GetNextWindowFocus());
  };

  // With no region, there is no capture label button and no settings menu
  // opened, so the bar is the only focusable capture session widget.
  views::Widget* bar_widget = GetCaptureModeBarWidget();
  check_a11y_overrides("bar", bar_widget, nullptr, nullptr);

  // With a region, the focus should go from the bar widget to the label widget
  // and back.
  SendKey(ui::VKEY_SPACE, GetEventGenerator());
  views::Widget* label_widget = GetCaptureModeLabelWidget();
  check_a11y_overrides("bar", bar_widget, label_widget, label_widget);
  check_a11y_overrides("label", label_widget, bar_widget, bar_widget);

  // With a settings menu open, the focus should go from the bar widget to the
  // label widget to the settings widget and back to the bar widget.
  ClickOnView(GetSettingsButton(), GetEventGenerator());
  views::Widget* settings_widget = GetCaptureModeSettingsWidget();
  ASSERT_TRUE(settings_widget);
  check_a11y_overrides("bar", bar_widget, settings_widget, label_widget);
  check_a11y_overrides("label", label_widget, bar_widget, settings_widget);
  check_a11y_overrides("settings", settings_widget, label_widget, bar_widget);
}

// Tests that a captured image is written to the clipboard.
TEST_F(CaptureModeTest, ClipboardWrite) {
  auto* clipboard = ui::Clipboard::GetForCurrentThread();
  ASSERT_NE(clipboard, nullptr);

  const ui::ClipboardSequenceNumberToken before_sequence_number =
      clipboard->GetSequenceNumber(ui::ClipboardBuffer::kCopyPaste);

  CaptureNotificationWaiter waiter;
  CaptureModeController::Get()->CaptureScreenshotsOfAllDisplays();
  waiter.Wait();

  const ui::ClipboardSequenceNumberToken after_sequence_number =
      clipboard->GetSequenceNumber(ui::ClipboardBuffer::kCopyPaste);

  EXPECT_NE(before_sequence_number, after_sequence_number);
}

// Tests the reverse tabbing behavior of the keyboard navigation.
TEST_F(CaptureModeTest, ReverseTabbingTest) {
  using FocusGroup = CaptureModeSessionFocusCycler::FocusGroup;
  auto* event_generator = GetEventGenerator();
  for (CaptureModeSource source :
       {CaptureModeSource::kFullscreen, CaptureModeSource::kRegion,
        CaptureModeSource::kWindow}) {
    auto* controller = StartCaptureSession(source, CaptureModeType::kImage);
    CaptureModeSessionTestApi test_api(controller->capture_mode_session());
    // Nothing is focused initially.
    EXPECT_EQ(FocusGroup::kNone, test_api.GetCurrentFocusGroup());
    EXPECT_EQ(0u, test_api.GetCurrentFocusIndex());
    // Reverse tabbing once and the focus should be on the close button.
    SendKey(ui::VKEY_TAB, event_generator, ui::EF_SHIFT_DOWN);
    EXPECT_EQ(FocusGroup::kSettingsClose, test_api.GetCurrentFocusGroup());
    EXPECT_TRUE(
        CaptureModeSessionFocusCycler::HighlightHelper::Get(GetCloseButton())
            ->has_focus());
    controller->Stop();
  }
}

// A regression test for a UAF issue reported at https://crbug.com/1350743, in
// which if a the native widget of the settings menu gets deleted without
// calling `Close()` or `CloseNow()` on the widget, we get a UAF. This can
// happen when all the windows in the window tree hierarchy gets deleted e.g.
// when shutting down.
TEST_F(CaptureModeTest, SettingsMenuWidgetDestruction) {
  CaptureModeTestApi().StartForFullscreen(true);
  ClickOnView(GetSettingsButton(), GetEventGenerator());
  auto* widget = GetCaptureModeSettingsWidget();
  ASSERT_TRUE(widget);
  delete widget->GetNativeWindow();
}

// A test class that uses a mock time task environment.
class CaptureModeMockTimeTest : public CaptureModeTest {
 public:
  CaptureModeMockTimeTest()
      : CaptureModeTest(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  CaptureModeMockTimeTest(const CaptureModeMockTimeTest&) = delete;
  CaptureModeMockTimeTest& operator=(const CaptureModeMockTimeTest&) = delete;
  ~CaptureModeMockTimeTest() override = default;
};

// Tests that the consecutive screenshots histogram is recorded properly.
TEST_F(CaptureModeMockTimeTest, ConsecutiveScreenshotsHistograms) {
  constexpr char kConsecutiveScreenshotsHistogram[] =
      "Ash.CaptureModeController.ConsecutiveScreenshots";
  base::HistogramTester histogram_tester;

  auto take_n_screenshots = [this](int n) {
    for (int i = 0; i < n; ++i) {
      auto* controller = StartImageRegionCapture();
      controller->PerformCapture();
    }
  };

  // Take three consecutive screenshots. Should only record after 5 seconds.
  StartImageRegionCapture();
  const gfx::Rect capture_region(200, 200, 400, 400);
  SelectRegion(capture_region);
  take_n_screenshots(3);
  histogram_tester.ExpectBucketCount(kConsecutiveScreenshotsHistogram, 3, 0);
  task_environment()->FastForwardBy(base::Seconds(5));
  histogram_tester.ExpectBucketCount(kConsecutiveScreenshotsHistogram, 3, 1);

  // Take only one screenshot. This should not be recorded.
  take_n_screenshots(1);
  histogram_tester.ExpectBucketCount(kConsecutiveScreenshotsHistogram, 1, 0);
  task_environment()->FastForwardBy(base::Seconds(5));
  histogram_tester.ExpectBucketCount(kConsecutiveScreenshotsHistogram, 1, 0);

  // Take a screenshot, change source and take another screenshot. This should
  // count as 2 consecutive screenshots.
  take_n_screenshots(1);
  auto* controller = StartCaptureSession(CaptureModeSource::kFullscreen,
                                         CaptureModeType::kImage);
  controller->PerformCapture();
  histogram_tester.ExpectBucketCount(kConsecutiveScreenshotsHistogram, 2, 0);
  task_environment()->FastForwardBy(base::Seconds(5));
  histogram_tester.ExpectBucketCount(kConsecutiveScreenshotsHistogram, 2, 1);
}

// Tests that the user capture region will be cleared up after a period of time.
TEST_F(CaptureModeMockTimeTest, ClearUserCaptureRegionBetweenSessions) {
  UpdateDisplay("900x800");
  auto* controller = StartImageRegionCapture();
  EXPECT_EQ(gfx::Rect(), controller->user_capture_region());

  const gfx::Rect capture_region(100, 100, 600, 700);
  SelectRegion(capture_region);
  EXPECT_EQ(capture_region, controller->user_capture_region());
  controller->PerformCapture();
  EXPECT_EQ(capture_region, controller->user_capture_region());

  // Start region image capture again shortly after the previous capture
  // session, we should still be able to reuse the previous capture region.
  task_environment()->FastForwardBy(base::Minutes(1));
  StartImageRegionCapture();
  EXPECT_EQ(capture_region, controller->user_capture_region());
  auto* event_generator = GetEventGenerator();
  // Even if the capture is cancelled, we still remember the capture region.
  SendKey(ui::VKEY_ESCAPE, event_generator);
  EXPECT_EQ(capture_region, controller->user_capture_region());

  // Wait for 8 second and then start region image capture again. We should have
  // forgot the previous capture region.
  task_environment()->FastForwardBy(base::Minutes(8));
  StartImageRegionCapture();
  EXPECT_EQ(gfx::Rect(), controller->user_capture_region());
}

// Tests that in Region mode, the capture bar hides and shows itself correctly.
TEST_F(CaptureModeTest, CaptureBarOpacity) {
  UpdateDisplay("800x700");

  auto* event_generator = GetEventGenerator();
  auto* controller = StartImageRegionCapture();
  EXPECT_TRUE(controller->IsActive());

  ui::Layer* capture_bar_layer = GetCaptureModeBarWidget()->GetLayer();

  // Check to see it starts off opaque.
  EXPECT_EQ(1.f, capture_bar_layer->GetTargetOpacity());

  // Make sure that the bar is transparent when selecting a region.
  const gfx::Rect target_region(gfx::BoundingRect(
      gfx::Point(0, 0),
      GetCaptureModeBarView()->GetBoundsInScreen().top_right() +
          gfx::Vector2d(0, -50)));
  event_generator->MoveMouseTo(target_region.origin());
  event_generator->PressLeftButton();
  EXPECT_EQ(0.f, capture_bar_layer->GetTargetOpacity());
  event_generator->MoveMouseTo(target_region.bottom_right());
  EXPECT_EQ(0.f, capture_bar_layer->GetTargetOpacity());
  event_generator->ReleaseLeftButton();

  // When there is no overlap of the selected region and the bar, the bar should
  // be opaque.
  EXPECT_EQ(1.f, capture_bar_layer->GetTargetOpacity());

  // Bar becomes transparent when the region is being moved.
  event_generator->MoveMouseTo(target_region.origin() + gfx::Vector2d(50, 50));
  event_generator->PressLeftButton();
  EXPECT_EQ(0.f, capture_bar_layer->GetTargetOpacity());
  event_generator->MoveMouseTo(target_region.bottom_center());
  EXPECT_EQ(0.f, capture_bar_layer->GetTargetOpacity());
  event_generator->ReleaseLeftButton();

  // The region overlaps the capture bar, so we set the opacity of the bar to
  // the overlapped opacity.
  EXPECT_EQ(capture_mode::kCaptureUiOverlapOpacity,
            capture_bar_layer->GetTargetOpacity());

  // When there is overlap, the toolbar turns opaque on mouseover.
  event_generator->MoveMouseTo(
      GetCaptureModeBarView()->GetBoundsInScreen().CenterPoint());
  EXPECT_EQ(1.f, capture_bar_layer->GetTargetOpacity());

  // Capture bar drops back to the overlapped opacity when the mouse is no
  // longer hovering.
  event_generator->MoveMouseTo(
      GetCaptureModeBarView()->GetBoundsInScreen().top_center() +
      gfx::Vector2d(0, -50));
  EXPECT_EQ(capture_mode::kCaptureUiOverlapOpacity,
            capture_bar_layer->GetTargetOpacity());

  // Check that the opacity is reset when we select another region.
  SelectRegion(target_region);
  EXPECT_EQ(1.f, capture_bar_layer->GetTargetOpacity());
}

TEST_F(CaptureModeTest, CaptureBarOpacityOnHoveringOnCaptureLabel) {
  UpdateDisplay("800x700");

  auto* event_generator = GetEventGenerator();
  auto* controller = StartImageRegionCapture();
  EXPECT_TRUE(controller->IsActive());
  ui::Layer* capture_bar_layer = GetCaptureModeBarWidget()->GetLayer();

  // Set the capture region to make it overlap with the capture bar. And then
  // move the mouse to the outside of the capture bar, verify it has the
  // overlapped opacity.
  const gfx::Rect capture_region(200, 500, 130, 130);
  SelectRegion(capture_region);
  event_generator->MoveMouseTo({10, 10});
  EXPECT_EQ(capture_mode::kCaptureUiOverlapOpacity,
            capture_bar_layer->GetTargetOpacity());

  // Move mouse on top of the capture label, verify the bar becomes fully
  // opaque.
  event_generator->MoveMouseTo(
      GetCaptureModeLabelWidget()->GetWindowBoundsInScreen().CenterPoint());
  EXPECT_EQ(1.f, capture_bar_layer->GetTargetOpacity());
}

// Tests that the quick action histogram is recorded properly.
TEST_F(CaptureModeTest, QuickActionHistograms) {
  constexpr char kQuickActionHistogramName[] =
      "Ash.CaptureModeController.QuickAction";
  base::HistogramTester histogram_tester;

  auto* controller = StartCaptureSession(CaptureModeSource::kFullscreen,
                                         CaptureModeType::kImage);
  EXPECT_TRUE(controller->IsActive());
  {
    CaptureNotificationWaiter waiter;
    controller->PerformCapture();
    waiter.Wait();
  }
  // Verify clicking delete on screenshot notification.
  base::RunLoop loop;
  SetUpFileDeletionVerifier(&loop);
  const int delete_button = 1;
  ClickOnNotification(delete_button);
  loop.Run();
  EXPECT_FALSE(GetPreviewNotification());
  histogram_tester.ExpectBucketCount(kQuickActionHistogramName,
                                     CaptureQuickAction::kDelete, 1);

  controller = StartCaptureSession(CaptureModeSource::kFullscreen,
                                   CaptureModeType::kImage);
  {
    CaptureNotificationWaiter waiter;
    controller->PerformCapture();
    waiter.Wait();
  }
  // Click on the notification body. This should open the default handler.
  ClickOnNotification(std::nullopt);
  EXPECT_FALSE(GetPreviewNotification());
  histogram_tester.ExpectBucketCount(kQuickActionHistogramName,
                                     CaptureQuickAction::kOpenDefault, 1);

  controller = StartCaptureSession(CaptureModeSource::kFullscreen,
                                   CaptureModeType::kImage);

  {
    CaptureNotificationWaiter waiter;
    controller->PerformCapture();
    waiter.Wait();
  }
  const int edit_button = 0;
  // Verify clicking edit on screenshot notification.
  ClickOnNotification(edit_button);
  EXPECT_FALSE(GetPreviewNotification());
  histogram_tester.ExpectBucketCount(kQuickActionHistogramName,
                                     CaptureQuickAction::kBacklight, 1);
}

TEST_F(CaptureModeTest, NotificationButtonOfVideoRecording) {
  auto* controller = StartCaptureSession(CaptureModeSource::kFullscreen,
                                         CaptureModeType::kVideo);
  StartVideoRecordingImmediately();
  EXPECT_TRUE(controller->is_recording_in_progress());

  CaptureModeTestApi test_api;
  test_api.FlushRecordingServiceForTesting();
  test_api.StopVideoRecording();
  CaptureNotificationWaiter().Wait();
  EXPECT_TRUE(GetPreviewNotification());

  // Verify clicking delete on video recording notification.
  base::RunLoop loop;
  SetUpFileDeletionVerifier(&loop);
  const int delete_button = 0;
  ClickOnNotification(delete_button);
  loop.Run();
  EXPECT_FALSE(GetPreviewNotification());
}

TEST_F(CaptureModeTest, CannotDoMultipleRecordings) {
  StartCaptureSession(CaptureModeSource::kFullscreen, CaptureModeType::kVideo);

  auto* controller = CaptureModeController::Get();
  StartVideoRecordingImmediately();
  EXPECT_TRUE(controller->is_recording_in_progress());
  EXPECT_EQ(CaptureModeType::kVideo, controller->type());

  // Start a new session with the current type which set to kVideo, the type
  // should be switched automatically to kImage, and video toggle button should
  // be disabled.
  controller->Start(CaptureModeEntryType::kQuickSettings);
  EXPECT_TRUE(controller->IsActive());
  EXPECT_EQ(CaptureModeType::kImage, controller->type());
  EXPECT_TRUE(GetImageToggleButton()->selected());
  EXPECT_FALSE(GetVideoToggleButton()->selected());
  EXPECT_FALSE(GetVideoToggleButton()->GetEnabled());

  // Clicking on the video button should do nothing.
  ClickOnView(GetVideoToggleButton(), GetEventGenerator());
  EXPECT_TRUE(GetImageToggleButton()->selected());
  EXPECT_FALSE(GetVideoToggleButton()->selected());
  EXPECT_EQ(CaptureModeType::kImage, controller->type());

  // Things should go back to normal when there's no recording going on and the
  // video file has been fully saved.
  controller->Stop();
  controller->EndVideoRecording(EndRecordingReason::kStopRecordingButton);
  EXPECT_FALSE(controller->can_start_new_recording());
  WaitForCaptureFileToBeSaved();
  EXPECT_TRUE(controller->can_start_new_recording());

  StartCaptureSession(CaptureModeSource::kFullscreen, CaptureModeType::kVideo);
  EXPECT_EQ(CaptureModeType::kVideo, controller->type());
  EXPECT_FALSE(GetImageToggleButton()->selected());
  EXPECT_TRUE(GetVideoToggleButton()->selected());
  EXPECT_TRUE(GetVideoToggleButton()->GetEnabled());
}

// Tests the basic settings menu functionality.
TEST_F(CaptureModeTest, SettingsMenuVisibilityBasic) {
  auto* event_generator = GetEventGenerator();
  auto* controller = StartImageRegionCapture();
  EXPECT_TRUE(controller->IsActive());

  // Session starts with settings menu not initialized.
  EXPECT_FALSE(GetCaptureModeSettingsWidget());

  // Test clicking the settings button toggles the button as well as
  // opens/closes the settings menu.
  ClickOnView(GetSettingsButton(), event_generator);
  EXPECT_TRUE(GetCaptureModeSettingsWidget());
  EXPECT_TRUE(GetSettingsButton()->toggled());
  ClickOnView(GetSettingsButton(), event_generator);
  EXPECT_FALSE(GetCaptureModeSettingsWidget());
  EXPECT_FALSE(GetSettingsButton()->toggled());
}

// Tests how interacting with the rest of the screen (i.e. clicking outside of
// the bar/menu, on other buttons) affects whether the settings menu should
// close or not.
TEST_F(CaptureModeTest, SettingsMenuVisibilityClicking) {
  UpdateDisplay("800x700");

  auto* event_generator = GetEventGenerator();
  auto* controller = StartImageRegionCapture();
  EXPECT_TRUE(controller->IsActive());

  // Test clicking on the option of settings menu doesn't close the
  // settings menu.
  ClickOnView(GetSettingsButton(), event_generator);
  ClickOnView(GetCaptureModeSettingsView(), event_generator);
  EXPECT_TRUE(GetCaptureModeSettingsWidget());
  EXPECT_TRUE(GetSettingsButton()->toggled());
  CaptureModeSettingsTestApi test_api;
  ClickOnView(test_api.GetAudioOffOption(), event_generator);
  EXPECT_TRUE(GetCaptureModeSettingsWidget());
  EXPECT_TRUE(GetSettingsButton()->toggled());

  // Test clicking on the capture bar closes the settings menu.
  event_generator->MoveMouseTo(
      GetCaptureModeBarView()->GetBoundsInScreen().top_center() +
      gfx::Vector2d(0, 2));
  event_generator->ClickLeftButton();
  EXPECT_FALSE(GetCaptureModeSettingsWidget());
  EXPECT_FALSE(GetSettingsButton()->toggled());

  // Test clicking on a different source closes the settings menu.
  ClickOnView(GetSettingsButton(), event_generator);
  ClickOnView(GetFullscreenToggleButton(), event_generator);
  EXPECT_FALSE(GetCaptureModeSettingsWidget());

  // Test clicking on a different type closes the settings menu.
  ClickOnView(GetSettingsButton(), event_generator);
  ClickOnView(GetVideoToggleButton(), event_generator);
  EXPECT_FALSE(GetCaptureModeSettingsWidget());

  // Exit the capture session with the settings menu open, and test to make sure
  // the new session starts with the settings menu hidden.
  ClickOnView(GetSettingsButton(), event_generator);
  SendKey(ui::VKEY_ESCAPE, event_generator);
  StartCaptureSession(CaptureModeSource::kFullscreen, CaptureModeType::kImage);
  EXPECT_FALSE(GetCaptureModeSettingsWidget());

  // Take a screenshot with the settings menu open, and test to make sure the
  // new session starts with the settings menu hidden.
  ClickOnView(GetSettingsButton(), event_generator);
  // Take screenshot.
  SendKey(ui::VKEY_RETURN, event_generator);
  StartImageRegionCapture();
  EXPECT_FALSE(GetCaptureModeSettingsWidget());
}

// Tests capture bar and settings menu visibility / opacity when capture region
// is being or after drawn.
TEST_F(CaptureModeTest, CaptureBarAndSettingsMenuVisibilityDrawingRegion) {
  UpdateDisplay("800x700");

  auto* event_generator = GetEventGenerator();
  auto* controller = StartImageRegionCapture();
  auto* capture_bar_widget = GetCaptureModeBarWidget();
  ui::Layer* capture_bar_layer = capture_bar_widget->GetLayer();
  EXPECT_TRUE(controller->IsActive());
  auto* session =
      static_cast<CaptureModeSession*>(controller->capture_mode_session());
  ASSERT_EQ(session->session_type(), SessionType::kReal);

  // Test the settings menu and capture bar are hidden when the user clicks to
  // start selecting a region.
  ClickOnView(GetSettingsButton(), event_generator);
  EXPECT_TRUE(GetCaptureModeSettingsWidget());
  const gfx::Rect target_region(gfx::BoundingRect(
      gfx::Point(0, 0),
      capture_bar_widget->GetWindowBoundsInScreen().top_right() +
          gfx::Vector2d(0, -50)));
  // Moving the cursor outside the bounds of the settings menu should update the
  // cursor to `kPointer`, since the only possible operation here when clicking
  // is to dismiss the settings menu rather than take a screenshot or update the
  // region.
  event_generator->MoveMouseTo(target_region.origin());
  auto* cursor_manager = Shell::Get()->cursor_manager();
  EXPECT_EQ(CursorType::kPointer, cursor_manager->GetCursor().type());

  // Pressing outside the bounds of the settings should dismiss it immediately,
  // update the cursor to `kCell` (to signal that it's now possible to select a
  // region), but region selection doesn't start until the next click event.
  event_generator->PressLeftButton();
  EXPECT_FALSE(GetCaptureModeSettingsWidget());
  EXPECT_EQ(CursorType::kCell, cursor_manager->GetCursor().type());
  EXPECT_FALSE(session->is_selecting_region());
  event_generator->ReleaseLeftButton();
  EXPECT_FALSE(session->is_selecting_region());

  event_generator->PressLeftButton();
  EXPECT_TRUE(session->is_selecting_region());
  event_generator->MoveMouseTo(target_region.bottom_right());
  EXPECT_EQ(0.f, capture_bar_layer->GetTargetOpacity());
  event_generator->ReleaseLeftButton();
  EXPECT_FALSE(GetCaptureModeSettingsWidget());

  // Test that the settings menu will dismiss immediately when clicking
  // somewhere in the middle of the capture region.
  ClickOnView(GetSettingsButton(), event_generator);
  event_generator->MoveMouseTo(target_region.origin() + gfx::Vector2d(50, 50));
  event_generator->PressLeftButton();
  EXPECT_FALSE(GetCaptureModeSettingsWidget());
  event_generator->ReleaseLeftButton();
  event_generator->PressLeftButton();
  EXPECT_EQ(CursorType::kMove, cursor_manager->GetCursor().type());
  EXPECT_FALSE(session->is_selecting_region());
  EXPECT_TRUE(session->is_drag_in_progress());
  // This creates a region that overlaps with the capture bar. The capture bar
  // should be fully transparent while dragging the region is in progress.
  event_generator->MoveMouseTo(target_region.bottom_center());
  EXPECT_EQ(0.f, capture_bar_layer->GetTargetOpacity());
  event_generator->ReleaseLeftButton();

  // With an overlapping region (as dragged to above), the capture bar opacity
  // is changed based on hover. If the settings menu is open/visible, the
  // capture bar will always be visible no matter if the mouse is hovered on it
  // or not.
  event_generator->MoveMouseTo(target_region.origin());
  EXPECT_EQ(capture_mode::kCaptureUiOverlapOpacity,
            capture_bar_layer->GetTargetOpacity());
  // Move mouse on top of the capture bar, verify that capture bar becomes
  // visible.
  event_generator->MoveMouseTo(
      capture_bar_widget->GetWindowBoundsInScreen().CenterPoint());
  EXPECT_EQ(1.f, capture_bar_layer->GetTargetOpacity());
  ClickOnView(GetSettingsButton(), event_generator);
  EXPECT_TRUE(GetCaptureModeSettingsWidget());
  EXPECT_EQ(1.f, capture_bar_layer->GetTargetOpacity());

  // Move mouse onto the settings menu, confirm the capture bar is still
  // visible.
  event_generator->MoveMouseTo(
      GetCaptureModeSettingsView()->GetBoundsInScreen().CenterPoint());
  EXPECT_EQ(1.f, capture_bar_layer->GetTargetOpacity());

  // Move mouse to the outside of the capture bar and settings, verify that
  // settings menu are still open and both capture bar and settings have full
  // opaque.
  event_generator->MoveMouseTo(target_region.origin());
  auto* settings_menu = GetCaptureModeSettingsView();
  EXPECT_TRUE(settings_menu);
  EXPECT_EQ(1.f, capture_bar_layer->GetTargetOpacity());
  EXPECT_EQ(1.f, settings_menu->layer()->GetTargetOpacity());

  // Close settings menu, and move mouse to the outside of the capture bar,
  // verify capture bar has the overlapped opacity.
  ClickOnView(GetSettingsButton(), event_generator);
  event_generator->MoveMouseTo(target_region.origin());
  EXPECT_EQ(capture_mode::kCaptureUiOverlapOpacity,
            capture_bar_layer->GetTargetOpacity());
}

TEST_F(CaptureModeTest, CaptureFolderSetting) {
  auto* controller = CaptureModeController::Get();
  auto* test_delegate = controller->delegate_for_testing();
  const auto default_downloads_folder =
      test_delegate->GetUserDefaultDownloadsFolder();

  auto capture_folder = controller->GetCurrentCaptureFolder();
  EXPECT_EQ(capture_folder.path, default_downloads_folder);
  EXPECT_TRUE(capture_folder.is_default_downloads_folder);

  const base::FilePath custom_folder(FILE_PATH_LITERAL("/home/tests"));
  controller->SetCustomCaptureFolder(custom_folder);

  capture_folder = controller->GetCurrentCaptureFolder();
  EXPECT_EQ(capture_folder.path, custom_folder);
  EXPECT_FALSE(capture_folder.is_default_downloads_folder);
}

TEST_F(CaptureModeTest, CaptureFolderSetToDefaultDownloads) {
  auto* controller = CaptureModeController::Get();
  auto* test_delegate = controller->delegate_for_testing();

  const base::FilePath custom_folder(FILE_PATH_LITERAL("/home/tests"));
  controller->SetCustomCaptureFolder(custom_folder);
  auto capture_folder = controller->GetCurrentCaptureFolder();
  EXPECT_FALSE(capture_folder.is_default_downloads_folder);

  // If the user selects the default downloads folder manually, we should be
  // able to detect that.
  const auto default_downloads_folder =
      test_delegate->GetUserDefaultDownloadsFolder();
  controller->SetCustomCaptureFolder(default_downloads_folder);

  capture_folder = controller->GetCurrentCaptureFolder();
  EXPECT_EQ(capture_folder.path, default_downloads_folder);
  EXPECT_TRUE(capture_folder.is_default_downloads_folder);
}

TEST_F(CaptureModeTest, UsesDefaultFolderWithCustomFolderSet) {
  auto* controller = CaptureModeController::Get();
  auto* test_delegate = controller->delegate_for_testing();

  const base::FilePath custom_folder(FILE_PATH_LITERAL("/home/tests"));
  controller->SetCustomCaptureFolder(custom_folder);
  auto capture_folder = controller->GetCurrentCaptureFolder();
  EXPECT_FALSE(capture_folder.is_default_downloads_folder);

  // If the user selects to force use the default downloads folder even while
  // a custom folder is set, we should respect that, but we shouldn't clear the
  // custom folder.
  controller->SetUsesDefaultCaptureFolder(true);
  const auto default_downloads_folder =
      test_delegate->GetUserDefaultDownloadsFolder();
  capture_folder = controller->GetCurrentCaptureFolder();
  EXPECT_EQ(capture_folder.path, default_downloads_folder);
  EXPECT_TRUE(capture_folder.is_default_downloads_folder);

  // Setting another custom folder value, would reset the
  // "UsesDefaultCaptureFolder" value, and the new custom folder will be used.
  const base::FilePath custom_folder2(FILE_PATH_LITERAL("/home/tests2"));
  controller->SetCustomCaptureFolder(custom_folder2);
  capture_folder = controller->GetCurrentCaptureFolder();
  EXPECT_EQ(capture_folder.path, custom_folder2);
  EXPECT_FALSE(capture_folder.is_default_downloads_folder);
}

TEST_F(CaptureModeTest, CaptureFolderSetToEmptyPath) {
  auto* controller = CaptureModeController::Get();
  auto* test_delegate = controller->delegate_for_testing();

  const base::FilePath custom_folder(FILE_PATH_LITERAL("/home/tests"));
  controller->SetCustomCaptureFolder(custom_folder);
  auto capture_folder = controller->GetCurrentCaptureFolder();
  EXPECT_FALSE(capture_folder.is_default_downloads_folder);

  // If we set the custom path to an empty folder to clear, we should revert
  // back to the default downloads folder.
  controller->SetCustomCaptureFolder(base::FilePath());

  const auto default_downloads_folder =
      test_delegate->GetUserDefaultDownloadsFolder();
  capture_folder = controller->GetCurrentCaptureFolder();
  EXPECT_EQ(capture_folder.path, default_downloads_folder);
  EXPECT_TRUE(capture_folder.is_default_downloads_folder);
}

TEST_F(CaptureModeTest, SimulateUserCancelingDlpWarningDialog) {
  auto* controller = StartCaptureSession(CaptureModeSource::kFullscreen,
                                         CaptureModeType::kVideo);
  StartVideoRecordingImmediately();
  EXPECT_TRUE(controller->is_recording_in_progress());

  // Simulate the user canceling the DLP warning dialog at the end of the
  // recording which is shown to the user to alert about restricted content
  // showing up on the screen during the recording. In this case, the user
  // requests the deletion of the file.
  auto* test_delegate =
      static_cast<TestCaptureModeDelegate*>(controller->delegate_for_testing());
  test_delegate->set_should_save_after_dlp_check(false);
  base::RunLoop loop;
  SetUpFileDeletionVerifier(&loop);
  controller->EndVideoRecording(EndRecordingReason::kStopRecordingButton);
  loop.Run();
  // No notification should show in this case, nor any thing on Tote.
  EXPECT_FALSE(GetPreviewNotification());
  ash::HoldingSpaceTestApi holding_space_api;
  EXPECT_TRUE(holding_space_api.GetScreenCaptureViews().empty());
  EXPECT_TRUE(controller->can_start_new_recording());
}

// Tests that `CaptureScreenshotOfGivenWindow` can take window screenshot
// successfully and that the image size matches the window size.
TEST_F(CaptureModeTest, InstantScreenshotForkWindow) {
  const gfx::Rect window_bounds(10, 20, 700, 500);
  std::unique_ptr<aura::Window> window(CreateTestWindow(window_bounds));
  CaptureModeController::Get()->CaptureScreenshotOfGivenWindow(window.get());
  const auto file_path = WaitForCaptureFileToBeSaved();
  gfx::Image image = ReadAndDecodeImageFile(file_path);
  EXPECT_EQ(image.Size(), window_bounds.size());
}

// Tests the capture mode behavior in the default capture mode session and
// during video recording.
TEST_F(CaptureModeTest, CaptureModeDefaultBehavior) {
  CaptureModeController* controller = StartCaptureSession(
      CaptureModeSource::kFullscreen, CaptureModeType::kVideo);
  ASSERT_TRUE(controller->IsActive());
  auto* session =
      static_cast<CaptureModeSession*>(controller->capture_mode_session());
  ASSERT_EQ(session->session_type(), SessionType::kReal);
  const CaptureModeBehavior* active_behavior = session->active_behavior();
  ASSERT_TRUE(active_behavior);

  auto expected_behavior = [&]() {
    EXPECT_TRUE(active_behavior->ShouldImageCaptureTypeBeAllowed());
    EXPECT_TRUE(active_behavior->ShouldVideoCaptureTypeBeAllowed());
    EXPECT_TRUE(active_behavior->ShouldFulscreenCaptureSourceBeAllowed());
    EXPECT_TRUE(active_behavior->ShouldRegionCaptureSourceBeAllowed());
    EXPECT_TRUE(active_behavior->ShouldWindowCaptureSourceBeAllowed());
    EXPECT_TRUE(
        active_behavior->SupportsAudioRecordingMode(AudioRecordingMode::kOff));
    EXPECT_TRUE(active_behavior->SupportsAudioRecordingMode(
        AudioRecordingMode::kMicrophone));
    EXPECT_TRUE(active_behavior->ShouldCameraSelectionSettingsBeIncluded());
    EXPECT_TRUE(active_behavior->ShouldDemoToolsSettingsBeIncluded());
    EXPECT_TRUE(active_behavior->ShouldSaveToSettingsBeIncluded());
    EXPECT_TRUE(active_behavior->ShouldGifBeSupported());
    EXPECT_TRUE(active_behavior->ShouldShowPreviewNotification());
    EXPECT_FALSE(active_behavior->ShouldSkipVideoRecordingCountDown());
    EXPECT_FALSE(active_behavior->ShouldCreateAnnotationsOverlayController());
    EXPECT_TRUE(active_behavior->ShouldShowUserNudge());
    EXPECT_FALSE(active_behavior->ShouldAutoSelectFirstCamera());
  };

  expected_behavior();
  views::Widget* bar_widget = GetCaptureModeBarWidget();
  ASSERT_TRUE(bar_widget);

  EXPECT_TRUE(GetImageToggleButton());
  EXPECT_TRUE(GetVideoToggleButton());
  EXPECT_TRUE(GetFullscreenToggleButton());
  EXPECT_TRUE(GetRegionToggleButton());
  EXPECT_TRUE(GetWindowToggleButton());
  EXPECT_FALSE(GetStartRecordingButton());
  EXPECT_TRUE(GetSettingsButton());
  EXPECT_TRUE(GetCloseButton());

  StartVideoRecordingImmediately();
  expected_behavior();
}

// Tests that the capture mode session can be started with the keyboard shortcut
// 'Ctrl + Shift + Overview' with `kImage` as the default type and `kRegion` as
// the default source. And the screen recording can be ended with the keyboard
// shortcut 'Search + Shift + X'.
TEST_F(CaptureModeTest, KeyboardShortcutTest) {
  base::HistogramTester histogram_tester;
  histogram_tester.ExpectBucketCount(
      kEndRecordingReasonInClamshellHistogramName,
      EndRecordingReason::kKeyboardShortcut, 0);

  auto* event_generator = GetEventGenerator();
  event_generator->PressAndReleaseKey(ui::VKEY_MEDIA_LAUNCH_APP1,
                                      ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN);
  auto* controller = CaptureModeController::Get();
  EXPECT_TRUE(controller->IsActive());
  EXPECT_EQ(controller->type(), CaptureModeType::kImage);
  EXPECT_EQ(controller->source(), CaptureModeSource::kRegion);
  controller->SetType(CaptureModeType::kVideo);
  controller->SetSource(CaptureModeSource::kFullscreen);

  StartVideoRecordingImmediately();
  EXPECT_TRUE(controller->is_recording_in_progress());

  event_generator->PressAndReleaseKey(ui::VKEY_X,
                                      ui::EF_SHIFT_DOWN | ui::EF_COMMAND_DOWN);
  EXPECT_FALSE(controller->is_recording_in_progress());
  histogram_tester.ExpectBucketCount(
      kEndRecordingReasonInClamshellHistogramName,
      EndRecordingReason::kKeyboardShortcut, 1);
}

namespace {

// -----------------------------------------------------------------------------
// TestVideoCaptureOverlay:

// Defines a fake video capture overlay to be used in testing the behavior of
// the cursor overlay. The VideoRecordingWatcher will control this overlay via
// mojo.
using Overlay = viz::mojom::FrameSinkVideoCaptureOverlay;
class TestVideoCaptureOverlay : public Overlay {
 public:
  explicit TestVideoCaptureOverlay(mojo::PendingReceiver<Overlay> receiver)
      : receiver_(this, std::move(receiver)) {}
  ~TestVideoCaptureOverlay() override = default;

  const gfx::RectF& last_bounds() const { return last_bounds_; }

  bool IsHidden() const { return last_bounds_ == gfx::RectF(); }

  // viz::mojom::FrameSinkVideoCaptureOverlay:
  void SetImageAndBounds(const SkBitmap& image,
                         const gfx::RectF& bounds) override {
    last_bounds_ = bounds;
  }
  void SetBounds(const gfx::RectF& bounds) override { last_bounds_ = bounds; }
  void OnCapturedMouseEvent(const gfx::Point& coordinates) override {}

 private:
  mojo::Receiver<viz::mojom::FrameSinkVideoCaptureOverlay> receiver_;
  gfx::RectF last_bounds_;
};

// -----------------------------------------------------------------------------
//  CaptureModeCursorOverlayTest:

// Defines a test fixure to test the behavior of the cursor overlay.
class CaptureModeCursorOverlayTest : public CaptureModeTest {
 public:
  CaptureModeCursorOverlayTest() = default;
  ~CaptureModeCursorOverlayTest() override = default;

  aura::Window* window() const { return window_.get(); }
  TestVideoCaptureOverlay* fake_overlay() const { return fake_overlay_.get(); }

  // CaptureModeTest:
  void SetUp() override {
    CaptureModeTest::SetUp();
    window_ = CreateTestWindow(gfx::Rect(200, 200));
  }

  void TearDown() override {
    window_.reset();
    CaptureModeTest::TearDown();
  }

  CaptureModeController* StartRecordingAndSetupFakeOverlay(
      CaptureModeSource source) {
    auto* controller = StartCaptureSession(source, CaptureModeType::kVideo);
    auto* event_generator = GetEventGenerator();
    if (source == CaptureModeSource::kWindow)
      event_generator->MoveMouseToCenterOf(window_.get());
    StartVideoRecordingImmediately();
    EXPECT_TRUE(controller->is_recording_in_progress());
    auto* recording_watcher = controller->video_recording_watcher_for_testing();
    mojo::PendingRemote<Overlay> overlay_pending_remote;
    fake_overlay_ = std::make_unique<TestVideoCaptureOverlay>(
        overlay_pending_remote.InitWithNewPipeAndPassReceiver());
    recording_watcher->BindCursorOverlayForTesting(
        std::move(overlay_pending_remote));

    // The overlay should be initially hidden until a mourse event is received.
    FlushOverlay();
    EXPECT_TRUE(fake_overlay()->IsHidden());

    // Generating some mouse events may or may not show the overlay, depending
    // on the conditions of the test. Each test will verify its expectation
    // after this returns.
    event_generator->MoveMouseBy(10, 10);
    FlushOverlay();

    return controller;
  }

  void FlushOverlay() {
    auto* controller = CaptureModeController::Get();
    DCHECK(controller->is_recording_in_progress());
    controller->video_recording_watcher_for_testing()
        ->FlushCursorOverlayForTesting();
  }

  // The docked magnifier is one of the features that force the software-
  // composited cursor to be used when enabled. We use it to test the behavior
  // of the cursor overlay in that case.
  void SetDockedMagnifierEnabled(bool enabled) {
    Shell::Get()->docked_magnifier_controller()->SetEnabled(enabled);
  }

  // Checks that capturing a screenshot hides the cursor. After the capture is
  // complete, checks that the cursor returns to the previous state, i.e.
  // hidden for tablet mode but visible for clamshell mode.
  void CaptureScreenshotAndCheckCursorVisibility(
      CaptureModeController* controller) {
    EXPECT_EQ(controller->type(), CaptureModeType::kImage);

    auto* cursor_manager = Shell::Get()->cursor_manager();
    bool in_tablet_mode = display::Screen::GetScreen()->InTabletMode();

    // The capture mode session locks the cursor for the whole active session
    // except in the tablet mode unless the cursor is visible.
    EXPECT_EQ(!in_tablet_mode, cursor_manager->IsCursorLocked());
    EXPECT_EQ(!in_tablet_mode, cursor_manager->IsCursorVisible());
    EXPECT_TRUE(controller->IsActive());

    // Make sure the cursor is hidden while capturing the screenshot.
    CaptureNotificationWaiter waiter;
    controller->PerformCapture();
    EXPECT_FALSE(cursor_manager->IsCursorVisible());
    EXPECT_FALSE(controller->IsActive());

    // The cursor visibility should be restored after the capture is done.
    waiter.Wait();
    EXPECT_EQ(!in_tablet_mode, cursor_manager->IsCursorVisible());
    EXPECT_FALSE(cursor_manager->IsCursorLocked());
  }

 private:
  std::unique_ptr<aura::Window> window_;
  std::unique_ptr<TestVideoCaptureOverlay> fake_overlay_;
};

}  // namespace

TEST_F(CaptureModeCursorOverlayTest, TabletModeHidesCursorOverlay) {
  StartRecordingAndSetupFakeOverlay(CaptureModeSource::kFullscreen);
  EXPECT_FALSE(fake_overlay()->IsHidden());

  // Entering tablet mode should hide the cursor overlay.
  SwitchToTabletMode();
  FlushOverlay();
  EXPECT_TRUE(fake_overlay()->IsHidden());

  // Exiting tablet mode should reshow the overlay.
  LeaveTabletMode();
  FlushOverlay();
  EXPECT_FALSE(fake_overlay()->IsHidden());
}

// Tests that the cursor is hidden while taking a screenshot in tablet mode and
// remains hidden afterward.
TEST_F(CaptureModeCursorOverlayTest, TabletModeHidesCursor) {
  // Enter tablet mode.
  SwitchToTabletMode();

  auto* cursor_manager = Shell::Get()->cursor_manager();
  CaptureModeController* controller = StartCaptureSession(
      CaptureModeSource::kFullscreen, CaptureModeType::kImage);

  // Test the hardware cursor.
  CaptureScreenshotAndCheckCursorVisibility(controller);

  // Test the software cursor enabled by docked magnifier.
  SetDockedMagnifierEnabled(true);
  EXPECT_TRUE(IsCursorCompositingEnabled());
  controller = StartCaptureSession(CaptureModeSource::kFullscreen,
                                   CaptureModeType::kImage);
  CaptureScreenshotAndCheckCursorVisibility(controller);

  // Exiting tablet mode.
  LeaveTabletMode();
  EXPECT_TRUE(cursor_manager->IsCursorVisible());
}

// Tests that a cursor is hidden while taking a fullscreen screenshot
// (crbug.com/1186652).
TEST_F(CaptureModeCursorOverlayTest, CursorInFullscreenScreenshot) {
  auto* cursor_manager = Shell::Get()->cursor_manager();
  EXPECT_FALSE(cursor_manager->IsCursorLocked());
  CaptureModeController* controller = StartCaptureSession(
      CaptureModeSource::kFullscreen, CaptureModeType::kImage);
  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(gfx::Point(175, 175));

  // Test the hardware cursor.
  CaptureScreenshotAndCheckCursorVisibility(controller);

  // Test the software cursor enabled by docked magnifier.
  SetDockedMagnifierEnabled(true);
  EXPECT_TRUE(IsCursorCompositingEnabled());
  controller = StartCaptureSession(CaptureModeSource::kFullscreen,
                                   CaptureModeType::kImage);
  CaptureScreenshotAndCheckCursorVisibility(controller);
}

// Tests that a cursor is hidden while taking a region screenshot
// (crbug.com/1186652).
TEST_F(CaptureModeCursorOverlayTest, CursorInPartialRegionScreenshot) {
  // Use a set display size as we will be choosing points in this test.
  UpdateDisplay("800x700");

  auto* cursor_manager = Shell::Get()->cursor_manager();
  EXPECT_FALSE(cursor_manager->IsCursorLocked());
  auto* event_generator = GetEventGenerator();
  auto* controller = StartImageRegionCapture();

  // Create the initial capture region.
  const gfx::Rect target_region(gfx::Rect(50, 50, 200, 200));
  SelectRegion(target_region);
  event_generator->MoveMouseTo(gfx::Point(175, 175));

  // Test the hardware cursor.
  CaptureScreenshotAndCheckCursorVisibility(controller);

  // Test the software cursor enabled by docked magnifier.
  SetDockedMagnifierEnabled(true);
  EXPECT_TRUE(IsCursorCompositingEnabled());
  controller = StartImageRegionCapture();
  CaptureScreenshotAndCheckCursorVisibility(controller);
}

TEST_F(CaptureModeCursorOverlayTest, SoftwareCursorInitiallyEnabled) {
  // The software cursor is enabled before recording starts.
  SetDockedMagnifierEnabled(true);
  EXPECT_TRUE(IsCursorCompositingEnabled());

  // Hence the overlay will be hidden initially.
  StartRecordingAndSetupFakeOverlay(CaptureModeSource::kFullscreen);
  EXPECT_TRUE(fake_overlay()->IsHidden());
}

TEST_F(CaptureModeCursorOverlayTest, SoftwareCursorInFullscreenRecording) {
  StartRecordingAndSetupFakeOverlay(CaptureModeSource::kFullscreen);
  EXPECT_FALSE(fake_overlay()->IsHidden());

  // When the software-composited cursor is enabled, the overlay is hidden to
  // avoid having two overlapping cursors in the video.
  SetDockedMagnifierEnabled(true);
  EXPECT_TRUE(IsCursorCompositingEnabled());
  FlushOverlay();
  EXPECT_TRUE(fake_overlay()->IsHidden());

  SetDockedMagnifierEnabled(false);
  EXPECT_FALSE(IsCursorCompositingEnabled());
  FlushOverlay();
  EXPECT_FALSE(fake_overlay()->IsHidden());
}

TEST_F(CaptureModeCursorOverlayTest, SoftwareCursorInPartialRegionRecording) {
  CaptureModeController::Get()->SetUserCaptureRegion(gfx::Rect(20, 20),
                                                     /*by_user=*/true);
  StartRecordingAndSetupFakeOverlay(CaptureModeSource::kRegion);
  EXPECT_FALSE(fake_overlay()->IsHidden());

  // The behavior in this case is exactly the same as in fullscreen recording.
  SetDockedMagnifierEnabled(true);
  EXPECT_TRUE(IsCursorCompositingEnabled());
  FlushOverlay();
  EXPECT_TRUE(fake_overlay()->IsHidden());
}

TEST_F(CaptureModeCursorOverlayTest, SoftwareCursorInWindowRecording) {
  StartRecordingAndSetupFakeOverlay(CaptureModeSource::kWindow);
  EXPECT_FALSE(fake_overlay()->IsHidden());

  // When recording a window, the software cursor has no effect of the cursor
  // overlay, since the cursor widget is not in the recorded window subtree, so
  // it cannot be captured by the frame sink capturer. We have to provide cursor
  // capturing through the overlay.
  SetDockedMagnifierEnabled(true);
  EXPECT_TRUE(IsCursorCompositingEnabled());
  FlushOverlay();
  EXPECT_FALSE(fake_overlay()->IsHidden());
}

TEST_F(CaptureModeCursorOverlayTest, OverlayHidesWhenOutOfBounds) {
  StartRecordingAndSetupFakeOverlay(CaptureModeSource::kWindow);
  EXPECT_FALSE(fake_overlay()->IsHidden());

  const gfx::Point bottom_right =
      window()->GetBoundsInRootWindow().bottom_right();
  auto* generator = GetEventGenerator();
  // Generate a click event to overcome throttling.
  generator->MoveMouseTo(bottom_right);
  generator->ClickLeftButton();
  FlushOverlay();
  EXPECT_TRUE(fake_overlay()->IsHidden());
}

namespace {

// A CursorShapeClient that always fails to return cursor data.
class FakeCursorShapeClient : public aura::client::CursorShapeClient {
 public:
  FakeCursorShapeClient() = default;
  FakeCursorShapeClient(const FakeCursorShapeClient&) = delete;
  FakeCursorShapeClient& operator=(const FakeCursorShapeClient&) = delete;
  ~FakeCursorShapeClient() override = default;

  // aura::client::CursorShapeClient:
  std::optional<ui::CursorData> GetCursorData(
      const ui::Cursor& cursor) const override {
    return std::nullopt;
  }
};

}  // namespace

TEST_F(CaptureModeCursorOverlayTest, OverlayWhenCursorIsHiddenOrFails) {
  StartRecordingAndSetupFakeOverlay(CaptureModeSource::kWindow);
  EXPECT_FALSE(fake_overlay()->IsHidden());

  // Move cursor, the overlay should update.
  gfx::RectF last_bounds = fake_overlay()->last_bounds();
  auto* generator = GetEventGenerator();
  // Generate a click event to overcome throttling.
  generator->MoveMouseBy(10, 10);
  generator->ClickLeftButton();
  FlushOverlay();
  EXPECT_FALSE(fake_overlay()->IsHidden());
  EXPECT_NE(fake_overlay()->last_bounds(), last_bounds);

  // Hide cursor, the overlay should be empty and hidden.
  auto* cursor_manager = Shell::Get()->cursor_manager();
  cursor_manager->SetCursor(CursorType::kNone);
  // Lock the cursor to prevent mouse events from changing it back.
  cursor_manager->LockCursor();
  generator->MoveMouseBy(10, 10);
  generator->ClickLeftButton();
  FlushOverlay();
  EXPECT_TRUE(fake_overlay()->IsHidden());
  EXPECT_EQ(fake_overlay()->last_bounds(), gfx::RectF());

  // While the cursor is hidden, the overlay shouldn't change.
  generator->MoveMouseBy(10, 10);
  generator->ClickLeftButton();
  FlushOverlay();
  EXPECT_TRUE(fake_overlay()->IsHidden());
  EXPECT_EQ(fake_overlay()->last_bounds(), gfx::RectF());

  // Unhide cursor, the overlay should update.
  cursor_manager->UnlockCursor();
  generator->ClickLeftButton();
  FlushOverlay();
  EXPECT_FALSE(fake_overlay()->IsHidden());
  EXPECT_NE(fake_overlay()->last_bounds(), gfx::RectF());

  // Set a fake cursor shape client so that retrieving the cursor data fails.
  // The overlay shouldn't change.
  FakeCursorShapeClient cursor_shape_client;
  aura::client::SetCursorShapeClient(&cursor_shape_client);
  last_bounds = fake_overlay()->last_bounds();
  generator->MoveMouseBy(10, 10);
  generator->ClickLeftButton();
  FlushOverlay();
  EXPECT_FALSE(fake_overlay()->IsHidden());
  EXPECT_EQ(fake_overlay()->last_bounds(), last_bounds);
}

// Verifies that the cursor overlay bounds calculation takes into account the
// cursor image scale factor. https://crbug.com/1222494.
TEST_F(CaptureModeCursorOverlayTest, OverlayBoundsAccountForCursorScaleFactor) {
  UpdateDisplay("500x400");
  StartRecordingAndSetupFakeOverlay(CaptureModeSource::kFullscreen);
  EXPECT_FALSE(fake_overlay()->IsHidden());

  auto* cursor_manager = Shell::Get()->cursor_manager();
  auto set_cursor = [cursor_manager](const gfx::Size& cursor_image_size,
                                     float cursor_image_scale_factor) {
    SkBitmap cursor_image;
    cursor_image.allocN32Pixels(cursor_image_size.width(),
                                cursor_image_size.height());
    ui::Cursor cursor = ui::Cursor::NewCustom(
        std::move(cursor_image), gfx::Point(), cursor_image_scale_factor);
    cursor.SetPlatformCursor(
        ui::CursorFactory::GetInstance()->CreateImageCursor(
            cursor.type(), cursor.custom_bitmap(), cursor.custom_hotspot(),
            cursor.image_scale_factor()));
    cursor_manager->SetCursor(std::move(cursor));
  };

  struct {
    gfx::Size cursor_size;
    float cursor_image_scale_factor;
  } kTestCases[] = {
      {
          gfx::Size(50, 50),
          /*cursor_image_scale_factor=*/2.f,
      },
      {
          gfx::Size(25, 25),
          /*cursor_image_scale_factor=*/1.f,
      },
  };

  // Both of the above test cases should yield the same cursor overlay relative
  // bounds when the cursor is at the center of the screen.
  // Origin is 0.5f (center)
  // Size is 25 (cursor image dip size) / {500,400} = {0.05f, 0.0625f}
  const gfx::RectF expected_overlay_bounds{0.5f, 0.5f, 0.05f, 0.0625f};

  const gfx::Point screen_center =
      window()->GetRootWindow()->bounds().CenterPoint();
  auto* generator = GetEventGenerator();

  for (const auto& test_case : kTestCases) {
    set_cursor(test_case.cursor_size, test_case.cursor_image_scale_factor);
    // Lock the cursor to prevent mouse events from changing it back to a
    // default kPointer cursor type.
    cursor_manager->LockCursor();

    // Generate a click event to overcome throttling.
    generator->MoveMouseTo(screen_center);
    generator->ClickLeftButton();
    FlushOverlay();
    EXPECT_FALSE(fake_overlay()->IsHidden());
    EXPECT_EQ(expected_overlay_bounds, fake_overlay()->last_bounds());

    // Unlock the cursor back.
    cursor_manager->UnlockCursor();
  }
}

// -----------------------------------------------------------------------------
// TODO(afakhry): Add more cursor overlay tests.

// Test fixture to verify capture mode + projector integration.

namespace {

constexpr char kProjectorCreationFlowHistogramName[] =
    "Ash.Projector.CreationFlow.ClamshellMode";

}  // namespace

class ProjectorCaptureModeIntegrationTests
    : public CaptureModeTest,
      public ::testing::WithParamInterface<CaptureModeSource> {
 public:
  ProjectorCaptureModeIntegrationTests() = default;
  ~ProjectorCaptureModeIntegrationTests() override = default;

  static constexpr gfx::Rect kUserRegion{20, 50, 60, 70};

  MockProjectorClient* projector_client() {
    return projector_helper_.projector_client();
  }
  aura::Window* window() const { return window_.get(); }

  // CaptureModeTest:
  void SetUp() override {
    CaptureModeTest::SetUp();
    projector_helper_.SetUp();
    window_ = CreateTestWindow(gfx::Rect(20, 30, 200, 200));
    CaptureModeController::Get()->SetUserCaptureRegion(kUserRegion,
                                                       /*by_user=*/true);
  }

  void TearDown() override {
    window_.reset();
    CaptureModeTest::TearDown();
  }

  void StartProjectorModeSession() {
    projector_helper_.StartProjectorModeSession();
  }

  void StartRecordingForProjectorFromSource(CaptureModeSource source) {
    StartProjectorModeSession();
    auto* controller = CaptureModeController::Get();
    controller->SetSource(source);

    switch (source) {
      case CaptureModeSource::kFullscreen:
      case CaptureModeSource::kRegion:
        break;
      case CaptureModeSource::kWindow:
        auto* generator = GetEventGenerator();
        generator->MoveMouseTo(window_->GetBoundsInScreen().CenterPoint());
        break;
    }
    CaptureModeTestApi().PerformCapture();
    WaitForRecordingToStart();
    EXPECT_TRUE(controller->is_recording_in_progress());
  }

 protected:
  ProjectorCaptureModeIntegrationHelper projector_helper_;
  std::unique_ptr<aura::Window> window_;
  base::HistogramTester histogram_tester_;
};

// static
constexpr gfx::Rect ProjectorCaptureModeIntegrationTests::kUserRegion;

TEST_F(ProjectorCaptureModeIntegrationTests, EntryPoint) {
  // With the most recent source type set to kImage, starting capture mode for
  // the projector workflow will still force it to kVideo.
  auto* controller = CaptureModeController::Get();
  controller->SetType(CaptureModeType::kImage);
  // Also, audio recording is initially disabled. However, the projector flow
  // forces it enabled.
  EXPECT_EQ(AudioRecordingMode::kOff,
            controller->GetEffectiveAudioRecordingMode());

  StartProjectorModeSession();
  EXPECT_TRUE(controller->IsActive());
  auto* session = controller->capture_mode_session();
  ASSERT_TRUE(session);
  const CaptureModeBehavior* behavior = session->active_behavior();
  ASSERT_TRUE(behavior);
  EXPECT_TRUE(
      behavior->SupportsAudioRecordingMode(AudioRecordingMode::kMicrophone));
  EXPECT_EQ(AudioRecordingMode::kMicrophone,
            controller->GetEffectiveAudioRecordingMode());

  constexpr char kEntryPointHistogram[] =
      "Ash.CaptureModeController.EntryPoint.ClamshellMode";
  histogram_tester_.ExpectBucketCount(kEntryPointHistogram,
                                      CaptureModeEntryType::kProjector, 1);
}

// Tests that a fullscreen screenshot can be taken via the keyboard shortcut
// while a Projector-initiated session is active without ending the session.
TEST_P(ProjectorCaptureModeIntegrationTests, FullscreenScreenshotKeyCombo) {
  StartProjectorModeSession();
  PressAndReleaseKey(ui::VKEY_MEDIA_LAUNCH_APP1, ui::EF_CONTROL_DOWN);
  WaitForCaptureFileToBeSaved();
  auto* controller = CaptureModeController::Get();
  ASSERT_TRUE(controller->IsActive());
  CaptureModeBehavior* active_behavior =
      controller->capture_mode_session()->active_behavior();
  ASSERT_TRUE(active_behavior);
  EXPECT_EQ(active_behavior->behavior_type(), BehaviorType::kProjector);
}

// Tests that the settings view is simplified in projector mode.
TEST_F(ProjectorCaptureModeIntegrationTests, CaptureModeSettings) {
  auto* controller = CaptureModeController::Get();
  StartProjectorModeSession();
  auto* event_generator = GetEventGenerator();

  ClickOnView(GetSettingsButton(), event_generator);

  CaptureModeSettingsTestApi test_api;

  // The "Save-to" menu group should never be added.
  CaptureModeMenuGroup* save_to_menu_group = test_api.GetSaveToMenuGroup();
  EXPECT_FALSE(save_to_menu_group);

  // The audio-off option should never be added.
  EXPECT_FALSE(test_api.GetAudioOffOption());

  CaptureModeMenuGroup* audio_input_menu_group =
      test_api.GetAudioInputMenuGroup();
  EXPECT_TRUE(audio_input_menu_group->IsOptionChecked(kAudioMicrophone));
  EXPECT_EQ(AudioRecordingMode::kMicrophone,
            controller->GetEffectiveAudioRecordingMode());
}

TEST_F(ProjectorCaptureModeIntegrationTests, AudioCaptureDisabledByPolicy) {
  auto* controller = CaptureModeController::Get();
  auto* delegate =
      static_cast<TestCaptureModeDelegate*>(controller->delegate_for_testing());
  delegate->set_is_audio_capture_disabled_by_policy(true);
  EXPECT_EQ(AudioRecordingMode::kOff,
            controller->GetEffectiveAudioRecordingMode());

  // A projector session is not allowed to start when audio recording is
  // disabled by policy.
  EXPECT_FALSE(projector_helper_.CanStartProjectorSession());
}

TEST_F(ProjectorCaptureModeIntegrationTests,
       AudioCaptureDisabledByPolicyAfterSessionStarts) {
  auto* controller = CaptureModeController::Get();
  auto* delegate =
      static_cast<TestCaptureModeDelegate*>(controller->delegate_for_testing());
  EXPECT_EQ(AudioRecordingMode::kOff,
            controller->GetEffectiveAudioRecordingMode());

  // At this point, a Projector session is allowed to begin.
  EXPECT_TRUE(projector_helper_.CanStartProjectorSession());
  StartProjectorModeSession();

  // Flip the audio policy now before recording begins. Attempt to start
  // recording, but expect that the capture mode session will end *without*
  // starting a new recording.
  delegate->set_is_audio_capture_disabled_by_policy(true);
  PressAndReleaseKey(ui::VKEY_RETURN);
  WaitForSessionToEnd();
  EXPECT_FALSE(controller->is_recording_in_progress());

  // The Projector session preconditions should now be up-to-date.
  EXPECT_FALSE(projector_helper_.CanStartProjectorSession());
}

// Tests the keyboard navigation for projector mode. The `image_toggle_button_`
// in `CaptureModeTypeView` and the `Off` audio input option in
// `CaptureModeSettingsView` are not available in projector mode.
TEST_F(ProjectorCaptureModeIntegrationTests, KeyboardNavigationBasic) {
  auto* controller = CaptureModeController::Get();
  // Use `kFullscreen` here to minimize the number of tabs to reach the setting
  // button.
  controller->SetSource(CaptureModeSource::kFullscreen);
  StartProjectorModeSession();

  using FocusGroup = CaptureModeSessionFocusCycler::FocusGroup;
  CaptureModeSessionTestApi test_api(controller->capture_mode_session());

  EXPECT_FALSE(GetImageToggleButton());
  // Tab once, check the current focused view is the video toggle button.
  auto* event_generator = GetEventGenerator();
  SendKey(ui::VKEY_TAB, event_generator);
  EXPECT_EQ(test_api.GetCurrentFocusedView()->GetView(),
            GetVideoToggleButton());

  // Now tab four times to focus the settings button and enter space to open the
  // settings menu.
  SendKey(ui::VKEY_TAB, event_generator, ui::EF_NONE, /*count=*/4);
  SendKey(ui::VKEY_SPACE, event_generator);
  EXPECT_EQ(FocusGroup::kPendingSettings, test_api.GetCurrentFocusGroup());
  CaptureModeSettingsView* settings_menu =
      test_api.GetCaptureModeSettingsView();
  ASSERT_TRUE(settings_menu);

  CaptureModeSettingsTestApi settings_test_api;
  // The `Off` option should not be visible.
  EXPECT_FALSE(settings_test_api.GetAudioOffOption());
  // Tab twice, the current focused view is the `Microphone` option.
  SendKey(ui::VKEY_TAB, event_generator, ui::EF_NONE, /*count=*/2);
  EXPECT_EQ(test_api.GetCurrentFocusedView()->GetView(),
            settings_test_api.GetMicrophoneOption());
}

TEST_F(ProjectorCaptureModeIntegrationTests, BarButtonsState) {
  auto* controller = CaptureModeController::Get();
  StartProjectorModeSession();
  EXPECT_TRUE(controller->IsActive());

  // The image toggle button shouldn't be available, whereas the video toggle
  // button should be enabled and active.
  EXPECT_FALSE(GetImageToggleButton());
  EXPECT_TRUE(GetVideoToggleButton()->GetEnabled());
  EXPECT_TRUE(GetVideoToggleButton()->selected());
}

TEST_F(ProjectorCaptureModeIntegrationTests, StartEndRecording) {
  auto* controller = CaptureModeController::Get();
  controller->SetSource(CaptureModeSource::kFullscreen);
  StartProjectorModeSession();
  EXPECT_TRUE(controller->IsActive());
  histogram_tester_.ExpectUniqueSample(kProjectorCreationFlowHistogramName,
                                       ProjectorCreationFlow::kSessionStarted,
                                       /*expected_bucket_count=*/1);

  // Hit Enter to begin recording. The recording session should be marked for
  // projector.
  PressAndReleaseKey(ui::VKEY_RETURN);
  EXPECT_CALL(*projector_client(), StartSpeechRecognition());
  WaitForRecordingToStart();
  histogram_tester_.ExpectBucketCount(kProjectorCreationFlowHistogramName,
                                      ProjectorCreationFlow::kRecordingStarted,
                                      /*expected_count=*/1);

  EXPECT_FALSE(controller->IsActive());
  EXPECT_TRUE(controller->is_recording_in_progress());
  const CaptureModeBehavior* active_behavior =
      controller->video_recording_watcher_for_testing()->active_behavior();
  ASSERT_TRUE(active_behavior);
  EXPECT_TRUE(active_behavior->ShouldCreateAnnotationsOverlayController());

  EXPECT_CALL(*projector_client(), StopSpeechRecognition());
  controller->EndVideoRecording(EndRecordingReason::kStopRecordingButton);
  WaitForCaptureFileToBeSaved();

  histogram_tester_.ExpectBucketCount(kProjectorCreationFlowHistogramName,
                                      ProjectorCreationFlow::kRecordingEnded,
                                      /*expected_count=*/1);
  histogram_tester_.ExpectBucketCount(kProjectorCreationFlowHistogramName,
                                      ProjectorCreationFlow::kSessionStopped,
                                      /*expected_count=*/1);
  histogram_tester_.ExpectTotalCount(kProjectorCreationFlowHistogramName,
                                     /*expected_count=*/4);
}

TEST_F(ProjectorCaptureModeIntegrationTests,
       ProjectorSessionNeverStartsWhenCaptureModeIsBlocked) {
  auto* controller = CaptureModeController::Get();
  controller->SetSource(CaptureModeSource::kFullscreen);

  auto* test_delegate =
      static_cast<TestCaptureModeDelegate*>(controller->delegate_for_testing());
  test_delegate->set_is_allowed_by_policy(false);
  ProjectorController::Get()->StartProjectorSession(
      base::SafeBaseName::Create("projector_data").value());

  // Both sessions will never start.
  EXPECT_FALSE(controller->IsActive());
  EXPECT_FALSE(ProjectorSession::Get()->is_active());
  EXPECT_FALSE(controller->is_recording_in_progress());
}

TEST_F(ProjectorCaptureModeIntegrationTests,
       ProjectorSessionNeverStartsWhenVideoRecordingIsOnGoing) {
  auto* controller = StartCaptureSession(CaptureModeSource::kFullscreen,
                                         CaptureModeType::kVideo);
  EXPECT_CALL(
      *projector_client(),
      OnNewScreencastPreconditionChanged(NewScreencastPrecondition(
          NewScreencastPreconditionState::kDisabled,
          {NewScreencastPreconditionReason::kScreenRecordingInProgress})));
  controller->StartVideoRecordingImmediatelyForTesting();

  EXPECT_TRUE(controller->is_recording_in_progress());
  EXPECT_FALSE(ProjectorSession::Get()->is_active());
  EXPECT_NE(ProjectorController::Get()->GetNewScreencastPrecondition().state,
            NewScreencastPreconditionState::kEnabled);
  // There is another OnNewScreencastPreconditionChanged() call during tear
  // down.
  EXPECT_CALL(*projector_client(),
              OnNewScreencastPreconditionChanged(NewScreencastPrecondition(
                  NewScreencastPreconditionState::kEnabled,
                  {NewScreencastPreconditionReason::kEnabledBySoda})));
}

// Tests that the capture mode configurations in normal capture mode session
// that include the capture mode type, capture mode source and capture mode
// audio settings will not be overridden by the projector-initiated capture mode
// session.
TEST_F(ProjectorCaptureModeIntegrationTests,
       RestoreCaptureSessionConfigurationsInNormalCaptureSession) {
  // Start an image capture mode session in window mode.
  auto* controller =
      StartCaptureSession(CaptureModeSource::kWindow, CaptureModeType::kImage);

  // Stop the normal capture mode session and start a new projector-initated
  // capture mode session. By default the session will be of type video and in
  // fullscreen mode with audio on.
  controller->Stop();
  StartProjectorModeSession();
  EXPECT_TRUE(controller->IsActive());
  EXPECT_EQ(controller->type(), CaptureModeType::kVideo);
  EXPECT_EQ(controller->source(), CaptureModeSource::kFullscreen);
  EXPECT_EQ(AudioRecordingMode::kMicrophone,
            controller->GetEffectiveAudioRecordingMode());

  // Stop the projector-initiated capture mode session and the original capture
  // mode configurations will be restored.
  controller->Stop();
  EXPECT_EQ(controller->type(), CaptureModeType::kImage);
  EXPECT_EQ(controller->source(), CaptureModeSource::kWindow);
  EXPECT_EQ(AudioRecordingMode::kOff,
            controller->GetEffectiveAudioRecordingMode());

  // Start a new projector-initiated capture mode session and start the region
  // recording.
  StartProjectorModeSession();
  controller->SetSource(CaptureModeSource::kRegion);
  CaptureModeTestApi test_api;
  test_api.SetUserSelectedRegion(gfx::Rect(100, 100, 200, 200));
  test_api.PerformCapture();
  WaitForSeconds(1);

  // Start another capture mode session and the source should be restored as
  // what has been set before the projector-initiated capture mode session.
  controller->Start(CaptureModeEntryType::kQuickSettings);
  EXPECT_EQ(controller->source(), CaptureModeSource::kWindow);
  controller->Stop();
  test_api.StopVideoRecording();

  // After completing the video recording in projector-initiated capture mode
  // session, the capture mode configurations will be restored as what has been
  // set before the projector-initiated capture mode session.
  EXPECT_EQ(controller->type(), CaptureModeType::kImage);
  EXPECT_EQ(AudioRecordingMode::kOff,
            controller->GetEffectiveAudioRecordingMode());
}

namespace {

enum AbortReason {
  kBlockedByDlp,
  kBlockedByPolicy,
  kUserPressedEsc,
};

struct {
  const std::string scope_trace;
  const AbortReason reason;
} kTestCases[] = {
    {"Blocked by DLP", kBlockedByDlp},
    {"Blocked by policy", kBlockedByPolicy},
    {"User Pressed Esc", kUserPressedEsc},
};

}  // namespace

TEST_F(ProjectorCaptureModeIntegrationTests,
       ProjectorSessionAbortedBeforeCountDownStarts) {
  auto* controller = CaptureModeController::Get();
  controller->SetSource(CaptureModeSource::kFullscreen);

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.scope_trace);
    StartProjectorModeSession();
    auto* test_delegate = static_cast<TestCaptureModeDelegate*>(
        controller->delegate_for_testing());

    switch (test_case.reason) {
      case kBlockedByDlp:
        test_delegate->set_is_allowed_by_dlp(false);
        PressAndReleaseKey(ui::VKEY_RETURN);
        break;
      case kBlockedByPolicy:
        test_delegate->set_is_allowed_by_policy(false);
        PressAndReleaseKey(ui::VKEY_RETURN);
        break;
      case kUserPressedEsc:
        PressAndReleaseKey(ui::VKEY_ESCAPE);
        break;
    }

    // The session will end immediately without a count down.
    EXPECT_FALSE(controller->IsActive());
    EXPECT_FALSE(ProjectorSession::Get()->is_active());
    EXPECT_FALSE(controller->is_recording_in_progress());

    // Prepare for next iteration by resetting things back to default.
    test_delegate->ResetAllowancesToDefault();
  }
  histogram_tester_.ExpectBucketCount(kProjectorCreationFlowHistogramName,
                                      ProjectorCreationFlow::kSessionStarted,
                                      /*expected_count=*/3);
  histogram_tester_.ExpectBucketCount(kProjectorCreationFlowHistogramName,
                                      ProjectorCreationFlow::kRecordingAborted,
                                      /*expected_count=*/3);
  histogram_tester_.ExpectBucketCount(kProjectorCreationFlowHistogramName,
                                      ProjectorCreationFlow::kSessionStopped,
                                      /*expected_count=*/3);
  histogram_tester_.ExpectTotalCount(kProjectorCreationFlowHistogramName,
                                     /*expected_count=*/9);
}

TEST_F(ProjectorCaptureModeIntegrationTests,
       ProjectorSessionAbortedAfterCountDownStarts) {
  ui::ScopedAnimationDurationScaleMode animation_scale(
      ui::ScopedAnimationDurationScaleMode::FAST_DURATION);
  auto* controller = CaptureModeController::Get();
  controller->SetSource(CaptureModeSource::kFullscreen);

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.scope_trace);
    StartProjectorModeSession();
    PressAndReleaseKey(ui::VKEY_RETURN);
    auto* test_delegate = static_cast<TestCaptureModeDelegate*>(
        controller->delegate_for_testing());

    switch (test_case.reason) {
      case kBlockedByDlp:
        test_delegate->set_is_allowed_by_dlp(false);
        break;
      case kBlockedByPolicy:
        test_delegate->set_is_allowed_by_policy(false);
        break;
      case kUserPressedEsc:
        PressAndReleaseKey(ui::VKEY_ESCAPE);
        break;
    }

    WaitForSessionToEnd();
    EXPECT_FALSE(ProjectorSession::Get()->is_active());
    EXPECT_FALSE(controller->is_recording_in_progress());

    // Prepare for next iteration by resetting things back to default.
    test_delegate->ResetAllowancesToDefault();
  }

  histogram_tester_.ExpectBucketCount(kProjectorCreationFlowHistogramName,
                                      ProjectorCreationFlow::kSessionStarted,
                                      /*expected_count=*/3);
  histogram_tester_.ExpectBucketCount(kProjectorCreationFlowHistogramName,
                                      ProjectorCreationFlow::kRecordingAborted,
                                      /*expected_count=*/3);
  histogram_tester_.ExpectBucketCount(kProjectorCreationFlowHistogramName,
                                      ProjectorCreationFlow::kSessionStopped,
                                      /*expected_count=*/3);
  histogram_tester_.ExpectTotalCount(kProjectorCreationFlowHistogramName,
                                     /*expected_count=*/9);
}

TEST_F(ProjectorCaptureModeIntegrationTests, AnnotationsOverlayWidget) {
  auto* controller = CaptureModeController::Get();
  controller->SetSource(CaptureModeSource::kFullscreen);
  StartProjectorModeSession();
  EXPECT_TRUE(controller->IsActive());

  PressAndReleaseKey(ui::VKEY_RETURN);
  WaitForRecordingToStart();
  CaptureModeTestApi test_api;
  AnnotationsOverlayController* overlay_controller =
      test_api.GetAnnotationsOverlayController();
  EXPECT_FALSE(overlay_controller->is_enabled());
  auto* overlay_window = overlay_controller->GetOverlayNativeWindow();
  VerifyOverlayEnabledState(overlay_window, /*overlay_enabled_state=*/false);

  auto* annotator_controller = Shell::Get()->annotator_controller();
  annotator_controller->EnableAnnotatorTool();
  EXPECT_TRUE(overlay_controller->is_enabled());
  VerifyOverlayEnabledState(overlay_window, /*overlay_enabled_state=*/true);

  annotator_controller->ResetTools();
  EXPECT_FALSE(overlay_controller->is_enabled());
  VerifyOverlayEnabledState(overlay_window, /*overlay_enabled_state=*/false);
}

TEST_F(ProjectorCaptureModeIntegrationTests,
       AnnotationsOverlayDockedMagnifier) {
  auto* controller = CaptureModeController::Get();
  controller->SetSource(CaptureModeSource::kFullscreen);
  StartProjectorModeSession();
  EXPECT_TRUE(controller->IsActive());

  PressAndReleaseKey(ui::VKEY_RETURN);
  WaitForRecordingToStart();
  CaptureModeTestApi test_api;
  AnnotationsOverlayController* overlay_controller =
      test_api.GetAnnotationsOverlayController();

  auto* annotator_controller = Shell::Get()->annotator_controller();
  annotator_controller->EnableAnnotatorTool();
  EXPECT_TRUE(overlay_controller->is_enabled());
  auto* overlay_window = overlay_controller->GetOverlayNativeWindow();

  // Before the docked magnifier gets enabled, the overlay's bounds should match
  // the root window's bounds.
  auto* root_window = overlay_window->GetRootWindow();
  const gfx::Rect root_window_bounds = root_window->bounds();
  EXPECT_EQ(root_window_bounds, overlay_window->GetBoundsInRootWindow());

  // Once the magnifier is enabled, the overlay should be pushed down so that
  // it doesn't cover the magnifier viewport.
  auto* docked_magnifier = Shell::Get()->docked_magnifier_controller();
  docked_magnifier->SetEnabled(true);
  const gfx::Rect expected_bounds = gfx::SubtractRects(
      root_window_bounds,
      docked_magnifier->GetTotalMagnifierBoundsForRoot(root_window));
  EXPECT_EQ(expected_bounds, overlay_window->GetBoundsInRootWindow());

  // It should go back to original bounds once the magnifier is disabled.
  docked_magnifier->SetEnabled(false);
  EXPECT_EQ(root_window_bounds, overlay_window->GetBoundsInRootWindow());
}

TEST_P(ProjectorCaptureModeIntegrationTests, AnnotationsOverlayWidgetBounds) {
  const auto capture_source = GetParam();
  StartRecordingForProjectorFromSource(capture_source);
  CaptureModeTestApi test_api;
  AnnotationsOverlayController* overlay_controller =
      test_api.GetAnnotationsOverlayController();
  EXPECT_FALSE(overlay_controller->is_enabled());
  auto* overlay_window = overlay_controller->GetOverlayNativeWindow();
  VerifyOverlayWindow(overlay_window, capture_source, kUserRegion);
}

// Regression test for https://crbug.com/1322655.
TEST_P(ProjectorCaptureModeIntegrationTests,
       AnnotationsOverlayWidgetBoundsSecondDisplay) {
  UpdateDisplay("800x700,801+0-800x700");
  const gfx::Point point_in_second_display = gfx::Point(1000, 500);
  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(point_in_second_display);
  window()->SetBoundsInScreen(
      gfx::Rect(900, 0, 600, 500),
      display::Screen::GetScreen()->GetDisplayNearestWindow(
          Shell::GetAllRootWindows()[1]));

  const auto capture_source = GetParam();
  StartRecordingForProjectorFromSource(capture_source);
  const auto roots = Shell::GetAllRootWindows();
  EXPECT_EQ(roots[1], GetWindowBeingRecorded()->GetRootWindow());

  CaptureModeTestApi test_api;
  AnnotationsOverlayController* overlay_controller =
      test_api.GetAnnotationsOverlayController();
  EXPECT_FALSE(overlay_controller->is_enabled());
  auto* overlay_window = overlay_controller->GetOverlayNativeWindow();
  VerifyOverlayWindow(overlay_window, capture_source, kUserRegion);
}

// Tests the projector behavior in the projector-initiated capture mode session
// and during video recording.
TEST_P(ProjectorCaptureModeIntegrationTests, ProjectorBehavior) {
  CaptureModeController* controller = CaptureModeController::Get();
  EXPECT_EQ(AudioRecordingMode::kOff,
            controller->GetEffectiveAudioRecordingMode());
  EXPECT_TRUE(projector_helper_.CanStartProjectorSession());
  StartProjectorModeSession();
  ASSERT_TRUE(controller->IsActive());
  auto* session =
      static_cast<CaptureModeSession*>(controller->capture_mode_session());
  ASSERT_TRUE(session);
  ASSERT_EQ(session->session_type(), SessionType::kReal);
  const CaptureModeBehavior* projector_active_behavior =
      session->active_behavior();
  ASSERT_TRUE(projector_active_behavior);

  auto expected_behavior = [&]() {
    EXPECT_FALSE(projector_active_behavior->ShouldImageCaptureTypeBeAllowed());
    EXPECT_TRUE(projector_active_behavior->ShouldVideoCaptureTypeBeAllowed());
    EXPECT_TRUE(
        projector_active_behavior->ShouldFulscreenCaptureSourceBeAllowed());
    EXPECT_TRUE(
        projector_active_behavior->ShouldRegionCaptureSourceBeAllowed());
    EXPECT_TRUE(
        projector_active_behavior->ShouldWindowCaptureSourceBeAllowed());
    EXPECT_FALSE(projector_active_behavior->SupportsAudioRecordingMode(
        AudioRecordingMode::kOff));
    EXPECT_TRUE(projector_active_behavior->SupportsAudioRecordingMode(
        AudioRecordingMode::kMicrophone));
    EXPECT_TRUE(
        projector_active_behavior->ShouldCameraSelectionSettingsBeIncluded());
    EXPECT_TRUE(projector_active_behavior->ShouldDemoToolsSettingsBeIncluded());
    EXPECT_FALSE(projector_active_behavior->ShouldSaveToSettingsBeIncluded());
    EXPECT_FALSE(projector_active_behavior->ShouldGifBeSupported());
    EXPECT_FALSE(projector_active_behavior->ShouldShowPreviewNotification());
    EXPECT_FALSE(
        projector_active_behavior->ShouldSkipVideoRecordingCountDown());
    EXPECT_TRUE(
        projector_active_behavior->ShouldCreateAnnotationsOverlayController());
    EXPECT_FALSE(projector_active_behavior->ShouldShowUserNudge());
    EXPECT_TRUE(projector_active_behavior->ShouldAutoSelectFirstCamera());
  };

  expected_behavior();
  views::Widget* bar_widget = GetCaptureModeBarWidget();
  ASSERT_TRUE(bar_widget);

  EXPECT_FALSE(GetImageToggleButton());
  EXPECT_TRUE(GetVideoToggleButton());
  EXPECT_TRUE(GetFullscreenToggleButton());
  EXPECT_TRUE(GetRegionToggleButton());
  EXPECT_TRUE(GetWindowToggleButton());
  EXPECT_FALSE(GetStartRecordingButton());
  EXPECT_TRUE(GetSettingsButton());
  EXPECT_TRUE(GetCloseButton());

  auto* annotator_controller = Shell::Get()->annotator_controller();
  annotator_controller->EnableAnnotatorTool();
  PressAndReleaseKey(ui::VKEY_RETURN);
  WaitForRecordingToStart();
  expected_behavior();
  CaptureModeTestApi().StopVideoRecording();
}

// Tests that neither preview notification nor recording in tote is shown if in
// projector mode.
TEST_P(ProjectorCaptureModeIntegrationTests,
       NotShowRecordingInToteOrNotificationForProjectorMode) {
  const auto capture_source = GetParam();
  StartRecordingForProjectorFromSource(capture_source);
  CaptureModeTestApi().StopVideoRecording();
  WaitForCaptureFileToBeSaved();
  EXPECT_FALSE(GetPreviewNotification());
  ash::HoldingSpaceTestApi holding_space_api;
  EXPECT_TRUE(holding_space_api.GetScreenCaptureViews().empty());
}

// Tests that metrics are recorded correctly for capture configuration entering
// from projector in both clamshell and tablet mode.
TEST_P(ProjectorCaptureModeIntegrationTests,
       ProjectorCaptureConfigurationMetrics) {
  const auto capture_source = GetParam();
  constexpr char kProjectorCaptureConfigurationHistogramBase[] =
      "CaptureConfiguration";
  ash::CaptureModeTestApi test_api;

  const bool kTabletEnabledStates[]{false, true};

  for (const bool tablet_enabled : kTabletEnabledStates) {
    if (tablet_enabled) {
      SwitchToTabletMode();
      EXPECT_TRUE(Shell::Get()->IsInTabletMode());
    } else {
      EXPECT_FALSE(Shell::Get()->IsInTabletMode());
    }

    const std::string histogram_name =
        BuildHistogramName(kProjectorCaptureConfigurationHistogramBase,
                           test_api.GetBehavior(BehaviorType::kProjector),
                           /*append_ui_mode_suffix=*/true);
    histogram_tester_.ExpectBucketCount(
        histogram_name,
        GetConfiguration(CaptureModeType::kVideo, capture_source,
                         RecordingType::kWebM),
        0);

    StartRecordingForProjectorFromSource(capture_source);
    WaitForSeconds(1);
    test_api.StopVideoRecording();
    EXPECT_FALSE(CaptureModeController::Get()->is_recording_in_progress());

    histogram_tester_.ExpectUniqueSample(
        histogram_name,
        GetConfiguration(CaptureModeType::kVideo, capture_source,
                         RecordingType::kWebM),
        1);

    WaitForCaptureFileToBeSaved();
  }
}

// Tests that metrics are recorded correctly for screen recording length
// entering from projector in both clamshell and tablet mode.
TEST_P(ProjectorCaptureModeIntegrationTests,
       ProjectorScreenRecordingLengthMetrics) {
  const auto capture_source = GetParam();
  constexpr char kProjectorRecordTimeHistogramBase[] = "ScreenRecordingLength";
  ash::CaptureModeTestApi test_api;

  const bool kTabletEnabledStates[]{false, true};

  for (const bool tablet_enabled : kTabletEnabledStates) {
    if (tablet_enabled) {
      SwitchToTabletMode();
      EXPECT_TRUE(Shell::Get()->IsInTabletMode());
    } else {
      EXPECT_FALSE(Shell::Get()->IsInTabletMode());
    }

    StartRecordingForProjectorFromSource(capture_source);
    WaitForSeconds(1);
    test_api.StopVideoRecording();
    EXPECT_FALSE(CaptureModeController::Get()->is_recording_in_progress());

    WaitForCaptureFileToBeSaved();

    histogram_tester_.ExpectUniqueSample(
        BuildHistogramName(kProjectorRecordTimeHistogramBase,
                           test_api.GetBehavior(BehaviorType::kProjector),
                           /*append_ui_mode_suffix=*/true),
        /*sample=*/1, /*expected_bucket_count=*/1);
  }
}

// Tests that metrics are recorded correctly for capture region adjustment
// entering from projector in both clamshell and tablet mode.
TEST_F(ProjectorCaptureModeIntegrationTests,
       ProjectorCaptureRegionAdjustmentTest) {
  constexpr char kProjectorCaptureRegionAdjustmentHistogramBase[] =
      "CaptureRegionAdjusted";

  auto resize_and_reset_region = [](ui::test::EventGenerator* event_generator,
                                    const gfx::Point& top_right) {
    // Enlarges the region and then resize it back to its original size.
    event_generator->set_current_screen_location(top_right);
    event_generator->DragMouseTo(top_right + gfx::Vector2d(50, 50));
    event_generator->DragMouseTo(top_right);
  };

  auto move_and_reset_region = [](ui::test::EventGenerator* event_generator,
                                  const gfx::Point& drag_point) {
    // Moves the region and then moves it back to its original position.
    event_generator->set_current_screen_location(drag_point);
    event_generator->DragMouseTo(drag_point + gfx::Vector2d(-50, -50));
    event_generator->DragMouseTo(drag_point);
  };

  ash::CaptureModeTestApi test_api;
  const std::string histogram_name =
      BuildHistogramName(kProjectorCaptureRegionAdjustmentHistogramBase,
                         test_api.GetBehavior(BehaviorType::kProjector),
                         /*append_ui_mode_suffix=*/true);
  histogram_tester_.ExpectBucketCount(histogram_name, 0, 0);
  auto* event_generator = GetEventGenerator();
  const gfx::Rect target_region(gfx::Rect(100, 100, 200, 200));
  auto top_right = target_region.top_right();

  const bool kTabletEnabledStates[] = {false, true};
  for (const bool tablet_enabled : kTabletEnabledStates) {
    if (tablet_enabled) {
      SwitchToTabletMode();
      EXPECT_TRUE(Shell::Get()->IsInTabletMode());
    } else {
      EXPECT_FALSE(Shell::Get()->IsInTabletMode());
    }

    StartProjectorModeSession();
    auto* controller = CaptureModeController::Get();
    controller->SetSource(CaptureModeSource::kRegion);
    test_api.SetUserSelectedRegion(target_region);

    // Resize the region twice by dragging the top right of the region out and
    // then back again.
    resize_and_reset_region(event_generator, top_right);

    // Move the region twice by dragging within the region.
    const gfx::Point drag_point(300, 300);
    move_and_reset_region(event_generator, drag_point);

    test_api.PerformCapture();
    WaitForSeconds(1);
    test_api.StopVideoRecording();
    EXPECT_FALSE(controller->is_recording_in_progress());

    histogram_tester_.ExpectBucketCount(histogram_name, 4, 1);

    WaitForCaptureFileToBeSaved();
  }
}

// Tests that if the user is in projector mode, then presses the shortcut to
// start default capture mode, it is ignored.
TEST_P(ProjectorCaptureModeIntegrationTests, SwitchToDefaultCaptureMode) {
  StartProjectorModeSession();
  VerifyActiveBehavior(BehaviorType::kProjector);
  PressAndReleaseKey(ui::VKEY_MEDIA_LAUNCH_APP1,
                     ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN);
  VerifyActiveBehavior(BehaviorType::kProjector);
}

INSTANTIATE_TEST_SUITE_P(All,
                         ProjectorCaptureModeIntegrationTests,
                         testing::Values(CaptureModeSource::kFullscreen,
                                         CaptureModeSource::kRegion,
                                         CaptureModeSource::kWindow));

class AnnotatorCaptureModeIntegrationTests
    : public CaptureModeTest,
      public ::testing::WithParamInterface<CaptureModeSource> {
 public:
  AnnotatorCaptureModeIntegrationTests() = default;
  ~AnnotatorCaptureModeIntegrationTests() override = default;

  static constexpr gfx::Rect kUserRegion{20, 50, 60, 70};

  aura::Window* window() const { return window_.get(); }

  // CaptureModeTest:
  void SetUp() override {
    CaptureModeTest::SetUp();
    annotator_helper_.SetUp();
    window_ = CreateTestWindow(gfx::Rect(20, 30, 200, 200));
    CaptureModeController::Get()->SetUserCaptureRegion(kUserRegion,
                                                       /*by_user=*/true);
  }

  void TearDown() override {
    window_.reset();
    CaptureModeTest::TearDown();
  }

  void StartRecordingFromSource(CaptureModeSource source) {
    ash::CaptureModeTestApi test_api;

    switch (source) {
      case CaptureModeSource::kFullscreen:
        test_api.StartForFullscreen(/*for_video=*/true);
        break;
      case CaptureModeSource::kRegion:
        test_api.StartForRegion(/*for_video=*/true);
        break;
      case CaptureModeSource::kWindow:
        test_api.StartForWindow(/*for_video=*/true);
        auto* generator = GetEventGenerator();
        generator->MoveMouseTo(window_->GetBoundsInScreen().CenterPoint());
        break;
    }
    CaptureModeTestApi().PerformCapture();
    WaitForRecordingToStart();
    EXPECT_TRUE(CaptureModeController::Get()->is_recording_in_progress());
  }

 protected:
  AnnotatorIntegrationHelper annotator_helper_;
  std::unique_ptr<aura::Window> window_;
  base::HistogramTester histogram_tester_;
};

TEST_F(AnnotatorCaptureModeIntegrationTests, AnnotationsOverlayWidget) {
  StartRecordingFromSource(CaptureModeSource::kFullscreen);

  PressAndReleaseKey(ui::VKEY_RETURN);
  WaitForRecordingToStart();
  CaptureModeTestApi test_api;
  AnnotationsOverlayController* overlay_controller =
      test_api.GetAnnotationsOverlayController();
  EXPECT_FALSE(overlay_controller->is_enabled());
  auto* overlay_window = overlay_controller->GetOverlayNativeWindow();
  VerifyOverlayEnabledState(overlay_window, /*overlay_enabled_state=*/false);

  auto* annotator_controller = Shell::Get()->annotator_controller();
  annotator_controller->EnableAnnotatorTool();
  EXPECT_TRUE(overlay_controller->is_enabled());
  VerifyOverlayEnabledState(overlay_window, /*overlay_enabled_state=*/true);

  annotator_controller->ResetTools();
  EXPECT_FALSE(overlay_controller->is_enabled());
  VerifyOverlayEnabledState(overlay_window, /*overlay_enabled_state=*/false);
}

TEST_F(AnnotatorCaptureModeIntegrationTests,
       AnnotationsOverlayDockedMagnifier) {
  StartRecordingFromSource(CaptureModeSource::kFullscreen);

  PressAndReleaseKey(ui::VKEY_RETURN);
  WaitForRecordingToStart();
  CaptureModeTestApi test_api;
  AnnotationsOverlayController* overlay_controller =
      test_api.GetAnnotationsOverlayController();

  auto* annotator_controller = Shell::Get()->annotator_controller();
  annotator_controller->EnableAnnotatorTool();
  EXPECT_TRUE(overlay_controller->is_enabled());
  auto* overlay_window = overlay_controller->GetOverlayNativeWindow();

  // Before the docked magnifier gets enabled, the overlay's bounds should match
  // the root window's bounds.
  auto* root_window = overlay_window->GetRootWindow();
  const gfx::Rect root_window_bounds = root_window->bounds();
  EXPECT_EQ(root_window_bounds, overlay_window->GetBoundsInRootWindow());

  // Once the magnifier is enabled, the overlay should be pushed down so that
  // it doesn't cover the magnifier viewport.
  auto* docked_magnifier = Shell::Get()->docked_magnifier_controller();
  docked_magnifier->SetEnabled(true);
  const gfx::Rect expected_bounds = gfx::SubtractRects(
      root_window_bounds,
      docked_magnifier->GetTotalMagnifierBoundsForRoot(root_window));
  EXPECT_EQ(expected_bounds, overlay_window->GetBoundsInRootWindow());

  // It should go back to original bounds once the magnifier is disabled.
  docked_magnifier->SetEnabled(false);
  EXPECT_EQ(root_window_bounds, overlay_window->GetBoundsInRootWindow());
}

namespace {

// Defines a class that intercepts the events at the post-target handling phase
// and caches the last event target to which the event was routed.
class EventTargetCatcher : public ui::EventHandler {
 public:
  EventTargetCatcher() {
    Shell::GetPrimaryRootWindow()->AddPostTargetHandler(this);
  }
  EventTargetCatcher(const EventTargetCatcher&) = delete;
  EventTargetCatcher& operator=(const EventTargetCatcher&) = delete;
  ~EventTargetCatcher() override {
    Shell::GetPrimaryRootWindow()->RemovePostTargetHandler(this);
  }

  ui::EventTarget* last_event_target() { return last_event_target_; }

  // ui::EventHandler:
  void OnEvent(ui::Event* event) override {
    ui::EventHandler::OnEvent(event);
    last_event_target_ = event->target();
  }

 private:
  raw_ptr<ui::EventTarget> last_event_target_ = nullptr;
};

}  // namespace

TEST_F(AnnotatorCaptureModeIntegrationTests,
       AnnotationsOverlayWidgetTargeting) {
  StartRecordingFromSource(CaptureModeSource::kFullscreen);

  PressAndReleaseKey(ui::VKEY_RETURN);
  WaitForRecordingToStart();
  CaptureModeTestApi test_api;
  AnnotationsOverlayController* overlay_controller =
      test_api.GetAnnotationsOverlayController();

  auto* annotator_controller = Shell::Get()->annotator_controller();
  annotator_controller->EnableAnnotatorTool();
  EXPECT_TRUE(overlay_controller->is_enabled());
  auto* overlay_window = overlay_controller->GetOverlayNativeWindow();
  VerifyOverlayEnabledState(overlay_window, /*overlay_enabled_state=*/true);

  // Open the annotation tray bubble.
  auto* root_window = Shell::GetPrimaryRootWindow();
  auto* status_area_widget =
      RootWindowController::ForWindow(root_window)->GetStatusAreaWidget();
  AnnotationTray* annotations_tray = status_area_widget->annotation_tray();
  annotations_tray->ShowBubble();
  EXPECT_TRUE(annotations_tray->GetBubbleView());

  // Clicking anywhere outside the projector shelf pod should be targeted to the
  // overlay widget window and close the annotation tray bubble.
  EventTargetCatcher event_target_catcher;
  auto* event_generator = GetEventGenerator();
  event_generator->set_current_screen_location(gfx::Point(10, 10));
  event_generator->ClickLeftButton();
  EXPECT_EQ(overlay_window, event_target_catcher.last_event_target());
  EXPECT_FALSE(annotations_tray->GetBubbleView());

  // Now move the mouse over the projector shelf pod, the overlay should not
  // consume the event, and it should instead go through to that pod.
  EXPECT_TRUE(annotations_tray->visible_preferred());
  event_generator->MoveMouseTo(
      annotations_tray->GetBoundsInScreen().CenterPoint());
  EXPECT_EQ(annotations_tray->GetWidget()->GetNativeWindow(),
            event_target_catcher.last_event_target());

  // The overlay status hasn't changed.
  VerifyOverlayEnabledState(overlay_window, /*overlay_enabled_state=*/true);

  // Now move the mouse and then click on the stop recording button, the overlay
  // should not consume the event. The video recording should be ended.
  StopRecordingButtonTray* stop_recording_button =
      status_area_widget->stop_recording_button_tray();
  const gfx::Point stop_button_center_point =
      stop_recording_button->GetBoundsInScreen().CenterPoint();
  event_generator->MoveMouseTo(stop_button_center_point);
  event_generator->ClickLeftButton();
  EXPECT_FALSE(CaptureModeController::Get()->is_recording_in_progress());
}

// Tests that auto hidden shelf can be brought back if user moves mouse to the
// shelf activation area even while annotation is active.
TEST_F(AnnotatorCaptureModeIntegrationTests,
       BringBackAutoHiddenShelfWhileAnnotationIsOn) {
  auto* root_window = Shell::GetPrimaryRootWindow();
  // Set `shelf` to always auto-hidden.
  Shelf* shelf = RootWindowController::ForWindow(root_window)->shelf();
  shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlways);

  StartRecordingFromSource(CaptureModeSource::kFullscreen);

  PressAndReleaseKey(ui::VKEY_RETURN);
  WaitForRecordingToStart();

  auto* event_generator = GetEventGenerator();
  auto* annotator_controller = Shell::Get()->annotator_controller();

  const gfx::Rect root_window_bounds_in_screen =
      root_window->GetBoundsInScreen();
  const int display_width = root_window_bounds_in_screen.width();
  const int display_height = root_window_bounds_in_screen.height();
  const gfx::Point display_center = root_window_bounds_in_screen.CenterPoint();

  struct {
    const std::string scope_trace;
    const ShelfAlignment shelf_alignment;
  } kAlignmentTestCases[] = {
      {"Shelf has botton alignment", ShelfAlignment::kBottom},
      {"Shelf has left alignment", ShelfAlignment::kLeft},
      {"Shelf has right alignment", ShelfAlignment::kRight},
  };

  for (const auto& test_case : kAlignmentTestCases) {
    SCOPED_TRACE(test_case.scope_trace);
    // Enable annotation.
    annotator_controller->EnableAnnotatorTool();

    // Verify shelf is invisible right now.
    EXPECT_FALSE(shelf->IsVisible());

    shelf->SetAlignment(test_case.shelf_alignment);
    switch (test_case.shelf_alignment) {
      case ShelfAlignment::kBottom:
      case ShelfAlignment::kBottomLocked:
        event_generator->MoveMouseTo(0, display_height);
        break;
      case ShelfAlignment::kLeft:
        event_generator->MoveMouseTo(0, display_height);
        break;
      case ShelfAlignment::kRight:
        event_generator->MoveMouseTo(display_width, display_height);
        break;
    }
    // Verify after mouse is moved on top of the shelf activation area, shelf is
    // brought back and visible once the animation to show shelf is finished.
    ShellTestApi().WaitForWindowFinishAnimating(shelf->GetWindow());
    EXPECT_TRUE(shelf->IsVisible());

    // Disable annotation.
    annotator_controller->ResetTools();
    // Move mouse to the outside of the shelf activation area, and wait for the
    // animation to hide shelf to finish.
    event_generator->MoveMouseTo(display_center);
    ShellTestApi().WaitForWindowFinishAnimating(shelf->GetWindow());
  }
}

TEST_P(AnnotatorCaptureModeIntegrationTests, AnnotationsOverlayWidgetBounds) {
  const auto capture_source = GetParam();
  StartRecordingFromSource(capture_source);
  CaptureModeTestApi test_api;
  AnnotationsOverlayController* overlay_controller =
      test_api.GetAnnotationsOverlayController();
  EXPECT_FALSE(overlay_controller->is_enabled());
  auto* overlay_window = overlay_controller->GetOverlayNativeWindow();
  VerifyOverlayWindow(overlay_window, capture_source, kUserRegion);
}

TEST_P(AnnotatorCaptureModeIntegrationTests,
       AnnotationsOverlayWidgetBoundsSecondDisplay) {
  UpdateDisplay("800x700,801+0-800x700");
  const gfx::Point point_in_second_display = gfx::Point(1000, 500);
  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(point_in_second_display);
  window()->SetBoundsInScreen(
      gfx::Rect(900, 0, 600, 500),
      display::Screen::GetScreen()->GetDisplayNearestWindow(
          Shell::GetAllRootWindows()[1]));

  const auto capture_source = GetParam();
  StartRecordingFromSource(capture_source);
  const auto roots = Shell::GetAllRootWindows();
  EXPECT_EQ(roots[1], GetWindowBeingRecorded()->GetRootWindow());

  CaptureModeTestApi test_api;
  AnnotationsOverlayController* overlay_controller =
      test_api.GetAnnotationsOverlayController();
  EXPECT_FALSE(overlay_controller->is_enabled());
  auto* overlay_window = overlay_controller->GetOverlayNativeWindow();
  VerifyOverlayWindow(overlay_window, capture_source, kUserRegion);
}

INSTANTIATE_TEST_SUITE_P(All,
                         AnnotatorCaptureModeIntegrationTests,
                         testing::Values(CaptureModeSource::kFullscreen,
                                         CaptureModeSource::kRegion,
                                         CaptureModeSource::kWindow));

// -----------------------------------------------------------------------------
// CaptureModeSettingsTest:

// Test fixture for CaptureMode settings view.
class CaptureModeSettingsTest : public CaptureModeTest {
 public:
  CaptureModeSettingsTest() = default;
  ~CaptureModeSettingsTest() override = default;

  // CaptureModeTest:
  void SetUp() override {
    CaptureModeTest::SetUp();
    FakeFolderSelectionDialogFactory::Start();
  }

  void TearDown() override {
    FakeFolderSelectionDialogFactory::Stop();
    CaptureModeTest::TearDown();
  }

  CaptureModeSettingsView* GetCaptureModeSettingsView() const {
    auto* session = CaptureModeController::Get()->capture_mode_session();
    DCHECK(session);
    return CaptureModeSessionTestApi(session).GetCaptureModeSettingsView();
  }

  void WaitForSettingsMenuToBeRefreshed() {
    base::RunLoop run_loop;
    CaptureModeSettingsTestApi().SetOnSettingsMenuRefreshedCallback(
        run_loop.QuitClosure());
    run_loop.Run();
  }
};

enum class NudgeDismissalCause {
  kPressSettingsButton,
  kCaptureViaEnterKey,
  kCaptureViaClickOnScreen,
  kCaptureViaLabelButton,
};

// Test fixture to test that various causes that lead to the dismissal of the
// user nudge, they dismiss it forever.
class CaptureModeNudgeDismissalTest
    : public CaptureModeSettingsTest,
      public ::testing::WithParamInterface<NudgeDismissalCause> {
 public:
  // Starts a session appropriate for the test param.
  CaptureModeController* StartSession() {
    switch (GetParam()) {
      case NudgeDismissalCause::kPressSettingsButton:
      case NudgeDismissalCause::kCaptureViaEnterKey:
      case NudgeDismissalCause::kCaptureViaClickOnScreen:
        return StartCaptureSession(CaptureModeSource::kFullscreen,
                                   CaptureModeType::kImage);
      case NudgeDismissalCause::kCaptureViaLabelButton:
        auto* controller = CaptureModeController::Get();
        controller->SetUserCaptureRegion(gfx::Rect(200, 300), /*by_user=*/true);
        StartCaptureSession(CaptureModeSource::kRegion,
                            CaptureModeType::kImage);
        return controller;
    }
  }

  void DoDismissalAction() {
    auto* controller = CaptureModeController::Get();
    auto* event_generator = GetEventGenerator();
    switch (GetParam()) {
      case NudgeDismissalCause::kPressSettingsButton:
        ClickOnView(GetSettingsButton(), event_generator);
        break;
      case NudgeDismissalCause::kCaptureViaEnterKey:
        PressAndReleaseKey(ui::VKEY_RETURN);
        EXPECT_FALSE(controller->IsActive());
        break;
      case NudgeDismissalCause::kCaptureViaClickOnScreen:
        event_generator->MoveMouseToCenterOf(Shell::GetPrimaryRootWindow());
        event_generator->ClickLeftButton();
        EXPECT_FALSE(controller->IsActive());
        break;
      case NudgeDismissalCause::kCaptureViaLabelButton:
        auto* label_button_widget =
            CaptureModeSessionTestApi(controller->capture_mode_session())
                .GetCaptureLabelWidget();
        EXPECT_TRUE(label_button_widget);
        ClickOnView(label_button_widget->GetContentsView(), event_generator);
        break;
    }
  }
};

TEST_P(CaptureModeNudgeDismissalTest, NudgeDismissedForever) {
  auto* controller = StartSession();
  auto* capture_session =
      static_cast<CaptureModeSession*>(controller->capture_mode_session());
  ASSERT_EQ(capture_session->session_type(), SessionType::kReal);
  auto* capture_toast_controller = capture_session->capture_toast_controller();
  auto* nudge_controller = GetUserNudgeController();
  ASSERT_TRUE(nudge_controller);
  EXPECT_TRUE(nudge_controller->is_visible());
  EXPECT_TRUE(capture_toast_controller->capture_toast_widget());
  ASSERT_TRUE(capture_toast_controller->current_toast_type());
  EXPECT_EQ(*(capture_toast_controller->current_toast_type()),
            CaptureToastType::kUserNudge);

  // Trigger the action that dismisses the nudge forever, it should be removed
  // in this session (if the action doesn't stop the session) and any future
  // sessions.
  DoDismissalAction();
  if (controller->IsActive()) {
    EXPECT_FALSE(GetUserNudgeController());
    // Close the session in preparation for opening a new one.
    controller->Stop();
  }

  // Reopen a new session, the nudge should not show anymore.
  StartImageRegionCapture();
  EXPECT_FALSE(GetUserNudgeController());
}

INSTANTIATE_TEST_SUITE_P(
    All,
    CaptureModeNudgeDismissalTest,
    testing::Values(NudgeDismissalCause::kPressSettingsButton,
                    NudgeDismissalCause::kCaptureViaEnterKey,
                    NudgeDismissalCause::kCaptureViaClickOnScreen,
                    NudgeDismissalCause::kCaptureViaLabelButton));

TEST_F(CaptureModeSettingsTest, NudgeChangesRootWithBar) {
  UpdateDisplay("800x700,801+0-800x700");

  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(gfx::Point(100, 500));

  auto* controller = StartCaptureSession(CaptureModeSource::kFullscreen,
                                         CaptureModeType::kImage);
  auto* session =
      static_cast<CaptureModeSession*>(controller->capture_mode_session());
  ASSERT_EQ(session->session_type(), SessionType::kReal);
  auto* capture_toast_controller = session->capture_toast_controller();

  EXPECT_EQ(Shell::GetAllRootWindows()[0], session->current_root());
  EXPECT_EQ(capture_toast_controller->capture_toast_widget()
                ->GetNativeWindow()
                ->GetRootWindow(),
            session->current_root());

  event_generator->MoveMouseTo(gfx::Point(1000, 500));
  EXPECT_EQ(Shell::GetAllRootWindows()[1], session->current_root());
  EXPECT_EQ(capture_toast_controller->capture_toast_widget()
                ->GetNativeWindow()
                ->GetRootWindow(),
            session->current_root());
}

TEST_F(CaptureModeSettingsTest, NudgeBehaviorWhenSelectingRegion) {
  UpdateDisplay("800x700,801+0-800x700");

  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(gfx::Point(100, 500));

  auto* controller = StartImageRegionCapture();
  auto* session =
      static_cast<CaptureModeSession*>(controller->capture_mode_session());
  ASSERT_EQ(session->session_type(), SessionType::kReal);
  EXPECT_EQ(Shell::GetAllRootWindows()[0], session->current_root());

  // Nudge hides while selecting a region, but doesn't change roots until the
  // region change is committed.
  auto* nudge_controller = GetUserNudgeController();
  event_generator->MoveMouseTo(gfx::Point(1000, 500));
  event_generator->PressLeftButton();
  EXPECT_FALSE(nudge_controller->is_visible());
  event_generator->MoveMouseBy(50, 60);
  event_generator->ReleaseLeftButton();
  EXPECT_EQ(Shell::GetAllRootWindows()[1], session->current_root());

  // The nudge shows again, and is on the second display.
  EXPECT_TRUE(nudge_controller->is_visible());
  EXPECT_EQ(session->capture_toast_controller()
                ->capture_toast_widget()
                ->GetNativeWindow()
                ->GetRootWindow(),
            session->current_root());
}

TEST_F(CaptureModeSettingsTest, NudgeDoesNotShowForAllUserTypes) {
  struct {
    std::string trace;
    user_manager::UserType user_type;
    bool can_see_nudge;
  } kUserTypeTestCases[] = {
      {"regular user", user_manager::UserType::kRegular, true},
      {"child", user_manager::UserType::kChild, true},
      {"guest", user_manager::UserType::kGuest, false},
      {"public account", user_manager::UserType::kPublicAccount, false},
      {"kiosk app", user_manager::UserType::kKioskApp, false},
      {"web kiosk app", user_manager::UserType::kWebKioskApp, false},
  };

  for (const auto& test_case : kUserTypeTestCases) {
    SCOPED_TRACE(test_case.trace);
    ClearLogin();
    SimulateUserLogin("example@gmail.com", test_case.user_type);

    auto* controller = StartImageRegionCapture();
    EXPECT_EQ(test_case.can_see_nudge, controller->CanShowUserNudge());

    auto* nudge_controller = GetUserNudgeController();
    EXPECT_EQ(test_case.can_see_nudge, !!nudge_controller);

    controller->Stop();
  }
}

// Tests that the capture mode settings menu is centered with respect to the
// capture bar.
TEST_F(CaptureModeSettingsTest, SettingsMenuCenteredWithCaptureBar) {
  StartCaptureSession(CaptureModeSource::kFullscreen, CaptureModeType::kImage);
  auto* bar_widget = GetCaptureModeBarWidget();
  ASSERT_TRUE(bar_widget);
  ClickOnView(GetSettingsButton(), GetEventGenerator());
  auto* settings_widget = GetCaptureModeSettingsWidget();
  ASSERT_TRUE(settings_widget);
  EXPECT_NEAR(settings_widget->GetWindowBoundsInScreen().CenterPoint().x(),
              bar_widget->GetWindowBoundsInScreen().CenterPoint().x(),
              /*abs_error=*/1);
}

// Tests that it's possbile to take a screenshot using the keyboard shortcut at
// the login screen without any crashes. https://crbug.com/1266728.
TEST_F(CaptureModeSettingsTest, TakeScreenshotAtLoginScreen) {
  ClearLogin();
  PressAndReleaseKey(ui::VKEY_MEDIA_LAUNCH_APP1, ui::EF_CONTROL_DOWN);
  WaitForCaptureFileToBeSaved();
}

// Tests that clicking on audio input buttons updates the state in the
// controller, and persists between sessions.
TEST_F(CaptureModeSettingsTest, AudioInputSettingsMenu) {
  auto* controller = StartImageRegionCapture();
  auto* event_generator = GetEventGenerator();

  // Test that the audio recording preference is defaulted to off.
  ClickOnView(GetSettingsButton(), event_generator);
  EXPECT_EQ(AudioRecordingMode::kOff,
            controller->GetEffectiveAudioRecordingMode());

  CaptureModeSettingsTestApi test_api;
  CaptureModeMenuGroup* audio_input_menu_group =
      test_api.GetAudioInputMenuGroup();
  EXPECT_TRUE(audio_input_menu_group->IsOptionChecked(kAudioOff));
  EXPECT_FALSE(audio_input_menu_group->IsOptionChecked(kAudioMicrophone));

  // Click on the |microphone| option. It should be checked after click along
  // with |off| is unchecked. Recording preference is set to microphone.
  views::View* microphone_option = test_api.GetMicrophoneOption();
  ClickOnView(microphone_option, event_generator);
  EXPECT_TRUE(audio_input_menu_group->IsOptionChecked(kAudioMicrophone));
  EXPECT_FALSE(audio_input_menu_group->IsOptionChecked(kAudioOff));
  EXPECT_EQ(AudioRecordingMode::kMicrophone,
            controller->GetEffectiveAudioRecordingMode());

  // Test that the user selected audio preference for audio recording is
  // remembered between sessions.
  SendKey(ui::VKEY_ESCAPE, event_generator);
  StartImageRegionCapture();
  EXPECT_EQ(AudioRecordingMode::kMicrophone,
            controller->GetEffectiveAudioRecordingMode());
}

TEST_F(CaptureModeSettingsTest, AccessibleName) {
  StartImageRegionCapture();
  ClickOnView(GetSettingsButton(), GetEventGenerator());
  CaptureModeSettingsTestApi test_api;

  CaptureModeMenuGroup* audio_input_menu_group =
      test_api.GetAudioInputMenuGroup();
  views::View* menu_header_view = audio_input_menu_group->menu_header();
  ui::AXNodeData data;
  menu_header_view->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_ASH_SCREEN_CAPTURE_AUDIO_INPUT),
            data.GetString16Attribute(ax::mojom::StringAttribute::kName));
}

TEST_F(CaptureModeSettingsTest, AudioCaptureDisabledByPolicy) {
  auto* controller = CaptureModeController::Get();

  // Even if audio recording is set to enabled, the policy setting will
  // overwrite it.
  controller->SetAudioRecordingMode(AudioRecordingMode::kMicrophone);
  auto* delegate =
      static_cast<TestCaptureModeDelegate*>(controller->delegate_for_testing());
  delegate->set_is_audio_capture_disabled_by_policy(true);
  EXPECT_EQ(AudioRecordingMode::kOff,
            controller->GetEffectiveAudioRecordingMode());

  StartImageRegionCapture();

  // Open the settings menu, and check that "Audio Off" setting is dimmed out,
  // and the "Microphone" setting was not added. This menu group should be
  // marked as "managed by policy".
  ClickOnView(GetSettingsButton(), GetEventGenerator());
  CaptureModeSettingsTestApi test_api;
  CaptureModeMenuGroup* audio_input_menu_group =
      test_api.GetAudioInputMenuGroup();
  EXPECT_TRUE(audio_input_menu_group->IsManagedByPolicy());
  EXPECT_TRUE(audio_input_menu_group->IsOptionChecked(kAudioOff));
  EXPECT_FALSE(audio_input_menu_group->IsOptionEnabled(kAudioOff));
  EXPECT_FALSE(test_api.GetMicrophoneOption());
}

TEST_F(CaptureModeSettingsTest, SelectFolderFromDialog) {
  auto* controller = StartImageRegionCapture();
  auto* event_generator = GetEventGenerator();
  ClickOnView(GetSettingsButton(), event_generator);

  // Initially there should only be an option for the default downloads folder.
  CaptureModeSettingsTestApi test_api;
  EXPECT_TRUE(test_api.GetDefaultDownloadsOption());
  EXPECT_FALSE(test_api.GetCustomFolderOptionIfAny());
  CaptureModeMenuGroup* save_to_menu_group = test_api.GetSaveToMenuGroup();
  EXPECT_TRUE(save_to_menu_group->IsOptionChecked(kDownloadsFolder));

  ClickOnView(test_api.GetSelectFolderMenuItem(), event_generator);
  EXPECT_TRUE(IsFolderSelectionDialogShown());
  EXPECT_FALSE(AreAllCaptureSessionUisVisible());

  auto* dialog_factory = FakeFolderSelectionDialogFactory::Get();
  auto* dialog_window = dialog_factory->GetDialogWindow();
  auto* window_state = WindowState::Get(dialog_window);
  ASSERT_TRUE(window_state);
  EXPECT_FALSE(window_state->CanMaximize());
  EXPECT_FALSE(window_state->CanMinimize());
  EXPECT_FALSE(window_state->CanResize());

  // Accepting the dialog with a folder selection should dismiss it and add a
  // new option for the custom selected folder in the settings menu.
  const base::FilePath custom_folder(
      CreateCustomFolderInUserDownloadsPath("test"));
  dialog_factory->AcceptPath(custom_folder);
  WaitForSettingsMenuToBeRefreshed();
  EXPECT_FALSE(IsFolderSelectionDialogShown());
  EXPECT_TRUE(AreAllCaptureSessionUisVisible());
  EXPECT_TRUE(save_to_menu_group->IsOptionChecked(kCustomFolder));
  EXPECT_FALSE(save_to_menu_group->IsOptionChecked(kDownloadsFolder));
  EXPECT_EQ(u"test",
            save_to_menu_group->GetOptionLabelForTesting(kCustomFolder));

  // This should update the folder that will be used by the controller.
  auto capture_folder = controller->GetCurrentCaptureFolder();
  EXPECT_EQ(capture_folder.path, custom_folder);
  EXPECT_FALSE(capture_folder.is_default_downloads_folder);
}

// Tests that folder selection dialog can be opened without crash while in
// window capture mode.
TEST_F(CaptureModeSettingsTest, SelectFolderInWindowCaptureMode) {
  std::unique_ptr<aura::Window> window1(
      CreateTestWindow(gfx::Rect(0, 0, 200, 300)));
  StartCaptureSession(CaptureModeSource::kWindow, CaptureModeType::kImage);
  auto* event_generator = GetEventGenerator();
  ClickOnView(GetSettingsButton(), event_generator);

  CaptureModeSettingsTestApi test_api;
  ClickOnView(test_api.GetSelectFolderMenuItem(), event_generator);
  EXPECT_TRUE(IsFolderSelectionDialogShown());
}

TEST_F(CaptureModeSettingsTest, DismissDialogWithoutSelection) {
  auto* controller = StartImageRegionCapture();
  const auto old_capture_folder = controller->GetCurrentCaptureFolder();

  // Open the settings menu, and click the "Select folder" menu item.
  auto* event_generator = GetEventGenerator();
  ClickOnView(GetSettingsButton(), event_generator);
  CaptureModeSettingsTestApi test_api;
  ClickOnView(test_api.GetSelectFolderMenuItem(), event_generator);
  EXPECT_TRUE(IsFolderSelectionDialogShown());

  // Cancel and dismiss the dialog. There should be no change in the folder
  // selection.
  auto* dialog_factory = FakeFolderSelectionDialogFactory::Get();
  dialog_factory->CancelDialog();
  EXPECT_FALSE(IsFolderSelectionDialogShown());
  EXPECT_FALSE(test_api.GetCustomFolderOptionIfAny());
  CaptureModeMenuGroup* save_to_menu_group = test_api.GetSaveToMenuGroup();
  EXPECT_TRUE(save_to_menu_group->IsOptionChecked(kDownloadsFolder));

  const auto new_capture_folder = controller->GetCurrentCaptureFolder();
  EXPECT_EQ(old_capture_folder.path, new_capture_folder.path);
  EXPECT_EQ(old_capture_folder.is_default_downloads_folder,
            new_capture_folder.is_default_downloads_folder);
}

TEST_F(CaptureModeSettingsTest, AcceptUpdatedCustomFolderFromDialog) {
  // Start a new session with a pre-configured custom folder.
  auto* controller = CaptureModeController::Get();
  const base::FilePath custom_folder(
      CreateCustomFolderInUserDownloadsPath("test"));
  controller->SetCustomCaptureFolder(custom_folder);
  StartImageRegionCapture();

  // Open the settings menu and check there already exists an item for that
  // pre-configured custom folder.
  auto* event_generator = GetEventGenerator();
  ClickOnView(GetSettingsButton(), event_generator);
  WaitForSettingsMenuToBeRefreshed();
  CaptureModeSettingsTestApi test_api;
  EXPECT_TRUE(test_api.GetDefaultDownloadsOption());
  auto* custom_folder_view = test_api.GetCustomFolderOptionIfAny();
  EXPECT_TRUE(custom_folder_view);
  CaptureModeMenuGroup* save_to_menu_group = test_api.GetSaveToMenuGroup();
  EXPECT_FALSE(save_to_menu_group->IsOptionChecked(kDownloadsFolder));
  EXPECT_TRUE(save_to_menu_group->IsOptionChecked(kCustomFolder));

  // Now open the folder selection dialog and select a different folder. The
  // existing *same* item in the menu should be updated.
  ClickOnView(test_api.GetSelectFolderMenuItem(), event_generator);
  EXPECT_TRUE(IsFolderSelectionDialogShown());

  auto* dialog_factory = FakeFolderSelectionDialogFactory::Get();
  const base::FilePath new_folder(
      CreateCustomFolderInUserDownloadsPath("test1"));
  dialog_factory->AcceptPath(new_folder);
  WaitForSettingsMenuToBeRefreshed();
  EXPECT_FALSE(IsFolderSelectionDialogShown());
  EXPECT_EQ(custom_folder_view, test_api.GetCustomFolderOptionIfAny());
  EXPECT_TRUE(save_to_menu_group->IsOptionChecked(kCustomFolder));
  EXPECT_FALSE(save_to_menu_group->IsOptionChecked(kDownloadsFolder));
  EXPECT_EQ(u"test1",
            save_to_menu_group->GetOptionLabelForTesting(kCustomFolder));

  // This should update the folder that will be used by the controller.
  const auto capture_folder = controller->GetCurrentCaptureFolder();
  EXPECT_EQ(capture_folder.path, new_folder);
  EXPECT_FALSE(capture_folder.is_default_downloads_folder);
}

TEST_F(CaptureModeSettingsTest,
       InitializeSettingsViewWithUnavailableCustomFolder) {
  // Start a new session with a pre-configured unavailable custom folder.
  auto* controller = CaptureModeController::Get();
  const base::FilePath default_folder =
      controller->delegate_for_testing()->GetUserDefaultDownloadsFolder();
  const base::FilePath custom_folder(FILE_PATH_LITERAL("/home/random"));
  controller->SetCustomCaptureFolder(custom_folder);
  StartImageRegionCapture();

  // Open the settings menu and check there already exists an item for that
  // pre-configured custom folder. Since the custom folder is unavailable, the
  // item should be disabled and dimmed. The item of the default folder should
  // be checked.
  auto* event_generator = GetEventGenerator();
  ClickOnView(GetSettingsButton(), event_generator);
  WaitForSettingsMenuToBeRefreshed();

  CaptureModeSettingsTestApi test_api;
  EXPECT_TRUE(test_api.GetDefaultDownloadsOption());
  auto* custom_folder_view = test_api.GetCustomFolderOptionIfAny();
  EXPECT_TRUE(custom_folder_view);
  CaptureModeMenuGroup* save_to_menu_group = test_api.GetSaveToMenuGroup();
  EXPECT_TRUE(save_to_menu_group->IsOptionChecked(kDownloadsFolder));
  EXPECT_FALSE(save_to_menu_group->IsOptionChecked(kCustomFolder));
  EXPECT_FALSE(custom_folder_view->GetEnabled());
  EXPECT_EQ(u"random",
            save_to_menu_group->GetOptionLabelForTesting(kCustomFolder));

  // Now open the folder selection dialog and select an available folder. The
  // item of the custom folder should be checked and enabled.
  ClickOnView(test_api.GetSelectFolderMenuItem(), event_generator);
  EXPECT_TRUE(IsFolderSelectionDialogShown());

  auto* dialog_factory = FakeFolderSelectionDialogFactory::Get();
  const base::FilePath new_folder(
      CreateCustomFolderInUserDownloadsPath("test"));
  dialog_factory->AcceptPath(new_folder);
  WaitForSettingsMenuToBeRefreshed();
  EXPECT_EQ(custom_folder_view, test_api.GetCustomFolderOptionIfAny());
  EXPECT_TRUE(save_to_menu_group->IsOptionChecked(kCustomFolder));
  EXPECT_FALSE(save_to_menu_group->IsOptionChecked(kDownloadsFolder));
  EXPECT_TRUE(custom_folder_view->GetEnabled());
  EXPECT_EQ(u"test",
            save_to_menu_group->GetOptionLabelForTesting(kCustomFolder));
}

TEST_F(CaptureModeSettingsTest, DeleteCustomFolderFromDialog) {
  // Start a new session with a pre-configured custom folder.
  auto* controller = CaptureModeController::Get();
  const base::FilePath custom_folder(
      CreateCustomFolderInUserDownloadsPath("test"));
  controller->SetCustomCaptureFolder(custom_folder);
  StartImageRegionCapture();

  // Open the settings menu and check there exists an item for that custom
  // folder. And the item is checked to indicate the current folder in use to
  // save the captured files is the custom folder.
  auto* event_generator = GetEventGenerator();
  ClickOnView(GetSettingsButton(), event_generator);
  WaitForSettingsMenuToBeRefreshed();

  CaptureModeSettingsTestApi test_api;
  EXPECT_TRUE(test_api.GetDefaultDownloadsOption());
  auto* custom_folder_view = test_api.GetCustomFolderOptionIfAny();
  EXPECT_TRUE(custom_folder_view);
  CaptureModeMenuGroup* save_to_menu_group = test_api.GetSaveToMenuGroup();
  EXPECT_TRUE(save_to_menu_group->IsOptionChecked(kCustomFolder));

  // Now open the folder selection dialog and delete the custom folder. Check
  // the item on the settings menu for custom folder is still there but disabled
  // and dimmed. The item of the default folder is checked now.
  ClickOnView(test_api.GetSelectFolderMenuItem(), event_generator);
  EXPECT_TRUE(IsFolderSelectionDialogShown());
  auto* dialog_factory = FakeFolderSelectionDialogFactory::Get();
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    const bool result = base::DeleteFile(custom_folder);
    DCHECK(result);
  }
  dialog_factory->CancelDialog();
  WaitForSettingsMenuToBeRefreshed();
  EXPECT_TRUE(custom_folder_view);
  EXPECT_FALSE(save_to_menu_group->IsOptionChecked(kCustomFolder));
  EXPECT_FALSE(custom_folder_view->GetEnabled());
  EXPECT_TRUE(save_to_menu_group->IsOptionChecked(kDownloadsFolder));
}

TEST_F(CaptureModeSettingsTest, AccessibleCheckedStateChange) {
  // Start a new session with a pre-configured custom folder.
  ui::AXNodeData data;
  auto* controller = CaptureModeController::Get();
  const base::FilePath custom_folder(
      CreateCustomFolderInUserDownloadsPath("test"));
  controller->SetCustomCaptureFolder(custom_folder);
  StartImageRegionCapture();
  auto* event_generator = GetEventGenerator();
  ClickOnView(GetSettingsButton(), event_generator);
  WaitForSettingsMenuToBeRefreshed();

  CaptureModeSettingsTestApi test_api;
  CaptureModeMenuGroup* save_to_menu_group = test_api.GetSaveToMenuGroup();

  auto* checked_custom_folder_view =
      save_to_menu_group->SetOptionCheckedForTesting(kCustomFolder, true);
  checked_custom_folder_view->GetViewAccessibility().GetAccessibleNodeData(
      &data);
  EXPECT_EQ(data.GetCheckedState(), ax::mojom::CheckedState::kTrue);

  data = ui::AXNodeData();
  auto* unchecked_custom_folder_view =
      save_to_menu_group->SetOptionCheckedForTesting(kCustomFolder, false);
  unchecked_custom_folder_view->GetViewAccessibility().GetAccessibleNodeData(
      &data);
  EXPECT_EQ(data.GetCheckedState(), ax::mojom::CheckedState::kFalse);
}

TEST_F(CaptureModeSettingsTest, AcceptDefaultDownloadsFolderFromDialog) {
  // Start a new session with a pre-configured custom folder.
  auto* controller = CaptureModeController::Get();
  controller->SetCustomCaptureFolder(
      base::FilePath(FILE_PATH_LITERAL("/home/tests/foo")));
  StartImageRegionCapture();

  auto* event_generator = GetEventGenerator();
  ClickOnView(GetSettingsButton(), event_generator);
  WaitForSettingsMenuToBeRefreshed();
  CaptureModeSettingsTestApi test_api;
  ClickOnView(test_api.GetSelectFolderMenuItem(), event_generator);

  // Selecting the same folder as the default downloads folder should result in
  // removing the custom folder option from the menu.
  auto* test_delegate = controller->delegate_for_testing();
  const auto default_downloads_folder =
      test_delegate->GetUserDefaultDownloadsFolder();
  auto* dialog_factory = FakeFolderSelectionDialogFactory::Get();
  dialog_factory->AcceptPath(default_downloads_folder);
  EXPECT_FALSE(IsFolderSelectionDialogShown());
  EXPECT_TRUE(test_api.GetDefaultDownloadsOption());
  EXPECT_FALSE(test_api.GetCustomFolderOptionIfAny());
  CaptureModeMenuGroup* save_to_menu_group = test_api.GetSaveToMenuGroup();
  EXPECT_TRUE(save_to_menu_group->IsOptionChecked(kDownloadsFolder));
}

TEST_F(CaptureModeSettingsTest, SwitchWhichFolderToUserFromOptions) {
  // Start a new session with a pre-configured custom folder.
  auto* controller = CaptureModeController::Get();
  const base::FilePath custom_path(
      (CreateCustomFolderInUserDownloadsPath("test")));
  controller->SetCustomCaptureFolder(custom_path);
  StartImageRegionCapture();
  auto* event_generator = GetEventGenerator();
  ClickOnView(GetSettingsButton(), event_generator);
  WaitForSettingsMenuToBeRefreshed();

  // Clicking the "Downloads" option will set it as the folder of choice, but
  // won't clear the custom folder.
  CaptureModeSettingsTestApi test_api;
  ClickOnView(test_api.GetDefaultDownloadsOption(), event_generator);
  CaptureModeMenuGroup* save_to_menu_group = test_api.GetSaveToMenuGroup();
  EXPECT_TRUE(save_to_menu_group->IsOptionChecked(kDownloadsFolder));
  EXPECT_FALSE(save_to_menu_group->IsOptionChecked(kCustomFolder));
  const auto default_downloads_folder =
      controller->delegate_for_testing()->GetUserDefaultDownloadsFolder();
  auto capture_folder = controller->GetCurrentCaptureFolder();
  EXPECT_EQ(capture_folder.path, default_downloads_folder);
  EXPECT_TRUE(capture_folder.is_default_downloads_folder);
  EXPECT_EQ(custom_path, controller->GetCustomCaptureFolder());

  // Clicking on the custom folder option will switch back to using it.
  ClickOnView(test_api.GetCustomFolderOptionIfAny(), event_generator);
  EXPECT_TRUE(save_to_menu_group->IsOptionChecked(kCustomFolder));
  EXPECT_FALSE(save_to_menu_group->IsOptionChecked(kDownloadsFolder));
  capture_folder = controller->GetCurrentCaptureFolder();
  EXPECT_EQ(capture_folder.path, custom_path);
  EXPECT_FALSE(capture_folder.is_default_downloads_folder);
}

// Tests that when there's no overlap betwwen capture label widget and settings
// widget, capture label widget is shown/hidden correctly after open/close the
// folder selection window.
TEST_F(CaptureModeSettingsTest, CaptureLabelViewNotOverlapsWithSettingsView) {
  // Update the display size to make sure capture label widget will not
  // overlap with settings widget
  UpdateDisplay("800x600");

  auto* controller = CaptureModeController::Get();
  // Set the region at an area far away from where the settings menu shows.
  controller->SetUserCaptureRegion(gfx::Rect(200, 200), /*by_user=*/true);

  StartImageRegionCapture();
  auto* event_generator = GetEventGenerator();

  // Tests that the capture label widget doesn't overlap with settings widget.
  // Both capture label widget and settings widget are visible.
  views::Widget* capture_label_widget = GetCaptureModeLabelWidget();
  ClickOnView(GetSettingsButton(), event_generator);
  views::Widget* settings_widget = GetCaptureModeSettingsWidget();
  EXPECT_FALSE(capture_label_widget->GetWindowBoundsInScreen().Intersects(
      settings_widget->GetWindowBoundsInScreen()));
  EXPECT_TRUE(capture_label_widget->IsVisible());
  EXPECT_TRUE(settings_widget->IsVisible());

  // Open folder selection window, check that both capture label widget and
  // settings widget are invisible.
  CaptureModeSettingsTestApi test_api;
  auto* dialog_factory = FakeFolderSelectionDialogFactory::Get();
  ClickOnView(test_api.GetSelectFolderMenuItem(), event_generator);
  EXPECT_TRUE(IsFolderSelectionDialogShown());
  EXPECT_FALSE(capture_label_widget->IsVisible());
  EXPECT_FALSE(settings_widget->IsVisible());

  // Now close folder selection window, check that capture label widget and
  // settings widget become visible.
  dialog_factory->CancelDialog();
  EXPECT_FALSE(IsFolderSelectionDialogShown());
  EXPECT_TRUE(capture_label_widget->IsVisible());
  EXPECT_EQ(capture_label_widget->GetLayer()->GetTargetOpacity(), 1.f);
  EXPECT_TRUE(settings_widget->IsVisible());

  // Close settings widget. Capture label widget is visible.
  ClickOnView(GetSettingsButton(), event_generator);
  EXPECT_TRUE(capture_label_widget->IsVisible());
  controller->Stop();
}

// Tests that when capture label widget overlaps with settings widget, capture
// label widget is shown/hidden correctly after open/close the folder selection
// window, open/close settings menu. Regression test for
// https://crbug.com/1279606.
TEST_F(CaptureModeSettingsTest, CaptureLabelViewOverlapsWithSettingsView) {
  // Update display size to make capture label widget overlap with settings
  // widget.
  UpdateDisplay("1100x700");
  auto* controller = StartImageRegionCapture();
  auto* event_generator = GetEventGenerator();

  // Tests that capture label widget overlaps with settings widget and is
  // hidden after setting widget is shown.
  auto* capture_label_widget = GetCaptureModeLabelWidget();
  ClickOnView(GetSettingsButton(), event_generator);
  auto* settings_widget = GetCaptureModeSettingsWidget();
  EXPECT_TRUE(capture_label_widget->GetWindowBoundsInScreen().Intersects(
      settings_widget->GetWindowBoundsInScreen()));
  EXPECT_FALSE(GetCaptureModeLabelWidget()->IsVisible());
  EXPECT_TRUE(settings_widget->IsVisible());

  // Open folder selection window, capture label widget is invisible.
  CaptureModeSettingsTestApi test_api;
  auto* dialog_factory = FakeFolderSelectionDialogFactory::Get();
  ClickOnView(test_api.GetSelectFolderMenuItem(), event_generator);
  EXPECT_TRUE(IsFolderSelectionDialogShown());
  EXPECT_FALSE(capture_label_widget->IsVisible());

  // Close folder selection window, capture label widget is invisible.
  dialog_factory->CancelDialog();
  EXPECT_FALSE(IsFolderSelectionDialogShown());
  EXPECT_FALSE(capture_label_widget->IsVisible());

  // Tests that capture label widget is visible after settings widget is
  // closed.
  ClickOnView(GetSettingsButton(), event_generator);
  EXPECT_TRUE(capture_label_widget->IsVisible());
  EXPECT_EQ(capture_label_widget->GetLayer()->GetTargetOpacity(), 1.f);
  controller->Stop();
}

TEST_F(CaptureModeSettingsTest, PressingEnterSelectsFocusedItem) {
  auto* controller =
      StartCaptureSession(CaptureModeSource::kRegion, CaptureModeType::kImage);

  using FocusGroup = CaptureModeSessionFocusCycler::FocusGroup;
  CaptureModeSessionTestApi session_test_api(
      controller->capture_mode_session());

  // Tab six times to focus on the settings button.
  auto* event_generator = GetEventGenerator();
  SendKey(ui::VKEY_TAB, event_generator, ui::EF_NONE, /*count=*/6);
  EXPECT_EQ(FocusGroup::kSettingsClose,
            session_test_api.GetCurrentFocusGroup());
  EXPECT_EQ(0u, session_test_api.GetCurrentFocusIndex());

  // Press the enter key to open the settings menu. The current focus group
  // should be `kPendingSettings`.
  SendKey(ui::VKEY_RETURN, event_generator);
  ASSERT_TRUE(GetCaptureModeSettingsView());
  EXPECT_EQ(FocusGroup::kPendingSettings,
            session_test_api.GetCurrentFocusGroup());

  // Tab once to enter focus into the settings menu.
  SendKey(ui::VKEY_TAB, event_generator, ui::EF_NONE);
  ASSERT_EQ(FocusGroup::kSettingsMenu, session_test_api.GetCurrentFocusGroup());

  // Tab until focus reaches the `kAudioMicrophone` option.
  CaptureModeSettingsTestApi settings_test_api;
  auto* mic_option = settings_test_api.GetMicrophoneOption();
  while (session_test_api.GetCurrentFocusedView()->GetView() != mic_option) {
    SendKey(ui::VKEY_TAB, event_generator, ui::EF_NONE);
  }

  CaptureModeMenuGroup* audio_input_menu_group =
      settings_test_api.GetAudioInputMenuGroup();
  EXPECT_TRUE(audio_input_menu_group->IsOptionChecked(kAudioOff));
  EXPECT_FALSE(audio_input_menu_group->IsOptionChecked(kAudioMicrophone));

  // Press the enter key, and now microphone should be on.
  SendKey(ui::VKEY_RETURN, event_generator);
  EXPECT_FALSE(audio_input_menu_group->IsOptionChecked(kAudioOff));
  EXPECT_TRUE(audio_input_menu_group->IsOptionChecked(kAudioMicrophone));
}

// Tests the basic keyboard navigation functions for the settings menu.
TEST_F(CaptureModeSettingsTest, KeyboardNavigationForSettingsMenu) {
  auto* controller =
      StartCaptureSession(CaptureModeSource::kRegion, CaptureModeType::kImage);

  using FocusGroup = CaptureModeSessionFocusCycler::FocusGroup;
  CaptureModeSessionTestApi session_test_api(
      controller->capture_mode_session());

  // Tab six times to focus on the settings button.
  auto* event_generator = GetEventGenerator();
  SendKey(ui::VKEY_TAB, event_generator, ui::EF_NONE, /*count=*/6);
  EXPECT_EQ(FocusGroup::kSettingsClose,
            session_test_api.GetCurrentFocusGroup());
  EXPECT_EQ(0u, session_test_api.GetCurrentFocusIndex());

  // Enter space to open the settings menu. The current focus group should be
  // `kPendingSettings`.
  SendKey(ui::VKEY_SPACE, event_generator);
  ASSERT_TRUE(GetCaptureModeSettingsView());
  EXPECT_EQ(FocusGroup::kPendingSettings,
            session_test_api.GetCurrentFocusGroup());

  CaptureModeSettingsTestApi settings_test_api;
  CaptureModeMenuGroup* audio_input_menu_group =
      settings_test_api.GetAudioInputMenuGroup();

  // Tab once to focus on the first item in the settings menu (`Audio input`
  // header).
  SendKey(ui::VKEY_TAB, event_generator);
  EXPECT_EQ(FocusGroup::kSettingsMenu, session_test_api.GetCurrentFocusGroup());
  EXPECT_EQ(0u, session_test_api.GetCurrentFocusIndex());

  // Tab once to enter focus into the settings menu.
  SendKey(ui::VKEY_TAB, event_generator, ui::EF_NONE);
  ASSERT_EQ(FocusGroup::kSettingsMenu, session_test_api.GetCurrentFocusGroup());

  // Check that the `Off` option is the checked option not the `Microphone`.
  EXPECT_TRUE(audio_input_menu_group->IsOptionChecked(kAudioOff));
  EXPECT_FALSE(audio_input_menu_group->IsOptionChecked(kAudioMicrophone));

  // Tab until focus reaches the `kAudioMicrophone` option.
  auto* mic_option = settings_test_api.GetMicrophoneOption();
  while (session_test_api.GetCurrentFocusedView()->GetView() != mic_option) {
    SendKey(ui::VKEY_TAB, event_generator, ui::EF_NONE);
  }

  // Enter space, and check that now the `Microphone` option is checked.
  SendKey(ui::VKEY_SPACE, event_generator);
  EXPECT_FALSE(audio_input_menu_group->IsOptionChecked(kAudioOff));
  EXPECT_TRUE(audio_input_menu_group->IsOptionChecked(kAudioMicrophone));

  Switch* toggle_button =
      settings_test_api.GetDemoToolsMenuToggleButton()->toggle_button();

    // The demo tools toggle button will be disabled by default.
    EXPECT_FALSE(toggle_button->GetIsOn());

    // Tab until focus reaches the demo tools toggle button and enter space to
    // enable it.
    while (session_test_api.GetCurrentFocusedView()->GetView() !=
           toggle_button) {
      SendKey(ui::VKEY_TAB, event_generator, ui::EF_NONE);
    }
    SendKey(ui::VKEY_SPACE, event_generator);
    EXPECT_TRUE(toggle_button->GetIsOn());

  // Tab until focus reaches the `Select folder...` menu item.
  auto* select_folder_option = settings_test_api.GetSelectFolderMenuItem();
  while (session_test_api.GetCurrentFocusedView()->GetView() !=
         select_folder_option) {
    SendKey(ui::VKEY_TAB, event_generator, ui::EF_NONE);
  }

  // Enter space to open the folder selection window.
  SendKey(ui::VKEY_SPACE, event_generator);
  auto* dialog_factory = FakeFolderSelectionDialogFactory::Get();
  EXPECT_TRUE(IsFolderSelectionDialogShown());

  // Close selection window.
  dialog_factory->CancelDialog();
  EXPECT_FALSE(IsFolderSelectionDialogShown());

  // Now tab once to focus on the settings button and enter space on it to close
  // the settings menu.
  SendKey(ui::VKEY_TAB, event_generator);
  EXPECT_EQ(FocusGroup::kSettingsClose,
            session_test_api.GetCurrentFocusGroup());
  EXPECT_EQ(0u, session_test_api.GetCurrentFocusIndex());
  SendKey(ui::VKEY_SPACE, event_generator);
  EXPECT_FALSE(GetCaptureModeSettingsView());
}

// Tests that the disabled option in the settings menu will be skipped while
// tabbing through.
TEST_F(CaptureModeSettingsTest,
       KeyboardNavigationForSettingsMenuWithDisabledOption) {
  // Start a new session with a pre-configured unavailable custom folder.
  auto* controller = CaptureModeController::Get();
  const base::FilePath custom_folder(FILE_PATH_LITERAL("/home/random"));
  controller->SetCustomCaptureFolder(custom_folder);
  StartImageRegionCapture();

  using FocusGroup = CaptureModeSessionFocusCycler::FocusGroup;
  CaptureModeSessionTestApi session_test_api(
      controller->capture_mode_session());

  // Tab six times to focus the settings button and enter space to open the
  // settings menu.
  auto* event_generator = GetEventGenerator();
  SendKey(ui::VKEY_TAB, event_generator, ui::EF_NONE, 6);
  SendKey(ui::VKEY_SPACE, event_generator);
  EXPECT_EQ(FocusGroup::kPendingSettings,
            session_test_api.GetCurrentFocusGroup());
  CaptureModeSettingsView* settings_menu = GetCaptureModeSettingsView();
  ASSERT_TRUE(settings_menu);

  // Since the custom folder is unavailable, the `kCustomFolder` should be
  // disabled and won't be returned via
  // `CaptureModeSettingsViews::GetHighlightableItems`.
  CaptureModeSettingsTestApi settings_test_api;
  auto* custom_folder_view = settings_test_api.GetCustomFolderOptionIfAny();
  ASSERT_TRUE(custom_folder_view);
  EXPECT_FALSE(custom_folder_view->GetEnabled());

  std::vector<CaptureModeSessionFocusCycler::HighlightableView*>
      highlightable_items = settings_menu->GetHighlightableItems();
  EXPECT_FALSE(base::Contains(
      highlightable_items, custom_folder_view,
      &CaptureModeSessionFocusCycler::HighlightableView::GetView));

  // Tab once to enter focus into the settings menu.
  SendKey(ui::VKEY_TAB, event_generator, ui::EF_NONE);
  ASSERT_EQ(FocusGroup::kSettingsMenu, session_test_api.GetCurrentFocusGroup());

  // Tab until the focus is on the default `Downloads` option.
  auto* downloads_option = settings_test_api.GetDefaultDownloadsOption();
  ASSERT_TRUE(downloads_option);
  while (session_test_api.GetCurrentFocusedView()->GetView() !=
         downloads_option) {
    SendKey(ui::VKEY_TAB, event_generator, ui::EF_NONE);
  }

  // Tab once to check the disabled `kCustomFolder` option is skipped and now
  // the `Select folder...` menu item gets focused.
  SendKey(ui::VKEY_TAB, event_generator);
  EXPECT_EQ(session_test_api.GetCurrentFocusedView()->GetView(),
            settings_test_api.GetSelectFolderMenuItem());
}

// Tests that selecting the default `Downloads` folder as the custom folder via
// keyboard navigation doesn't lead to a crash. Regression test for
// https://crbug.com/1269373.
TEST_F(CaptureModeSettingsTest,
       KeyboardNavigationForRemovingCustomFolderOption) {
  // Start a new session with a pre-configured custom folder.
  auto* controller = CaptureModeController::Get();
  const base::FilePath custom_folder(
      CreateCustomFolderInUserDownloadsPath("test"));
  controller->SetCustomCaptureFolder(custom_folder);
  StartImageRegionCapture();

  using FocusGroup = CaptureModeSessionFocusCycler::FocusGroup;
  CaptureModeSessionTestApi test_api(controller->capture_mode_session());

  // Tab six times to focus the settings button, then enter space to open the
  // settings menu. Wait for the settings menu to be refreshed.
  auto* event_generator = GetEventGenerator();
  SendKey(ui::VKEY_TAB, event_generator, ui::EF_NONE, /*count=*/6);
  SendKey(ui::VKEY_SPACE, event_generator);
  WaitForSettingsMenuToBeRefreshed();
  EXPECT_EQ(FocusGroup::kPendingSettings, test_api.GetCurrentFocusGroup());
  CaptureModeSettingsView* settings_menu = GetCaptureModeSettingsView();
  ASSERT_TRUE(settings_menu);

  // Tab once to enter focus into the settings menu.
  SendKey(ui::VKEY_TAB, event_generator, ui::EF_NONE);
  ASSERT_EQ(FocusGroup::kSettingsMenu, test_api.GetCurrentFocusGroup());

  // Tab until focus reaches the `Select folder...` menu item.
  CaptureModeSettingsTestApi settings_test_api;
  auto* select_folder_option = settings_test_api.GetSelectFolderMenuItem();
  while (test_api.GetCurrentFocusedView()->GetView() != select_folder_option) {
    SendKey(ui::VKEY_TAB, event_generator, ui::EF_NONE);
  }

  // Enter space to open the folder selection window.
  SendKey(ui::VKEY_SPACE, event_generator);
  EXPECT_TRUE(IsFolderSelectionDialogShown());

  // Select the default `Downloads` folder as the custom folder which will
  // have custom folder option get removed.
  auto* test_delegate = controller->delegate_for_testing();
  const auto default_downloads_folder =
      test_delegate->GetUserDefaultDownloadsFolder();
  auto* dialog_factory = FakeFolderSelectionDialogFactory::Get();
  dialog_factory->AcceptPath(default_downloads_folder);

  // Press space to ensure the folder selection window can be opened after the
  // custom folder is removed from the settings menu.
  SendKey(ui::VKEY_SPACE, event_generator);
  EXPECT_TRUE(IsFolderSelectionDialogShown());
  dialog_factory->CancelDialog();

  // Tab once to make sure there's no crash and the focus gets moved to
  // settings button.
  SendKey(ui::VKEY_TAB, event_generator);
  EXPECT_EQ(FocusGroup::kSettingsClose, test_api.GetCurrentFocusGroup());
  EXPECT_EQ(0u, test_api.GetCurrentFocusIndex());
}

// Tests that first time selecting a custom folder via keyboard navigation.
// After the custom folder is selected, tabbing one more time will move focus
// from the settings menu to the settings button.
TEST_F(CaptureModeSettingsTest, KeyboardNavigationForAddingCustomFolderOption) {
  auto* controller = CaptureModeController::Get();
  StartImageRegionCapture();

  using FocusGroup = CaptureModeSessionFocusCycler::FocusGroup;
  CaptureModeSessionTestApi session_test_api(
      controller->capture_mode_session());

  // Tab six times to focus on the settings button, then enter space to open
  // the settings menu.
  auto* event_generator = GetEventGenerator();
  SendKey(ui::VKEY_TAB, event_generator, ui::EF_NONE, 6);
  SendKey(ui::VKEY_SPACE, event_generator);
  EXPECT_EQ(FocusGroup::kPendingSettings,
            session_test_api.GetCurrentFocusGroup());
  CaptureModeSettingsView* settings_menu = GetCaptureModeSettingsView();
  ASSERT_TRUE(settings_menu);

  // Tab once to enter focus into the settings menu.
  SendKey(ui::VKEY_TAB, event_generator, ui::EF_NONE);
  ASSERT_EQ(FocusGroup::kSettingsMenu, session_test_api.GetCurrentFocusGroup());

  // Tab until focus reaches the `Select folder...` menu item.
  CaptureModeSettingsTestApi settings_test_api;
  auto* select_folder_option = settings_test_api.GetSelectFolderMenuItem();
  while (session_test_api.GetCurrentFocusedView()->GetView() !=
         select_folder_option) {
    SendKey(ui::VKEY_TAB, event_generator, ui::EF_NONE);
  }

  // Enter space to open the folder selection window.
  SendKey(ui::VKEY_SPACE, event_generator);
  EXPECT_TRUE(IsFolderSelectionDialogShown());

  // Select the custom folder and wait for the settings menu to be refreshed.
  // The custom folder option should be added to the settings menu and checked.
  const base::FilePath custom_folder(
      CreateCustomFolderInUserDownloadsPath("test"));
  controller->SetCustomCaptureFolder(custom_folder);
  auto* dialog_factory = FakeFolderSelectionDialogFactory::Get();
  dialog_factory->AcceptPath(custom_folder);
  WaitForSettingsMenuToBeRefreshed();
  EXPECT_TRUE(settings_test_api.GetCustomFolderOptionIfAny());

  // Press space to ensure the folder selection window can be opened after the
  // custom folder is added to the settings menu.
  SendKey(ui::VKEY_SPACE, event_generator);
  EXPECT_TRUE(IsFolderSelectionDialogShown());
  dialog_factory->CancelDialog();

  // Tab once to make sure the focus gets moved to settings button.
  SendKey(ui::VKEY_TAB, event_generator);
  EXPECT_EQ(FocusGroup::kSettingsClose,
            session_test_api.GetCurrentFocusGroup());
  EXPECT_EQ(0u, session_test_api.GetCurrentFocusIndex());
}

// Tests the folder selection settings when it's recommended by policy.
TEST_F(CaptureModeSettingsTest, FolderRecommendedByPolicy) {
  auto* controller = StartImageRegionCapture();

  // Set the pref to recommended values.
  const base::FilePath custom_folder(
      CreateCustomFolderInUserDownloadsPath("test"));
  auto* test_delegate =
      static_cast<TestCaptureModeDelegate*>(controller->delegate_for_testing());
  test_delegate->set_policy_capture_path(
      {custom_folder,
       CaptureModeDelegate::CapturePathEnforcement::kRecommended});

  // Open settings.
  auto* event_generator = GetEventGenerator();
  ClickOnView(GetSettingsButton(), event_generator);
  std::unique_ptr<CaptureModeSettingsTestApi> test_api =
      std::make_unique<CaptureModeSettingsTestApi>();
  WaitForSettingsMenuToBeRefreshed();

  // Custom folder is set, but Downloads option and select folder is enabled.
  EXPECT_FALSE(controller->IsCustomFolderManagedByPolicy());
  CaptureModeMenuGroup* save_to_menu_group = test_api->GetSaveToMenuGroup();
  EXPECT_FALSE(save_to_menu_group->IsManagedByPolicy());

  EXPECT_TRUE(test_api->GetCustomFolderOptionIfAny()->GetEnabled());
  EXPECT_TRUE(save_to_menu_group->IsOptionChecked(kCustomFolder));

  EXPECT_TRUE(test_api->GetDefaultDownloadsOption()->GetEnabled());
  EXPECT_FALSE(save_to_menu_group->IsOptionChecked(kDownloadsFolder));

  EXPECT_TRUE(test_api->GetSelectFolderMenuItem()->GetEnabled());
}

// Tests the folder selection settings when it's enforced by policy.
TEST_F(CaptureModeSettingsTest, FolderSetByPolicy) {
  auto* controller = StartImageRegionCapture();

  // Set the pref to managed values.
  const base::FilePath custom_folder(
      CreateCustomFolderInUserDownloadsPath("test"));
  auto* test_delegate =
      static_cast<TestCaptureModeDelegate*>(controller->delegate_for_testing());
  test_delegate->set_policy_capture_path(
      {custom_folder, CaptureModeDelegate::CapturePathEnforcement::kManaged});

  // Open settings.
  auto* event_generator = GetEventGenerator();
  ClickOnView(GetSettingsButton(), event_generator);
  std::unique_ptr<CaptureModeSettingsTestApi> test_api =
      std::make_unique<CaptureModeSettingsTestApi>();
  WaitForSettingsMenuToBeRefreshed();

  // Custom folder is set, but Downloads option and select folder are not
  // enabled.
  EXPECT_TRUE(controller->IsCustomFolderManagedByPolicy());
  CaptureModeMenuGroup* save_to_menu_group = test_api->GetSaveToMenuGroup();
  EXPECT_TRUE(save_to_menu_group->IsManagedByPolicy());

  EXPECT_TRUE(test_api->GetCustomFolderOptionIfAny()->GetEnabled());
  EXPECT_TRUE(save_to_menu_group->IsOptionChecked(kCustomFolder));

  EXPECT_FALSE(test_api->GetDefaultDownloadsOption()->GetEnabled());
  EXPECT_FALSE(save_to_menu_group->IsOptionChecked(kDownloadsFolder));

  EXPECT_FALSE(test_api->GetSelectFolderMenuItem()->GetEnabled());
}

// -----------------------------------------------------------------------------
// CaptureModeHistogramTest:

// Test fixture to verify screen capture histograms depending on the test
// param (true for tablet mode, false for clamshell mode).
class CaptureModeHistogramTest : public CaptureModeSettingsTest,
                                 public ::testing::WithParamInterface<bool> {
 public:
  CaptureModeHistogramTest() = default;
  ~CaptureModeHistogramTest() override = default;

  // CaptureModeSettingsTest:
  void SetUp() override {
    CaptureModeSettingsTest::SetUp();
    if (GetParam())
      SwitchToTabletMode();
  }

  void StartSessionForVideo() {
    StartCaptureSession(CaptureModeSource::kFullscreen,
                        CaptureModeType::kVideo);
  }

  void StartRecording() { CaptureModeTestApi().PerformCapture(); }

  void StopRecording() { CaptureModeTestApi().StopVideoRecording(); }

  void OpenView(const views::View* view,
                ui::test::EventGenerator* event_generator) {
    if (GetParam())
      TouchOnView(view, event_generator);
    else
      ClickOnView(view, event_generator);
  }
};

// Tests that metrics are recorded properly for various capture mode entry
// points.
TEST_P(CaptureModeHistogramTest, CaptureModeEntryPointHistograms) {
  constexpr char kHistogramNameBase[] = "EntryPoint";
  const std::string histogram_name = BuildHistogramName(
      kHistogramNameBase, /*behavior=*/nullptr, /*append_ui_mode_suffix=*/true);
  base::HistogramTester histogram_tester;

  auto* controller = CaptureModeController::Get();

  controller->Start(CaptureModeEntryType::kAccelTakeWindowScreenshot);
  histogram_tester.ExpectBucketCount(
      histogram_name, CaptureModeEntryType::kAccelTakeWindowScreenshot, 1);
  controller->Stop();

  controller->Start(CaptureModeEntryType::kAccelTakePartialScreenshot);
  histogram_tester.ExpectBucketCount(
      histogram_name, CaptureModeEntryType::kAccelTakePartialScreenshot, 1);
  controller->Stop();

  controller->Start(CaptureModeEntryType::kQuickSettings);
  histogram_tester.ExpectBucketCount(histogram_name,
                                     CaptureModeEntryType::kQuickSettings, 1);
  controller->Stop();

  controller->Start(CaptureModeEntryType::kStylusPalette);
  histogram_tester.ExpectBucketCount(histogram_name,
                                     CaptureModeEntryType::kStylusPalette, 1);
  controller->Stop();

  std::unique_ptr<aura::Window> window(
      CreateTestWindow(gfx::Rect(10, 20, 700, 500)));
  controller->CaptureScreenshotOfGivenWindow(window.get());
  WaitForCaptureFileToBeSaved();
  histogram_tester.ExpectBucketCount(
      histogram_name, CaptureModeEntryType::kCaptureGivenWindow, 1);

  // Check total counts for each histogram to ensure calls aren't counted in
  // multiple buckets.
  histogram_tester.ExpectTotalCount(histogram_name, 5);
  histogram_tester.ExpectTotalCount(histogram_name, 5);
}

// Tests that metrics are recorded properly for capture mode configurations when
// taking a screenshot.
TEST_P(CaptureModeHistogramTest, ScreenshotConfigurationHistogram) {
  constexpr char kHistogramNameBase[] = "CaptureConfiguration";
  const std::string histogram_name = BuildHistogramName(
      kHistogramNameBase, /*behavior=*/nullptr, /*append_ui_mode_suffix=*/true);
  base::HistogramTester histogram_tester;
  // Use a set display size as we will be choosing points in this test.
  UpdateDisplay("800x700");

  // Create a window for window captures later.
  std::unique_ptr<aura::Window> window(
      CreateTestWindow(gfx::Rect(600, 600, 100, 100)));

  // Perform a fullscreen screenshot.
  auto* controller = StartCaptureSession(CaptureModeSource::kFullscreen,
                                         CaptureModeType::kImage);
  controller->PerformCapture();
  histogram_tester.ExpectBucketCount(
      histogram_name, CaptureModeConfiguration::kFullscreenScreenshot, 1);

  // Perform a region screenshot.
  controller =
      StartCaptureSession(CaptureModeSource::kRegion, CaptureModeType::kImage);
  const gfx::Rect capture_region(200, 200, 400, 400);
  SelectRegion(capture_region);
  controller->PerformCapture();
  histogram_tester.ExpectBucketCount(
      histogram_name, CaptureModeConfiguration::kRegionScreenshot, 1);

  // Perform a window screenshot.
  controller =
      StartCaptureSession(CaptureModeSource::kWindow, CaptureModeType::kImage);
  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseToCenterOf(window.get());
  EXPECT_EQ(window.get(),
            controller->capture_mode_session()->GetSelectedWindow());
  controller->PerformCapture();
  histogram_tester.ExpectBucketCount(
      histogram_name, CaptureModeConfiguration::kWindowScreenshot, 1);
}

TEST_P(CaptureModeHistogramTest, VideoRecordingAudioVideoMetrics) {
  constexpr char kHistogramNameBase[] = "AudioRecordingMode";
  const std::string histogram_name = BuildHistogramName(
      kHistogramNameBase, /*behavior=*/nullptr, /*append_ui_mode_suffix=*/true);
  base::HistogramTester histogram_tester;
  histogram_tester.ExpectBucketCount(histogram_name, AudioRecordingMode::kOff,
                                     0);
  histogram_tester.ExpectBucketCount(histogram_name,
                                     AudioRecordingMode::kMicrophone, 0);
  histogram_tester.ExpectBucketCount(histogram_name,
                                     AudioRecordingMode::kSystem, 0);
  histogram_tester.ExpectBucketCount(
      histogram_name, AudioRecordingMode::kSystemAndMicrophone, 0);

  // Perform a video recording with audio off. `kOff` should be recorded.
  StartSessionForVideo();
  CaptureModeTestApi().SetAudioRecordingMode(AudioRecordingMode::kOff);
  StartRecording();
  histogram_tester.ExpectBucketCount(histogram_name, AudioRecordingMode::kOff,
                                     1);
  histogram_tester.ExpectBucketCount(histogram_name,
                                     AudioRecordingMode::kMicrophone, 0);
  histogram_tester.ExpectBucketCount(histogram_name,
                                     AudioRecordingMode::kSystem, 0);
  histogram_tester.ExpectBucketCount(
      histogram_name, AudioRecordingMode::kSystemAndMicrophone, 0);
  WaitForSeconds(1);
  StopRecording();
  WaitForCaptureFileToBeSaved();

  histogram_tester.ExpectTotalCount(
      BuildHistogramName("ScreenRecordingFileSize", /*behavior=*/nullptr,
                         /*append_ui_mode_suffix=*/true),
      /*expected_count=*/1);

  // Perform a video recording with microphone audio recording on. `kMicrophone`
  // should be recorded.
  StartSessionForVideo();
  CaptureModeTestApi().SetAudioRecordingMode(AudioRecordingMode::kMicrophone);
  StartRecording();
  histogram_tester.ExpectBucketCount(histogram_name, AudioRecordingMode::kOff,
                                     1);
  histogram_tester.ExpectBucketCount(histogram_name,
                                     AudioRecordingMode::kMicrophone, 1);
  histogram_tester.ExpectBucketCount(histogram_name,
                                     AudioRecordingMode::kSystem, 0);
  histogram_tester.ExpectBucketCount(
      histogram_name, AudioRecordingMode::kSystemAndMicrophone, 0);
  StopRecording();
}

TEST_P(CaptureModeHistogramTest, CaptureModeSwitchToDefaultReasonMetric) {
  constexpr char kHistogramNameBase[] = "SwitchToDefaultReason";
  const std::string histogram_name = BuildHistogramName(
      kHistogramNameBase, /*behavior=*/nullptr, /*append_ui_mode_suffix=*/true);
  base::HistogramTester histogram_tester;
  auto* controller = CaptureModeController::Get();
  const auto downloads_folder =
      controller->delegate_for_testing()->GetUserDefaultDownloadsFolder();
  const base::FilePath non_available_custom_folder(
      FILE_PATH_LITERAL("/home/test"));
  const base::FilePath available_custom_folder =
      CreateCustomFolderInUserDownloadsPath("test");

  histogram_tester.ExpectBucketCount(
      histogram_name, CaptureModeSwitchToDefaultReason::kFolderUnavailable, 0);
  histogram_tester.ExpectBucketCount(
      histogram_name,
      CaptureModeSwitchToDefaultReason::kUserSelectedFromFolderSelectionDialog,
      0);
  histogram_tester.ExpectBucketCount(
      histogram_name,
      CaptureModeSwitchToDefaultReason::kUserSelectedFromSettingsMenu, 0);

  StartImageRegionCapture();

  // Set the custom folder to an unavailable folder the switch to default
  // reason should be recorded as `kFolderUnavailable`.
  controller->SetCustomCaptureFolder(non_available_custom_folder);
  EXPECT_EQ(controller->GetCurrentCaptureFolder().path,
            non_available_custom_folder);
  auto* event_generator = GetEventGenerator();
  OpenView(GetSettingsButton(), event_generator);
  WaitForSettingsMenuToBeRefreshed();
  CaptureModeSettingsTestApi test_api;
  CaptureModeMenuGroup* save_to_menu_group = test_api.GetSaveToMenuGroup();
  EXPECT_TRUE(save_to_menu_group->IsOptionChecked(kDownloadsFolder));
  histogram_tester.ExpectBucketCount(
      histogram_name, CaptureModeSwitchToDefaultReason::kFolderUnavailable, 1);

  // Select the save-to location to default downloads folder from folder
  // selection dialog and the switch to default reason should be recorded as
  // kUserSelectedFromSettingsMenu.
  controller->SetCustomCaptureFolder(available_custom_folder);
  EXPECT_EQ(controller->GetCurrentCaptureFolder().path,
            available_custom_folder);
  OpenView(test_api.GetSelectFolderMenuItem(), event_generator);
  EXPECT_TRUE(IsFolderSelectionDialogShown());
  auto* dialog_factory = FakeFolderSelectionDialogFactory::Get();
  dialog_factory->AcceptPath(downloads_folder);
  EXPECT_TRUE(save_to_menu_group->IsOptionChecked(kDownloadsFolder));
  histogram_tester.ExpectBucketCount(
      histogram_name,
      CaptureModeSwitchToDefaultReason::kUserSelectedFromFolderSelectionDialog,
      1);

  // Select the save-to location to default downloads folder from settings
  // menu and the switch to default reason should be recorded as
  // `kUserSelectedFromFolderSelectionDialog`.
  controller->SetCustomCaptureFolder(available_custom_folder);
  EXPECT_EQ(controller->GetCurrentCaptureFolder().path,
            available_custom_folder);
  OpenView(test_api.GetDefaultDownloadsOption(), event_generator);
  EXPECT_TRUE(save_to_menu_group->IsOptionChecked(kDownloadsFolder));
  histogram_tester.ExpectBucketCount(
      histogram_name,
      CaptureModeSwitchToDefaultReason::kUserSelectedFromSettingsMenu, 1);
}

INSTANTIATE_TEST_SUITE_P(All, CaptureModeHistogramTest, ::testing::Bool());

}  // namespace ash
