// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/unified_system_tray.h"

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/constants/ash_features.h"
#include "ash/ime/ime_controller_impl.h"
#include "ash/public/cpp/test/shell_test_api.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shell.h"
#include "ash/system/message_center/unified_message_center_bubble.h"
#include "ash/system/message_center/unified_message_center_view.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/status_area_widget_test_helper.h"
#include "ash/system/time/time_tray_item_view.h"
#include "ash/system/time/time_view.h"
#include "ash/system/unified/ime_mode_view.h"
#include "ash/system/unified/unified_slider_bubble_controller.h"
#include "ash/system/unified/unified_system_tray_bubble.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "ash/system/unified/unified_system_tray_view.h"
#include "ash/test/ash_test_base.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/message_center/message_center.h"

namespace ash {

using message_center::MessageCenter;
using message_center::Notification;

class UnifiedSystemTrayTest : public AshTestBase {
 public:
  UnifiedSystemTrayTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  UnifiedSystemTrayTest(const UnifiedSystemTrayTest&) = delete;
  UnifiedSystemTrayTest& operator=(const UnifiedSystemTrayTest&) = delete;
  ~UnifiedSystemTrayTest() override = default;

  void SetUp() override {
    feature_list_.InitAndEnableFeature(features::kCalendarView);
    AshTestBase::SetUp();
  }

 protected:
  const std::string AddNotification() {
    const std::string id = base::NumberToString(id_++);
    MessageCenter::Get()->AddNotification(
        std::make_unique<message_center::Notification>(
            message_center::NOTIFICATION_TYPE_BASE_FORMAT, id, u"test title",
            u"test message", ui::ImageModel(),
            std::u16string() /* display_source */, GURL(),
            message_center::NotifierId(),
            message_center::RichNotificationData(),
            new message_center::NotificationDelegate()));
    return id;
  }

  void RemoveNotification(const std::string id) {
    MessageCenter::Get()->RemoveNotification(id, /*by_user=*/false);
  }

  bool IsSliderBubbleShown() {
    return GetPrimaryUnifiedSystemTray()
        ->slider_bubble_controller_->bubble_widget_;
  }

  bool MoreThanOneVisibleTrayItem() const {
    return GetPrimaryUnifiedSystemTray()->MoreThanOneVisibleTrayItem();
  }

  UnifiedSliderBubbleController::SliderType GetSliderBubbleType() {
    return GetPrimaryUnifiedSystemTray()
        ->slider_bubble_controller_->slider_type_;
  }

  UnifiedSystemTrayBubble* GetUnifiedSystemTrayBubble() {
    return GetPrimaryUnifiedSystemTray()->bubble_.get();
  }

  void UpdateAutoHideStateNow() {
    GetPrimaryShelf()->shelf_layout_manager()->UpdateAutoHideStateNow();
  }

  gfx::Rect GetBubbleViewBounds() {
    auto* bubble =
        GetPrimaryUnifiedSystemTray()->slider_bubble_controller_->bubble_view_;
    return bubble ? bubble->GetBoundsInScreen() : gfx::Rect();
  }

  TimeTrayItemView* time_view() {
    return GetPrimaryUnifiedSystemTray()->time_view_;
  }

  ImeModeView* ime_mode_view() {
    return GetPrimaryUnifiedSystemTray()->ime_mode_view_;
  }

  std::list<TrayItemView*> tray_items() {
    return GetPrimaryUnifiedSystemTray()->tray_items_;
  }

  views::View* vertical_clock_padding() {
    return GetPrimaryUnifiedSystemTray()->vertical_clock_padding_;
  }

