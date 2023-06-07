// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/notification_center/notification_center_view.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/focus_cycler.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/message_center/ash_message_center_lock_screen_controller.h"
#include "ash/system/message_center/message_center_constants.h"
#include "ash/system/notification_center/notification_center_test_api.h"
#include "ash/system/notification_center/stacked_notification_bar.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/system/unified/unified_system_tray_model.h"
#include "ash/test/ash_test_base.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/test/scoped_feature_list.h"
#include "components/prefs/pref_service.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/views/message_view.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/test/views_test_utils.h"
#include "url/gurl.h"

using message_center::MessageCenter;
using message_center::MessageView;
using message_center::Notification;

namespace ash {

class NotificationCenterViewTest : public AshTestBase,
                                   public views::ViewObserver,
                                   public testing::WithParamInterface<bool> {
 public:
  NotificationCenterViewTest() = default;

  NotificationCenterViewTest(const NotificationCenterViewTest&) = delete;
  NotificationCenterViewTest& operator=(const NotificationCenterViewTest&) =
      delete;

  ~NotificationCenterViewTest() override = default;

  // AshTestBase:
  void SetUp() override {
    scoped_feature_list_ = std::make_unique<base::test::ScopedFeatureList>();
    scoped_feature_list_->InitWithFeatureState(features::kQsRevamp,
                                               /*enabled=*/IsQsRevampEnabled());

    AshTestBase::SetUp();

    test_api_ = std::make_unique<NotificationCenterTestApi>(/*tray=*/nullptr);
  }

  bool IsQsRevampEnabled() const { return GetParam(); }

  void TearDown() override {
    base::RunLoop().RunUntilIdle();
    AshTestBase::TearDown();
  }

  // views::ViewObserver:
  void OnViewPreferredSizeChanged(views::View* view) override {
    if (view->GetPreferredSize() == view->size()) {
      return;
    }
    view->SetBoundsRect(view->GetVisible() ? gfx::Rect(view->GetPreferredSize())
                                           : gfx::Rect());
    views::test::RunScheduledLayout(view);
    ++size_changed_count_;
  }

 protected:
  // Adds more than enough notifications to make the message center scrollable.
  std::vector<std::string> AddManyNotifications() {
    std::vector<std::string> ids;
    for (int i = 0; i < 10; ++i) {
      ids.push_back(test_api()->AddNotification());
    }
    return ids;
  }

  void AnimateNotificationListToValue(float value) {
    GetNotificationListView()->animation_->SetCurrentValue(value);
    GetNotificationListView()->AnimationProgressed(
        GetNotificationListView()->animation_.get());
  }

  void AnimateNotificationListToMiddle() {
    AnimateNotificationListToValue(0.5);
  }

  void AnimateNotificationListToEnd() {
    FinishNotificationListSlideOutAnimations();
    GetNotificationListView()->animation_->End();
  }

  void AnimateNotificationListUntilIdle() {
    while (GetNotificationListView()->animation_->is_animating()) {
      GetNotificationListView()->animation_->End();
    }
  }

  void FinishNotificationListSlideOutAnimations() {
    base::RunLoop().RunUntilIdle();
  }

  gfx::Rect GetMessageViewVisibleBounds(size_t index) {
    gfx::Rect bounds = GetNotificationListView()->children()[index]->bounds();
    bounds -= GetScroller()->GetVisibleRect().OffsetFromOrigin();
    bounds += GetScroller()->bounds().OffsetFromOrigin();
    return bounds;
  }

  NotificationListView* GetNotificationListView() {
    return notification_center_view()->notification_list_view_;
  }

  gfx::LinearAnimation* GetMessageCenterAnimation() {
    return notification_center_view()->animation_.get();
  }

  views::ScrollView* GetScroller() {
    return notification_center_view()->scroller_;
  }

  views::ScrollBar* GetScrollBar() {
    return notification_center_view()->scroll_bar_;
  }

  views::View* GetScrollerContents() {
    return notification_center_view()->scroller_->contents();
  }

  StackedNotificationBar* GetNotificationBar() {
    return notification_center_view()->notification_bar_;
  }

  views::View* GetNotificationBarIconsContainer() {
    return notification_center_view()
        ->notification_bar_->notification_icons_container_;
  }

  views::View* GetNotificationBarLabel() {
    return notification_center_view()->notification_bar_->count_label_;
  }

  views::View* GetNotificationBarClearAllButton() {
    return notification_center_view()->notification_bar_->clear_all_button_;
  }

  views::View* GetNotificationBarExpandAllButton() {
    return notification_center_view()->notification_bar_->expand_all_button_;
  }

  int total_notification_count() {
    return GetNotificationBar()->total_notification_count_;
  }

  int pinned_notification_count() {
    return GetNotificationBar()->pinned_notification_count_;
  }

  int unpinned_notification_count() {
    return GetNotificationBar()->total_notification_count_ -
           GetNotificationBar()->pinned_notification_count_;
  }

  int stacked_notification_count() {
    return GetNotificationBar()->stacked_notification_count_;
  }

  message_center::MessageView* FocusNotificationView(const std::string& id) {
    auto* focus_manager = notification_center_view()->GetFocusManager();
    if (!focus_manager) {
      return nullptr;
    }

    auto* focused_message_view = test_api()->GetNotificationViewForId(id);
    focus_manager->SetFocusedView(focused_message_view);
    return focused_message_view;
  }

  void FocusClearAllButton() {
    auto* widget = GetNotificationBarClearAllButton()->GetWidget();
    widget->widget_delegate()->SetCanActivate(true);
    Shell::Get()->focus_cycler()->FocusWidget(widget);
    GetNotificationBarClearAllButton()->RequestFocus();
  }

  void RelayoutMessageCenterViewForTest() {
    // Outside of tests, any changes to bubble's size as well as scrolling
    // through notification list will trigger TrayBubbleView's BoxLayout to
    // relayout, and then this view will relayout. In test, we don't have
    // TrayBubbleView as the parent, so we need to ensure Layout() is executed
    // in some circumstances.
    views::test::RunScheduledLayout(test_api()->GetNotificationCenterView());
  }

  void UpdateNotificationBarForTest() {
    // TODO(crbug/1357232): Refactor so this code mirrors production better.
    // Outside of tests, the notification bar is updated with a call to
    // NotificationCenterBubble::UpdatePosition(), but this function is not
    // triggered when adding notifications in tests.
    test_api_->GetNotificationCenterView()->UpdateNotificationBar();
  }

  virtual NotificationCenterView* notification_center_view() {
    return test_api_->GetNotificationCenterView();
  }

  int size_changed_count() const { return size_changed_count_; }

  NotificationCenterTestApi* test_api() { return test_api_.get(); }

 private:
  int size_changed_count_ = 0;

  std::unique_ptr<NotificationCenterTestApi> test_api_;
  std::unique_ptr<base::test::ScopedFeatureList> scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         NotificationCenterViewTest,
                         testing::Bool() /* IsQsRevampEnabed() */);

// Flaky: https://crbug.com/1293165
TEST_P(NotificationCenterViewTest, DISABLED_AddAndRemoveNotification) {
  test_api()->ToggleBubble();
  EXPECT_FALSE(notification_center_view()->GetVisible());

  auto id0 = test_api()->AddNotification();
  EXPECT_TRUE(notification_center_view()->GetVisible());

  // The notification first slides out of the list.
  test_api()->RemoveNotification(id0);
  AnimateNotificationListToEnd();

  // After all the last notifiation slides out, the message center and list
  // should collapse.
  auto* collapse_animation = GetMessageCenterAnimation();
  collapse_animation->SetCurrentValue(0.5);
  notification_center_view()->AnimationProgressed(collapse_animation);
  EXPECT_TRUE(notification_center_view()->GetVisible());

  // The message center is now hidden after all animations complete.
  collapse_animation->End();
  AnimateNotificationListToEnd();
  EXPECT_FALSE(notification_center_view()->GetVisible());
}

TEST_P(NotificationCenterViewTest, ContentsRelayout) {
  // TODO(b/266996101): Enable this test for QsRevamp after this sizing bug is
  // fixed.
  if (IsQsRevampEnabled()) {
    return;
  }

  std::vector<std::string> ids = AddManyNotifications();
  test_api()->ToggleBubble();
  EXPECT_TRUE(notification_center_view()->GetVisible());
  // MessageCenterView is maxed out.
  EXPECT_GT(GetNotificationListView()->bounds().height(),
            notification_center_view()->bounds().height());
  const int previous_contents_height = GetScrollerContents()->height();
  const int previous_list_height = GetNotificationListView()->height();

  test_api()->RemoveNotification(ids.back());
  AnimateNotificationListToEnd();
  RelayoutMessageCenterViewForTest();

  EXPECT_TRUE(notification_center_view()->GetVisible());
  EXPECT_GT(previous_contents_height, GetScrollerContents()->height());
  EXPECT_GT(previous_list_height, GetNotificationListView()->height());
}

TEST_P(NotificationCenterViewTest, InsufficientHeight) {
  // The notification center is not stacked on top of the quick settings if
  // QSRevamp is enabled so this test is irrelevant.
  if (IsQsRevampEnabled()) {
    return;
  }

  test_api()->ToggleBubble();
  test_api()->AddNotification();
  EXPECT_TRUE(notification_center_view()->GetVisible());

  notification_center_view()->SetAvailableHeight(
      kUnifiedNotificationMinimumHeight - 1);
  EXPECT_FALSE(notification_center_view()->GetVisible());

  notification_center_view()->SetAvailableHeight(
      kUnifiedNotificationMinimumHeight);
  EXPECT_TRUE(notification_center_view()->GetVisible());
}

TEST_P(NotificationCenterViewTest, NotVisibleWhenLocked) {
  // Disable the lock screen notification if the feature is enable.
  PrefService* user_prefs =
      Shell::Get()->session_controller()->GetLastActiveUserPrefService();
  user_prefs->SetString(prefs::kMessageCenterLockScreenMode,
                        prefs::kMessageCenterLockScreenModeHide);

  ASSERT_FALSE(AshMessageCenterLockScreenController::IsEnabled());

  test_api()->AddNotification();
  test_api()->AddNotification();

  BlockUserSession(BLOCKED_BY_LOCK_SCREEN);
  test_api()->ToggleBubble();

  // The visibility of the notification center is controlled by
  // `NotificationCenterTray` with QSRevamp enabled.
  if (!IsQsRevampEnabled()) {
    EXPECT_FALSE(notification_center_view()->GetVisible());
  }
}

TEST_P(NotificationCenterViewTest, VisibleWhenLocked) {
  // This test is only valid if the lock screen feature is enabled.
  // TODO(yoshiki): Clean up after the feature is launched crbug.com/913764.
  if (!features::IsLockScreenNotificationsEnabled()) {
    return;
  }

  // Enables the lock screen notification if the feature is disabled.
  PrefService* user_prefs =
      Shell::Get()->session_controller()->GetLastActiveUserPrefService();
  user_prefs->SetString(prefs::kMessageCenterLockScreenMode,
                        prefs::kMessageCenterLockScreenModeShow);

  ASSERT_TRUE(AshMessageCenterLockScreenController::IsEnabled());

  test_api()->AddNotification();
  test_api()->AddNotification();

  BlockUserSession(BLOCKED_BY_LOCK_SCREEN);
  test_api()->ToggleBubble();

  EXPECT_TRUE(notification_center_view()->GetVisible());
}

TEST_P(NotificationCenterViewTest, ClearAllPressed) {
  test_api()->AddNotification();
  test_api()->AddNotification();
  test_api()->ToggleBubble();
  EXPECT_TRUE(notification_center_view()->GetVisible());
  EXPECT_TRUE(GetNotificationBar()->GetVisible());

  // When Clear All button is pressed, all notifications are removed and the
  // view becomes invisible.
  notification_center_view()->ClearAllNotifications();

  // The visibility of the notification center is controlled by
  // `NotificationCenterTray` with QSRevamp enabled.
  if (!IsQsRevampEnabled()) {
    AnimateNotificationListUntilIdle();
    EXPECT_FALSE(notification_center_view()->GetVisible());
  }
}

TEST_P(NotificationCenterViewTest, InitialPosition) {
  test_api()->AddNotification();
  test_api()->AddNotification();
  test_api()->ToggleBubble();
  EXPECT_TRUE(notification_center_view()->GetVisible());

  // MessageCenterView is not maxed out.
  EXPECT_LT(GetNotificationListView()->bounds().height(),
            notification_center_view()->bounds().height());
}

TEST_P(NotificationCenterViewTest, InitialPositionMaxOut) {
  AddManyNotifications();
  test_api()->ToggleBubble();
  EXPECT_TRUE(notification_center_view()->GetVisible());

  // MessageCenterView is maxed out.
  EXPECT_GT(GetNotificationListView()->bounds().height(),
            notification_center_view()->bounds().height());
}

TEST_P(NotificationCenterViewTest, InitialPositionWithLargeNotification) {
  if (IsQsRevampEnabled()) {
    return;
  }

  UpdateDisplay("600x360");
  test_api()->AddNotification();
  test_api()->AddNotification();
  test_api()->ToggleBubble();

  OnViewPreferredSizeChanged(test_api()->GetNotificationCenterView());

  EXPECT_TRUE(notification_center_view()->GetVisible());

  // MessageCenterView is shorter than the notification.
  gfx::Rect message_view_bounds = GetMessageViewVisibleBounds(0);
  EXPECT_LT(notification_center_view()->bounds().height(),
            message_view_bounds.height());
}

// Tests basic layout of the StackingNotificationBar.
TEST_P(NotificationCenterViewTest, StackingCounterLabelLayout) {
  UpdateDisplay("800x500");

  AddManyNotifications();

  // MessageCenterView is maxed out.
  test_api()->ToggleBubble();

  EXPECT_GT(GetNotificationListView()->bounds().height(),
            notification_center_view()->bounds().height());

  EXPECT_TRUE(GetNotificationBar()->GetVisible());

  EXPECT_EQ(kMessageCenterPadding, GetScroller()->bounds().y());
  EXPECT_EQ(GetNotificationBar()->bounds().y(),
            GetScroller()->bounds().bottom());

  EXPECT_TRUE(GetNotificationBarLabel()->GetVisible());

  EXPECT_TRUE(GetNotificationBarClearAllButton()->GetVisible());
}

// Tests that the NotificationBarLabel is invisible when scrolled to the top.
TEST_P(NotificationCenterViewTest, StackingCounterLabelInvisible) {
  UpdateDisplay("800x500");

  AddManyNotifications();
  test_api()->ToggleBubble();

  // Scroll to the bottom, the counter label should be invisible.
  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(
      notification_center_view()->GetBoundsInScreen().CenterPoint());
  event_generator->MoveMouseWheel(0, -10000);

  EXPECT_FALSE(GetNotificationBarLabel()->GetVisible());
  // ClearAll label should always be visible.
  EXPECT_TRUE(GetNotificationBarClearAllButton()->GetVisible());
}

// Tests that the NotificationBarLabel is visible when there are enough excess
// notifications.
TEST_P(NotificationCenterViewTest, StackingCounterLabelVisible) {
  UpdateDisplay("800x500");

  AddManyNotifications();
  test_api()->ToggleBubble();

  notification_center_view()->OnMessageCenterScrolled();

  EXPECT_TRUE(GetNotificationBarLabel()->GetVisible());
  // ClearAll label should always be visible.
  EXPECT_TRUE(GetNotificationBarClearAllButton()->GetVisible());
}

// Tests that the +n notifications label hides after being shown.
TEST_P(NotificationCenterViewTest, StackingCounterLabelHidesAfterShown) {
  UpdateDisplay("800x500");

  AddManyNotifications();
  test_api()->ToggleBubble();

  // Scroll to the bottom, making the counter label invisible.
  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(
      notification_center_view()->GetBoundsInScreen().CenterPoint());
  event_generator->MoveMouseWheel(0, -10000);

  EXPECT_FALSE(GetNotificationBarLabel()->GetVisible());

  // Scrolling past 5 notifications should make the counter label visible.
  const int scroll_amount = (GetMessageViewVisibleBounds(0).height() * 5) + 1;
  event_generator->MoveMouseWheel(0, scroll_amount);

  ASSERT_TRUE(GetNotificationBarLabel()->GetVisible());

  // Scrolling back to the bottom should make the
  // counter label invisible again.
  event_generator->MoveMouseWheel(0, -10000);

  EXPECT_FALSE(GetNotificationBarLabel()->GetVisible());
  // ClearAll label should always be visible.
  EXPECT_TRUE(GetNotificationBarClearAllButton()->GetVisible());
}

// Tests that there are never more than 3 stacked icons in the
// StackedNotificationBar. Also verifies that only one animation happens at a
// time (this prevents the user from over-scrolling and showing multiple
// animations when they scroll very quickly). Before, users could scroll fast
// and have a large amount of icons, instead of keeping it to 3.
TEST_P(NotificationCenterViewTest, StackingIconsNeverMoreThanThree) {
  for (int i = 0; i < 20; ++i) {
    test_api()->AddNotification();
  }
  test_api()->ToggleBubble();

  auto bottom_position = GetScrollBar()->bounds().bottom();
  GetScroller()->ScrollToPosition(GetScrollBar(), bottom_position);
  notification_center_view()->OnMessageCenterScrolled();

  // Force animations to happen, so we can see if multiple animations trigger.
  ui::ScopedAnimationDurationScaleMode scoped_duration_modifier(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);
  // Scroll past 20 notifications, so we can scroll back up quickly.
  for (int i = 20; i >= 0; --i) {
    const int scroll_amount = (GetMessageViewVisibleBounds(0).height() * i) + 1;
    GetScroller()->ScrollToPosition(GetScrollBar(),
                                    bottom_position - scroll_amount);
    notification_center_view()->OnMessageCenterScrolled();

    auto icons_container_children =
        GetNotificationBarIconsContainer()->children();
    int animating_count = 0;
    for (auto* child : icons_container_children) {
      // Verify that no more than one icon is animating at any one time.
      if (child->layer()->GetAnimator()->is_animating()) {
        animating_count++;
      }
    }
    EXPECT_GE(1, animating_count);
    // Verify that no more than 3 icons are added to the bar at any one time,
    // regardless of how fast the user scrolls. This test scrolls faster than
    // the icons can animate away, and animating icons should be removed prior
    // to starting a new animation.
    EXPECT_GE(3u, icons_container_children.size());
  }
}

// Flaky: crbug.com/1163575
TEST_P(NotificationCenterViewTest,
       DISABLED_StackingCounterNotificationRemoval) {
  std::vector<std::string> ids = AddManyNotifications();
  test_api()->ToggleBubble();
  EXPECT_TRUE(notification_center_view()->GetVisible());

  // MessageCenterView is maxed out.
  EXPECT_GT(GetNotificationListView()->bounds().height(),
            notification_center_view()->bounds().height());

  // Dismiss until there are 2 notifications. The bar should still be visible.
  EXPECT_TRUE(GetNotificationBar()->GetVisible());
  for (size_t i = 0; (i + 2) < ids.size(); ++i) {
    test_api()->RemoveNotification(ids[i]);
    AnimateNotificationListToEnd();
  }
  EXPECT_TRUE(GetNotificationBar()->GetVisible());
  EXPECT_FALSE(GetNotificationBarLabel()->GetVisible());
  EXPECT_TRUE(GetNotificationBarClearAllButton()->GetVisible());

  // The MessageCenterView should be tall enough to contain the bar, two
  // notifications.
  EXPECT_EQ(kStackedNotificationBarHeight + GetNotificationListView()->height(),
            notification_center_view()->height());

  // Dismiss until there is only 1 notification left. The bar should be
  // hidden after an animation.
  test_api()->RemoveNotification(ids[ids.size() - 2]);
  EXPECT_TRUE(GetNotificationBar()->GetVisible());

  // The HIDE_STACKING_BAR animation starts after the notification is slid out.
  AnimateNotificationListToEnd();
  auto* hide_animation = GetMessageCenterAnimation();
  EXPECT_TRUE(hide_animation->is_animating());
  EXPECT_TRUE(GetNotificationBar()->GetVisible());

  // Animate to middle. The bar should still be visible.
  AnimateNotificationListToMiddle();
  hide_animation->SetCurrentValue(0.5);
  notification_center_view()->AnimationProgressed(hide_animation);
  EXPECT_TRUE(GetNotificationBar()->GetVisible());

  // Animate to end. The bar should now be hidden.
  AnimateNotificationListToEnd();
  hide_animation->End();
  EXPECT_FALSE(GetNotificationBar()->GetVisible());
}

TEST_P(NotificationCenterViewTest, StackingCounterLabelRelaidOutOnScroll) {
  // Open the message center at the top of the notification list so the stacking
  // bar is hidden by default.
  std::string id = test_api()->AddNotification();
  int total_notifications = 30;
  for (int i = 0; i < total_notifications; ++i) {
    test_api()->AddNotification();
  }
  GetPrimaryUnifiedSystemTray()->model()->SetTargetNotification(id);

  test_api()->ToggleBubble();

  auto bottom_position =
      GetMessageViewVisibleBounds(total_notifications - 1).bottom();

  GetScroller()->ScrollToPosition(GetScrollBar(), bottom_position);
  notification_center_view()->OnMessageCenterScrolled();

  EXPECT_FALSE(GetNotificationBarLabel()->GetVisible());

  // Scroll past 6 notifications so the count label becomes visible
  int scroll_amount = (GetMessageViewVisibleBounds(0).height() * 6) + 1;
  GetScroller()->ScrollToPosition(GetScrollBar(),
                                  bottom_position - scroll_amount);
  notification_center_view()->OnMessageCenterScrolled();
  RelayoutMessageCenterViewForTest();
  EXPECT_TRUE(GetNotificationBarLabel()->GetVisible());
  int label_width = GetNotificationBarLabel()->bounds().width();
  EXPECT_GT(label_width, 0);

  // Scroll past 14 notifications so the label width must be expanded to
  // contain longer 2-digit label.
  scroll_amount = (GetMessageViewVisibleBounds(0).height() * 14) + 1;
  GetScroller()->ScrollToPosition(GetScrollBar(),
                                  bottom_position - scroll_amount);
  notification_center_view()->OnMessageCenterScrolled();
  RelayoutMessageCenterViewForTest();
  EXPECT_GT(GetNotificationBarLabel()->bounds().width(), label_width);
}

TEST_P(NotificationCenterViewTest, FocusClearedAfterNotificationRemoval) {
  test_api()->AddNotification();
  auto id1 = test_api()->AddNotification();

  test_api()->ToggleBubble();

  // Focus the latest notification MessageView.
  auto* focused_message_view = FocusNotificationView(id1);
  ASSERT_TRUE(focused_message_view);
  EXPECT_EQ(id1, focused_message_view->notification_id());

  // Remove the notification and observe that the focus is cleared.
  test_api()->RemoveNotification(id1);
  AnimateNotificationListToEnd();
  EXPECT_FALSE(notification_center_view()->GetFocusManager()->GetFocusedView());
}

TEST_P(NotificationCenterViewTest, CollapseAndExpand_NonAnimated) {
  test_api()->AddNotification();
  test_api()->AddNotification();
  test_api()->ToggleBubble();
  EXPECT_TRUE(GetScroller()->GetVisible());
  EXPECT_TRUE(GetNotificationBarClearAllButton()->GetVisible());
  EXPECT_FALSE(GetNotificationBarExpandAllButton()->GetVisible());

  // Set to collapsed state.
  notification_center_view()->SetCollapsed(false /* animate */);
  EXPECT_FALSE(GetScroller()->GetVisible());
  EXPECT_TRUE(GetNotificationBar()->GetVisible());
  EXPECT_TRUE(GetNotificationBarExpandAllButton()->GetVisible());
  EXPECT_FALSE(GetNotificationBarClearAllButton()->GetVisible());

  // Set back to expanded state.
  notification_center_view()->SetExpanded();
  EXPECT_FALSE(GetNotificationBarExpandAllButton()->GetVisible());
  EXPECT_TRUE(GetNotificationBarClearAllButton()->GetVisible());
  EXPECT_TRUE(GetScroller()->GetVisible());
}

TEST_P(NotificationCenterViewTest, CollapseAndExpand_Animated) {
  test_api()->AddNotification();
  test_api()->AddNotification();
  test_api()->ToggleBubble();
  EXPECT_TRUE(GetScroller()->GetVisible());

  // Set to collapsed state with animation.
  notification_center_view()->SetCollapsed(true /* animate */);
  auto* collapse_animation = GetMessageCenterAnimation();
  EXPECT_TRUE(collapse_animation->is_animating());

  // The scroller should be hidden at the half way point.
  collapse_animation->SetCurrentValue(0.5);
  notification_center_view()->AnimationProgressed(collapse_animation);
  EXPECT_FALSE(GetScroller()->GetVisible());
  EXPECT_TRUE(GetNotificationBar()->GetVisible());

  collapse_animation->End();
  AnimateNotificationListToEnd();
  EXPECT_TRUE(GetNotificationBarExpandAllButton()->GetVisible());
  EXPECT_FALSE(GetNotificationBarClearAllButton()->GetVisible());

  // Set back to expanded state.
  notification_center_view()->SetExpanded();
  EXPECT_FALSE(GetNotificationBarExpandAllButton()->GetVisible());
  EXPECT_TRUE(GetNotificationBarClearAllButton()->GetVisible());
  EXPECT_TRUE(GetScroller()->GetVisible());
}

TEST_P(NotificationCenterViewTest, CollapseAndExpand_NoNotifications) {
  // No collapsed/expanded state for notification center when QSRevamp is
  // enabled.
  if (IsQsRevampEnabled()) {
    return;
  }

  test_api()->ToggleBubble();
  EXPECT_FALSE(notification_center_view()->GetVisible());

  // Setting to the collapsed state should do nothing.
  notification_center_view()->SetCollapsed(true /* animate */);
  EXPECT_FALSE(notification_center_view()->GetVisible());

  // Same with setting it back to the expanded state.
  notification_center_view()->SetExpanded();
  EXPECT_FALSE(notification_center_view()->GetVisible());
}

TEST_P(NotificationCenterViewTest, ClearAllButtonHeight) {
  std::string id0 = test_api()->AddNotification();
  std::string id1 = test_api()->AddNotification();
  test_api()->ToggleBubble();
  EXPECT_TRUE(notification_center_view()->GetVisible());
  EXPECT_TRUE(GetNotificationBar()->GetVisible());
  EXPECT_TRUE(GetNotificationBarClearAllButton()->GetVisible());

  // Get ClearAll Button height.
  const int previous_button_height =
      GetNotificationBarClearAllButton()->height();

  // Remove a notification.
  test_api()->RemoveNotification(id0);

  // ClearAll Button height should remain the same.
  EXPECT_EQ(previous_button_height,
            GetNotificationBarClearAllButton()->height());
}

// Tests that the "Clear all" button is not focusable when it is disabled.
TEST_P(NotificationCenterViewTest, ClearAllNotFocusableWhenDisabled) {
  // Add a pinned notification and toggle the bubble.
  test_api()->AddPinnedNotification();
  test_api()->ToggleBubble();

  // Verify that the "Clear all" button is visible but disabled.
  ASSERT_TRUE(GetNotificationBarClearAllButton()->GetVisible());
  ASSERT_FALSE(GetNotificationBarClearAllButton()->GetEnabled());

  // Attempt to focus the "Clear all" button.
  FocusClearAllButton();

  // Verify that the "Clear all" button did not receive focus.
  EXPECT_FALSE(GetNotificationBarClearAllButton()->HasFocus());
}

TEST_P(NotificationCenterViewTest, StackedNotificationCount) {
  // There should not be any stacked notifications in the expanded message
  // center with just one notification added.
  test_api()->AddNotification();
  test_api()->ToggleBubble();
  notification_center_view()->SetExpanded();
  EXPECT_TRUE(notification_center_view()->GetVisible());
  EXPECT_EQ(1, total_notification_count());
  EXPECT_EQ(0, stacked_notification_count());

  // There should be at least one stacked notification in the expanded message
  // center with many notifications added.
  AddManyNotifications();
  RelayoutMessageCenterViewForTest();
  UpdateNotificationBarForTest();
  EXPECT_EQ(11, total_notification_count());
  EXPECT_LT(0, stacked_notification_count());
}

// Test for notification swipe control visibility.
TEST_P(NotificationCenterViewTest, NotificationPartialSwipe) {
  auto id1 = test_api()->AddNotification();
  test_api()->ToggleBubble();
  auto* view = test_api()->GetNotificationViewForId(id1);

  int x_start = view->GetBoundsInScreen().x();
  GetEventGenerator()->GestureScrollSequence(
      view->GetBoundsInScreen().CenterPoint(),
      view->GetBoundsInScreen().right_center(), base::Milliseconds(1000), 1000);

  // The notification view should go back to it's original location after a
  // partial swipe when there is no settings button.
  EXPECT_EQ(x_start, view->GetBoundsInScreen().x());

  message_center::RichNotificationData optional_fields;
  optional_fields.settings_button_handler =
      message_center::SettingsButtonHandler::INLINE;
  auto id2 = test_api()->AddCustomNotification(
      u"title", u"message", ui::ImageModel(), base::EmptyString16(), GURL(),
      message_center::NotifierId(), optional_fields);

  view = test_api()->GetNotificationViewForId(id2);

  x_start = view->GetBoundsInScreen().x();
  GetEventGenerator()->GestureScrollSequence(
      view->GetBoundsInScreen().CenterPoint(),
      view->GetBoundsInScreen().right_center(), base::Milliseconds(1000), 1000);

  // The notification view should be offset forwards from it's start position to
  // make space for the settings button at the end of a swipe.
  EXPECT_LT(x_start, view->GetBoundsInScreen().x());
}

}  // namespace ash
