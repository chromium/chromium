// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "ash/capture_mode/capture_mode_bar_view.h"
#include "ash/capture_mode/capture_mode_button.h"
#include "ash/capture_mode/capture_mode_constants.h"
#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/capture_mode/capture_mode_metrics.h"
#include "ash/capture_mode/capture_mode_session.h"
#include "ash/capture_mode/capture_mode_source_view.h"
#include "ash/capture_mode/capture_mode_toggle_button.h"
#include "ash/capture_mode/capture_mode_type_view.h"
#include "ash/capture_mode/capture_mode_types.h"
#include "ash/capture_mode/capture_mode_util.h"
#include "ash/capture_mode/stop_recording_button_tray.h"
#include "ash/display/cursor_window_controller.h"
#include "ash/display/screen_orientation_controller_test_api.h"
#include "ash/display/window_tree_host_manager.h"
#include "ash/home_screen/home_screen_controller.h"
#include "ash/magnifier/magnifier_glass.h"
#include "ash/public/cpp/ash_features.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/system/status_area_widget.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller_test_api.h"
#include "ash/wm/window_state.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "chromeos/dbus/power_manager/suspend.pb.h"
#include "components/account_id/account_id.h"
#include "ui/aura/window_tracker.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/message_center_observer.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget_observer.h"

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
      ->ShouldEnableCursorCompositing();
}

void ClickOnView(const views::View* view,
                 ui::test::EventGenerator* event_generator) {
  DCHECK(view);
  DCHECK(event_generator);

  const gfx::Point view_center = view->GetBoundsInScreen().CenterPoint();
  event_generator->MoveMouseTo(view_center);
  event_generator->ClickLeftButton();
}