 private:
  int id_ = 0;
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(UnifiedSystemTrayTest, ShowVolumeSliderBubble) {
  // The volume popup is not visible initially.
  EXPECT_FALSE(IsSliderBubbleShown());

  // When set to autohide, the shelf shouldn't be shown.
  StatusAreaWidget* status = StatusAreaWidgetTestHelper::GetStatusAreaWidget();
  EXPECT_FALSE(status->ShouldShowShelf());

  // Simulate ARC asking to show the volume view.
  GetPrimaryUnifiedSystemTray()->ShowVolumeSliderBubble();

  // Volume view is now visible.
  EXPECT_TRUE(IsSliderBubbleShown());
  EXPECT_EQ(UnifiedSliderBubbleController::SLIDER_TYPE_VOLUME,
            GetSliderBubbleType());

  // This does not force the shelf to automatically show. Regression tests for
  // crbug.com/729188
  EXPECT_FALSE(status->ShouldShowShelf());
}

TEST_F(UnifiedSystemTrayTest, SliderBubbleMovesOnShelfAutohide) {
  // The slider button should be moved when the autohidden shelf is shown, so
  // as to not overlap. Regression test for crbug.com/1136564
  auto* shelf = GetPrimaryShelf();
  shelf->SetAlignment(ShelfAlignment::kBottom);
  shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlways);

  // Create a test widget to make auto-hiding work. Auto-hidden shelf will
  // remain visible if no windows are shown, making it impossible to properly
  // test.
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_WINDOW);
  params.bounds = gfx::Rect(0, 0, 200, 200);
  params.context = GetContext();
  views::Widget* widget = new views::Widget;
  widget->Init(std::move(params));
  widget->Show();

  // Start off the mouse nowhere near the shelf; the shelf should be hidden.
  display::Display display = display::Screen::GetScreen()->GetPrimaryDisplay();
  auto center = display.bounds().CenterPoint();
  auto bottom_center = display.bounds().bottom_center();
  bottom_center.set_y(bottom_center.y() - 1);
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(center);
  UpdateAutoHideStateNow();

  GetPrimaryUnifiedSystemTray()->ShowVolumeSliderBubble();

  gfx::Rect before_bounds = GetBubbleViewBounds();

  // Now move the mouse close to the edge, so that the shelf shows, and verify
  // that the volume slider adjusts accordingly.
  generator->MoveMouseTo(bottom_center);
  UpdateAutoHideStateNow();
  gfx::Rect after_bounds = GetBubbleViewBounds();
  EXPECT_NE(after_bounds, before_bounds);

  // Also verify that the shelf and slider bubble would have overlapped, but do
  // not now that we've moved the slider bubble.
  gfx::Rect shelf_bounds = shelf->GetShelfBoundsInScreen();
  EXPECT_TRUE(before_bounds.Intersects(shelf_bounds));
  EXPECT_FALSE(after_bounds.Intersects(shelf_bounds));

  // Move the mouse away and verify that it adjusts back to its original
  // position.
  generator->MoveMouseTo(center);
  UpdateAutoHideStateNow();
  after_bounds = GetBubbleViewBounds();
  EXPECT_EQ(after_bounds, before_bounds);

  // Now fullscreen and restore our window with autohide disabled and verify
  // that the bubble moves down as the shelf disappears and reappears. Disable
  // autohide so that the shelf is initially showing.
  shelf->SetAlignment(ShelfAlignment::kRight);
  after_bounds = GetBubbleViewBounds();
  EXPECT_NE(after_bounds, before_bounds);
  shelf->SetAlignment(ShelfAlignment::kBottom);
  after_bounds = GetBubbleViewBounds();
  EXPECT_EQ(after_bounds, before_bounds);

  // Adjust the alignment of the shelf, and verify that the bubble moves along
  // with it.
  shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kNever);
  before_bounds = GetBubbleViewBounds();
  widget->SetFullscreen(true);
  after_bounds = GetBubbleViewBounds();
  EXPECT_NE(after_bounds, before_bounds);
  widget->SetFullscreen(false);
  after_bounds = GetBubbleViewBounds();
  EXPECT_EQ(after_bounds, before_bounds);
}

