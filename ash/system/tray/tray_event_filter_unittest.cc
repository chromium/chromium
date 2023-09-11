// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/tray_event_filter.h"

#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/constants/ash_features.h"
#include "ash/ime/ime_controller_impl.h"
#include "ash/root_window_controller.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/system/ime_menu/ime_menu_tray.h"
#include "ash/system/message_center/ash_message_popup_collection.h"
#include "ash/system/message_center/ash_notification_expand_button.h"
#include "ash/system/message_center/ash_notification_view.h"
#include "ash/system/message_center/unified_message_center_bubble.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/status_area_widget_test_helper.h"
#include "ash/system/tray/tray_utils.h"
#include "ash/system/unified/date_tray.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window.h"
#include "ui/message_center/message_center.h"

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
  void ClickedOutsideBubble() override {
    clicked_outside_bubble_called_ = true;
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

  void ShowBubble() override {
    auto bubble_view = std::make_unique<TrayBubbleView>(
        CreateInitParamsForTrayBubble(/*tray=*/this));
    bubble_ = std::make_unique<TrayBubbleWrapper>(this,
                                                  /*event_handling=*/false);
    bubble_->ShowBubble(std::move(bubble_view));
  }

  void CloseBubble() override { bubble_.reset(); }

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

class TrayEventFilterTest : public AshTestBase,
                            public testing::WithParamInterface<bool> {
 public:
  TrayEventFilterTest() = default;

  TrayEventFilterTest(const TrayEventFilterTest&) = delete;
  TrayEventFilterTest& operator=(const TrayEventFilterTest&) = delete;

  ~TrayEventFilterTest() override = default;

  // AshTestBase:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatureState(features::kQsRevamp,
                                              /*enabled=*/IsQsRevampEnabed());
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

  ui::MouseEvent outside_event() {
    const gfx::Rect tray_bounds = GetSystemTrayBoundsInScreen();
    const gfx::Point point = tray_bounds.bottom_right() + gfx::Vector2d(1, 1);
    const base::TimeTicks time = base::TimeTicks::Now();
    return ui::MouseEvent(ui::ET_MOUSE_PRESSED, point, point, time, 0, 0);
  }

  ui::MouseEvent inside_event() {
    const gfx::Rect tray_bounds = GetSystemTrayBoundsInScreen();
    const gfx::Point point = tray_bounds.origin();
    const base::TimeTicks time = base::TimeTicks::Now();
    return ui::MouseEvent(ui::ET_MOUSE_PRESSED, point, point, time, 0, 0);
  }

  ui::MouseEvent InsideMessageCenterEvent() {
    const gfx::Rect message_center_bounds = GetMessageCenterBoundsInScreen();
    const gfx::Point point = message_center_bounds.origin();
    const base::TimeTicks time = base::TimeTicks::Now();
    return ui::MouseEvent(ui::ET_MOUSE_PRESSED, point, point, time, 0, 0);
  }

 protected:
  bool IsQsRevampEnabed() { return GetParam(); }

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

  void ShowSystemTrayMainView() { GetPrimaryUnifiedSystemTray()->ShowBubble(); }

  bool IsBubbleShown() {
    return GetPrimaryUnifiedSystemTray()->IsBubbleShown();
  }

  bool IsMessageCenterBubbleShown() {
    return GetPrimaryUnifiedSystemTray()->IsMessageCenterBubbleShown();
  }

  gfx::Rect GetSystemTrayBoundsInScreen() {
    return GetPrimaryUnifiedSystemTray()->GetBubbleBoundsInScreen();
  }

  TrayEventFilter* GetTrayEventFilter() {
    return GetPrimaryUnifiedSystemTray()->tray_event_filter();
  }

  UnifiedSystemTray* GetPrimaryUnifiedSystemTray() {
    return GetPrimaryShelf()->GetStatusAreaWidget()->unified_system_tray();
  }

  UnifiedMessageCenterBubble* GetMessageCenterBubble() {
    return GetPrimaryUnifiedSystemTray()->message_center_bubble();
  }
  gfx::Rect GetMessageCenterBoundsInScreen() {
    return GetMessageCenterBubble()->GetBubbleView()->GetBoundsInScreen();
  }

  void AnimatePopupAnimationUntilIdle() {
    AshMessagePopupCollection* popup_collection =
        GetPrimaryUnifiedSystemTray()->GetMessagePopupCollection();

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
  base::test::ScopedFeatureList scoped_feature_list_;
  raw_ptr<TestTrayBackgroundView> test_tray_background_view_ = nullptr;
};

INSTANTIATE_TEST_SUITE_P(IsQsRevampEnabled,
                         TrayEventFilterTest,
                         testing::Bool());

TEST_P(TrayEventFilterTest, ClickingOutsideCloseBubble) {
  ShowSystemTrayMainView();
  EXPECT_TRUE(IsBubbleShown());

  // Clicking outside should close the bubble.
  ui::MouseEvent event = outside_event();
  GetTrayEventFilter()->OnMouseEvent(&event);
  EXPECT_FALSE(IsBubbleShown());
}

TEST_P(TrayEventFilterTest, ClickingInsideDoesNotCloseBubble) {
  ShowSystemTrayMainView();
  EXPECT_TRUE(IsBubbleShown());

  // Clicking inside should not close the bubble
  ui::MouseEvent event = inside_event();
  GetTrayEventFilter()->OnMouseEvent(&event);
  EXPECT_TRUE(IsBubbleShown());
}

TEST_P(TrayEventFilterTest, ClickingOnTray) {
  auto* test_tray = test_tray_background_view();
  LeftClickOn(test_tray);
  EXPECT_TRUE(test_tray->bubble());

  // Clicking on the tray when bubble is open will not trigger
  // `ClickedOutsideBubble()`, since this will be handled in the tray level.
  LeftClickOn(test_tray);
  EXPECT_FALSE(test_tray->clicked_outside_bubble_called());
}

TEST_P(TrayEventFilterTest, CaptureMode) {
  ShowSystemTrayMainView();
  EXPECT_TRUE(IsBubbleShown());

  CaptureModeController::Get()->Start(CaptureModeEntryType::kQuickSettings);

  // Clicking outside of the bubble during capture mode still keeps the bubble
  // open.
  ui::MouseEvent event = outside_event();
  GetTrayEventFilter()->OnMouseEvent(&event);

  EXPECT_TRUE(IsBubbleShown());
}

TEST_P(TrayEventFilterTest, ClickingOnMenuContainerDoesNotCloseBubble) {
  // Create a menu window and place it in the menu container window.
  std::unique_ptr<aura::Window> menu_window = CreateTestWindow();
  menu_window->set_owned_by_parent(false);
  Shell::GetPrimaryRootWindowController()
      ->GetContainer(kShellWindowId_MenuContainer)
      ->AddChild(menu_window.get());

  ShowSystemTrayMainView();
  EXPECT_TRUE(IsBubbleShown());

  // Clicking on MenuContainer should not close the bubble.
  ui::MouseEvent event = outside_event();
  ui::Event::DispatcherApi(&event).set_target(menu_window.get());
  GetTrayEventFilter()->OnMouseEvent(&event);
  EXPECT_TRUE(IsBubbleShown());
}

TEST_P(TrayEventFilterTest, ClickingOnPopupWhenBubbleOpen) {
  // Update display so that the screen is height enough and expand/collapse
  // notification is allowed on top of the tray bubble.
  UpdateDisplay("901x900");

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kNotifierCollision);

  ShowSystemTrayMainView();
  EXPECT_TRUE(IsBubbleShown());

  auto notification_id = AddNotification();
  auto* popup_view = GetPrimaryUnifiedSystemTray()
                         ->GetMessagePopupCollection()
                         ->GetMessageViewForNotificationId(notification_id);

  if (!IsQsRevampEnabed()) {
    // When QsRevamp is not enabled, the popup will not be shown when Quick
    // Settings is open.
    EXPECT_FALSE(popup_view);
    return;
  }

  auto* ash_notification_popup = static_cast<AshNotificationView*>(popup_view);

  AnimatePopupAnimationUntilIdle();

  // Collapsing the popup should not close the bubble.
  LeftClickOn(ash_notification_popup->expand_button_for_test());
  // Wait until the animation is complete.
  AnimatePopupAnimationUntilIdle();
  EXPECT_FALSE(ash_notification_popup->IsExpanded());
  EXPECT_TRUE(IsBubbleShown());

  // Expanding the popup should not close the bubble.
  LeftClickOn(ash_notification_popup->expand_button_for_test());
  // Wait until the animation is complete.
  AnimatePopupAnimationUntilIdle();
  EXPECT_TRUE(ash_notification_popup->IsExpanded());
  EXPECT_TRUE(IsBubbleShown());
}

