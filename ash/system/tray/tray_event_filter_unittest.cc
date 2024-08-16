// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/tray_event_filter.h"

#include "ash/constants/ash_features.h"
#include "ash/ime/ime_controller_impl.h"
#include "ash/root_window_controller.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shell.h"
#include "ash/system/ime_menu/ime_menu_tray.h"
#include "ash/system/notification_center/ash_message_popup_collection.h"
#include "ash/system/notification_center/views/ash_notification_expand_button.h"
#include "ash/system/notification_center/views/ash_notification_view.h"
#include "ash/system/network/network_detailed_view.h"
#include "ash/system/notification_center/notification_center_tray.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/status_area_widget_test_helper.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_utils.h"
#include "ash/system/unified/date_tray.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/system/unified/unified_system_tray_bubble.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/tablet_mode/tablet_mode_controller_test_api.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window.h"
#include "ui/gfx/geometry/size.h"
#include "ui/message_center/message_center.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/window_util.h"

using message_center::MessageCenter;
using message_center::Notification;

namespace ash {

namespace {

class TestTrayBackgroundView : public TrayBackgroundView {
 public:
  explicit TestTrayBackgroundView(Shelf* shelf)
      : TrayBackgroundView(shelf,
                           TrayBackgroundViewCatalogName::kTestCatalogName,
                           RoundedCornerBehavior::kAllRounded) {
    SetCallback(base::BindRepeating(&TestTrayBackgroundView::OnButtonPressed,
                                    base::Unretained(this)));
  }

  TestTrayBackgroundView(const TestTrayBackgroundView&) = delete;
  TestTrayBackgroundView& operator=(const TestTrayBackgroundView&) = delete;

  ~TestTrayBackgroundView() override = default;

  // TrayBackgroundView:
  void ClickedOutsideBubble(const ui::LocatedEvent& event) override {
    clicked_outside_bubble_called_ = true;
    CloseBubble();
  }

  void UpdateTrayItemColor(bool is_active) override {}
  std::u16string GetAccessibleNameForTray() override {
    return u"TestTrayBackgroundView";
  }

  void HandleLocaleChange() override {}

  void HideBubbleWithView(const TrayBubbleView* bubble_view) override {
    if (bubble_view == bubble_->GetBubbleView()) {
      CloseBubble();
    }
  }

  void HideBubble(const TrayBubbleView* bubble_view) override {
    if (bubble_view == bubble_->GetBubbleView()) {
      CloseBubble();
    }
  }

  void ShowBubble() override {
    auto bubble_view = std::make_unique<TrayBubbleView>(
        CreateInitParamsForTrayBubble(/*tray=*/this));
    bubble_view->SetPreferredSize(gfx::Size(kTrayMenuWidth, 100));
    bubble_ = std::make_unique<TrayBubbleWrapper>(this,
                                                  /*event_handling=*/true);
    bubble_->ShowBubble(std::move(bubble_view));
  }

  void CloseBubbleInternal() override { bubble_.reset(); }

  TrayBubbleWrapper* bubble() { return bubble_.get(); }

  bool clicked_outside_bubble_called() const {
    return clicked_outside_bubble_called_;
  }

 private:
  void OnButtonPressed(const ui::Event& event) {
    if (bubble_) {
      CloseBubble();
      return;
    }
    ShowBubble();
  }

  std::unique_ptr<TrayBubbleWrapper> bubble_;
  bool clicked_outside_bubble_called_ = false;
};

}  // namespace

class TrayEventFilterTest : public AshTestBase {
 public:
  TrayEventFilterTest() = default;

  TrayEventFilterTest(const TrayEventFilterTest&) = delete;
  TrayEventFilterTest& operator=(const TrayEventFilterTest&) = delete;

  ~TrayEventFilterTest() override = default;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();

