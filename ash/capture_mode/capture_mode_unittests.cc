// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "ash/accessibility/magnifier/docked_magnifier_controller.h"
#include "ash/accessibility/magnifier/magnifier_glass.h"
#include "ash/app_list/app_list_controller_impl.h"
#include "ash/capture_mode/capture_mode_bar_view.h"
#include "ash/capture_mode/capture_mode_button.h"
#include "ash/capture_mode/capture_mode_constants.h"
#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/capture_mode/capture_mode_metrics.h"
#include "ash/capture_mode/capture_mode_session.h"
#include "ash/capture_mode/capture_mode_settings_entry_view.h"
#include "ash/capture_mode/capture_mode_settings_view.h"
#include "ash/capture_mode/capture_mode_source_view.h"
#include "ash/capture_mode/capture_mode_toggle_button.h"
#include "ash/capture_mode/capture_mode_type_view.h"
#include "ash/capture_mode/capture_mode_types.h"
#include "ash/capture_mode/capture_mode_util.h"
#include "ash/capture_mode/stop_recording_button_tray.h"
#include "ash/capture_mode/test_capture_mode_delegate.h"
#include "ash/capture_mode/video_recording_watcher.h"
#include "ash/constants/ash_features.h"
#include "ash/display/cursor_window_controller.h"
#include "ash/display/output_protection_delegate.h"
#include "ash/display/screen_orientation_controller_test_api.h"
#include "ash/display/window_tree_host_manager.h"
#include "ash/public/cpp/capture_mode_test_api.h"
#include "ash/root_window_controller.h"
#include "ash/services/recording/recording_service_test_api.h"
#include "ash/shell.h"
#include "ash/system/status_area_widget.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/desks/desk.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/desks/desks_test_util.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller_test_api.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "chromeos/dbus/power_manager/suspend.pb.h"
#include "components/account_id/account_id.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/viz/privileged/mojom/compositing/frame_sink_video_capture.mojom.h"
#include "ui/aura/client/capture_client.h"
#include "ui/aura/client/capture_client_observer.h"
#include "ui/aura/window_tracker.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/clipboard_buffer.h"
#include "ui/base/cursor/cursor_factory.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/display/types/display_constants.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/message_center_observer.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget_observer.h"
#include "ui/wm/core/window_util.h"

namespace ash {

namespace {

constexpr char kEndRecordingReasonInClamshellHistogramName[] =
    "Ash.CaptureModeController.EndRecordingReason.ClamshellMode";
constexpr char kScreenCaptureNotificationId[] = "capture_mode_notification";

// Returns true if the software-composited cursor is enabled.
bool IsCursorCompositingEnabled() {
  return Shell::Get()
      ->window_tree_host_manager()
      ->cursor_window_controller()
      ->is_cursor_compositing_enabled();
}

void ClickOnView(const views::View* view,
                 ui::test::EventGenerator* event_generator) {
  DCHECK(view);
  DCHECK(event_generator);

  const gfx::Point view_center = view->GetBoundsInScreen().CenterPoint();
  event_generator->MoveMouseTo(view_center);
  event_generator->ClickLeftButton();
}

// Sends a press release key combo |count| times.
void SendKey(ui::KeyboardCode key_code,
             ui::test::EventGenerator* event_generator,
             bool shift_down = false,
             int count = 1) {
  const int flags = shift_down ? ui::EF_SHIFT_DOWN : 0;
  for (int i = 0; i < count; ++i) {
    event_generator->PressKey(key_code, flags);
    event_generator->ReleaseKey(key_code, flags);
  }
}

const message_center::Notification* GetPreviewNotification() {
  const message_center::NotificationList::Notifications notifications =
      message_center::MessageCenter::Get()->GetVisibleNotifications();
  for (const auto* notification : notifications) {
    if (notification->id() == kScreenCaptureNotificationId)
      return notification;
  }
  return nullptr;
}

void ClickNotification(absl::optional<int> button_index) {
  const message_center::Notification* notification = GetPreviewNotification();
  DCHECK(notification);
  notification->delegate()->Click(button_index, absl::nullopt);
}

// Moves the mouse and updates the cursor's display manually to imitate what a
// real mouse move event does in shell.
// TODO(crbug.com/990589): Unit tests should be able to simulate mouse input
// without having to call |CursorManager::SetDisplay|.
void MoveMouseToAndUpdateCursorDisplay(
    const gfx::Point& point,
    ui::test::EventGenerator* event_generator) {
  Shell::Get()->cursor_manager()->SetDisplay(
      display::Screen::GetScreen()->GetDisplayNearestPoint(point));
  event_generator->MoveMouseTo(point);
}

bool IsLayerStackedRightBelow(ui::Layer* layer, ui::Layer* sibling) {
  DCHECK_EQ(layer->parent(), sibling->parent());
  const auto& children = layer->parent()->children();
  const int sibling_index =
      std::find(children.begin(), children.end(), sibling) - children.begin();
  return sibling_index > 0 && children[sibling_index - 1] == layer;
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

// Wrapper for CaptureModeSession that exposes internal state to test functions.
class CaptureModeSessionTestApi {
 public:
  explicit CaptureModeSessionTestApi(CaptureModeSession* session)
      : session_(session) {}
  CaptureModeSessionTestApi(const CaptureModeSessionTestApi&) = delete;
  CaptureModeSessionTestApi& operator=(const CaptureModeSessionTestApi&) =
      delete;
  ~CaptureModeSessionTestApi() = default;

  CaptureModeBarView* capture_mode_bar_view() const {
    return session_->capture_mode_bar_view_;
  }

  CaptureModeSettingsView* capture_mode_settings_view() const {
    return session_->capture_mode_settings_view_;
  }

  views::Widget* capture_mode_settings_widget() const {
    return session_->capture_mode_settings_widget_.get();
  }

  views::Widget* capture_label_widget() const {
    return session_->capture_label_widget_.get();
  }

  views::Widget* dimensions_label_widget() const {
    return session_->dimensions_label_widget_.get();
  }

  const MagnifierGlass& magnifier_glass() const {
    return session_->magnifier_glass_;
  }

  bool IsUsingCustomCursor(CaptureModeType type) const {
    return session_->IsUsingCustomCursor(type);
  }

  CaptureModeSessionFocusCycler::FocusGroup GetCurrentFocusGroup() const {
    return session_->focus_cycler_->current_focus_group_;
  }

  size_t GetCurrentFocusIndex() const {
    return session_->focus_cycler_->focus_index_;
  }

  bool HasFocus() const { return session_->focus_cycler_->HasFocus(); }

 private:
  const CaptureModeSession* const session_;
};

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
    scoped_feature_list_.InitAndEnableFeature(features::kCaptureMode);
    AshTestBase::SetUp();
  }

  CaptureModeBarView* GetCaptureModeBarView() const {
    auto* session = CaptureModeController::Get()->capture_mode_session();
    DCHECK(session);
    return CaptureModeSessionTestApi(session).capture_mode_bar_view();
  }

  views::Widget* GetCaptureModeBarWidget() const {
    auto* session = CaptureModeController::Get()->capture_mode_session();
    DCHECK(session);
    return session->capture_mode_bar_widget();
  }

  views::Widget* GetCaptureModeLabelWidget() const {
    auto* session = CaptureModeController::Get()->capture_mode_session();
    DCHECK(session);
    return CaptureModeSessionTestApi(session).capture_label_widget();
  }

  CaptureModeSettingsView* GetCaptureModeSettingsView() const {
    auto* session = CaptureModeController::Get()->capture_mode_session();
    DCHECK(session);
    return CaptureModeSessionTestApi(session).capture_mode_settings_view();
  }

  views::Widget* GetCaptureModeSettingsWidget() const {
    auto* session = CaptureModeController::Get()->capture_mode_session();
    DCHECK(session);
    return CaptureModeSessionTestApi(session).capture_mode_settings_widget();
  }

  CaptureModeToggleButton* GetImageToggleButton() const {
    auto* controller = CaptureModeController::Get();
    DCHECK(controller->IsActive());
    return GetCaptureModeBarView()->capture_type_view()->image_toggle_button();
  }

  CaptureModeToggleButton* GetVideoToggleButton() const {
    auto* controller = CaptureModeController::Get();
    DCHECK(controller->IsActive());
    return GetCaptureModeBarView()->capture_type_view()->video_toggle_button();
  }

  CaptureModeToggleButton* GetFullscreenToggleButton() const {
    auto* controller = CaptureModeController::Get();
    DCHECK(controller->IsActive());
    return GetCaptureModeBarView()
        ->capture_source_view()
        ->fullscreen_toggle_button();
  }

  CaptureModeToggleButton* GetRegionToggleButton() const {
    auto* controller = CaptureModeController::Get();
    DCHECK(controller->IsActive());
    return GetCaptureModeBarView()
        ->capture_source_view()
        ->region_toggle_button();
  }