TEST_F(UnifiedSystemTrayTest, ShowBubble_MultipleDisplays_OpenedOnSameDisplay) {
  // Initialize two displays with 800x700 resolution.
  UpdateDisplay("400+400-800x600,1220+400-800x600");
  auto* screen = display::Screen::GetScreen();
  EXPECT_EQ(2, screen->GetNumDisplays());

  // The tray bubble for each display should be opened on the same display.
  // See crbug.com/937420.
  for (int i = 0; i < screen->GetNumDisplays(); ++i) {
    auto* system_tray = GetPrimaryUnifiedSystemTray();
    system_tray->ShowBubble();
    const gfx::Rect primary_display_bounds = GetPrimaryDisplay().bounds();
    const gfx::Rect tray_bubble_bounds =
        GetPrimaryUnifiedSystemTray()->GetBubbleBoundsInScreen();
    EXPECT_TRUE(primary_display_bounds.Contains(tray_bubble_bounds))
        << "primary display bounds=" << primary_display_bounds.ToString()
        << ", tray bubble bounds=" << tray_bubble_bounds.ToString();

    SwapPrimaryDisplay();
  }
}

TEST_F(UnifiedSystemTrayTest, HorizontalImeAndTimeLabelAlignment) {
  ime_mode_view()->label()->SetText(u"US");
  ime_mode_view()->SetVisible(true);

  gfx::Rect time_bounds = time_view()
                              ->time_view()
                              ->horizontal_label_for_test()
                              ->GetBoundsInScreen();
  gfx::Rect ime_bounds = ime_mode_view()->label()->GetBoundsInScreen();

  EXPECT_EQ(time_bounds.y(), ime_bounds.y());
  EXPECT_EQ(time_bounds.height(), ime_bounds.height());
}

TEST_F(UnifiedSystemTrayTest, VerticalClockPadding) {
  // Padding can only be visible if shelf is vertically aligned.
  GetPrimaryShelf()->SetAlignment(ShelfAlignment::kLeft);

  // Sets all tray items' visibility to false except TimeView.
  for (TrayItemView* item : tray_items()) {
    item->SetVisible(item == time_view());
  }

  // Only one visible tray item, padding should not be visible.
  EXPECT_FALSE(vertical_clock_padding()->GetVisible());

  // Sets another tray item visibility to true.
  ime_mode_view()->SetVisible(true);

  // Two visible tray items, padding should be visible.
  EXPECT_TRUE(vertical_clock_padding()->GetVisible());
}

TEST_F(UnifiedSystemTrayTest, VerticalClockPaddingAfterAlignmentChange) {
  auto* shelf = GetPrimaryShelf();

  // Padding can only be visible if shelf is vertically aligned.
  shelf->SetAlignment(ShelfAlignment::kLeft);

  // Ensure two tray items are visible, padding should be visible.
  time_view()->SetVisible(true);
  ime_mode_view()->SetVisible(true);

  EXPECT_TRUE(vertical_clock_padding()->GetVisible());

  // Padding should not be visible when shelf is horizontal.
  shelf->SetAlignment(ShelfAlignment::kBottom);
  EXPECT_FALSE(vertical_clock_padding()->GetVisible());
}

TEST_F(UnifiedSystemTrayTest, FocusMessageCenter) {
  auto* tray = GetPrimaryUnifiedSystemTray();
  tray->ShowBubble();

  auto* message_center_view =
      tray->message_center_bubble()->message_center_view();
  auto* focus_manager = message_center_view->GetFocusManager();

  AddNotification();
  AddNotification();
  message_center_view->SetVisible(true);

  EXPECT_FALSE(message_center_view->Contains(focus_manager->GetFocusedView()));
  EXPECT_FALSE(message_center_view->collapsed());

  auto did_focus = tray->FocusMessageCenter(false);

  EXPECT_TRUE(did_focus);

  EXPECT_TRUE(tray->IsMessageCenterBubbleShown());
  EXPECT_FALSE(message_center_view->collapsed());
  EXPECT_TRUE(message_center_view->Contains(focus_manager->GetFocusedView()));
}