    // Adds this `test_tray_background_view_` to the mock `StatusAreaWidget`.
    // Can't use std::make_unique() here, because we need base class type for
    // template method to link successfully without adding test code to
    // status_area_widget.cc.
    test_tray_background_view_ = static_cast<TestTrayBackgroundView*>(
        StatusAreaWidgetTestHelper::GetStatusAreaWidget()->AddTrayButton(
            std::unique_ptr<TrayBackgroundView>(
                new TestTrayBackgroundView(GetPrimaryShelf()))));

    test_tray_background_view_->SetVisiblePreferred(true);
  }

  void TearDown() override {
    test_tray_background_view_ = nullptr;
    AshTestBase::TearDown();
  }

  void ClickInsideWidget(views::Widget* widget) {
    auto* event_generator = GetEventGenerator();
    event_generator->MoveMouseTo(
        widget->GetWindowBoundsInScreen().CenterPoint());
    event_generator->ClickLeftButton();
  }

  void ClickOutsideWidget(views::Widget* widget) {
    auto* event_generator = GetEventGenerator();
    event_generator->MoveMouseTo(widget->GetWindowBoundsInScreen().origin() -
                                 gfx::Vector2d(1, 1));
    event_generator->ClickLeftButton();
  }

 protected:
  std::string AddNotification() {
    std::string notification_id = base::NumberToString(notification_id_++);
    MessageCenter::Get()->AddNotification(std::make_unique<Notification>(
        message_center::NOTIFICATION_TYPE_SIMPLE, notification_id,
        u"test title", u"test message", ui::ImageModel(),
        std::u16string() /* display_source */, GURL(),
        message_center::NotifierId(), message_center::RichNotificationData(),
        new message_center::NotificationDelegate()));
    return notification_id;
  }

  void ShowTestBubble() { test_tray_background_view_->ShowBubble(); }

  views::Widget* GetTestBubbleWidget() {
    if (!test_tray_background_view_->bubble()) {
      return nullptr;
    }

    return test_tray_background_view_->bubble()->GetBubbleWidget();
  }

  void ShowQuickSettingsBubble() {
    GetPrimaryUnifiedSystemTray()->ShowBubble();
  }

  bool IsQuickSettingsBubbleShown() {
    return GetPrimaryUnifiedSystemTray()->IsBubbleShown();
  }

  UnifiedSystemTray* GetPrimaryUnifiedSystemTray() {
    return GetPrimaryShelf()->GetStatusAreaWidget()->unified_system_tray();
  }

  void AnimatePopupAnimationUntilIdle() {
    AshMessagePopupCollection* popup_collection =
        GetPrimaryNotificationCenterTray()->popup_collection();

    while (popup_collection->animation()->is_animating()) {
      popup_collection->animation()->SetCurrentValue(1.0);
      popup_collection->animation()->End();
    }
  }

  TestTrayBackgroundView* test_tray_background_view() {
    return test_tray_background_view_;
  }

 private:
  int notification_id_ = 0;
  raw_ptr<TestTrayBackgroundView> test_tray_background_view_ = nullptr;
};

// Tests that clicking on notification popup when bubble is open will not result
// in the bubble closes. The logic for this is handled in
// `bubble_utils::ShouldCloseBubbleForEvent()` where we ignore events happen
// inside a `kShellWindowId_SettingBubbleContainer`.
TEST_F(TrayEventFilterTest, ClickOnPopupWhenBubbleOpen) {
  // Update display so that the screen is height enough and expand/collapse
  // notification is allowed on top of the tray bubble.
  UpdateDisplay("901x900");

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kNotifierCollision);

  ShowQuickSettingsBubble();
  EXPECT_TRUE(IsQuickSettingsBubbleShown());

  auto notification_id = AddNotification();
  auto* popup_view = GetPrimaryNotificationCenterTray()
                         ->popup_collection()
                         ->GetMessageViewForNotificationId(notification_id);

  auto* ash_notification_popup = static_cast<AshNotificationView*>(popup_view);

  AnimatePopupAnimationUntilIdle();

  // Collapsing the popup should not close the bubble.
  LeftClickOn(ash_notification_popup->expand_button_for_test());
  // Wait until the animation is complete.
  AnimatePopupAnimationUntilIdle();
  EXPECT_FALSE(ash_notification_popup->IsExpanded());
  EXPECT_TRUE(IsQuickSettingsBubbleShown());

  // Expanding the popup should not close the bubble.
  LeftClickOn(ash_notification_popup->expand_button_for_test());
  // Wait until the animation is complete.
  AnimatePopupAnimationUntilIdle();
  EXPECT_TRUE(ash_notification_popup->IsExpanded());
  EXPECT_TRUE(IsQuickSettingsBubbleShown());
}