TEST_P(TrayEventFilterTest, ClickingOnKeyboardContainerDoesNotCloseBubble) {
  // Simulate the virtual keyboard being open. In production the virtual
  // keyboard container only exists while the keyboard is open.
  std::unique_ptr<aura::Window> keyboard_container =
      CreateTestWindow(gfx::Rect(), aura::client::WINDOW_TYPE_NORMAL,
                       kShellWindowId_VirtualKeyboardContainer);
  std::unique_ptr<aura::Window> keyboard_window = CreateTestWindow();
  keyboard_window->set_owned_by_parent(false);
  keyboard_container->AddChild(keyboard_window.get());

  ShowSystemTrayMainView();
  EXPECT_TRUE(IsBubbleShown());

  // Clicking on KeyboardContainer should not close the bubble.
  ui::MouseEvent event = outside_event();
  ui::Event::DispatcherApi(&event).set_target(keyboard_window.get());
  GetTrayEventFilter()->OnMouseEvent(&event);
  EXPECT_TRUE(IsBubbleShown());
}

TEST_P(TrayEventFilterTest, DraggingInsideDoesNotCloseBubble) {
  ShowSystemTrayMainView();
  EXPECT_TRUE(IsBubbleShown());

  // Dragging within the bubble should not close the bubble.
  const gfx::Rect tray_bounds = GetSystemTrayBoundsInScreen();
  const gfx::Point start = tray_bounds.origin();
  const gfx::Point end_inside = start + gfx::Vector2d(5, 5);
  GetEventGenerator()->GestureScrollSequence(start, end_inside,
                                             base::Milliseconds(100), 4);
  EXPECT_TRUE(IsBubbleShown());

  // Dragging from inside to outside of the bubble should not close the bubble.
  const gfx::Point start_inside = end_inside;
  const gfx::Point end_outside = start + gfx::Vector2d(-5, -5);
  GetEventGenerator()->GestureScrollSequence(start_inside, end_outside,
                                             base::Milliseconds(100), 4);
  EXPECT_TRUE(IsBubbleShown());
}