  CaptureModeToggleButton* GetWindowToggleButton() const {
    auto* controller = CaptureModeController::Get();
    DCHECK(controller->IsActive());
    return GetCaptureModeBarView()
        ->capture_source_view()
        ->window_toggle_button();
  }

  CaptureModeToggleButton* GetSettingsButton() const {
    auto* controller = CaptureModeController::Get();
    DCHECK(controller->IsActive());
    return GetCaptureModeBarView()->settings_button();
  }

  CaptureModeButton* GetCloseButton() const {
    auto* controller = CaptureModeController::Get();
    DCHECK(controller->IsActive());
    return GetCaptureModeBarView()->close_button();
  }

  views::ToggleButton* GetMicrophoneToggle() const {
    auto* controller = CaptureModeController::Get();
    DCHECK(controller->IsActive());
    DCHECK(GetCaptureModeSettingsView());
    return GetCaptureModeSettingsView()
        ->microphone_view()
        ->toggle_button_view();
  }

  aura::Window* GetDimensionsLabelWindow() const {
    auto* controller = CaptureModeController::Get();
    DCHECK(controller->IsActive());
    auto* widget = CaptureModeSessionTestApi(controller->capture_mode_session())
                       .dimensions_label_widget();
    return widget ? widget->GetNativeWindow() : nullptr;
  }

  absl::optional<gfx::Point> GetMagnifierGlassCenterPoint() const {
    auto* controller = CaptureModeController::Get();
    DCHECK(controller->IsActive());
    auto& magnifier =
        CaptureModeSessionTestApi(controller->capture_mode_session())
            .magnifier_glass();
    if (magnifier.host_widget_for_testing()) {
      return magnifier.host_widget_for_testing()
          ->GetWindowBoundsInScreen()
          .CenterPoint();
    }
    return absl::nullopt;
  }

  CaptureModeController* StartCaptureSession(CaptureModeSource source,
                                             CaptureModeType type) {
    auto* controller = CaptureModeController::Get();
    controller->SetSource(source);
    controller->SetType(type);
    controller->Start(CaptureModeEntryType::kQuickSettings);
    DCHECK(controller->IsActive());
    return controller;
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
    controller->StartVideoRecordingImmediatelyForTesting();
    EXPECT_TRUE(controller->is_recording_in_progress());
    return controller;
  }

  // Select a region by pressing and dragging the mouse.
  void SelectRegion(const gfx::Rect& region, bool release_mouse = true) {
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

  void WaitForCountDownToFinish() {
    auto* controller = CaptureModeController::Get();
    DCHECK(controller->IsActive());
    DCHECK_EQ(controller->type(), CaptureModeType::kVideo);
    while (!controller->is_recording_in_progress()) {
      base::RunLoop run_loop;
      base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
          FROM_HERE, run_loop.QuitClosure(),
          base::TimeDelta::FromMilliseconds(100));
      run_loop.Run();
    }
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

  void WaitForSeconds(int seconds) {
    base::RunLoop loop;
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, base::BindLambdaForTesting([&]() { loop.Quit(); }),
        base::TimeDelta::FromSeconds(seconds));
    loop.Run();
  }