TEST_F(TrayEventFilterTest, DraggingInsideDoesNotCloseBubble) {
  ShowTestBubble();
  auto* bubble_widget = GetTestBubbleWidget();
  EXPECT_TRUE(bubble_widget);

  // Dragging within the bubble should not trigger `ClickedOutsideBubble()` and
  // close the bubble.
  const gfx::Rect tray_bounds = bubble_widget->GetWindowBoundsInScreen();
  auto* test_tray = test_tray_background_view();
  auto border_insets = test_tray->bubble()->bubble_view()->GetBorderInsets();
  const gfx::Point start =
      tray_bounds.origin() +
      gfx::Vector2d(border_insets.left(), border_insets.top());
  const gfx::Point end_inside = start + gfx::Vector2d(5, 5);
  GetEventGenerator()->GestureScrollSequence(start, end_inside,
                                             base::Milliseconds(100), 4);

  EXPECT_FALSE(test_tray_background_view()->clicked_outside_bubble_called());
  EXPECT_TRUE(test_tray_background_view()->bubble());

  // Dragging from inside to outside of the bubble should not trigger
  // `ClickedOutsideBubble()` and close the bubble.
  const gfx::Point start_inside = end_inside;
  const gfx::Point end_outside = start + gfx::Vector2d(-5, -5);
  GetEventGenerator()->GestureScrollSequence(start_inside, end_outside,
                                             base::Milliseconds(100), 4);

  EXPECT_FALSE(test_tray_background_view()->clicked_outside_bubble_called());
  EXPECT_TRUE(test_tray_background_view()->bubble());
}

TEST_F(TrayEventFilterTest, DraggingOnTrayClosesBubble) {
  ShowTestBubble();
  EXPECT_TRUE(GetTestBubbleWidget());

  // Dragging on the tray background view should trigger
  // `ClickedOutsideBubble()` and close the bubble.
  const gfx::Rect tray_bounds =
      test_tray_background_view()->GetBoundsInScreen();
  const gfx::Point start = tray_bounds.CenterPoint();
  const gfx::Point end_inside = start + gfx::Vector2d(0, 10);
  GetEventGenerator()->GestureScrollSequence(start, end_inside,
                                             base::Milliseconds(100), 4);

  EXPECT_TRUE(test_tray_background_view()->clicked_outside_bubble_called());
  EXPECT_FALSE(test_tray_background_view()->bubble());
}

// Tests that when we drag up to show the hotseat, the open bubble will be close
// to make sure it does not overlap with the hotseat (crbug/1329327).
TEST_F(TrayEventFilterTest, ShowHotseatClosesBubble) {
  TabletModeControllerTestApi().EnterTabletMode();
  std::unique_ptr<aura::Window> window =
      CreateTestWindow(gfx::Rect(0, 0, 400, 400));
  wm::ActivateWindow(window.get());
  ASSERT_EQ(HotseatState::kHidden,
            GetPrimaryShelf()->shelf_layout_manager()->hotseat_state());

  ShowTestBubble();
  EXPECT_TRUE(GetTestBubbleWidget());

  // Dragging up to show the hotseat.
  gfx::Rect display_bounds =
      display::Screen::GetScreen()->GetPrimaryDisplay().bounds();
  const gfx::Point start = display_bounds.bottom_center();
  const gfx::Point end = start + gfx::Vector2d(0, -80);
  GetEventGenerator()->GestureScrollSequence(
      start, end, /*duration=*/base::Milliseconds(100),
      /*steps=*/4);
  ASSERT_EQ(HotseatState::kExtended,
            GetPrimaryShelf()->shelf_layout_manager()->hotseat_state());

  // `ClickedOutsideBubble()` should be triggered to close the bubble.
  EXPECT_TRUE(test_tray_background_view()->clicked_outside_bubble_called());
  EXPECT_FALSE(test_tray_background_view()->bubble());
}