void SendKey(ui::KeyboardCode key_code,
             ui::test::EventGenerator* event_generator,
             bool shift_down = false) {
  int flags = shift_down ? ui::EF_SHIFT_DOWN : 0;
  event_generator->PressKey(key_code, flags);
  event_generator->ReleaseKey(key_code, flags);
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

void ClickNotification(base::Optional<int> button_index) {
  const message_center::Notification* notification = GetPreviewNotification();
  DCHECK(notification);
  notification->delegate()->Click(button_index, base::nullopt);
}

// Moves the mouse and updates the cursor's display manually to imitate what a
// real mouse move event does in shell.
void MoveMouseToAndUpdateCursorDisplay(
    const gfx::Point& point,
    ui::test::EventGenerator* event_generator) {
  Shell::Get()->cursor_manager()->SetDisplay(
      display::Screen::GetScreen()->GetDisplayNearestPoint(point));
  event_generator->MoveMouseTo(point);
}

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

  views::Widget* capture_mode_bar_widget() const {
    return session_->capture_mode_bar_widget_.get();
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

 private:
  const CaptureModeSession* const session_;
};

class CaptureModeTest : public AshTestBase {
 public:
  CaptureModeTest() = default;
  CaptureModeTest(base::test::TaskEnvironment::TimeSource time)
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
    return CaptureModeSessionTestApi(session).capture_mode_bar_widget();
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

  CaptureModeButton* GetFeedbackButton() const {
    auto* controller = CaptureModeController::Get();
    DCHECK(controller->IsActive());
    return GetCaptureModeBarView()->feedback_button_for_testing();
  }

  CaptureModeButton* GetCloseButton() const {
    auto* controller = CaptureModeController::Get();
    DCHECK(controller->IsActive());
    return GetCaptureModeBarView()->close_button_for_testing();
  }

  aura::Window* GetDimensionsLabelWindow() const {
    auto* controller = CaptureModeController::Get();
    DCHECK(controller->IsActive());
    auto* widget = CaptureModeSessionTestApi(controller->capture_mode_session())
                       .dimensions_label_widget();
    return widget ? widget->GetNativeWindow() : nullptr;
  }

  base::Optional<gfx::Point> GetMagnifierGlassCenterPoint() const {
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
    return base::nullopt;
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
  CaptureModeSessionTestApi test_api(controller->capture_mode_session());
  auto* bar_window = test_api.capture_mode_bar_widget()->GetNativeWindow();
  aura::WindowTracker tracker({bar_window});
  controller->Stop();
  EXPECT_TRUE(tracker.windows().empty());
  EXPECT_FALSE(controller->IsActive());
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

TEST_F(CaptureModeTest, FeedbackButtonExits) {
  auto* controller = CaptureModeController::Get();
  controller->Start(CaptureModeEntryType::kQuickSettings);
  EXPECT_TRUE(controller->IsActive());

  ClickOnView(GetFeedbackButton(), GetEventGenerator());
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
  WaitForCountDownToFinish();
  EXPECT_FALSE(controller->IsActive());
  EXPECT_TRUE(controller->is_recording_in_progress());

  // The composited cursor should be enabled, and the stop-recording button
  // should show up in the status area widget.
  EXPECT_TRUE(IsCursorCompositingEnabled());
  auto* stop_recording_button = Shell::GetPrimaryRootWindowController()
                                    ->GetStatusAreaWidget()
                                    ->stop_recording_button_tray();
  EXPECT_TRUE(stop_recording_button->visible_preferred());

  // End recording via the stop-recording button. Expect that it's now hidden,
  // and the cursor compositing is now disabled.
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
  UpdateDisplay("800x800");

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
  UpdateDisplay("800x800");

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
  UpdateDisplay("800x800");

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
  UpdateDisplay("800x800");

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
  UpdateDisplay("800x800");

  // Start Capture Mode in a region in image mode.
  StartImageRegionCapture();

  // Press down and drag to select a region. The magnifier should not be
  // visible yet.
  gfx::Rect capture_region{200, 200, 400, 400};
  SelectRegion(capture_region);
  EXPECT_EQ(base::nullopt, GetMagnifierGlassCenterPoint());

  auto check_magnifier_shows_properly = [this](const gfx::Point& origin,
                                               const gfx::Point& destination,
                                               bool should_show_magnifier) {
    // If |should_show_magnifier|, check that the magnifying glass is centered
    // on the mouse after press and during drag, and that the cursor is hidden.
    // If not |should_show_magnifier|, check that the magnifying glass never
    // shows. Should always be not visible when mouse button is released.
    auto* event_generator = GetEventGenerator();
    base::Optional<gfx::Point> expected_origin =
        should_show_magnifier ? base::make_optional(origin) : base::nullopt;
    base::Optional<gfx::Point> expected_destination =
        should_show_magnifier ? base::make_optional(destination)
                              : base::nullopt;

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
    EXPECT_EQ(base::nullopt, GetMagnifierGlassCenterPoint());
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
  UpdateDisplay("800x800");

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
  // (800).
  capture_region.SetRect(796, 100, 2, 100);
  SelectRegion(capture_region, /*release_mouse=*/false);
  EXPECT_EQ(800, GetDimensionsLabelWindow()->bounds().right());
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
  UpdateDisplay("800x800");

  auto* controller = StartImageRegionCapture();

  // Select a large region. Verify that the capture button widget is centered.
  SelectRegion(gfx::Rect(100, 100, 600, 600));

  views::Widget* capture_button_widget =
      CaptureModeSessionTestApi(controller->capture_mode_session())
          .capture_label_widget();
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
  UpdateDisplay("800x800,801+0-800x800");

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
  UpdateDisplay("800x800,801+0-800x800");

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
  UpdateDisplay("800x800,801+0-800x800");
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
  UpdateDisplay("800x800,801+0-800x800");
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
  UpdateDisplay("800x800,801+0-800x800");
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
  histogram_tester.ExpectBucketCount(
      kEndRecordingReasonInClamshellHistogramName,
      EndRecordingReason::kDisplayOrWindowClosing, 1);
}

TEST_F(CaptureModeTest, DetachDisplayWhileWindowRecording) {
  UpdateDisplay("400x400,401+0-400x400");
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
  UpdateDisplay("400x400,401+0-400x400");
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
  UpdateDisplay("800x800");

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
  UpdateDisplay("800x800");

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
  CaptureModeSessionTestApi test_api(controller->capture_mode_session());
  DCHECK(test_api.capture_label_widget());
  event_generator->set_current_screen_location(
      test_api.capture_label_widget()->GetWindowBoundsInScreen().CenterPoint());
  event_generator->PressTouch();
  event_generator->ReleaseTouch();

  // There are no windows and home screen window is excluded from window capture
  // mode, so capture mode will still remain active.
  EXPECT_TRUE(Shell::Get()->home_screen_controller()->IsHomeScreenVisible());
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
  views::Widget* capture_label_widget =
      CaptureModeSessionTestApi(controller->capture_mode_session())
          .capture_label_widget();
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
  UpdateDisplay("600x600");
  EXPECT_EQ(gfx::Rect(600, 400), controller->user_capture_region());
  EXPECT_EQ(300,
            GetCaptureModeBarView()->GetBoundsInScreen().CenterPoint().x());
}

TEST_F(CaptureModeTest, ReenterOnSmallerDisplay) {
  UpdateDisplay("1200x600,1201+0-600x600");

  // Start off with the primary display as the targeted display. Create a region
  // that fits the primary display but would be too big for the secondary
  // display.
  auto* event_generator = GetEventGenerator();
  MoveMouseToAndUpdateCursorDisplay(gfx::Point(600, 300), event_generator);
  auto* controller = StartImageRegionCapture();
  SelectRegion(gfx::Rect(1200, 400));
  EXPECT_EQ(gfx::Rect(1200, 400), controller->user_capture_region());
  controller->Stop();

  // Make the secondary display the targeted display. Test that the region has
  // shrunk to fit the display.
  MoveMouseToAndUpdateCursorDisplay(gfx::Point(1500, 300), event_generator);
  StartImageRegionCapture();
  EXPECT_EQ(gfx::Rect(600, 400), controller->user_capture_region());
}

// Tests that functionality to create and adjust a region with keyboard
// shortcuts works as intended.
TEST_F(CaptureModeTest, SelectRegionWithKeyboard) {
  auto* controller = StartImageRegionCapture();
  auto* event_generator = GetEventGenerator();
  ASSERT_TRUE(controller->user_capture_region().IsEmpty());

  // Test that hitting space will create a default region.
  SendKey(ui::VKEY_SPACE, event_generator);
  gfx::Rect capture_region = controller->user_capture_region();
  EXPECT_FALSE(capture_region.IsEmpty());

  // Test that hitting an arrow key will do nothing as nothing is focused
  // initially.
  SendKey(ui::VKEY_RIGHT, event_generator);
  EXPECT_EQ(capture_region, controller->user_capture_region());

  const int arrow_shift = capture_mode::kArrowKeyboardRegionChangeDp;

  // Hit tab so that the whole region is focused. Arrow keys should shift the
  // whole region.
  SendKey(ui::VKEY_TAB, event_generator);
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
  SendKey(ui::VKEY_TAB, event_generator);
  SendKey(ui::VKEY_TAB, event_generator);
  SendKey(ui::VKEY_TAB, event_generator);
  SendKey(ui::VKEY_TAB, event_generator);

  SendKey(ui::VKEY_LEFT, event_generator);
  SendKey(ui::VKEY_UP, event_generator);
  EXPECT_EQ(capture_region.size() - gfx::Size(arrow_shift, arrow_shift),
            controller->user_capture_region().size());
  SendKey(ui::VKEY_RIGHT, event_generator);
  SendKey(ui::VKEY_DOWN, event_generator);
  EXPECT_EQ(capture_region.size(), controller->user_capture_region().size());
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
  UpdateDisplay("800x800");
  auto* controller = StartImageRegionCapture();
  EXPECT_EQ(gfx::Rect(), controller->user_capture_region());

  const gfx::Rect capture_region(100, 100, 600, 600);
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
  UpdateDisplay("800x800");

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
  ClickNotification(base::nullopt);
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

}  // namespace ash