TEST_F(UnifiedSystemTrayTest, FocusMessageCenter_MessageCenterBubbleNotShown) {
  auto* tray = GetPrimaryUnifiedSystemTray();
  tray->ShowBubble();
  auto* message_center_bubble = tray->message_center_bubble();

  EXPECT_FALSE(message_center_bubble->IsMessageCenterVisible());

  auto did_focus = tray->FocusMessageCenter(false);

  EXPECT_FALSE(did_focus);
}

TEST_F(UnifiedSystemTrayTest, FocusMessageCenter_VoxEnabled) {
  auto* tray = GetPrimaryUnifiedSystemTray();
  tray->ShowBubble();

  auto* message_center_bubble = tray->message_center_bubble();
  auto* message_center_view = message_center_bubble->message_center_view();

  AddNotification();
  AddNotification();
  message_center_view->SetVisible(true);
  Shell::Get()->accessibility_controller()->spoken_feedback().SetEnabled(true);

  EXPECT_FALSE(message_center_bubble->GetBubbleWidget()->IsActive());

  auto did_focus = tray->FocusMessageCenter(false);

  EXPECT_TRUE(did_focus);

  auto* focus_manager = tray->GetFocusManager();

  EXPECT_TRUE(tray->IsMessageCenterBubbleShown());
  EXPECT_TRUE(message_center_bubble->GetBubbleWidget()->IsActive());
  EXPECT_FALSE(message_center_view->Contains(focus_manager->GetFocusedView()));
}

TEST_F(UnifiedSystemTrayTest, FocusQuickSettings) {
  auto* tray = GetPrimaryUnifiedSystemTray();
  tray->ShowBubble();
  auto* unified_system_tray_view = tray->bubble()->unified_view();
  auto* focus_manager = unified_system_tray_view->GetFocusManager();

  EXPECT_FALSE(
      unified_system_tray_view->Contains(focus_manager->GetFocusedView()));

  auto did_focus = tray->FocusQuickSettings(false);

  EXPECT_TRUE(did_focus);

  EXPECT_TRUE(
      unified_system_tray_view->Contains(focus_manager->GetFocusedView()));
}

TEST_F(UnifiedSystemTrayTest, FocusQuickSettings_BubbleNotShown) {
  auto* tray = GetPrimaryUnifiedSystemTray();

  auto did_focus = tray->FocusQuickSettings(false);

  EXPECT_FALSE(did_focus);
}

TEST_F(UnifiedSystemTrayTest, FocusQuickSettings_VoxEnabled) {
  auto* tray = GetPrimaryUnifiedSystemTray();
  tray->ShowBubble();
  auto* tray_bubble_widget = tray->bubble()->GetBubbleWidget();

  Shell::Get()->accessibility_controller()->spoken_feedback().SetEnabled(true);

  EXPECT_FALSE(tray_bubble_widget->IsActive());

  auto did_focus = tray->FocusQuickSettings(false);

  EXPECT_TRUE(did_focus);

  auto* unified_system_tray_view = tray->bubble()->unified_view();
  auto* focus_manager = unified_system_tray_view->GetFocusManager();

  EXPECT_TRUE(tray_bubble_widget->IsActive());
  EXPECT_FALSE(
      unified_system_tray_view->Contains(focus_manager->GetFocusedView()));
}