TEST_F(TrayEventFilterTest, ClickOnCalendarBubbleClosesOtherTrays) {
  Shell::Get()->ime_controller()->ShowImeMenuOnShelf(true);
  auto* status_area = GetPrimaryShelf()->GetStatusAreaWidget();
  auto* ime_tray = status_area->ime_menu_tray();

  LeftClickOn(ime_tray);
  EXPECT_TRUE(ime_tray->GetBubbleWidget());

  auto* date_tray = status_area->date_tray();
  LeftClickOn(date_tray);

  // When opening the calendar, the unified system tray bubble should be open
  // with the calendar view, and the IME bubble should be closed.
  EXPECT_TRUE(IsQuickSettingsBubbleShown());
  EXPECT_FALSE(ime_tray->GetBubbleWidget());
}

// Tests that when we open the calendar while Quick Settings bubble is open, the
// bubble will not be closed.
TEST_F(TrayEventFilterTest, TransitionFromQsToCalendar) {
  ShowQuickSettingsBubble();
  EXPECT_TRUE(IsQuickSettingsBubbleShown());

  LeftClickOn(GetPrimaryShelf()->GetStatusAreaWidget()->date_tray());
  EXPECT_TRUE(IsQuickSettingsBubbleShown());
}

TEST_F(TrayEventFilterTest, CloseTrayBubbleWhenWindowActivated) {
  StatusAreaWidget* status_area = GetPrimaryShelf()->GetStatusAreaWidget();
  UnifiedSystemTray* system_tray = status_area->unified_system_tray();

  LeftClickOn(system_tray);
  ASSERT_EQ(status_area->open_shelf_pod_bubble(),
            system_tray->bubble()->GetBubbleView());

  // Showing a new window and activating it will close the system bubble.
  std::unique_ptr<views::Widget> widget(
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET));
  EXPECT_TRUE(widget->IsActive());
  EXPECT_FALSE(system_tray->bubble());

  // Show a second widget.
  std::unique_ptr<views::Widget> second_widget(
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET));
  EXPECT_TRUE(second_widget->IsActive());

  // Re-show the system bubble.
  LeftClickOn(system_tray);

  // Re-activate the first widget. The system bubble should hide again.
  widget->Activate();
  EXPECT_FALSE(system_tray->bubble());

  Shell::Get()->ime_controller()->ShowImeMenuOnShelf(true);
  TrayBackgroundView* ime_menu = status_area->ime_menu_tray();

  // Test the same thing with the ime tray.
  LeftClickOn(ime_menu);
  ASSERT_EQ(status_area->open_shelf_pod_bubble(), ime_menu->GetBubbleView());

  second_widget->Activate();
  EXPECT_FALSE(ime_menu->GetBubbleView());
}

TEST_F(TrayEventFilterTest, NotCloseTrayBubbleWhenTranscientChildActivated) {
  UnifiedSystemTray* system_tray =
      GetPrimaryShelf()->GetStatusAreaWidget()->unified_system_tray();

  ShowQuickSettingsBubble();

  auto* bubble = system_tray->bubble();

  // Show the network detailed view.
  bubble->unified_system_tray_controller()->ShowNetworkDetailedView();

  // Click on the info button in the network detailed view so that a transient
  // bubble is opened.
  auto* info_button = bubble->quick_settings_view()
                          ->GetDetailedViewForTest<NetworkDetailedView>()
                          ->info_button_for_testing();
  LeftClickOn(info_button);

  // Since a transcient child of the bubble is activated, the bubble should
  // remain open.
  EXPECT_TRUE(system_tray->bubble());
}

}  // namespace ash