  void WaitForCaptureFileToBeSaved() {
    base::RunLoop run_loop;
    CaptureModeTestApi().SetOnCaptureFileSavedCallback(
        base::BindLambdaForTesting(
            [&run_loop](const base::FilePath& path) { run_loop.Quit(); }));
    run_loop.Run();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

class CaptureSessionWidgetObserver : public views::WidgetObserver {
 public:
  explicit CaptureSessionWidgetObserver(views::Widget* widget) {
    DCHECK(widget);
    observer_.Observe(widget);
  }
  CaptureSessionWidgetObserver(const CaptureSessionWidgetObserver&) = delete;
  CaptureSessionWidgetObserver& operator=(const CaptureSessionWidgetObserver&) =
      delete;
  ~CaptureSessionWidgetObserver() override = default;

  bool GetWidgetDestroyed() const { return !observer_.IsObserving(); }

  // views::WidgetObserver
  void OnWidgetClosing(views::Widget* widget) override {
    DCHECK(observer_.IsObservingSource(widget));
    observer_.Reset();
  }

 private:
  base::ScopedObservation<views::Widget, views::WidgetObserver> observer_{this};
};

class CaptureNotificationWaiter : public message_center::MessageCenterObserver {
 public:
  CaptureNotificationWaiter() {
    message_center::MessageCenter::Get()->AddObserver(this);
  }
  ~CaptureNotificationWaiter() override {
    message_center::MessageCenter::Get()->RemoveObserver(this);
  }

  void Wait() { run_loop_.Run(); }

  // message_center::MessageCenterObserver:
  void OnNotificationAdded(const std::string& notification_id) override {
    if (notification_id == kScreenCaptureNotificationId)
      run_loop_.Quit();
  }

 private:
  base::RunLoop run_loop_;
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
  CaptureSessionWidgetObserver observer(GetCaptureModeBarWidget());
  EXPECT_FALSE(observer.GetWidgetDestroyed());
  controller->Stop();
  EXPECT_FALSE(controller->IsActive());
  EXPECT_FALSE(controller->capture_mode_session());
  // The Widget should have been destroyed by now.
  EXPECT_TRUE(observer.GetWidgetDestroyed());
}

TEST_F(CaptureModeTest, StartWithMostRecentTypeAndSource) {
  auto* controller = CaptureModeController::Get();
  controller->SetSource(CaptureModeSource::kFullscreen);
  controller->SetType(CaptureModeType::kVideo);
  controller->Start(CaptureModeEntryType::kQuickSettings);
  EXPECT_TRUE(controller->IsActive());

  EXPECT_FALSE(GetImageToggleButton()->GetToggled());
  EXPECT_TRUE(GetVideoToggleButton()->GetToggled());
  EXPECT_TRUE(GetFullscreenToggleButton()->GetToggled());
  EXPECT_FALSE(GetRegionToggleButton()->GetToggled());
  EXPECT_FALSE(GetWindowToggleButton()->GetToggled());

  ClickOnView(GetCloseButton(), GetEventGenerator());
  EXPECT_FALSE(controller->IsActive());
}

TEST_F(CaptureModeTest, ChangeTypeAndSourceFromUI) {
  auto* controller = CaptureModeController::Get();
  controller->Start(CaptureModeEntryType::kQuickSettings);
  EXPECT_TRUE(controller->IsActive());

  EXPECT_TRUE(GetImageToggleButton()->GetToggled());
  EXPECT_FALSE(GetVideoToggleButton()->GetToggled());
  auto* event_generator = GetEventGenerator();
  ClickOnView(GetVideoToggleButton(), event_generator);
  EXPECT_FALSE(GetImageToggleButton()->GetToggled());
  EXPECT_TRUE(GetVideoToggleButton()->GetToggled());
  EXPECT_EQ(controller->type(), CaptureModeType::kVideo);

  ClickOnView(GetWindowToggleButton(), event_generator);
  EXPECT_FALSE(GetFullscreenToggleButton()->GetToggled());
  EXPECT_FALSE(GetRegionToggleButton()->GetToggled());
  EXPECT_TRUE(GetWindowToggleButton()->GetToggled());
  EXPECT_EQ(controller->source(), CaptureModeSource::kWindow);

  ClickOnView(GetFullscreenToggleButton(), event_generator);
  EXPECT_TRUE(GetFullscreenToggleButton()->GetToggled());
  EXPECT_FALSE(GetRegionToggleButton()->GetToggled());
  EXPECT_FALSE(GetWindowToggleButton()->GetToggled());
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
  EXPECT_EQ(ui::mojom::CursorType::kPointer,
            Shell::Get()->cursor_manager()->GetCursor().type());
  WaitForCountDownToFinish();
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
  struct {
    std::string trace;
    gfx::Point drag_point;
    // True if horizontal direction (left, right). Height stays the same while
    // dragging if true, width stays the same while dragging if false.
    bool horizontal;
    // The edge that stays the same while dragging. It is the opposite edge to
    // |drag_point|. For example, if |drag_point| is the left center of
    // |target_region|, then |anchor_edge| is the right edge.
    int anchor_edge;
  } kDragEdgeCases[] = {
      {"left", target_region.left_center(), true, target_region.right()},
      {"top", target_region.top_center(), false, target_region.bottom()},
      {"right", target_region.right_center(), true, target_region.x()},
      {"bottom", target_region.bottom_center(), false, target_region.y()},
  };

  // Drag to a couple of points that change both x and y. In all these cases,
  // only the width or height should change.
  auto drag_test_points = {gfx::Point(150, 150), gfx::Point(350, 350),
                           gfx::Point(450, 450)};
  auto* event_generator = GetEventGenerator();
  for (auto test_case : kDragEdgeCases) {
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
  SelectRegion(gfx::Rect(5, 5, 795, 795));
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
  EXPECT_EQ(absl::nullopt, GetMagnifierGlassCenterPoint());

  auto check_magnifier_shows_properly = [this](const gfx::Point& origin,
                                               const gfx::Point& destination,
                                               bool should_show_magnifier) {
    // If |should_show_magnifier|, check that the magnifying glass is centered
    // on the mouse after press and during drag, and that the cursor is hidden.
    // If not |should_show_magnifier|, check that the magnifying glass never
    // shows. Should always be not visible when mouse button is released.
    auto* event_generator = GetEventGenerator();
    absl::optional<gfx::Point> expected_origin =
        should_show_magnifier ? absl::make_optional(origin) : absl::nullopt;
    absl::optional<gfx::Point> expected_destination =
        should_show_magnifier ? absl::make_optional(destination)
                              : absl::nullopt;

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
    EXPECT_EQ(absl::nullopt, GetMagnifierGlassCenterPoint());
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
  } kFineTunePositions[] = {{"top_left", FineTunePosition::kTopLeft},
                            {"top_right", FineTunePosition::kTopRight},
                            {"bottom_right", FineTunePosition::kBottomRight},
                            {"bottom_left", FineTunePosition::kBottomLeft}};
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
  capture_region.SetRect(100, 700, 600, 790);
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
  region_bounds.Inset(-20, -20);
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
                         20, 800 - capture_bar_bounds.bottom()));
  EXPECT_FALSE(capture_bar_bounds.Intersects(
      GetCaptureModeLabelWidget()->GetWindowBoundsInScreen()));

  // Create a thin region that is vertical as tall as the display, and at the
  // left edge of the display. The capture label button should be right of the
  // region.
  event_generator->set_current_screen_location(gfx::Point());
  event_generator->ClickLeftButton();
  SelectRegion(gfx::Rect(20, 800));
  EXPECT_GT(GetCaptureModeLabelWidget()->GetWindowBoundsInScreen().x(), 20);
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

// Tests that the capture bar is located on the root with the cursor when
// starting capture mode.
TEST_F(CaptureModeTest, MultiDisplayCaptureBarInitialLocation) {
  UpdateDisplay("800x700,801+0-800x700");

  auto* event_generator = GetEventGenerator();
  MoveMouseToAndUpdateCursorDisplay(gfx::Point(1000, 500), event_generator);

  auto* controller = StartImageRegionCapture();
  EXPECT_TRUE(gfx::Rect(801, 0, 800, 800)
                  .Contains(GetCaptureModeBarView()->GetBoundsInScreen()));
  controller->Stop();

  MoveMouseToAndUpdateCursorDisplay(gfx::Point(100, 500), event_generator);
  StartImageRegionCapture();
  EXPECT_TRUE(gfx::Rect(800, 800).Contains(
      GetCaptureModeBarView()->GetBoundsInScreen()));
}

// Tests behavior of a capture mode session if the active display is removed.
TEST_F(CaptureModeTest, DisplayRemoval) {
  UpdateDisplay("800x700,801+0-800x700");

  // Start capture mode on the secondary display.
  MoveMouseToAndUpdateCursorDisplay(gfx::Point(1000, 500), GetEventGenerator());
  auto* controller = StartImageRegionCapture();
  auto* session = controller->capture_mode_session();
  EXPECT_TRUE(gfx::Rect(801, 0, 800, 800)
                  .Contains(GetCaptureModeBarView()->GetBoundsInScreen()));
  ASSERT_EQ(Shell::GetAllRootWindows()[1], session->current_root());

  RemoveSecondaryDisplay();

  // Tests that the capture mode bar is now on the primary display.
  EXPECT_TRUE(gfx::Rect(800, 800).Contains(
      GetCaptureModeBarView()->GetBoundsInScreen()));
  ASSERT_EQ(Shell::GetAllRootWindows()[0], session->current_root());
}

// Tests that using fullscreen or window source, moving the mouse across
// displays will change the root window of the capture session.
TEST_F(CaptureModeTest, MultiDisplayFullscreenOrWindowSourceRootWindow) {
  UpdateDisplay("800x700,801+0-800x700");
  ASSERT_EQ(2u, Shell::GetAllRootWindows().size());

  auto* event_generator = GetEventGenerator();
  MoveMouseToAndUpdateCursorDisplay(gfx::Point(100, 500), event_generator);

  for (auto source :
       {CaptureModeSource::kFullscreen, CaptureModeSource::kWindow}) {
    SCOPED_TRACE(source == CaptureModeSource::kFullscreen ? "Fullscreen source"
                                                          : "Window source");

    auto* controller = StartCaptureSession(CaptureModeSource::kFullscreen,
                                           CaptureModeType::kImage);
    auto* session = controller->capture_mode_session();
    EXPECT_EQ(Shell::GetAllRootWindows()[0], session->current_root());

    MoveMouseToAndUpdateCursorDisplay(gfx::Point(1000, 500), event_generator);
    EXPECT_EQ(Shell::GetAllRootWindows()[1], session->current_root());

    MoveMouseToAndUpdateCursorDisplay(gfx::Point(100, 500), event_generator);
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
  MoveMouseToAndUpdateCursorDisplay(gfx::Point(100, 500), event_generator);

  auto* controller = StartImageRegionCapture();
  auto* session = controller->capture_mode_session();
  EXPECT_EQ(Shell::GetAllRootWindows()[0], session->current_root());

  // Tests that moving the mouse to the secondary display does not change the
  // root.
  MoveMouseToAndUpdateCursorDisplay(gfx::Point(1000, 500), event_generator);
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
  using ui::mojom::CursorType;

  auto* cursor_manager = Shell::Get()->cursor_manager();
  CursorType original_cursor_type = cursor_manager->GetCursor().type();
  EXPECT_FALSE(cursor_manager->IsCursorLocked());
  EXPECT_EQ(CursorType::kPointer, original_cursor_type);

  auto* event_generator = GetEventGenerator();
  auto* controller = StartImageRegionCapture();
  EXPECT_TRUE(cursor_manager->IsCursorLocked());
  event_generator->MoveMouseTo(gfx::Point(175, 175));
  EXPECT_TRUE(cursor_manager->IsCursorVisible());
  EXPECT_EQ(CursorType::kCell, cursor_manager->GetCursor().type());

  const gfx::Rect target_region(gfx::Rect(200, 200, 200, 200));
  SelectRegion(target_region);

  // Makes sure that the cursor is updated when the user releases the region
  // select and is still hovering in the same location.
  EXPECT_EQ(CursorType::kSouthEastResize, cursor_manager->GetCursor().type());

  // Verify that all of the |FineTunePosition| locations have the correct cursor
  // when hovered over.
  event_generator->MoveMouseTo(target_region.origin());
  EXPECT_EQ(CursorType::kNorthWestResize, cursor_manager->GetCursor().type());
  event_generator->MoveMouseTo(target_region.top_center());
  EXPECT_EQ(CursorType::kNorthSouthResize, cursor_manager->GetCursor().type());
  event_generator->MoveMouseTo(target_region.top_right());
  EXPECT_EQ(CursorType::kNorthEastResize, cursor_manager->GetCursor().type());
  event_generator->MoveMouseTo(target_region.right_center());
  EXPECT_EQ(CursorType::kEastWestResize, cursor_manager->GetCursor().type());
  event_generator->MoveMouseTo(target_region.bottom_right());
  EXPECT_EQ(CursorType::kSouthEastResize, cursor_manager->GetCursor().type());
  event_generator->MoveMouseTo(target_region.bottom_center());
  EXPECT_EQ(CursorType::kNorthSouthResize, cursor_manager->GetCursor().type());
  event_generator->MoveMouseTo(target_region.bottom_left());
  EXPECT_EQ(CursorType::kSouthWestResize, cursor_manager->GetCursor().type());
  event_generator->MoveMouseTo(target_region.left_center());
  EXPECT_EQ(CursorType::kEastWestResize, cursor_manager->GetCursor().type());

  // Tests that within the bounds of the selected region, the cursor is a hand
  // when hovering over the capture button, otherwise it is a multi-directional
  // move cursor.
  event_generator->MoveMouseTo(gfx::Point(250, 250));
  EXPECT_EQ(CursorType::kMove, cursor_manager->GetCursor().type());
  event_generator->MoveMouseTo(target_region.CenterPoint());
  EXPECT_EQ(CursorType::kHand, cursor_manager->GetCursor().type());

  // Tests that the cursor changes to a cell type when hovering over the
  // unselected region.
  event_generator->MoveMouseTo(gfx::Point(50, 50));
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
  EXPECT_EQ(original_cursor_type, cursor_manager->GetCursor().type());

  // Tests that on changing back to region capture mode, the cursor becomes
  // locked, and is still a pointer type over the bar, whilst a cell cursor
  // otherwise (not over the selected region).
  event_generator->MoveMouseTo(
      GetRegionToggleButton()->GetBoundsInScreen().CenterPoint());
  original_cursor_type = cursor_manager->GetCursor().type();
  event_generator->ClickLeftButton();
  EXPECT_TRUE(cursor_manager->IsCursorLocked());
  EXPECT_EQ(CursorType::kPointer, cursor_manager->GetCursor().type());
  // Tests that clicking on the button again doesn't change the cursor.
  event_generator->ClickLeftButton();
  EXPECT_EQ(CursorType::kPointer, cursor_manager->GetCursor().type());
  event_generator->MoveMouseTo(gfx::Point(50, 50));
  EXPECT_EQ(CursorType::kCell, cursor_manager->GetCursor().type());

  // Enter tablet mode, the cursor should be hidden.
  TabletModeControllerTestApi tablet_mode_controller_test_api;
  // To avoid flaky failures due to mouse devices blocking entering tablet mode,
  // we detach all mouse devices. This shouldn't affect testing the cursor
  // status.
  tablet_mode_controller_test_api.DetachAllMice();
  tablet_mode_controller_test_api.EnterTabletMode();
  EXPECT_FALSE(cursor_manager->IsCursorVisible());
  EXPECT_TRUE(cursor_manager->IsCursorLocked());

  // Move mouse but it should still be invisible.
  event_generator->MoveMouseTo(gfx::Point(100, 100));
  EXPECT_FALSE(cursor_manager->IsCursorVisible());
  EXPECT_TRUE(cursor_manager->IsCursorLocked());

  // Return to clamshell mode, mouse should appear again.
  tablet_mode_controller_test_api.LeaveTabletMode();
  EXPECT_TRUE(cursor_manager->IsCursorVisible());
  EXPECT_EQ(CursorType::kCell, cursor_manager->GetCursor().type());

  // Tests that when exiting capture mode that the cursor is restored to its
  // original state.
  controller->Stop();
  EXPECT_FALSE(controller->IsActive());
  EXPECT_FALSE(cursor_manager->IsCursorLocked());
  EXPECT_EQ(original_cursor_type, cursor_manager->GetCursor().type());
}

TEST_F(CaptureModeTest, FullscreenCursorStates) {
  using ui::mojom::CursorType;

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
  event_generator->MoveMouseTo(
      test_api.capture_label_widget()->GetWindowBoundsInScreen().CenterPoint());
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
  TabletModeControllerTestApi tablet_mode_controller_test_api;
  // To avoid flaky failures due to mouse devices blocking entering tablet mode,
  // we detach all mouse devices. This shouldn't affect testing the cursor
  // status.
  tablet_mode_controller_test_api.DetachAllMice();
  tablet_mode_controller_test_api.EnterTabletMode();
  EXPECT_FALSE(cursor_manager->IsCursorVisible());
  EXPECT_TRUE(cursor_manager->IsCursorLocked());

  // Exit tablet mode, the cursor should appear again.
  tablet_mode_controller_test_api.LeaveTabletMode();
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
  using ui::mojom::CursorType;

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

  // If the mouse is not above the window, use the original mouse cursor.
  event_generator->MoveMouseTo(gfx::Point(300, 300));
  EXPECT_FALSE(cursor_manager->IsCursorLocked());
  EXPECT_TRUE(cursor_manager->IsCursorVisible());
  EXPECT_EQ(original_cursor_type, cursor_manager->GetCursor().type());

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
  EXPECT_FALSE(cursor_manager->IsCursorLocked());
  EXPECT_TRUE(cursor_manager->IsCursorVisible());
  EXPECT_EQ(original_cursor_type, cursor_manager->GetCursor().type());

  // Move above the window again, the cursor should change back to the video
  // record icon.
  event_generator->MoveMouseTo(gfx::Point(150, 150));
  EXPECT_TRUE(cursor_manager->IsCursorLocked());
  EXPECT_TRUE(cursor_manager->IsCursorVisible());
  EXPECT_EQ(CursorType::kCustom, cursor_manager->GetCursor().type());
  EXPECT_TRUE(test_api.IsUsingCustomCursor(CaptureModeType::kVideo));

  // Enter tablet mode, the cursor should be hidden.
  TabletModeControllerTestApi tablet_mode_controller_test_api;
  // To avoid flaky failures due to mouse devices blocking entering tablet mode,
  // we detach all mouse devices. This shouldn't affect testing the cursor
  // status.
  tablet_mode_controller_test_api.DetachAllMice();
  tablet_mode_controller_test_api.EnterTabletMode();
  EXPECT_FALSE(cursor_manager->IsCursorVisible());
  EXPECT_TRUE(cursor_manager->IsCursorLocked());

  // Exit tablet mode, the cursor should appear again.
  tablet_mode_controller_test_api.LeaveTabletMode();
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
  using ui::mojom::CursorType;

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
  // should revert back to the original cursor.
  window2.reset();
  EXPECT_FALSE(cursor_manager->IsCursorLocked());
  EXPECT_TRUE(cursor_manager->IsCursorVisible());
  EXPECT_EQ(original_cursor_type, cursor_manager->GetCursor().type());

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
  using ui::mojom::CursorType;

  UpdateDisplay("600x400");
  const int64_t display_id =
      display::Screen::GetScreen()->GetPrimaryDisplay().id();
  display::Display::SetInternalDisplayId(display_id);
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
  CaptureModeSessionTestApi test_api(controller->capture_mode_session());
  EXPECT_TRUE(test_api.IsUsingCustomCursor(CaptureModeType::kImage));

  // Rotate the screen.
  orientation_test_api.SetDisplayRotation(
      display::Display::ROTATE_270, display::Display::RotationSource::ACTIVE);
  const ui::Cursor portrait_cursor = cursor_manager->GetCursor();
  EXPECT_TRUE(test_api.IsUsingCustomCursor(CaptureModeType::kImage));
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

  WaitForCountDownToFinish();
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
  WaitForCountDownToFinish();
  EXPECT_FALSE(controller->IsActive());
  EXPECT_TRUE(controller->is_recording_in_progress());
}

// Tests that metrics are recorded properly for capture mode entry points.
TEST_F(CaptureModeTest, CaptureModeEntryPointHistograms) {
  constexpr char kClamshellHistogram[] =
      "Ash.CaptureModeController.EntryPoint.ClamshellMode";
  constexpr char kTabletHistogram[] =
      "Ash.CaptureModeController.EntryPoint.TabletMode";
  base::HistogramTester histogram_tester;

  auto* controller = CaptureModeController::Get();

  // Test the various entry points in clamshell mode.
  controller->Start(CaptureModeEntryType::kAccelTakeWindowScreenshot);
  histogram_tester.ExpectBucketCount(
      kClamshellHistogram, CaptureModeEntryType::kAccelTakeWindowScreenshot, 1);
  controller->Stop();

  controller->Start(CaptureModeEntryType::kAccelTakePartialScreenshot);
  histogram_tester.ExpectBucketCount(
      kClamshellHistogram, CaptureModeEntryType::kAccelTakePartialScreenshot,
      1);
  controller->Stop();

  controller->Start(CaptureModeEntryType::kQuickSettings);
  histogram_tester.ExpectBucketCount(kClamshellHistogram,
                                     CaptureModeEntryType::kQuickSettings, 1);
  controller->Stop();

  controller->Start(CaptureModeEntryType::kStylusPalette);
  histogram_tester.ExpectBucketCount(kClamshellHistogram,
                                     CaptureModeEntryType::kStylusPalette, 1);
  controller->Stop();

  // Enter tablet mode and test the various entry points in tablet mode.
  auto* tablet_mode_controller = Shell::Get()->tablet_mode_controller();
  tablet_mode_controller->SetEnabledForTest(true);
  ASSERT_TRUE(tablet_mode_controller->InTabletMode());

  controller->Start(CaptureModeEntryType::kAccelTakeWindowScreenshot);
  histogram_tester.ExpectBucketCount(
      kTabletHistogram, CaptureModeEntryType::kAccelTakeWindowScreenshot, 1);
  controller->Stop();

  controller->Start(CaptureModeEntryType::kAccelTakePartialScreenshot);
  histogram_tester.ExpectBucketCount(
      kTabletHistogram, CaptureModeEntryType::kAccelTakePartialScreenshot, 1);
  controller->Stop();

  controller->Start(CaptureModeEntryType::kQuickSettings);
  histogram_tester.ExpectBucketCount(kTabletHistogram,
                                     CaptureModeEntryType::kQuickSettings, 1);
  controller->Stop();

  controller->Start(CaptureModeEntryType::kStylusPalette);
  histogram_tester.ExpectBucketCount(kTabletHistogram,
                                     CaptureModeEntryType::kStylusPalette, 1);
  controller->Stop();

  // Check total counts for each histogram to ensure calls aren't counted in
  // multiple buckets.
  histogram_tester.ExpectTotalCount(kClamshellHistogram, 4);
  histogram_tester.ExpectTotalCount(kTabletHistogram, 4);

  // Check that histogram isn't counted if we don't actually enter capture mode.
  controller->Start(CaptureModeEntryType::kAccelTakePartialScreenshot);
  histogram_tester.ExpectBucketCount(
      kTabletHistogram, CaptureModeEntryType::kAccelTakePartialScreenshot, 2);
  controller->Start(CaptureModeEntryType::kAccelTakePartialScreenshot);
  histogram_tester.ExpectBucketCount(
      kTabletHistogram, CaptureModeEntryType::kAccelTakePartialScreenshot, 2);
}

// Verifies that the video notification will show the same thumbnail image as
// sent by recording service.
TEST_F(CaptureModeTest, VideoNotificationThumbnail) {
  auto* controller = StartCaptureSession(CaptureModeSource::kFullscreen,
                                         CaptureModeType::kVideo);
  controller->StartVideoRecordingImmediatelyForTesting();
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

TEST_F(CaptureModeTest, WindowRecordingCaptureId) {
  auto window = CreateTestWindow(gfx::Rect(200, 200));
  StartCaptureSession(CaptureModeSource::kWindow, CaptureModeType::kVideo);

  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseToCenterOf(window.get());
  auto* controller = CaptureModeController::Get();
  controller->StartVideoRecordingImmediatelyForTesting();
  EXPECT_TRUE(controller->is_recording_in_progress());

  // The window should have a valid capture ID.
  EXPECT_TRUE(window->subtree_capture_id().is_valid());

  // Once recording ends, the window should no longer be marked as capturable.
  controller->EndVideoRecording(EndRecordingReason::kStopRecordingButton);
  EXPECT_FALSE(controller->is_recording_in_progress());
  EXPECT_FALSE(window->subtree_capture_id().is_valid());
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
  controller->StartVideoRecordingImmediatelyForTesting();
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
            test_delegate->GetCurrentFrameSinkSize());
  EXPECT_EQ(window->bounds().size(), test_delegate->GetCurrentVideoSize());

  // Moving a window to a different display should be propagated to the service,
  // with the new root's frame sink ID, and the new root's size.
  window_util::MoveWindowToDisplay(window.get(),
                                   roots[1]->GetHost()->GetDisplayId());
  test_api.FlushRecordingServiceForTesting();
  ASSERT_EQ(window->GetRootWindow(), roots[1]);
  EXPECT_EQ(roots[1]->GetFrameSinkId(), test_delegate->GetCurrentFrameSinkId());
  EXPECT_EQ(roots[1]->bounds().size(),
            test_delegate->GetCurrentFrameSinkSize());
  EXPECT_EQ(window->bounds().size(), test_delegate->GetCurrentVideoSize());

  // The shield layer should move with the window, and maintain the stacking
  // below the window's layer.
  EXPECT_EQ(shield_layer->parent(), window->layer()->parent());
  EXPECT_TRUE(IsLayerStackedRightBelow(shield_layer, window->layer()));
  EXPECT_EQ(shield_layer->bounds(), roots[1]->bounds());
}

TEST_F(CaptureModeTest, WindowResizing) {
  UpdateDisplay("700x600");
  auto window = CreateTestWindow(gfx::Rect(200, 200));
  auto* controller =
      StartCaptureSession(CaptureModeSource::kWindow, CaptureModeType::kVideo);

  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseToCenterOf(window.get());
  controller->StartVideoRecordingImmediatelyForTesting();
  EXPECT_TRUE(controller->is_recording_in_progress());
  auto* test_delegate =
      static_cast<TestCaptureModeDelegate*>(controller->delegate_for_testing());

  CaptureModeTestApi test_api;
  test_api.FlushRecordingServiceForTesting();
  EXPECT_EQ(gfx::Size(200, 200), test_delegate->GetCurrentVideoSize());
  EXPECT_EQ(gfx::Size(700, 600), test_delegate->GetCurrentFrameSinkSize());

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
  EXPECT_EQ(gfx::Size(700, 600), test_delegate->GetCurrentFrameSinkSize());

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
  controller->StartVideoRecordingImmediatelyForTesting();
  EXPECT_TRUE(controller->is_recording_in_progress());

  // Initially the frame sink size matches the un-rotated display size in DIPs,
  // but the video size matches the size of the crop region.
  CaptureModeTestApi test_api;
  test_api.FlushRecordingServiceForTesting();
  auto* test_delegate =
      static_cast<TestCaptureModeDelegate*>(controller->delegate_for_testing());
  EXPECT_EQ(gfx::Size(600, 800), test_delegate->GetCurrentFrameSinkSize());
  EXPECT_EQ(gfx::Size(100, 200), test_delegate->GetCurrentVideoSize());

  // Rotate by 90 degree, the frame sink size should be updated to match that.
  // The video size should remain unaffected.
  Shell::Get()->display_manager()->SetDisplayRotation(
      WindowTreeHostManager::GetPrimaryDisplayId(), display::Display::ROTATE_90,
      display::Display::RotationSource::USER);
  test_api.FlushRecordingServiceForTesting();
  EXPECT_EQ(gfx::Size(800, 600), test_delegate->GetCurrentFrameSinkSize());
  EXPECT_EQ(gfx::Size(100, 200), test_delegate->GetCurrentVideoSize());
}

// Tests that the video frames delivered to the service for recorded windows are
// valid (i.e. they have the correct size, and suffer from no letterboxing, even
// when the window gets resized).
// This is a regression test for https://crbug.com/1214023.
TEST_F(CaptureModeTest, VerifyWindowRecordingVideoFrames) {
  auto window = CreateTestWindow(gfx::Rect(100, 50, 200, 200));
  StartCaptureSession(CaptureModeSource::kWindow, CaptureModeType::kVideo);

  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseToCenterOf(window.get());
  auto* controller = CaptureModeController::Get();
  controller->StartVideoRecordingImmediatelyForTesting();
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

  // Starts video recording from the capture mode source set by the test param.
  void StartRecording() {
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
  StartRecording();

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

TEST_P(CaptureModeHdcpTest, ProtectedWindowDestruction) {
  auto window_2 = CreateTestWindow(gfx::Rect(100, 50));
  OutputProtectionDelegate protection_delegate_2(window_2.get());
  protection_delegate_2.SetProtection(display::CONTENT_PROTECTION_METHOD_HDCP,
                                      base::DoNothing());

  StartSessionForVideo();
  StartRecording();

  // Recording cannot start because of another protected window on the screen,
  // except when we're capturing a different |window_|.
  auto* controller = CaptureModeController::Get();
  EXPECT_FALSE(controller->IsActive());
  if (GetParam() == CaptureModeSource::kWindow) {
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
  StartRecording();

  EXPECT_FALSE(controller->IsActive());
  EXPECT_TRUE(controller->is_recording_in_progress());
}

TEST_P(CaptureModeHdcpTest, WindowBecomesProtectedBeforeRecording) {
  protection_delegate_->SetProtection(display::CONTENT_PROTECTION_METHOD_HDCP,
                                      base::DoNothing());
  StartSessionForVideo();
  StartRecording();

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
  MoveMouseToAndUpdateCursorDisplay(roots[1]->GetBoundsInScreen().CenterPoint(),
                                    event_generator);
  StartSessionForVideo();
  // Also, make sure the selected region is in the secondary display.
  auto* controller = CaptureModeController::Get();
  EXPECT_EQ(controller->capture_mode_session()->current_root(), roots[1]);
  StartRecording();

  // Recording should be able to start (since the protected window is on the
  // first display) unless the protected window itself is the one being
  // recorded.
  if (GetParam() == CaptureModeSource::kWindow) {
    EXPECT_FALSE(controller->is_recording_in_progress());
  } else {
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
  controller->StartVideoRecordingImmediatelyForTesting();
  EXPECT_TRUE(controller->is_recording_in_progress());

  // The window should have a valid capture ID.
  EXPECT_TRUE(window->subtree_capture_id().is_valid());

  // Closing the window being recorded should end video recording.
  base::HistogramTester histogram_tester;
  window.reset();

  auto* stop_recording_button = Shell::GetPrimaryRootWindowController()
                                    ->GetStatusAreaWidget()
                                    ->stop_recording_button_tray();
  EXPECT_FALSE(stop_recording_button->visible_preferred());
  EXPECT_FALSE(controller->is_recording_in_progress());
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
  MoveMouseToAndUpdateCursorDisplay(window->GetBoundsInScreen().CenterPoint(),
                                    event_generator);
  auto* controller = CaptureModeController::Get();
  controller->StartVideoRecordingImmediatelyForTesting();
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
  controller->StartVideoRecordingImmediatelyForTesting();
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
  controller->StartVideoRecordingImmediatelyForTesting();
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
  MoveMouseToAndUpdateCursorDisplay(roots[1]->GetBoundsInScreen().CenterPoint(),
                                    event_generator);
  auto* controller = CaptureModeController::Get();
  controller->StartVideoRecordingImmediatelyForTesting();
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
  controller->StartVideoRecordingImmediatelyForTesting();
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
  auto* tablet_mode_controller = Shell::Get()->tablet_mode_controller();
  tablet_mode_controller->SetEnabledForTest(true);
  ASSERT_TRUE(tablet_mode_controller->InTabletMode());

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
  SendKey(ui::VKEY_ESCAPE, event_generator);
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
  auto* tablet_mode_controller = Shell::Get()->tablet_mode_controller();
  tablet_mode_controller->SetEnabledForTest(true);
  ASSERT_TRUE(tablet_mode_controller->InTabletMode());
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
  WaitForCountDownToFinish();
  EXPECT_FALSE(controller->IsActive());
}

// Tests that metrics are recorded properly for capture mode configurations when
// taking a screenshot.
TEST_F(CaptureModeTest, ScreenshotConfigurationHistogram) {
  constexpr char kClamshellHistogram[] =
      "Ash.CaptureModeController.CaptureConfiguration.ClamshellMode";
  constexpr char kTabletHistogram[] =
      "Ash.CaptureModeController.CaptureConfiguration.TabletMode";
  base::HistogramTester histogram_tester;
  // Use a set display size as we will be choosing points in this test.
  UpdateDisplay("800x700");

  // Create a window for window captures later.
  std::unique_ptr<aura::Window> window1(
      CreateTestWindow(gfx::Rect(600, 600, 100, 100)));

  // Perform a fullscreen screenshot.
  auto* controller = StartCaptureSession(CaptureModeSource::kFullscreen,
                                         CaptureModeType::kImage);
  controller->PerformCapture();
  histogram_tester.ExpectBucketCount(
      kClamshellHistogram, CaptureModeConfiguration::kFullscreenScreenshot, 1);

  // Perform a region screenshot.
  controller =
      StartCaptureSession(CaptureModeSource::kRegion, CaptureModeType::kImage);
  const gfx::Rect capture_region(200, 200, 400, 400);
  SelectRegion(capture_region);
  controller->PerformCapture();
  histogram_tester.ExpectBucketCount(
      kClamshellHistogram, CaptureModeConfiguration::kRegionScreenshot, 1);

  // Perform a window screenshot.
  controller =
      StartCaptureSession(CaptureModeSource::kWindow, CaptureModeType::kImage);
  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseToCenterOf(window1.get());
  EXPECT_EQ(window1.get(),
            controller->capture_mode_session()->GetSelectedWindow());
  controller->PerformCapture();
  histogram_tester.ExpectBucketCount(
      kClamshellHistogram, CaptureModeConfiguration::kWindowScreenshot, 1);

  // Switch to tablet mode.
  auto* tablet_mode_controller = Shell::Get()->tablet_mode_controller();
  tablet_mode_controller->SetEnabledForTest(true);
  ASSERT_TRUE(tablet_mode_controller->InTabletMode());

  // Perform a fullscreen screenshot.
  controller = StartCaptureSession(CaptureModeSource::kFullscreen,
                                   CaptureModeType::kImage);
  controller->PerformCapture();
  histogram_tester.ExpectBucketCount(
      kTabletHistogram, CaptureModeConfiguration::kFullscreenScreenshot, 1);

  // Perform a region screenshot.
  controller =
      StartCaptureSession(CaptureModeSource::kRegion, CaptureModeType::kImage);
  controller->PerformCapture();
  histogram_tester.ExpectBucketCount(
      kTabletHistogram, CaptureModeConfiguration::kRegionScreenshot, 1);

  // Perform a window screenshot.
  controller =
      StartCaptureSession(CaptureModeSource::kWindow, CaptureModeType::kImage);
  event_generator->MoveMouseToCenterOf(window1.get());
  EXPECT_EQ(window1.get(),
            controller->capture_mode_session()->GetSelectedWindow());
  controller->PerformCapture();
  histogram_tester.ExpectBucketCount(
      kTabletHistogram, CaptureModeConfiguration::kWindowScreenshot, 1);
}

// Tests that there is no crash when touching the capture label widget in tablet
// mode when capturing a window. Regression test for https://crbug.com/1152938.
TEST_F(CaptureModeTest, TabletTouchCaptureLabelWidgetWindowMode) {
  TabletModeControllerTestApi tablet_mode_controller_test_api;
  tablet_mode_controller_test_api.DetachAllMice();
  tablet_mode_controller_test_api.EnterTabletMode();

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

  // Rotate the primary display by 90 degrees. Test that the region and capture
  // bar fit within the rotated bounds, and the capture label widget is still
  // centered in the region.
  Shell::Get()->display_manager()->SetDisplayRotation(
      WindowTreeHostManager::GetPrimaryDisplayId(), display::Display::ROTATE_90,
      display::Display::RotationSource::USER);
  const gfx::Rect rotated_root_bounds(600, 1200);
  EXPECT_TRUE(rotated_root_bounds.Contains(controller->user_capture_region()));
  EXPECT_TRUE(rotated_root_bounds.Contains(
      GetCaptureModeBarView()->GetBoundsInScreen()));
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
  MoveMouseToAndUpdateCursorDisplay(gfx::Point(700, 300), event_generator);
  auto* controller = StartImageRegionCapture();
  SelectRegion(gfx::Rect(1200, 400));
  EXPECT_EQ(gfx::Rect(1200, 400), controller->user_capture_region());
  controller->Stop();

  // Make the secondary display the targeted display. Test that the region has
  // shrunk to fit the display.
  MoveMouseToAndUpdateCursorDisplay(gfx::Point(1500, 300), event_generator);
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
  SendKey(ui::VKEY_TAB, event_generator, /*shift_down=*/false, /*count=*/4);
  EXPECT_EQ(FocusGroup::kTypeSource, test_api.GetCurrentFocusGroup());
  EXPECT_EQ(4u, test_api.GetCurrentFocusIndex());

  // Tab once to focus the settings and close buttons group on the capture bar.
  SendKey(ui::VKEY_TAB, event_generator);
  EXPECT_EQ(FocusGroup::kSettingsClose, test_api.GetCurrentFocusGroup());
  EXPECT_EQ(0u, test_api.GetCurrentFocusIndex());

  // Shift tab to focus the last source button again.
  SendKey(ui::VKEY_TAB, event_generator, /*shift_down=*/true);
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

// Tests that pressing space on a focused button will click it.
TEST_F(CaptureModeTest, KeyboardNavigationSpaceToClickButtons) {
  auto* controller = StartImageRegionCapture();
  SelectRegion(gfx::Rect(200, 200));

  auto* event_generator = GetEventGenerator();

  // Tab to the button which changes the capture type to video and hit space.
  SendKey(ui::VKEY_TAB, event_generator, /*shift_down=*/false, 2);
  SendKey(ui::VKEY_SPACE, event_generator);
  EXPECT_EQ(CaptureModeType::kVideo, controller->type());

  // Shift tab and space to change the capture type back to image.
  SendKey(ui::VKEY_TAB, event_generator, /*shift_down=*/true);
  SendKey(ui::VKEY_SPACE, event_generator);
  EXPECT_EQ(CaptureModeType::kImage, controller->type());

  // Tab to the fullscreen button and hit space.
  SendKey(ui::VKEY_TAB, event_generator, /*shift_down=*/false, 2);
  SendKey(ui::VKEY_SPACE, event_generator);
  EXPECT_EQ(CaptureModeSource::kFullscreen, controller->source());

  // Tab to the region button and hit space to return to region capture mode.
  SendKey(ui::VKEY_TAB, event_generator, /*shift_down=*/false);
  SendKey(ui::VKEY_SPACE, event_generator);
  EXPECT_EQ(CaptureModeSource::kRegion, controller->source());

  // Tab to the capture button and hit space to perform a capture, which exits
  // capture mode.
  SendKey(ui::VKEY_TAB, event_generator, /*shift_down=*/false, 11);
  SendKey(ui::VKEY_SPACE, event_generator);
  EXPECT_FALSE(controller->IsActive());
}

TEST_F(CaptureModeTest, KeyboardNavigationSettingsMenuBehavior) {
  using FocusGroup = CaptureModeSessionFocusCycler::FocusGroup;

  // Use window capture mode to avoid having to tab through the selection
  // region.
  auto* controller =
      StartCaptureSession(CaptureModeSource::kWindow, CaptureModeType::kImage);
  auto* event_generator = GetEventGenerator();

  // Tests that pressing tab closes the settings menu if it was opened with a
  // button click.
  ClickOnView(GetSettingsButton(), event_generator);
  ASSERT_TRUE(GetCaptureModeSettingsWidget());
  SendKey(ui::VKEY_TAB, event_generator);
  EXPECT_FALSE(GetCaptureModeSettingsWidget());

  // Tab until we reach the settings button and press space to open the settings
  // menu.
  SendKey(ui::VKEY_TAB, event_generator, /*shift_down=*/false, 6);
  CaptureModeSessionTestApi test_api(controller->capture_mode_session());
  ASSERT_EQ(FocusGroup::kSettingsClose, test_api.GetCurrentFocusGroup());
  ASSERT_EQ(0u, test_api.GetCurrentFocusIndex());
  SendKey(ui::VKEY_SPACE, event_generator);
  ASSERT_TRUE(GetCaptureModeSettingsWidget());

  // Verify that at this point, the focus cycler is in a pending state.
  EXPECT_EQ(FocusGroup::kPendingSettings, test_api.GetCurrentFocusGroup());
  EXPECT_EQ(0u, test_api.GetCurrentFocusIndex());

  // The next tab should focus the microphone settings entry. Pressing space
  // should toggle the setting.
  SendKey(ui::VKEY_TAB, event_generator);
  ASSERT_FALSE(controller->enable_audio_recording());
  SendKey(ui::VKEY_SPACE, event_generator);
  EXPECT_TRUE(controller->enable_audio_recording());

  // Tests that the next tab will keep the menu open and move focus to the
  // settings button.
  SendKey(ui::VKEY_TAB, event_generator);
  EXPECT_TRUE(GetCaptureModeSettingsWidget());
  EXPECT_EQ(FocusGroup::kSettingsClose, test_api.GetCurrentFocusGroup());
  EXPECT_EQ(0u, test_api.GetCurrentFocusIndex());

  // Tests that pressing escape will close the menu and clear focus.
  SendKey(ui::VKEY_ESCAPE, event_generator);
  EXPECT_FALSE(GetCaptureModeSettingsWidget());
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
  SendKey(ui::VKEY_TAB, event_generator, /*shift_down=*/false, /*count=*/6);
  CaptureModeSessionTestApi test_api(controller->capture_mode_session());
  EXPECT_EQ(CaptureModeSessionFocusCycler::FocusGroup::kSelection,
            test_api.GetCurrentFocusGroup());
  EXPECT_EQ(0u, test_api.GetCurrentFocusIndex());

  // Arrow keys should shift the whole region.
  SendKey(ui::VKEY_RIGHT, event_generator);
  EXPECT_EQ(capture_region.origin() + gfx::Vector2d(arrow_shift, 0),
            controller->user_capture_region().origin());
  EXPECT_EQ(capture_region.size(), controller->user_capture_region().size());
  SendKey(ui::VKEY_RIGHT, event_generator, /*shift_down=*/true);
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
  SendKey(ui::VKEY_TAB, event_generator, /*shift_down=*/false, 4);

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
  SendKey(ui::VKEY_TAB, event_generator,
          /*is_shift_down=*/false,
          /*count=*/4);
  SendKey(ui::VKEY_SPACE, event_generator);
  EXPECT_EQ(expected_default_region, controller->user_capture_region());

  // Tests that hitting space while focusing the region toggle button when not
  // in region capture mode does nothing to the capture region.
  SelectRegion(gfx::Rect());
  ClickOnView(GetWindowToggleButton(), event_generator);
  SendKey(ui::VKEY_TAB, event_generator,
          /*is_shift_down=*/false,
          /*count=*/4);
  ASSERT_EQ(CaptureModeSessionFocusCycler::FocusGroup::kTypeSource,
            test_api.GetCurrentFocusGroup());
  ASSERT_EQ(3u, test_api.GetCurrentFocusIndex());
  SendKey(ui::VKEY_SPACE, event_generator);
  EXPECT_EQ(gfx::Rect(), controller->user_capture_region());
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
    EXPECT_EQ(expected_previous, view_accessibility.GetPreviousFocus());
    EXPECT_EQ(expected_next, view_accessibility.GetNextFocus());
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

  const uint64_t before_sequence_number =
      clipboard->GetSequenceNumber(ui::ClipboardBuffer::kCopyPaste);

  CaptureNotificationWaiter waiter;
  CaptureModeController::Get()->CaptureScreenshotsOfAllDisplays();
  waiter.Wait();

  const uint64_t after_sequence_number =
      clipboard->GetSequenceNumber(ui::ClipboardBuffer::kCopyPaste);

  EXPECT_NE(before_sequence_number, after_sequence_number);
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
  task_environment()->FastForwardBy(base::TimeDelta::FromSeconds(5));
  histogram_tester.ExpectBucketCount(kConsecutiveScreenshotsHistogram, 3, 1);

  // Take only one screenshot. This should not be recorded.
  take_n_screenshots(1);
  histogram_tester.ExpectBucketCount(kConsecutiveScreenshotsHistogram, 1, 0);
  task_environment()->FastForwardBy(base::TimeDelta::FromSeconds(5));
  histogram_tester.ExpectBucketCount(kConsecutiveScreenshotsHistogram, 1, 0);

  // Take a screenshot, change source and take another screenshot. This should
  // count as 2 consecutive screenshots.
  take_n_screenshots(1);
  auto* controller = StartCaptureSession(CaptureModeSource::kFullscreen,
                                         CaptureModeType::kImage);
  controller->PerformCapture();
  histogram_tester.ExpectBucketCount(kConsecutiveScreenshotsHistogram, 2, 0);
  task_environment()->FastForwardBy(base::TimeDelta::FromSeconds(5));
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
  task_environment()->FastForwardBy(base::TimeDelta::FromMinutes(1));
  StartImageRegionCapture();
  EXPECT_EQ(capture_region, controller->user_capture_region());
  auto* event_generator = GetEventGenerator();
  // Even if the capture is cancelled, we still remember the capture region.
  SendKey(ui::VKEY_ESCAPE, event_generator);
  EXPECT_EQ(capture_region, controller->user_capture_region());

  // Wait for 8 second and then start region image capture again. We should have
  // forgot the previous capture region.
  task_environment()->FastForwardBy(base::TimeDelta::FromMinutes(8));
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
  // 0.1f.
  EXPECT_EQ(0.1f, capture_bar_layer->GetTargetOpacity());

  // When there is overlap, the toolbar turns opaque on mouseover.
  event_generator->MoveMouseTo(
      GetCaptureModeBarView()->GetBoundsInScreen().CenterPoint());
  EXPECT_EQ(1.f, capture_bar_layer->GetTargetOpacity());

  // Capture bar drops back to 0.1 opacity when the mouse is no longer hovering.
  event_generator->MoveMouseTo(
      GetCaptureModeBarView()->GetBoundsInScreen().top_center() +
      gfx::Vector2d(0, -50));
  EXPECT_EQ(0.1f, capture_bar_layer->GetTargetOpacity());

  // Check that the opacity is reset when we select another region.
  SelectRegion(target_region);
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
  const int delete_button = 1;
  ClickNotification(delete_button);
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
  // Click on the notification body. This should take us to the files app.
  ClickNotification(absl::nullopt);
  EXPECT_FALSE(GetPreviewNotification());
  histogram_tester.ExpectBucketCount(kQuickActionHistogramName,
                                     CaptureQuickAction::kFiles, 1);

  controller = StartCaptureSession(CaptureModeSource::kFullscreen,
                                   CaptureModeType::kImage);

  {
    CaptureNotificationWaiter waiter;
    controller->PerformCapture();
    waiter.Wait();
  }
  const int edit_button = 0;
  // Verify clicking edit on screenshot notification.
  ClickNotification(edit_button);
  EXPECT_FALSE(GetPreviewNotification());
  histogram_tester.ExpectBucketCount(kQuickActionHistogramName,
                                     CaptureQuickAction::kBacklight, 1);
}

TEST_F(CaptureModeTest, CannotDoMultipleRecordings) {
  StartCaptureSession(CaptureModeSource::kFullscreen, CaptureModeType::kVideo);

  auto* controller = CaptureModeController::Get();
  controller->StartVideoRecordingImmediatelyForTesting();
  EXPECT_TRUE(controller->is_recording_in_progress());
  EXPECT_EQ(CaptureModeType::kVideo, controller->type());

  // Start a new session with the current type which set to kVideo, the type
  // should be switched automatically to kImage, and video toggle button should
  // be disabled.
  controller->Start(CaptureModeEntryType::kQuickSettings);
  EXPECT_TRUE(controller->IsActive());
  EXPECT_EQ(CaptureModeType::kImage, controller->type());
  EXPECT_TRUE(GetImageToggleButton()->GetToggled());
  EXPECT_FALSE(GetVideoToggleButton()->GetToggled());
  EXPECT_FALSE(GetVideoToggleButton()->GetEnabled());

  // Clicking on the video button should do nothing.
  ClickOnView(GetVideoToggleButton(), GetEventGenerator());
  EXPECT_TRUE(GetImageToggleButton()->GetToggled());
  EXPECT_FALSE(GetVideoToggleButton()->GetToggled());
  EXPECT_EQ(CaptureModeType::kImage, controller->type());

  // Things should go back to normal when there's no recording going on.
  controller->Stop();
  controller->EndVideoRecording(EndRecordingReason::kStopRecordingButton);
  StartCaptureSession(CaptureModeSource::kFullscreen, CaptureModeType::kVideo);
  EXPECT_EQ(CaptureModeType::kVideo, controller->type());
  EXPECT_FALSE(GetImageToggleButton()->GetToggled());
  EXPECT_TRUE(GetVideoToggleButton()->GetToggled());
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
  EXPECT_TRUE(GetSettingsButton()->GetToggled());
  ClickOnView(GetSettingsButton(), event_generator);
  EXPECT_FALSE(GetCaptureModeSettingsWidget());
  EXPECT_FALSE(GetSettingsButton()->GetToggled());
}

// Tests how interacting with the rest of the screen (i.e. clicking outside of
// the bar/menu, on other buttons) affects whether the settings menu should
// close or not.
TEST_F(CaptureModeTest, SettingsMenuVisibilityClicking) {
  UpdateDisplay("800x700");

  auto* event_generator = GetEventGenerator();
  auto* controller = StartImageRegionCapture();
  EXPECT_TRUE(controller->IsActive());

  // Test clicking on the settings menu and toggling settings doesn't close the
  // settings menu.
  ClickOnView(GetSettingsButton(), event_generator);
  ClickOnView(GetCaptureModeSettingsView(), event_generator);
  EXPECT_TRUE(GetCaptureModeSettingsWidget());
  EXPECT_TRUE(GetSettingsButton()->GetToggled());
  ClickOnView(GetMicrophoneToggle(), event_generator);
  EXPECT_TRUE(GetCaptureModeSettingsWidget());
  EXPECT_TRUE(GetSettingsButton()->GetToggled());

  // Test clicking on the capture bar closes the settings menu.
  event_generator->MoveMouseTo(
      GetCaptureModeBarView()->GetBoundsInScreen().top_center() +
      gfx::Vector2d(0, 2));
  event_generator->ClickLeftButton();
  EXPECT_FALSE(GetCaptureModeSettingsWidget());
  EXPECT_FALSE(GetSettingsButton()->GetToggled());

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

// Tests the settings menu functionality when in region mode.
TEST_F(CaptureModeTest, SettingsMenuVisibilityDrawingRegion) {
  UpdateDisplay("800x700");

  auto* event_generator = GetEventGenerator();
  auto* controller = StartImageRegionCapture();
  EXPECT_TRUE(controller->IsActive());

  // Test the settings menu is hidden when the user clicks to start selecting a
  // region.
  ClickOnView(GetSettingsButton(), event_generator);
  EXPECT_TRUE(GetCaptureModeSettingsWidget());
  const gfx::Rect target_region(gfx::BoundingRect(
      gfx::Point(0, 0),
      GetCaptureModeBarView()->GetBoundsInScreen().top_right() +
          gfx::Vector2d(0, -50)));
  event_generator->MoveMouseTo(target_region.origin());
  event_generator->PressLeftButton();
  EXPECT_FALSE(GetCaptureModeSettingsWidget());
  event_generator->MoveMouseTo(target_region.bottom_right());
  event_generator->ReleaseLeftButton();
  EXPECT_FALSE(GetCaptureModeSettingsWidget());

  // Test that the settings menu is hidden when we drag a region. This drags a
  // region that overlapps the capture bar for later steps of testing.
  ClickOnView(GetSettingsButton(), event_generator);
  event_generator->MoveMouseTo(target_region.origin() + gfx::Vector2d(50, 50));
  event_generator->PressLeftButton();
  EXPECT_FALSE(GetCaptureModeSettingsWidget());
  event_generator->MoveMouseTo(target_region.bottom_center());
  event_generator->ReleaseLeftButton();

  // With an overlapping region (as dragged to above), the capture bar opacity
  // is changed based on hover. If the settings menu is open/visible, we close
  // it when we hide the capture bar. Capture bar starts off opaque.
  ui::Layer* capture_bar_layer = GetCaptureModeBarWidget()->GetLayer();
  event_generator->MoveMouseTo(target_region.origin());
  EXPECT_EQ(0.1f, capture_bar_layer->GetTargetOpacity());
  ClickOnView(GetSettingsButton(), event_generator);
  EXPECT_EQ(1.f, capture_bar_layer->GetTargetOpacity());
  // Move mouse onto the settings menu, confirm the capture bar is still
  // visible.
  event_generator->MoveMouseTo(
      GetCaptureModeSettingsView()->GetBoundsInScreen().CenterPoint());
  EXPECT_EQ(1.f, capture_bar_layer->GetTargetOpacity());
  // Move the mouse off both the capture bar and the settings menu, and confirm
  // that both bars are no longer visible.
  event_generator->MoveMouseTo(
      GetCaptureModeSettingsView()->GetBoundsInScreen().top_center() +
      gfx::Vector2d(0, -50));
  EXPECT_EQ(0.1f, capture_bar_layer->GetTargetOpacity());
  EXPECT_FALSE(GetCaptureModeSettingsWidget());
}

// Tests that toggling the microphone setting updates the state in the
// controller, and persists between sessions.
TEST_F(CaptureModeTest, AudioRecordingSetting) {
  auto* controller = StartImageRegionCapture();
  auto* event_generator = GetEventGenerator();

  // Test that the audio recording preference is defaulted to false, so the
  // toggle should start in the off position.
  EXPECT_FALSE(controller->enable_audio_recording());

  // Test that toggling on the micophone updates the preference in the
  // controller, as well as displaying the toggle as on.
  ClickOnView(GetSettingsButton(), event_generator);
  EXPECT_FALSE(GetMicrophoneToggle()->GetIsOn());
  ClickOnView(GetMicrophoneToggle(), event_generator);
  EXPECT_TRUE(controller->enable_audio_recording());
  EXPECT_TRUE(GetMicrophoneToggle()->GetIsOn());

  // Test that the user selected audio preference for audio recording is
  // remembered between sessions.
  SendKey(ui::VKEY_ESCAPE, event_generator);
  EXPECT_TRUE(controller->enable_audio_recording());
  StartImageRegionCapture();
  EXPECT_TRUE(controller->enable_audio_recording());
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
    controller->StartVideoRecordingImmediatelyForTesting();
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

    auto* shell = Shell::Get();
    auto* cursor_manager = shell->cursor_manager();
    bool in_tablet_mode = shell->tablet_mode_controller()->InTabletMode();

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
  TabletModeControllerTestApi tablet_mode_controller_test_api;
  tablet_mode_controller_test_api.DetachAllMice();
  tablet_mode_controller_test_api.EnterTabletMode();
  FlushOverlay();
  EXPECT_TRUE(fake_overlay()->IsHidden());

  // Exiting tablet mode should reshow the overlay.
  tablet_mode_controller_test_api.LeaveTabletMode();
  FlushOverlay();
  EXPECT_FALSE(fake_overlay()->IsHidden());
}

// Tests that the cursor is hidden while taking a screenshot in tablet mode and
// remains hidden afterward.
TEST_F(CaptureModeCursorOverlayTest, TabletModeHidesCursor) {
  // Enter tablet mode.
  TabletModeControllerTestApi tablet_mode_controller_test_api;
  tablet_mode_controller_test_api.DetachAllMice();
  tablet_mode_controller_test_api.EnterTabletMode();

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
  tablet_mode_controller_test_api.LeaveTabletMode();
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

// Verifies that the cursor overlay bounds calculation takes into account the
// cursor image scale factor. https://crbug.com/1222494.
TEST_F(CaptureModeCursorOverlayTest, OverlayBoundsAccountForCursorScaleFactor) {
  UpdateDisplay("500x400");
  StartRecordingAndSetupFakeOverlay(CaptureModeSource::kFullscreen);
  EXPECT_FALSE(fake_overlay()->IsHidden());

  auto* cursor_manager = Shell::Get()->cursor_manager();
  auto set_cursor = [cursor_manager](const gfx::Size& cursor_image_size,
                                     float cursor_image_scale_factor) {
    const auto cursor_type = ui::mojom::CursorType::kCustom;
    gfx::NativeCursor cursor{cursor_type};
    SkBitmap cursor_image;
    cursor_image.allocN32Pixels(cursor_image_size.width(),
                                cursor_image_size.height());
    cursor.set_image_scale_factor(cursor_image_scale_factor);
    cursor.set_custom_bitmap(cursor_image);
    auto* platform_cursor_factory =
        ui::OzonePlatform::GetInstance()->GetCursorFactory();
    cursor.SetPlatformCursor(platform_cursor_factory->CreateImageCursor(
        cursor_type, cursor_image, cursor.custom_hotspot()));
    cursor_manager->SetCursor(cursor);
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

// TODO(afakhry): Add more cursor overlay tests.

}  // namespace ash