TEST_F(UnifiedSystemTrayTest, TimeInQuickSettingsMetric) {
  base::HistogramTester histogram_tester;
  constexpr base::TimeDelta kTimeInQuickSettings = base::Seconds(3);
  auto* tray = GetPrimaryUnifiedSystemTray();

  // Open the tray.
  tray->ShowBubble();

  // Spend cool-down time with tray open.
  task_environment()->FastForwardBy(kTimeInQuickSettings);

  // Close and record the metric.
  tray->CloseBubble();

  // Ensure metric recorded time passed while Quick Setting was open.
  histogram_tester.ExpectTimeBucketCount("Ash.QuickSettings.UserJourneyTime",
                                         kTimeInQuickSettings,
                                         /*count=*/1);

  // Re-open the tray.
  tray->ShowBubble();

  // Metric isn't recorded when adding and removing a notification.
  std::string id = AddNotification();
  RemoveNotification(id);
  histogram_tester.ExpectTotalCount("Ash.QuickSettings.UserJourneyTime",
                                    /*count=*/1);

  // Metric is recorded after closing bubble.
  tray->CloseBubble();
  histogram_tester.ExpectTotalCount("Ash.QuickSettings.UserJourneyTime",
                                    /*count=*/2);
}

// Tests that pressing the TOGGLE_CALENDAR accelerator once results in the
// calendar view showing.
TEST_F(UnifiedSystemTrayTest, PressCalendarAccelerator) {
  ShellTestApi().PressAccelerator(
      ui::Accelerator(ui::VKEY_C, ui::EF_COMMAND_DOWN));

  EXPECT_TRUE(GetPrimaryUnifiedSystemTray()->IsShowingCalendarView());
}

// Tests that pressing the TOGGLE_CALENDAR accelerator twice results in a hidden
// QuickSettings bubble.
TEST_F(UnifiedSystemTrayTest, ToggleCalendarViewAccelerator) {
  ShellTestApi().PressAccelerator(
      ui::Accelerator(ui::VKEY_C, ui::EF_COMMAND_DOWN));

  ShellTestApi().PressAccelerator(
      ui::Accelerator(ui::VKEY_C, ui::EF_COMMAND_DOWN));

  EXPECT_FALSE(GetUnifiedSystemTrayBubble());
}

// Tests that showing the calendar view by the TOGGLE_CALENDAR accelerator
// results in the CalendarDateCellView being focused.
TEST_F(UnifiedSystemTrayTest, CalendarAcceleratorFocusesDateCell) {
  ShellTestApi().PressAccelerator(
      ui::Accelerator(ui::VKEY_C, ui::EF_COMMAND_DOWN));

  auto* focus_manager =
      GetUnifiedSystemTrayBubble()->GetBubbleWidget()->GetFocusManager();
  EXPECT_TRUE(focus_manager->GetFocusedView());
  EXPECT_STREQ(focus_manager->GetFocusedView()->GetClassName(),
               "CalendarDateCellView");
}

// Tests that CalendarView switches back to Quick Settings when screen size is
// limited and the bubble requires a collapsed state.
TEST_F(UnifiedSystemTrayTest, CalendarGoesToMainView) {
  auto* tray = GetPrimaryUnifiedSystemTray();
  tray->ShowBubble();

  // Set a limited screen size.
  UpdateDisplay("800x600");

  // Generate a notification, close and open the bubble so we can show the
  // collapsed message center.
  AddNotification();
  tray->CloseBubble();
  tray->ShowBubble();

  // Ensure message center is collapsed when Calendar is not being shown.
  auto* message_center_view =
      tray->message_center_bubble()->message_center_view();
  EXPECT_FALSE(tray->IsShowingCalendarView());
  EXPECT_TRUE(message_center_view->collapsed());

  // Ensure message center is collapsed when the Calendar is being shown.
  ShellTestApi().PressAccelerator(
      ui::Accelerator(ui::VKEY_C, ui::EF_COMMAND_DOWN));
  EXPECT_TRUE(tray->IsShowingCalendarView());
  EXPECT_TRUE(message_center_view->collapsed());

  // Test that Calendar is no longer shown after expanding the collapsed
  // message center.
  tray->message_center_bubble()->ExpandMessageCenter();
  EXPECT_FALSE(message_center_view->collapsed());
  EXPECT_FALSE(tray->IsShowingCalendarView());
}

}  // namespace ash