TEST_P(TrayEventFilterTest, DraggingOnTrayClosesBubble) {
  ShowSystemTrayMainView();
  EXPECT_TRUE(IsBubbleShown());

  // Dragging on the tray background view should close the bubble.
  const gfx::Rect tray_bounds =
      GetPrimaryUnifiedSystemTray()->GetBoundsInScreen();
  const gfx::Point start = tray_bounds.CenterPoint();
  const gfx::Point end_inside = start + gfx::Vector2d(0, 10);
  GetEventGenerator()->GestureScrollSequence(start, end_inside,
                                             base::Milliseconds(100), 4);
  EXPECT_FALSE(IsBubbleShown());
}

TEST_P(TrayEventFilterTest, ClickOnCalendarBubbleClosesOtherTrays) {
  Shell::Get()->ime_controller()->ShowImeMenuOnShelf(true);
  auto* status_area = GetPrimaryShelf()->GetStatusAreaWidget();
  auto* ime_tray = status_area->ime_menu_tray();

  LeftClickOn(ime_tray);
  EXPECT_TRUE(ime_tray->GetBubbleWidget());

  auto* date_tray = status_area->date_tray();
  LeftClickOn(date_tray);

  // When opening the calendar, the unified system tray bubble should be open
  // with the calendar view, and the IME bubble should be closed.
  EXPECT_TRUE(IsBubbleShown());
  EXPECT_FALSE(ime_tray->GetBubbleWidget());
}

// Tests that when we open the calendar while Quick Settings bubble is open, the
// bubble will not be closed.
TEST_P(TrayEventFilterTest, TransitionFromQsToCalendar) {
  ShowSystemTrayMainView();
  EXPECT_TRUE(IsBubbleShown());

  LeftClickOn(GetPrimaryShelf()->GetStatusAreaWidget()->date_tray());
  EXPECT_TRUE(IsBubbleShown());
}

using TrayEventFilterQsRevampDisabledTest = TrayEventFilterTest;

INSTANTIATE_TEST_SUITE_P(QsRevampDisabled,
                         TrayEventFilterQsRevampDisabledTest,
                         testing::Values(false));

TEST_P(TrayEventFilterQsRevampDisabledTest,
       MessageCenterAndSystemTrayStayOpenTogether) {
  AddNotification();

  ShowSystemTrayMainView();
  EXPECT_TRUE(GetMessageCenterBubble()->GetBubbleWidget()->IsVisible());
  EXPECT_TRUE(IsBubbleShown());

  // Clicking inside system tray should not close either bubble.
  ui::MouseEvent event = inside_event();
  GetTrayEventFilter()->OnMouseEvent(&event);
  EXPECT_TRUE(GetMessageCenterBubble()->GetBubbleWidget()->IsVisible());
  EXPECT_TRUE(IsBubbleShown());

  // Clicking inside the message center bubble should not close either bubble.
  event = InsideMessageCenterEvent();
  GetTrayEventFilter()->OnMouseEvent(&event);
  EXPECT_TRUE(GetMessageCenterBubble()->GetBubbleWidget()->IsVisible());
  EXPECT_TRUE(IsBubbleShown());
}

TEST_P(TrayEventFilterQsRevampDisabledTest,
       MessageCenterAndSystemTrayCloseTogether) {
  AddNotification();

  ShowSystemTrayMainView();
  EXPECT_TRUE(IsMessageCenterBubbleShown());
  EXPECT_TRUE(IsBubbleShown());

  // Clicking outside should close both bubbles.
  ui::MouseEvent event = outside_event();
  GetTrayEventFilter()->OnMouseEvent(&event);
  EXPECT_FALSE(IsMessageCenterBubbleShown());
  EXPECT_FALSE(IsBubbleShown());
}

}  // namespace ash
