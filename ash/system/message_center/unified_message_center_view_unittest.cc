// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/message_center/unified_message_center_view.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/message_center/ash_message_center_lock_screen_controller.h"
#include "ash/system/message_center/message_center_constants.h"
#include "ash/system/message_center/message_center_scroll_bar.h"
#include "ash/system/message_center/stacked_notification_bar.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "ash/system/unified/unified_system_tray_model.h"
#include "ash/test/ash_test_base.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "components/prefs/pref_service.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/views/message_view.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/widget/widget.h"

using message_center::MessageCenter;
using message_center::MessageView;
using message_center::Notification;

namespace ash {

namespace {

constexpr int kDefaultMaxHeight = 500;

class DummyEvent : public ui::Event {
 public:
  DummyEvent() : Event(ui::ET_UNKNOWN, base::TimeTicks(), 0) {}
  ~DummyEvent() override = default;
};

class TestUnifiedMessageCenterView : public UnifiedMessageCenterView {
 public:
  explicit TestUnifiedMessageCenterView(UnifiedSystemTrayModel* model)
      : UnifiedMessageCenterView(nullptr /*parent*/,
                                 model,
                                 nullptr /*bubble*/) {}

  TestUnifiedMessageCenterView(const TestUnifiedMessageCenterView&) = delete;
  TestUnifiedMessageCenterView& operator=(const TestUnifiedMessageCenterView&) =
      delete;

  ~TestUnifiedMessageCenterView() override = default;
};

}  // namespace

class UnifiedMessageCenterViewTest : public AshTestBase,
                                     public views::ViewObserver,
                                     public testing::WithParamInterface<bool> {
 public:
  UnifiedMessageCenterViewTest() = default;

  UnifiedMessageCenterViewTest(const UnifiedMessageCenterViewTest&) = delete;
  UnifiedMessageCenterViewTest& operator=(const UnifiedMessageCenterViewTest&) =
      delete;

  ~UnifiedMessageCenterViewTest() override = default;

  // AshTestBase:
  void SetUp() override {
    scoped_feature_list_ = std::make_unique<base::test::ScopedFeatureList>();
    scoped_feature_list_->InitWithFeatureState(features::kNotificationsRefresh,
                                               IsNotificationsRefreshEnabled());

    AshTestBase::SetUp();
    model_ = base::MakeRefCounted<UnifiedSystemTrayModel>(nullptr);
  }

  bool IsNotificationsRefreshEnabled() const { return GetParam(); }

  void TearDown() override {
    base::RunLoop().RunUntilIdle();
    message_center_view_.reset();
    model_.reset();
    AshTestBase::TearDown();
  }

  // views::ViewObserver:
  void OnViewPreferredSizeChanged(views::View* view) override {
    if (view->GetPreferredSize() == view->size())
      return;
    view->SetBoundsRect(view->GetVisible() ? gfx::Rect(view->GetPreferredSize())
                                           : gfx::Rect());
    view->Layout();
    ++size_changed_count_;
  }

 protected:
  std::string AddNotification(bool pinned) {
    std::string id = base::NumberToString(id_++);
    message_center::RichNotificationData data;
    data.pinned = pinned;
    MessageCenter::Get()->AddNotification(std::make_unique<Notification>(
        message_center::NOTIFICATION_TYPE_BASE_FORMAT, id, u"test title",
        u"test message", ui::ImageModel(),
        std::u16string() /* display_source */, GURL(),
        message_center::NotifierId(), data,
        new message_center::NotificationDelegate()));
    return id;
  }

  // Adds more than enough notifications to make the message center scrollable.
  std::vector<std::string> AddManyNotifications() {
    std::vector<std::string> ids;
    for (int i = 0; i < 10; ++i)
      ids.push_back(AddNotification(false));
    return ids;
  }

  std::unique_ptr<TestUnifiedMessageCenterView> CreateMessageCenterViewImpl(
      int max_height) {
    auto message_center_view =
        std::make_unique<TestUnifiedMessageCenterView>(model_.get());
    message_center_view->Init();
    message_center_view->AddObserver(this);
    message_center_view->SetMaxHeight(max_height);
    message_center_view->SetAvailableHeight(max_height);
    OnViewPreferredSizeChanged(message_center_view.get());
    size_changed_count_ = 0;

    return message_center_view;
  }

  virtual void CreateMessageCenterView(int max_height = kDefaultMaxHeight) {
    message_center_view_ = CreateMessageCenterViewImpl(max_height);
  }

  void AnimateMessageListToValue(float value) {
    GetMessageListView()->animation_->SetCurrentValue(value);
    GetMessageListView()->AnimationProgressed(
        GetMessageListView()->animation_.get());
  }

  void AnimateMessageListToMiddle() { AnimateMessageListToValue(0.5); }

  void AnimateMessageListToEnd() {
    FinishMessageListSlideOutAnimations();
    GetMessageListView()->animation_->End();
  }

  void AnimateMessageListUntilIdle() {
    while (GetMessageListView()->animation_->is_animating()) {
      GetMessageListView()->animation_->End();
    }
  }

  void FinishMessageListSlideOutAnimations() { base::RunLoop().RunUntilIdle(); }

  gfx::Rect GetMessageViewVisibleBounds(size_t index) {
    gfx::Rect bounds = GetMessageListView()->children()[index]->bounds();
    bounds -= GetScroller()->GetVisibleRect().OffsetFromOrigin();
    bounds += GetScroller()->bounds().OffsetFromOrigin();
    return bounds;
  }

  UnifiedMessageListView* GetMessageListView() {
    return message_center_view()->message_list_view_;
  }

  gfx::LinearAnimation* GetMessageCenterAnimation() {
    return message_center_view()->animation_.get();
  }

  views::ScrollView* GetScroller() { return message_center_view()->scroller_; }

  views::ScrollBar* GetScrollBar() {
    return message_center_view()->scroll_bar_;
  }

  views::View* GetScrollerContents() {
    return message_center_view()->scroller_->contents();
  }

  StackedNotificationBar* GetNotificationBar() {
    return message_center_view()->notification_bar_;
  }

  views::View* GetNotificationBarIconsContainer() {
    return message_center_view()
        ->notification_bar_->notification_icons_container_;
  }

  views::View* GetNotificationBarLabel() {
    return message_center_view()->notification_bar_->count_label_;
  }

  views::View* GetNotificationBarClearAllButton() {
    return message_center_view()->notification_bar_->clear_all_button_;
  }

  views::View* GetNotificationBarExpandAllButton() {
    return message_center_view()->notification_bar_->expand_all_button_;
  }

  message_center::MessageView* ToggleFocusToMessageView(size_t index,
                                                        bool reverse) {
    auto* focus_manager = message_center_view()->GetFocusManager();
    if (!focus_manager)
      return nullptr;

    message_center::MessageView* focused_message_view = nullptr;
    const size_t max_focus_toggles =
        GetMessageListView()->children().size() * 5;
    for (size_t i = 0; i < max_focus_toggles; ++i) {
      focus_manager->AdvanceFocus(reverse);
      auto* focused_view = focus_manager->GetFocusedView();
      // The MessageView is wrapped in container view in the MessageList.
      if (focused_view->parent() == GetMessageListView()->children()[index]) {
        focused_message_view =
            static_cast<message_center::MessageView*>(focused_view);
        break;
      }
    }
    return focused_message_view;
  }

  void RelayoutMessageCenterViewForTest() {
    // Outside of tests, any changes to bubble's size as well as scrolling
    // through notification list will trigger TrayBubbleView's BoxLayout to
    // relayout, and then this view will relayout. In test, we don't have
    // TrayBubbleView as the parent, so we need to explicitly call Layout()
    // in some circumstances.
    message_center_view_->Layout();
  }

  virtual TestUnifiedMessageCenterView* message_center_view() {
    return message_center_view_.get();
  }

  int size_changed_count() const { return size_changed_count_; }

  UnifiedSystemTrayModel* model() { return model_.get(); }

 private:
  int id_ = 0;
  int size_changed_count_ = 0;

  scoped_refptr<UnifiedSystemTrayModel> model_;
  std::unique_ptr<TestUnifiedMessageCenterView> message_center_view_;
  std::unique_ptr<base::test::ScopedFeatureList> scoped_feature_list_;
};

class UnifiedMessageCenterViewInWidgetTest
    : public UnifiedMessageCenterViewTest {
 public:
  UnifiedMessageCenterViewInWidgetTest() = default;
  UnifiedMessageCenterViewInWidgetTest(
      const UnifiedMessageCenterViewInWidgetTest&) = delete;
  UnifiedMessageCenterViewInWidgetTest& operator=(
      const UnifiedMessageCenterViewInWidgetTest&) = delete;
  ~UnifiedMessageCenterViewInWidgetTest() override = default;

  void TearDown() override {
    widget_.reset();

    UnifiedMessageCenterViewTest::TearDown();
  }

 protected:
  void CreateMessageCenterView(int max_height = kDefaultMaxHeight) override {
    widget_ = CreateTestWidget();
    message_center_ = widget_->GetRootView()->AddChildView(
        CreateMessageCenterViewImpl(max_height));
  }

  TestUnifiedMessageCenterView* message_center_view() override {
    return message_center_;
  }

  views::Widget* widget() { return widget_.get(); }

 private:
  std::unique_ptr<views::Widget> widget_;
  TestUnifiedMessageCenterView* message_center_ = nullptr;
};

INSTANTIATE_TEST_SUITE_P(All,
                         UnifiedMessageCenterViewTest,
                         testing::Bool() /* IsNotificationsRefreshEnabled() */);

// Flaky: https://crbug.com/1293165
TEST_P(UnifiedMessageCenterViewTest, DISABLED_AddAndRemoveNotification) {
  CreateMessageCenterView();
  EXPECT_FALSE(message_center_view()->GetVisible());

  auto id0 = AddNotification(false /* pinned */);
  EXPECT_TRUE(message_center_view()->GetVisible());

  // The notification first slides out of the list.
  MessageCenter::Get()->RemoveNotification(id0, true /* by_user */);
  AnimateMessageListToEnd();

  // After all the last notifiation slides out, the message center and list
  // should collapse.
  auto* collapse_animation = GetMessageCenterAnimation();
  collapse_animation->SetCurrentValue(0.5);
  message_center_view()->AnimationProgressed(collapse_animation);
  EXPECT_TRUE(message_center_view()->GetVisible());

  // The message center is now hidden after all animations complete.
  collapse_animation->End();
  AnimateMessageListToEnd();
  EXPECT_FALSE(message_center_view()->GetVisible());
}

TEST_P(UnifiedMessageCenterViewTest, RemoveNotificationAtTail) {
  // No special scroll behavior with the Notifications Refresh anymore.
  if (IsNotificationsRefreshEnabled())
    return;
  // Show message center with multiple notifications.
  AddManyNotifications();
  CreateMessageCenterView();
  EXPECT_TRUE(message_center_view()->GetVisible());

  // The message center should autoscroll to the bottom of the list after adding
  // a new notification.
  auto id_to_remove = AddNotification(false /* pinned */);
  RelayoutMessageCenterViewForTest();
  int scroll_position = GetScroller()->GetVisibleRect().y();
  EXPECT_EQ(GetMessageListView()->height() - GetScroller()->height(),
            scroll_position);

  // Get the height of last notification and then remove it.
  int removed_notification_height =
      GetMessageViewVisibleBounds(GetMessageListView()->children().size() - 1)
          .height();
  MessageCenter::Get()->RemoveNotification(id_to_remove, true /* by_user */);
  scroll_position = GetScroller()->GetVisibleRect().y();

  // The scroll position should be reduced by the height of the removed
  // notification after collapsing.
  AnimateMessageListToEnd();
  RelayoutMessageCenterViewForTest();

  EXPECT_EQ(scroll_position - removed_notification_height -
                kUnifiedNotificationSeparatorThickness,
            GetScroller()->GetVisibleRect().y());

  // Check that the list is still scrolled to the bottom.
  EXPECT_EQ(GetMessageListView()->height() - GetScroller()->height(),
            GetScroller()->GetVisibleRect().y());
}

TEST_P(UnifiedMessageCenterViewTest, ContentsRelayout) {
  std::vector<std::string> ids = AddManyNotifications();
  CreateMessageCenterView();
  EXPECT_TRUE(message_center_view()->GetVisible());
  // MessageCenterView is maxed out.
  EXPECT_GT(GetMessageListView()->bounds().height(),
            message_center_view()->bounds().height());
  const int previous_contents_height = GetScrollerContents()->height();
  const int previous_list_height = GetMessageListView()->height();

  MessageCenter::Get()->RemoveNotification(ids.back(), true /* by_user */);
  AnimateMessageListToEnd();
  RelayoutMessageCenterViewForTest();

  EXPECT_TRUE(message_center_view()->GetVisible());
  EXPECT_GT(previous_contents_height, GetScrollerContents()->height());
  EXPECT_GT(previous_list_height, GetMessageListView()->height());
}

TEST_P(UnifiedMessageCenterViewTest, InsufficientHeight) {
  CreateMessageCenterView();
  AddNotification(false /* pinned */);
  EXPECT_TRUE(message_center_view()->GetVisible());

  message_center_view()->SetAvailableHeight(kUnifiedNotificationMinimumHeight -
                                            1);
  EXPECT_FALSE(message_center_view()->GetVisible());

  message_center_view()->SetAvailableHeight(kUnifiedNotificationMinimumHeight);
  EXPECT_TRUE(message_center_view()->GetVisible());
}

TEST_P(UnifiedMessageCenterViewTest, NotVisibleWhenLocked) {
  // Disable the lock screen notification if the feature is enable.
  PrefService* user_prefs =
      Shell::Get()->session_controller()->GetLastActiveUserPrefService();
  user_prefs->SetString(prefs::kMessageCenterLockScreenMode,
                        prefs::kMessageCenterLockScreenModeHide);

  ASSERT_FALSE(AshMessageCenterLockScreenController::IsEnabled());

  AddNotification(false /* pinned */);
  AddNotification(false /* pinned */);

  BlockUserSession(BLOCKED_BY_LOCK_SCREEN);
  CreateMessageCenterView();

  EXPECT_FALSE(message_center_view()->GetVisible());
}

TEST_P(UnifiedMessageCenterViewTest, VisibleWhenLocked) {
  // This test is only valid if the lock screen feature is enabled.
  // TODO(yoshiki): Clean up after the feature is launched crbug.com/913764.
  if (!features::IsLockScreenNotificationsEnabled())
    return;

  // Enables the lock screen notification if the feature is disabled.
  PrefService* user_prefs =
      Shell::Get()->session_controller()->GetLastActiveUserPrefService();
  user_prefs->SetString(prefs::kMessageCenterLockScreenMode,
                        prefs::kMessageCenterLockScreenModeShow);

  ASSERT_TRUE(AshMessageCenterLockScreenController::IsEnabled());

  AddNotification(false /* pinned */);
  AddNotification(false /* pinned */);

  BlockUserSession(BLOCKED_BY_LOCK_SCREEN);
  CreateMessageCenterView();

  EXPECT_TRUE(message_center_view()->GetVisible());
}

TEST_P(UnifiedMessageCenterViewTest, ClearAllPressed) {
  AddNotification(false /* pinned */);
  AddNotification(false /* pinned */);
  CreateMessageCenterView();
  EXPECT_TRUE(message_center_view()->GetVisible());
  EXPECT_TRUE(GetNotificationBar()->GetVisible());

  // When Clear All button is pressed, all notifications are removed and the
  // view becomes invisible.
  message_center_view()->ClearAllNotifications();
  AnimateMessageListUntilIdle();
  EXPECT_FALSE(message_center_view()->GetVisible());
}

TEST_P(UnifiedMessageCenterViewTest, InitialPosition) {
  AddNotification(false /* pinned */);
  AddNotification(false /* pinned */);
  CreateMessageCenterView();
  EXPECT_TRUE(message_center_view()->GetVisible());

  // MessageCenterView is not maxed out.
  EXPECT_LT(GetMessageListView()->bounds().height(),
            message_center_view()->bounds().height());
}

TEST_P(UnifiedMessageCenterViewTest, InitialPositionMaxOut) {
  AddManyNotifications();
  CreateMessageCenterView();
  EXPECT_TRUE(message_center_view()->GetVisible());

  // MessageCenterView is maxed out.
  EXPECT_GT(GetMessageListView()->bounds().height(),
            message_center_view()->bounds().height());
}

TEST_P(UnifiedMessageCenterViewTest, InitialPositionWithLargeNotification) {
  AddNotification(false /* pinned */);
  AddNotification(false /* pinned */);
  CreateMessageCenterView(60 /* max_height */);
  EXPECT_TRUE(message_center_view()->GetVisible());

  // MessageCenterView is shorter than the notification.
  gfx::Rect message_view_bounds = GetMessageViewVisibleBounds(1);
  EXPECT_LT(message_center_view()->bounds().height(),
            message_view_bounds.height());

  // Top of the second notification aligns with the top of MessageCenterView.
  if (!IsNotificationsRefreshEnabled())
    EXPECT_EQ(kStackedNotificationBarHeight, message_view_bounds.y());
}

TEST_P(UnifiedMessageCenterViewTest, ScrollPositionWhenResized) {
  // We keep the scroll position at the top after the notifications refresh.
  if (IsNotificationsRefreshEnabled())
    return;

  AddManyNotifications();
  CreateMessageCenterView();
  EXPECT_TRUE(message_center_view()->GetVisible());

  // MessageCenterView is maxed out.
  EXPECT_GT(GetMessageListView()->bounds().height(),
            message_center_view()->bounds().height());
  gfx::Rect previous_visible_rect = GetScroller()->GetVisibleRect();

  gfx::Size new_size = message_center_view()->size();
  new_size.set_height(250);
  message_center_view()->SetPreferredSize(new_size);
  OnViewPreferredSizeChanged(message_center_view());

  EXPECT_EQ(previous_visible_rect.bottom(),
            GetScroller()->GetVisibleRect().bottom());

  GetScroller()->ScrollToPosition(GetScrollBar(), 200);
  message_center_view()->OnMessageCenterScrolled();
  previous_visible_rect = GetScroller()->GetVisibleRect();

  new_size.set_height(300);
  message_center_view()->SetPreferredSize(new_size);
  OnViewPreferredSizeChanged(message_center_view());

  EXPECT_EQ(previous_visible_rect.bottom(),
            GetScroller()->GetVisibleRect().bottom());
}

// Tests basic layout of the StackingNotificationBar.
TEST_P(UnifiedMessageCenterViewTest, StackingCounterLabelLayout) {
  AddManyNotifications();

  // MessageCenterView is maxed out.
  CreateMessageCenterView();

  EXPECT_GT(GetMessageListView()->bounds().height(),
            message_center_view()->bounds().height());

  EXPECT_TRUE(GetNotificationBar()->GetVisible());

  if (!features::IsNotificationsRefreshEnabled()) {
    EXPECT_EQ(0, GetNotificationBar()->bounds().y());
    EXPECT_EQ(GetNotificationBar()->bounds().bottom(),
              GetScroller()->bounds().y());
  } else {
    EXPECT_EQ(kMessageCenterPadding, GetScroller()->bounds().y());
    EXPECT_EQ(GetNotificationBar()->bounds().y(),
              GetScroller()->bounds().bottom());
  }

  EXPECT_TRUE(GetNotificationBarLabel()->GetVisible());

  EXPECT_TRUE(GetNotificationBarClearAllButton()->GetVisible());
}

// Tests that the NotificationBarLabel is invisible when scrolled to the top.
TEST_P(UnifiedMessageCenterViewTest, StackingCounterLabelInvisible) {
  AddManyNotifications();
  CreateMessageCenterView();

  // Scroll to the top, the counter label should be invisible. After
  // NotificationsRefresh, scrolling to the bottom should make the counter
  // invisible.
  GetScroller()->ScrollToPosition(GetScrollBar(),
                                  features::IsNotificationsRefreshEnabled()
                                      ? GetScrollBar()->bounds().bottom()
                                      : 0);
  message_center_view()->OnMessageCenterScrolled();

  EXPECT_FALSE(GetNotificationBarLabel()->GetVisible());
  // ClearAll label should always be visible.
  EXPECT_TRUE(GetNotificationBarClearAllButton()->GetVisible());
}

// Tests that the NotificationBarLabel is visible when scrolling down.
TEST_P(UnifiedMessageCenterViewTest, StackingCounterLabelVisible) {
  AddManyNotifications();
  CreateMessageCenterView();

  // Scrolling past 5 notifications should make the counter label visible.
  const int scroll_amount = (GetMessageViewVisibleBounds(0).height() * 5) + 1;
  GetScroller()->ScrollToPosition(
      GetScrollBar(),
      features::IsNotificationsRefreshEnabled() ? 0 : scroll_amount);
  message_center_view()->OnMessageCenterScrolled();

  EXPECT_TRUE(GetNotificationBarLabel()->GetVisible());
  // ClearAll label should always be visible.
  EXPECT_TRUE(GetNotificationBarClearAllButton()->GetVisible());
}

// Tests that the +n notifications label hides after being shown.
TEST_P(UnifiedMessageCenterViewTest, StackingCounterLabelHidesAfterShown) {
  AddManyNotifications();
  CreateMessageCenterView();

  // Scroll to the top, making the counter label invisible. In
  // NotificationsRefresh we must scroll to the bottom instead.
  auto bottom_position = GetScrollBar()->bounds().bottom();
  GetScroller()->ScrollToPosition(
      GetScrollBar(),
      features::IsNotificationsRefreshEnabled() ? bottom_position : 0);
  message_center_view()->OnMessageCenterScrolled();

  EXPECT_FALSE(GetNotificationBarLabel()->GetVisible());

  // Scrolling past 5 notifications should make the counter label visible.
  const int scroll_amount = (GetMessageViewVisibleBounds(0).height() * 5) + 1;
  GetScroller()->ScrollToPosition(GetScrollBar(),
                                  features::IsNotificationsRefreshEnabled()
                                      ? bottom_position - scroll_amount
                                      : scroll_amount);
  message_center_view()->OnMessageCenterScrolled();

  ASSERT_TRUE(GetNotificationBarLabel()->GetVisible());

  // Scrolling back to the top (bottom in NotificationsRefresh) should make the
  // counter label invisible again.
  GetScroller()->ScrollToPosition(GetScrollBar(),
                                  features::IsNotificationsRefreshEnabled()
                                      ? GetScrollBar()->bounds().bottom()
                                      : 0);
  message_center_view()->OnMessageCenterScrolled();

  EXPECT_FALSE(GetNotificationBarLabel()->GetVisible());
  // ClearAll label should always be visible.
  EXPECT_TRUE(GetNotificationBarClearAllButton()->GetVisible());
}

// Tests that there are never more than 3 stacked icons in the
// StackedNotificationBar. Also verifies that only one animation happens at a
// time (this prevents the user from over-scrolling and showing multiple
// animations when they scroll very quickly). Before, users could scroll fast
// and have a large amount of icons, instead of keeping it to 3.
TEST_P(UnifiedMessageCenterViewTest, StackingIconsNeverMoreThanThree) {
  for (int i = 0; i < 20; ++i)
    AddNotification(false);
  CreateMessageCenterView();

  auto bottom_position = GetScrollBar()->bounds().bottom();
  if (features::IsNotificationsRefreshEnabled()) {
    GetScroller()->ScrollToPosition(GetScrollBar(), bottom_position);
    message_center_view()->OnMessageCenterScrolled();
  }

  // Force animations to happen, so we can see if multiple animations trigger.
  ui::ScopedAnimationDurationScaleMode scoped_duration_modifier(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);
  // Scroll past 20 notifications, so we can scroll back up quickly.
  for (int i = 20; i >= 0; --i) {
    const int scroll_amount = (GetMessageViewVisibleBounds(0).height() * i) + 1;
    GetScroller()->ScrollToPosition(GetScrollBar(),
                                    features::IsNotificationsRefreshEnabled()
                                        ? bottom_position - scroll_amount
                                        : scroll_amount);
    message_center_view()->OnMessageCenterScrolled();

    auto icons_container_children =
        GetNotificationBarIconsContainer()->children();
    int animating_count = 0;
    for (auto* child : icons_container_children) {
      // Verify that no more than one icon is animating at any one time.
      if (child->layer()->GetAnimator()->is_animating())
        animating_count++;
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
TEST_P(UnifiedMessageCenterViewTest,
       DISABLED_StackingCounterNotificationRemoval) {
  std::vector<std::string> ids = AddManyNotifications();
  CreateMessageCenterView();
  EXPECT_TRUE(message_center_view()->GetVisible());

  // MessageCenterView is maxed out.
  EXPECT_GT(GetMessageListView()->bounds().height(),
            message_center_view()->bounds().height());

  // Dismiss until there are 2 notifications. The bar should still be visible.
  EXPECT_TRUE(GetNotificationBar()->GetVisible());
  for (size_t i = 0; (i + 2) < ids.size(); ++i) {
    MessageCenter::Get()->RemoveNotification(ids[i], true /* by_user */);
    AnimateMessageListToEnd();
  }
  EXPECT_TRUE(GetNotificationBar()->GetVisible());
  EXPECT_FALSE(GetNotificationBarLabel()->GetVisible());
  EXPECT_TRUE(GetNotificationBarClearAllButton()->GetVisible());

  // The MessageCenterView should be tall enough to contain the bar, two
  // notifications.
  EXPECT_EQ(kStackedNotificationBarHeight + GetMessageListView()->height(),
            message_center_view()->height());

  // Dismiss until there is only 1 notification left. The bar should be
  // hidden after an animation.
  MessageCenter::Get()->RemoveNotification(ids[ids.size() - 2],
                                           true /* by_user */);
  EXPECT_TRUE(GetNotificationBar()->GetVisible());

  // The HIDE_STACKING_BAR animation starts after the notification is slid out.
  AnimateMessageListToEnd();
  auto* hide_animation = GetMessageCenterAnimation();
  EXPECT_TRUE(hide_animation->is_animating());
  EXPECT_TRUE(GetNotificationBar()->GetVisible());

  // Animate to middle. The bar should still be visible.
  AnimateMessageListToMiddle();
  hide_animation->SetCurrentValue(0.5);
  message_center_view()->AnimationProgressed(hide_animation);
  EXPECT_TRUE(GetNotificationBar()->GetVisible());

  // Animate to end. The bar should now be hidden.
  AnimateMessageListToEnd();
  hide_animation->End();
  EXPECT_FALSE(GetNotificationBar()->GetVisible());
}

TEST_P(UnifiedMessageCenterViewTest, StackingCounterLabelRelaidOutOnScroll) {
  // Open the message center at the top of the notification list so the stacking
  // bar is hidden by default.
  std::string id = AddNotification(false /* pinned */);
  int total_notifications = 30;
  for (int i = 0; i < total_notifications; ++i)
    AddNotification(false /* pinned */);
  model()->SetTargetNotification(id);

  CreateMessageCenterView();

  auto bottom_position =
      GetMessageViewVisibleBounds(total_notifications - 1).bottom();

  if (features::IsNotificationsRefreshEnabled()) {
    GetScroller()->ScrollToPosition(GetScrollBar(), bottom_position);
    message_center_view()->OnMessageCenterScrolled();
  }

  EXPECT_FALSE(GetNotificationBarLabel()->GetVisible());

  // Scroll past 6 notifications so the count label becomes visible
  int scroll_amount = (GetMessageViewVisibleBounds(0).height() * 6) + 1;
  GetScroller()->ScrollToPosition(GetScrollBar(),
                                  features::IsNotificationsRefreshEnabled()
                                      ? bottom_position - scroll_amount
                                      : scroll_amount);
  message_center_view()->OnMessageCenterScrolled();
  RelayoutMessageCenterViewForTest();
  EXPECT_TRUE(GetNotificationBarLabel()->GetVisible());
  int label_width = GetNotificationBarLabel()->bounds().width();
  EXPECT_GT(label_width, 0);

  // Scroll past 14 notifications so the label width must be expanded to
  // contain longer 2-digit label.
  scroll_amount = (GetMessageViewVisibleBounds(0).height() * 14) + 1;
  GetScroller()->ScrollToPosition(GetScrollBar(),
                                  features::IsNotificationsRefreshEnabled()
                                      ? bottom_position - scroll_amount
                                      : scroll_amount);
  message_center_view()->OnMessageCenterScrolled();
  RelayoutMessageCenterViewForTest();
  EXPECT_GT(GetNotificationBarLabel()->bounds().width(), label_width);
}

TEST_P(UnifiedMessageCenterViewTest, StackingCounterVisibility) {
  std::string id0 = AddNotification(false /* pinned */);
  std::string id1 = AddNotification(false /* pinned */);
  CreateMessageCenterView();

  // The bar should be visible with 2 unpinned notifications.
  EXPECT_TRUE(GetNotificationBar()->GetVisible());
  EXPECT_TRUE(GetNotificationBarClearAllButton()->GetVisible());

  MessageCenter::Get()->RemoveNotification(id0, true /* by_user */);
  AnimateMessageListToEnd();
  auto* hide_animation = GetMessageCenterAnimation();
  hide_animation->End();

  // The bar should be hidden with 1 notification. Note that in the new
  // notification UI, the bar and clear all button are always shown.
  if (!IsNotificationsRefreshEnabled())
    EXPECT_FALSE(GetNotificationBar()->GetVisible());

  MessageCenter::Get()->RemoveNotification(id1, true /* by_user */);
  AddNotification(true /* pinned */);
  AddNotification(true /* pinned */);

  // The bar should not be visible with 2 pinned notifications (none of the
  // notifications are hidden).
  if (!IsNotificationsRefreshEnabled())
    EXPECT_FALSE(GetNotificationBar()->GetVisible());

  for (size_t i = 0; i < 8; ++i)
    AddNotification(true /* pinned */);

  // The bar should be visible with 10 pinned notifications (some of the
  // notifications are hidden). However, clear all button should not be shown.
  EXPECT_TRUE(GetNotificationBar()->GetVisible());
  if (!IsNotificationsRefreshEnabled())
    EXPECT_FALSE(GetNotificationBarClearAllButton()->GetVisible());

  // Add 1 unpinned notifications. Clear all should now be shown.
  AddNotification(false /* pinned */);
  RelayoutMessageCenterViewForTest();
  EXPECT_TRUE(GetNotificationBarClearAllButton()->GetVisible());
}

INSTANTIATE_TEST_SUITE_P(All,
                         UnifiedMessageCenterViewInWidgetTest,
                         testing::Bool() /* IsNotificationsRefreshEnabled()
                         */);

// We need a widget to initialize a FocusManager.
TEST_P(UnifiedMessageCenterViewInWidgetTest,
       FocusClearedAfterNotificationRemoval) {
  CreateMessageCenterView();

  widget()->Show();

  // Add notifications and focus on a child view in the last notification.
  AddNotification(false /* pinned */);
  auto id1 = AddNotification(false /* pinned */);

  // Toggle focus to the last notification MessageView.
  auto* focused_message_view = ToggleFocusToMessageView(
      features::IsNotificationsRefreshEnabled() ? 0 : 1 /* index */,
      true /* reverse */);
  ASSERT_TRUE(focused_message_view);
  EXPECT_EQ(id1, focused_message_view->notification_id());

  // Remove the notification and observe that the focus is cleared.
  MessageCenter::Get()->RemoveNotification(id1, true /* by_user */);
  AnimateMessageListToEnd();
  EXPECT_FALSE(message_center_view()->GetFocusManager()->GetFocusedView());
}

TEST_P(UnifiedMessageCenterViewTest, CollapseAndExpand_NonAnimated) {
  AddNotification(false /* pinned */);
  AddNotification(false /* pinned */);
  CreateMessageCenterView();
  EXPECT_TRUE(GetScroller()->GetVisible());
  EXPECT_TRUE(GetNotificationBarClearAllButton()->GetVisible());
  EXPECT_FALSE(GetNotificationBarExpandAllButton()->GetVisible());

  // Set to collapsed state.
  message_center_view()->SetCollapsed(false /* animate */);
  EXPECT_FALSE(GetScroller()->GetVisible());
  EXPECT_TRUE(GetNotificationBar()->GetVisible());
  EXPECT_TRUE(GetNotificationBarExpandAllButton()->GetVisible());
  EXPECT_FALSE(GetNotificationBarClearAllButton()->GetVisible());

  // Set back to expanded state.
  message_center_view()->SetExpanded();
  EXPECT_FALSE(GetNotificationBarExpandAllButton()->GetVisible());
  EXPECT_TRUE(GetNotificationBarClearAllButton()->GetVisible());
  EXPECT_TRUE(GetScroller()->GetVisible());
}

TEST_P(UnifiedMessageCenterViewTest, CollapseAndExpand_Animated) {
  AddNotification(false /* pinned */);
  AddNotification(false /* pinned */);
  CreateMessageCenterView();
  EXPECT_TRUE(GetScroller()->GetVisible());

  // Set to collapsed state with animation.
  message_center_view()->SetCollapsed(true /* animate */);
  auto* collapse_animation = GetMessageCenterAnimation();
  EXPECT_TRUE(collapse_animation->is_animating());

  // The scroller should be hidden at the half way point.
  collapse_animation->SetCurrentValue(0.5);
  message_center_view()->AnimationProgressed(collapse_animation);
  EXPECT_FALSE(GetScroller()->GetVisible());
  EXPECT_TRUE(GetNotificationBar()->GetVisible());

  collapse_animation->End();
  AnimateMessageListToEnd();
  EXPECT_TRUE(GetNotificationBarExpandAllButton()->GetVisible());
  EXPECT_FALSE(GetNotificationBarClearAllButton()->GetVisible());

  // Set back to expanded state.
  message_center_view()->SetExpanded();
  EXPECT_FALSE(GetNotificationBarExpandAllButton()->GetVisible());
  EXPECT_TRUE(GetNotificationBarClearAllButton()->GetVisible());
  EXPECT_TRUE(GetScroller()->GetVisible());
}

TEST_P(UnifiedMessageCenterViewTest, CollapseAndExpand_NoNotifications) {
  CreateMessageCenterView();
  EXPECT_FALSE(message_center_view()->GetVisible());

  // Setting to the collapsed state should do nothing.
  message_center_view()->SetCollapsed(true /* animate */);
  EXPECT_FALSE(message_center_view()->GetVisible());

  // Same with setting it back to the expanded state.
  message_center_view()->SetExpanded();
  EXPECT_FALSE(message_center_view()->GetVisible());
}

}  // namespace ash
