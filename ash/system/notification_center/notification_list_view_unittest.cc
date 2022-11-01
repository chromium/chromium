// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/notification_center/notification_list_view.h"

#include "ash/bubble/bubble_constants.h"
#include "ash/constants/ash_features.h"
#include "ash/system/message_center/message_center_constants.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/unified/unified_system_tray_model.h"
#include "ash/test/ash_test_base.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/views/message_view.h"
#include "ui/message_center/views/notification_view.h"
#include "ui/views/test/views_test_utils.h"

using message_center::MessageCenter;
using message_center::MessageView;
using message_center::Notification;

namespace ash {

namespace {

class TestNotificationView : public message_center::NotificationView {
 public:
  TestNotificationView(const message_center::Notification& notification)
      : NotificationView(notification) {
    layer()->GetAnimator()->set_preemption_strategy(
        ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
  }

  TestNotificationView(const TestNotificationView&) = delete;
  TestNotificationView& operator=(const TestNotificationView&) = delete;

  ~TestNotificationView() override = default;

  // message_center::NotificationView:
  void UpdateCornerRadius(int top_radius, int bottom_radius) override {
    top_radius_ = top_radius;
    bottom_radius_ = bottom_radius;
    message_center::NotificationViewBase::UpdateCornerRadius(top_radius,
                                                             bottom_radius);
  }

  float GetSlideAmount() const override {
    return slide_amount_.value_or(
        message_center::NotificationViewBase::GetSlideAmount());
  }

  int top_radius() const { return top_radius_; }
  int bottom_radius() const { return bottom_radius_; }

  void set_slide_amount(float slide_amount) { slide_amount_ = slide_amount; }

 private:
  int top_radius_ = 0;
  int bottom_radius_ = 0;

  absl::optional<float> slide_amount_;
};

class TestNotificationListView : public NotificationListView {
 public:
  explicit TestNotificationListView(UnifiedSystemTrayModel* model)
      : NotificationListView(nullptr, model) {}

  TestNotificationListView(const TestNotificationListView&) = delete;
  TestNotificationListView& operator=(const TestNotificationListView&) = delete;

  ~TestNotificationListView() override = default;

  void set_stacked_notification_count(int stacked_notification_count) {
    stacked_notifications_.clear();
    notification_id_list_.clear();
    for (int i = 0; i < stacked_notification_count; i++) {
      std::string id = base::NumberToString(0);
      auto notification = std::make_unique<Notification>(
          message_center::NOTIFICATION_TYPE_SIMPLE, id, u"test title",
          u"test message", ui::ImageModel(),
          std::u16string() /* display_source */, GURL(),
          message_center::NotifierId(), message_center::RichNotificationData(),
          new message_center::NotificationDelegate());

      stacked_notifications_.push_back(notification.get());
      notification_id_list_.push_back(id);
    }
  }

  // NotificationListView:
  message_center::MessageView* CreateMessageView(
      const message_center::Notification& notification) override {
    auto* message_view = new TestNotificationView(notification);
    ConfigureMessageView(message_view);
    return message_view;
  }

  std::vector<message_center::Notification*> GetStackedNotifications()
      const override {
    return stacked_notifications_;
  }

  std::vector<std::string> GetNonVisibleNotificationIdsInViewHierarchy()
      const override {
    return notification_id_list_;
  }

 private:
  std::vector<message_center::Notification*> stacked_notifications_;
  std::vector<std::string> notification_id_list_;
};

}  // namespace

// The base test class, has no params so tests with no params can inherit from
// this.
class NotificationListViewTest : public AshTestBase,
                                 public views::ViewObserver {
 public:
  NotificationListViewTest() = default;

  NotificationListViewTest(const NotificationListViewTest&) = delete;
  NotificationListViewTest& operator=(const NotificationListViewTest&) = delete;

  ~NotificationListViewTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();
    model_ = base::MakeRefCounted<UnifiedSystemTrayModel>(nullptr);
  }

  void TearDown() override {
    notification_list_view_.reset();
    model_.reset();
    AshTestBase::TearDown();
  }

  // views::ViewObserver:
  void OnViewPreferredSizeChanged(views::View* view) override {
    view->SetBoundsRect(gfx::Rect(view->GetPreferredSize()));
    views::test::RunScheduledLayout(view);
    ++size_changed_count_;
  }

 protected:
  std::string AddNotification(bool pinned = false, bool expandable = false) {
    std::string id = base::NumberToString(id_++);
    // Make the message long enough to be collapsible. Generated by SpaceIpsum.
    auto notification = std::make_unique<Notification>(
        message_center::NOTIFICATION_TYPE_SIMPLE, id,
        u"Message To Flight Control",
        expandable ? u"From this day forward, Flight Control will be known by "
                     u"two words: "
                     u"‘Tough’ and ‘Competent.’ Tough means we are forever "
                     u"accountable for "
                     u"what we do or what we fail to do. We will never again "
                     u"compromise our "
                     u"responsibilities. Every time we walk into Mission "
                     u"Control we will "
                     u"know what we stand for. Competent means we will never "
                     u"take anything "
                     u"for granted. We will never be found short in our "
                     u"knowledge and in "
                     u"our skills. Mission Control will be perfect."
                   : u"Hey Flight Control, who brought donuts?",
        ui::ImageModel(), std::u16string() /* display_source */, GURL(),
        message_center::NotifierId(), message_center::RichNotificationData(),
        new message_center::NotificationDelegate());
    notification->set_pinned(pinned);
    MessageCenter::Get()->AddNotification(std::move(notification));
    return id;
  }

  void OffsetNotificationTimestamp(const std::string& id,
                                   const int milliseconds) {
    MessageCenter::Get()->FindVisibleNotificationById(id)->set_timestamp(
        base::Time::Now() - base::Milliseconds(milliseconds));
  }

  void CreateMessageListView() {
    notification_list_view_ =
        std::make_unique<TestNotificationListView>(model_.get());
    notification_list_view_->Init();
    notification_list_view_->AddObserver(this);
    OnViewPreferredSizeChanged(notification_list_view_.get());
    size_changed_count_ = 0;
  }

  void DestroyMessageListView() { notification_list_view_.reset(); }

  TestNotificationView* GetMessageViewAt(size_t index) const {
    return static_cast<TestNotificationView*>(
        message_list_view()->children()[index]->children()[1]);
  }

  gfx::Rect GetMessageViewBounds(size_t index) const {
    return message_list_view()->children()[index]->bounds();
  }

  void FinishSlideOutAnimation() { base::RunLoop().RunUntilIdle(); }

  void AnimateToMiddle() {
    EXPECT_TRUE(IsAnimating());
    message_list_view()->animation_->SetCurrentValue(0.5);
    message_list_view()->AnimationProgressed(
        message_list_view()->animation_.get());
  }

  void AnimateToEnd() { message_list_view()->animation_->End(); }

  void AnimateUntilIdle() {
    while (message_list_view()->animation_->is_animating())
      message_list_view()->animation_->End();
  }

  bool IsAnimating() { return message_list_view()->animation_->is_animating(); }

  TestNotificationListView* message_list_view() const {
    return notification_list_view_.get();
  }

  int size_changed_count() const { return size_changed_count_; }

  ui::LayerAnimator* LayerAnimatorAt(int i) {
    return GetMessageViewAt(i)->layer()->GetAnimator();
  }

 private:
  int id_ = 0;
  int size_changed_count_ = 0;

  scoped_refptr<UnifiedSystemTrayModel> model_;
  std::unique_ptr<TestNotificationListView> notification_list_view_;
};

// Tests with NotificationsRefresh enabled and disabled.
class ParameterizedNotificationListViewTest
    : public NotificationListViewTest,
      public testing::WithParamInterface<bool> {
 public:
  ParameterizedNotificationListViewTest() = default;

  ParameterizedNotificationListViewTest(
      const ParameterizedNotificationListViewTest&) = delete;
  ParameterizedNotificationListViewTest& operator=(
      const ParameterizedNotificationListViewTest&) = delete;

  ~ParameterizedNotificationListViewTest() override = default;

  // AshTestBase:
  void SetUp() override {
    scoped_feature_list_ = std::make_unique<base::test::ScopedFeatureList>();
    if (IsNotificationsRefreshEnabled()) {
      scoped_feature_list_->InitWithFeatures(
          /*enabled_features=*/{features::kNotificationsRefresh,
                                chromeos::features::kDarkLightMode},
          /*disabled_features=*/{});
    } else {
      scoped_feature_list_->InitWithFeatures(
          /*enabled_features=*/{},
          /*disabled_features=*/{features::kNotificationsRefresh,
                                 chromeos::features::kDarkLightMode});
    }

    NotificationListViewTest::SetUp();
  }

  int GetMessageCenterNotificationCornerRadius() {
    return IsNotificationsRefreshEnabled()
               ? kMessageCenterNotificationInnerCornerRadius
               : 0;
  }

  bool IsNotificationsRefreshEnabled() const { return GetParam(); }

 private:
  std::unique_ptr<base::test::ScopedFeatureList> scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         ParameterizedNotificationListViewTest,
                         testing::Bool() /* IsNotificationsRefreshEnabled() */);

TEST_P(ParameterizedNotificationListViewTest, Open) {
  auto id0 = AddNotification();
  auto id1 = AddNotification();
  auto id2 = AddNotification();
  CreateMessageListView();

  EXPECT_EQ(3u, message_list_view()->children().size());

  if (!features::IsNotificationsRefreshEnabled()) {
    EXPECT_EQ(id0, GetMessageViewAt(0)->notification_id());
    EXPECT_EQ(id1, GetMessageViewAt(1)->notification_id());
    EXPECT_EQ(id2, GetMessageViewAt(2)->notification_id());

    EXPECT_FALSE(GetMessageViewAt(0)->IsExpanded());
    EXPECT_FALSE(GetMessageViewAt(1)->IsExpanded());
    EXPECT_TRUE(GetMessageViewAt(2)->IsExpanded());
  } else {
    EXPECT_EQ(id0, GetMessageViewAt(2)->notification_id());
    EXPECT_EQ(id1, GetMessageViewAt(1)->notification_id());
    EXPECT_EQ(id2, GetMessageViewAt(0)->notification_id());

    EXPECT_FALSE(GetMessageViewAt(2)->IsExpanded());
    EXPECT_FALSE(GetMessageViewAt(1)->IsExpanded());
    EXPECT_TRUE(GetMessageViewAt(0)->IsExpanded());
  }
  // Check the position of notifications within the list. When the new feature
  // is enabled, we have extra spacing between notifications.
  if (IsNotificationsRefreshEnabled()) {
    EXPECT_EQ(
        GetMessageViewBounds(0).bottom() + kMessageListNotificationSpacing,
        GetMessageViewBounds(1).y());
    EXPECT_EQ(
        GetMessageViewBounds(1).bottom() + kMessageListNotificationSpacing,
        GetMessageViewBounds(2).y());
  } else {
    EXPECT_EQ(GetMessageViewBounds(0).bottom(), GetMessageViewBounds(1).y());
    EXPECT_EQ(GetMessageViewBounds(1).bottom(), GetMessageViewBounds(2).y());
  }

  int top_most_corner_radius =
      IsNotificationsRefreshEnabled()
          ? kMessageCenterNotificationTopBottomCornerRadius
          : GetMessageCenterNotificationCornerRadius();
  EXPECT_EQ(top_most_corner_radius, GetMessageViewAt(0)->top_radius());
  EXPECT_EQ(GetMessageCenterNotificationCornerRadius(),
            GetMessageViewAt(1)->top_radius());
  EXPECT_EQ(GetMessageCenterNotificationCornerRadius(),
            GetMessageViewAt(2)->top_radius());

  EXPECT_EQ(GetMessageCenterNotificationCornerRadius(),
            GetMessageViewAt(0)->bottom_radius());
  EXPECT_EQ(GetMessageCenterNotificationCornerRadius(),
            GetMessageViewAt(1)->bottom_radius());

  EXPECT_EQ(kMessageCenterNotificationTopBottomCornerRadius,
            GetMessageViewAt(2)->bottom_radius());

  EXPECT_LT(0, message_list_view()->GetPreferredSize().height());
}

TEST_P(ParameterizedNotificationListViewTest, AddNotifications) {
  CreateMessageListView();
  EXPECT_EQ(0, message_list_view()->GetPreferredSize().height());

  auto id0 = AddNotification();
  EXPECT_EQ(1, size_changed_count());
  EXPECT_EQ(1u, message_list_view()->children().size());
  EXPECT_EQ(id0, GetMessageViewAt(0)->notification_id());

  EXPECT_EQ(kMessageCenterNotificationTopBottomCornerRadius,
            GetMessageViewAt(0)->top_radius());
  EXPECT_EQ(kMessageCenterNotificationTopBottomCornerRadius,
            GetMessageViewAt(0)->bottom_radius());

  int previous_notification_list_view_height =
      message_list_view()->GetPreferredSize().height();
  EXPECT_LT(0, previous_notification_list_view_height);

  gfx::Rect previous_bounds = GetMessageViewBounds(0);
  auto id1 = AddNotification();
  EXPECT_EQ(2, size_changed_count());
  EXPECT_EQ(2u, message_list_view()->children().size());
  EXPECT_EQ(id1,
            GetMessageViewAt(features::IsNotificationsRefreshEnabled() ? 0 : 1)
                ->notification_id());

  EXPECT_LT(previous_notification_list_view_height,
            message_list_view()->GetPreferredSize().height());

  if (!IsNotificationsRefreshEnabled()) {
    // 1dip larger because now it has separator border.
    previous_bounds.Inset(gfx::Insets::TLBR(0, 0, -1, 0));
  }
  EXPECT_EQ(previous_bounds, GetMessageViewBounds(0));

  // When the new feature is enabled, we have extra spacing between
  // notifications.
  if (IsNotificationsRefreshEnabled()) {
    EXPECT_EQ(
        GetMessageViewBounds(0).bottom() + kMessageListNotificationSpacing,
        GetMessageViewBounds(1).y());
  } else {
    EXPECT_EQ(GetMessageViewBounds(0).bottom(), GetMessageViewBounds(1).y());
  }

  int top_most_corner_radius =
      IsNotificationsRefreshEnabled()
          ? kMessageCenterNotificationTopBottomCornerRadius
          : GetMessageCenterNotificationCornerRadius();
  EXPECT_EQ(top_most_corner_radius, GetMessageViewAt(0)->top_radius());
  EXPECT_EQ(GetMessageCenterNotificationCornerRadius(),
            GetMessageViewAt(1)->top_radius());

  EXPECT_EQ(kMessageCenterNotificationTopBottomCornerRadius,
            GetMessageViewAt(1)->bottom_radius());
}

TEST_P(ParameterizedNotificationListViewTest, RemoveNotification) {
  auto id0 = AddNotification();
  auto id1 = AddNotification();

  CreateMessageListView();
  int previous_height = message_list_view()->GetPreferredSize().height();

  EXPECT_EQ(2u, message_list_view()->children().size());

  int top_most_corner_radius =
      IsNotificationsRefreshEnabled()
          ? kMessageCenterNotificationTopBottomCornerRadius
          : GetMessageCenterNotificationCornerRadius();
  EXPECT_EQ(top_most_corner_radius, GetMessageViewAt(0)->top_radius());
  EXPECT_EQ(GetMessageCenterNotificationCornerRadius(),
            GetMessageViewAt(0)->bottom_radius());

  gfx::Rect previous_bounds = GetMessageViewBounds(0);
  MessageCenter::Get()->RemoveNotification(id0, true /* by_user */);
  FinishSlideOutAnimation();
  AnimateUntilIdle();
  EXPECT_EQ(1u, message_list_view()->children().size());
  EXPECT_EQ(previous_bounds.y(), GetMessageViewBounds(0).y());
  EXPECT_LT(0, message_list_view()->GetPreferredSize().height());
  EXPECT_GT(previous_height, message_list_view()->GetPreferredSize().height());

  EXPECT_EQ(kMessageCenterNotificationTopBottomCornerRadius,
            GetMessageViewAt(0)->top_radius());
  EXPECT_EQ(kMessageCenterNotificationTopBottomCornerRadius,
            GetMessageViewAt(0)->bottom_radius());

  MessageCenter::Get()->RemoveNotification(id1, true /* by_user */);
  FinishSlideOutAnimation();
  AnimateUntilIdle();
  EXPECT_EQ(0u, message_list_view()->children().size());
  EXPECT_EQ(0, message_list_view()->GetPreferredSize().height());
}

TEST_P(ParameterizedNotificationListViewTest, CollapseOlderNotifications) {
  AddNotification();
  CreateMessageListView();
  EXPECT_TRUE(GetMessageViewAt(0)->IsExpanded());

  AddNotification();

  if (!features::IsNotificationsRefreshEnabled()) {
    EXPECT_FALSE(GetMessageViewAt(0)->IsExpanded());
    EXPECT_TRUE(GetMessageViewAt(1)->IsExpanded());
  } else {
    EXPECT_FALSE(GetMessageViewAt(1)->IsExpanded());
    EXPECT_TRUE(GetMessageViewAt(0)->IsExpanded());
  }

  AddNotification();

  if (!features::IsNotificationsRefreshEnabled()) {
    EXPECT_FALSE(GetMessageViewAt(0)->IsExpanded());
    EXPECT_FALSE(GetMessageViewAt(1)->IsExpanded());
    EXPECT_TRUE(GetMessageViewAt(2)->IsExpanded());
  } else {
    EXPECT_FALSE(GetMessageViewAt(2)->IsExpanded());
    EXPECT_FALSE(GetMessageViewAt(1)->IsExpanded());
    EXPECT_TRUE(GetMessageViewAt(0)->IsExpanded());
  }

  GetMessageViewAt(1)->SetExpanded(true);
  GetMessageViewAt(1)->SetManuallyExpandedOrCollapsed(true);

  AddNotification();

  if (!features::IsNotificationsRefreshEnabled()) {
    EXPECT_FALSE(GetMessageViewAt(0)->IsExpanded());
    EXPECT_TRUE(GetMessageViewAt(1)->IsExpanded());
    EXPECT_FALSE(GetMessageViewAt(2)->IsExpanded());
    EXPECT_TRUE(GetMessageViewAt(3)->IsExpanded());
  } else {
    EXPECT_TRUE(GetMessageViewAt(0)->IsExpanded());
    EXPECT_FALSE(GetMessageViewAt(1)->IsExpanded());
    EXPECT_TRUE(GetMessageViewAt(2)->IsExpanded());
    EXPECT_FALSE(GetMessageViewAt(3)->IsExpanded());
  }
}

TEST_P(ParameterizedNotificationListViewTest, RemovingNotificationAnimation) {
  auto id0 = AddNotification(/*pinned=*/false);
  auto id1 = AddNotification();
  auto id2 = AddNotification();
  CreateMessageListView();
  int previous_height = message_list_view()->GetPreferredSize().height();
  gfx::Rect bounds0 = GetMessageViewBounds(0);
  gfx::Rect bounds1 = GetMessageViewBounds(1);

  MessageCenter::Get()->RemoveNotification(id1, true /* by_user */);
  FinishSlideOutAnimation();
  AnimateToEnd();
  EXPECT_GT(previous_height, message_list_view()->GetPreferredSize().height());
  previous_height = message_list_view()->GetPreferredSize().height();
  if (!IsNotificationsRefreshEnabled()) {
    // Now it lost separator border.
    bounds1.Inset(gfx::Insets::TLBR(0, 0, 1, 0));
  }
  EXPECT_EQ(bounds0, GetMessageViewBounds(0));
  EXPECT_EQ(bounds1, GetMessageViewBounds(1));

  MessageCenter::Get()->RemoveNotification(id2, true /* by_user */);
  FinishSlideOutAnimation();
  AnimateToEnd();
  EXPECT_GT(previous_height, message_list_view()->GetPreferredSize().height());
  previous_height = message_list_view()->GetPreferredSize().height();
  if (!IsNotificationsRefreshEnabled()) {
    // Now it lost separator border.
    bounds0.Inset(gfx::Insets::TLBR(0, 0, 1, 0));
  }
  EXPECT_EQ(bounds0, GetMessageViewBounds(0));

  MessageCenter::Get()->RemoveNotification(id0, true /* by_user */);
  FinishSlideOutAnimation();
  AnimateToEnd();
  EXPECT_GT(previous_height, message_list_view()->GetPreferredSize().height());
  previous_height = message_list_view()->GetPreferredSize().height();

  EXPECT_EQ(0, message_list_view()->GetPreferredSize().height());
}

// Flaky: https://crbug.com/1292774.
TEST_P(ParameterizedNotificationListViewTest, DISABLED_ResetAnimation) {
  auto id0 = AddNotification();
  auto id1 = AddNotification();
  CreateMessageListView();

  MessageCenter::Get()->RemoveNotification(id0, true /* by_user */);
  FinishSlideOutAnimation();
  EXPECT_TRUE(IsAnimating());
  AnimateToMiddle();

  // New event resets the animation.
  auto id2 = AddNotification();
  EXPECT_FALSE(IsAnimating());

  EXPECT_EQ(2u, message_list_view()->children().size());
  EXPECT_EQ(id1, GetMessageViewAt(0)->notification_id());
  EXPECT_EQ(id2, GetMessageViewAt(1)->notification_id());
}

TEST_P(ParameterizedNotificationListViewTest, KeepManuallyExpanded) {
  AddNotification();
  AddNotification();
  CreateMessageListView();

  if (!features::IsNotificationsRefreshEnabled()) {
    EXPECT_FALSE(GetMessageViewAt(0)->IsExpanded());
    EXPECT_TRUE(GetMessageViewAt(1)->IsExpanded());
  } else {
    EXPECT_FALSE(GetMessageViewAt(1)->IsExpanded());
    EXPECT_TRUE(GetMessageViewAt(0)->IsExpanded());
  }
  EXPECT_FALSE(GetMessageViewAt(0)->IsManuallyExpandedOrCollapsed());
  EXPECT_FALSE(GetMessageViewAt(1)->IsManuallyExpandedOrCollapsed());

  // Manually expand the first notification & manually collapse the second one.
  GetMessageViewAt(0)->SetExpanded(true);
  GetMessageViewAt(0)->SetManuallyExpandedOrCollapsed(true);
  GetMessageViewAt(1)->SetExpanded(false);
  GetMessageViewAt(1)->SetManuallyExpandedOrCollapsed(true);

  DestroyMessageListView();

  // Reopen and confirm the expanded state & manually expanded flags are kept.
  CreateMessageListView();
  EXPECT_TRUE(GetMessageViewAt(0)->IsExpanded());
  EXPECT_FALSE(GetMessageViewAt(1)->IsExpanded());
  EXPECT_TRUE(GetMessageViewAt(0)->IsManuallyExpandedOrCollapsed());
  EXPECT_TRUE(GetMessageViewAt(1)->IsManuallyExpandedOrCollapsed());

  DestroyMessageListView();

  // Add a new notification.
  auto id = AddNotification();
  CreateMessageListView();

  // Confirm the new notification isn't affected & others are still kept.
  if (!features::IsNotificationsRefreshEnabled()) {
    EXPECT_TRUE(GetMessageViewAt(0)->IsExpanded());
    EXPECT_FALSE(GetMessageViewAt(1)->IsExpanded());
    EXPECT_TRUE(GetMessageViewAt(2)->IsExpanded());
    EXPECT_TRUE(GetMessageViewAt(0)->IsManuallyExpandedOrCollapsed());
    EXPECT_TRUE(GetMessageViewAt(1)->IsManuallyExpandedOrCollapsed());
    EXPECT_FALSE(GetMessageViewAt(2)->IsManuallyExpandedOrCollapsed());
  } else {
    EXPECT_FALSE(GetMessageViewAt(2)->IsExpanded());
    EXPECT_TRUE(GetMessageViewAt(1)->IsExpanded());
    EXPECT_TRUE(GetMessageViewAt(0)->IsExpanded());
    EXPECT_TRUE(GetMessageViewAt(2)->IsManuallyExpandedOrCollapsed());
    EXPECT_TRUE(GetMessageViewAt(1)->IsManuallyExpandedOrCollapsed());
    EXPECT_FALSE(GetMessageViewAt(0)->IsManuallyExpandedOrCollapsed());
  }
}

TEST_P(ParameterizedNotificationListViewTest,
       ClearAllWithOnlyVisibleNotifications) {
  AddNotification();
  AddNotification();
  CreateMessageListView();

  EXPECT_EQ(2u, message_list_view()->children().size());
  int previous_height = message_list_view()->GetPreferredSize().height();
  int removed_view_index = features::IsNotificationsRefreshEnabled() ? 1 : 0;
  gfx::Rect previous_bounds = GetMessageViewBounds(removed_view_index);

  message_list_view()->ClearAllWithAnimation();
  AnimateToMiddle();
  EXPECT_LT(previous_bounds.x(), GetMessageViewBounds(removed_view_index).x());
  EXPECT_EQ(previous_height, message_list_view()->GetPreferredSize().height());

  AnimateToEnd();
  EXPECT_EQ(1u, message_list_view()->children().size());
  EXPECT_EQ(previous_height, message_list_view()->GetPreferredSize().height());

  previous_bounds = GetMessageViewBounds(0);
  AnimateToMiddle();
  EXPECT_LT(previous_bounds.x(), GetMessageViewBounds(0).x());
  EXPECT_GT(previous_height, message_list_view()->GetPreferredSize().height());
  previous_height = message_list_view()->GetPreferredSize().height();

  AnimateToEnd();
  EXPECT_GT(previous_height, message_list_view()->GetPreferredSize().height());
  previous_height = message_list_view()->GetPreferredSize().height();
  EXPECT_EQ(0u, message_list_view()->children().size());

  AnimateToMiddle();
  EXPECT_GT(previous_height, message_list_view()->GetPreferredSize().height());
  previous_height = message_list_view()->GetPreferredSize().height();

  AnimateToEnd();
  EXPECT_GT(previous_height, message_list_view()->GetPreferredSize().height());
  EXPECT_EQ(0, message_list_view()->GetPreferredSize().height());
  EXPECT_TRUE(MessageCenter::Get()->GetVisibleNotifications().empty());

  EXPECT_FALSE(IsAnimating());
}

TEST_P(ParameterizedNotificationListViewTest,
       ClearAllWithStackingNotifications) {
  AddNotification();
  AddNotification();
  AddNotification();
  CreateMessageListView();

  message_list_view()->set_stacked_notification_count(2);
  EXPECT_EQ(3u, message_list_view()->children().size());

  message_list_view()->ClearAllWithAnimation();
  EXPECT_EQ(2u, message_list_view()->children().size());

  message_list_view()->set_stacked_notification_count(1);
  int previous_height = message_list_view()->GetPreferredSize().height();
  gfx::Rect previous_bounds = GetMessageViewBounds(1);
  AnimateToMiddle();
  EXPECT_EQ(previous_height, message_list_view()->GetPreferredSize().height());
  EXPECT_EQ(previous_bounds, GetMessageViewBounds(1));
  AnimateToEnd();
  EXPECT_EQ(1u, message_list_view()->children().size());

  message_list_view()->set_stacked_notification_count(0);
  previous_height = message_list_view()->GetPreferredSize().height();
  AnimateToMiddle();
  EXPECT_EQ(previous_height, message_list_view()->GetPreferredSize().height());
  AnimateToEnd();
  EXPECT_EQ(1u, message_list_view()->children().size());

  previous_bounds = GetMessageViewBounds(0);
  AnimateToMiddle();
  EXPECT_LT(previous_bounds.x(), GetMessageViewBounds(0).x());
  AnimateToEnd();
  EXPECT_EQ(0u, message_list_view()->children().size());

  previous_height = message_list_view()->GetPreferredSize().height();
  AnimateToMiddle();
  EXPECT_GT(previous_height, message_list_view()->GetPreferredSize().height());
  previous_height = message_list_view()->GetPreferredSize().height();

  AnimateToEnd();
  EXPECT_EQ(0u, message_list_view()->children().size());
  EXPECT_GT(previous_height, message_list_view()->GetPreferredSize().height());
  EXPECT_EQ(0, message_list_view()->GetPreferredSize().height());

  EXPECT_FALSE(IsAnimating());
}

TEST_P(ParameterizedNotificationListViewTest, ClearAllClosedInTheMiddle) {
  AddNotification();
  AddNotification();
  AddNotification();
  CreateMessageListView();

  message_list_view()->ClearAllWithAnimation();
  AnimateToMiddle();

  DestroyMessageListView();
  EXPECT_TRUE(MessageCenter::Get()->GetVisibleNotifications().empty());
}

TEST_P(ParameterizedNotificationListViewTest, ClearAllInterrupted) {
  AddNotification();
  AddNotification();
  AddNotification();
  CreateMessageListView();

  message_list_view()->ClearAllWithAnimation();
  AnimateToMiddle();
  auto new_id = AddNotification();

  EXPECT_EQ(1u, MessageCenter::Get()->GetVisibleNotifications().size());
  EXPECT_TRUE(MessageCenter::Get()->FindVisibleNotificationById(new_id));
}

TEST_P(ParameterizedNotificationListViewTest, ClearAllWithPinnedNotifications) {
  AddNotification(/*pinned=*/true);
  AddNotification();
  AddNotification();
  CreateMessageListView();

  message_list_view()->ClearAllWithAnimation();
  AnimateUntilIdle();
  EXPECT_EQ(1u, message_list_view()->children().size());
}

// Flaky: https://crbug.com/1292701.
TEST_P(ParameterizedNotificationListViewTest,
       DISABLED_UserSwipesAwayNotification) {
  // Show message list with two notifications.
  AddNotification();
  auto id1 = AddNotification();
  CreateMessageListView();

  // Start swiping the notification away.
  GetMessageViewAt(1)->OnSlideStarted();
  GetMessageViewAt(1)->OnSlideChanged(true);
  EXPECT_EQ(2u, MessageCenter::Get()->GetVisibleNotifications().size());
  EXPECT_EQ(2u, message_list_view()->children().size());

  // Swiping away the notification should remove it both in the MessageCenter
  // and the MessageListView.
  MessageCenter::Get()->RemoveNotification(id1, true /* by_user */);
  FinishSlideOutAnimation();
  EXPECT_EQ(1u, MessageCenter::Get()->GetVisibleNotifications().size());
  EXPECT_EQ(1u, message_list_view()->children().size());

  // The next and only animation should be the move down animation.
  int previous_height = message_list_view()->GetPreferredSize().height();
  AnimateToEnd();
  EXPECT_GT(previous_height, message_list_view()->GetPreferredSize().height());
  EXPECT_FALSE(message_list_view()->IsAnimating());
}

TEST_P(ParameterizedNotificationListViewTest, InitInSortedOrder) {
  // MessageViews should be ordered, from top down: [ id1, id2, id0 ].
  auto id0 = AddNotification(/*pinned=*/true);
  OffsetNotificationTimestamp(id0, 2000 /* milliseconds */);
  auto id1 = AddNotification();
  OffsetNotificationTimestamp(id1, 1000 /* milliseconds */);
  auto id2 = AddNotification();
  CreateMessageListView();

  EXPECT_EQ(3u, message_list_view()->children().size());

  if (!features::IsNotificationsRefreshEnabled()) {
    EXPECT_EQ(id1, GetMessageViewAt(0)->notification_id());
    EXPECT_EQ(id2, GetMessageViewAt(1)->notification_id());
    EXPECT_EQ(id0, GetMessageViewAt(2)->notification_id());
  } else {
    EXPECT_EQ(id1, GetMessageViewAt(2)->notification_id());
    EXPECT_EQ(id2, GetMessageViewAt(1)->notification_id());
    EXPECT_EQ(id0, GetMessageViewAt(0)->notification_id());
  }
}

TEST_P(ParameterizedNotificationListViewTest, NotificationAddedInSortedOrder) {
  auto id0 = AddNotification(/*pinned=*/true);
  OffsetNotificationTimestamp(id0, 3000 /* milliseconds */);
  auto id1 = AddNotification();
  OffsetNotificationTimestamp(id1, 2000 /* milliseconds */);
  auto id2 = AddNotification();
  OffsetNotificationTimestamp(id2, 1000 /* milliseconds */);
  CreateMessageListView();

  auto id3 = AddNotification(/*pinned=*/true);
  EXPECT_EQ(4u, message_list_view()->children().size());
  if (!features::IsNotificationsRefreshEnabled()) {
    // New pinned notification should be added to the end.
    EXPECT_EQ(id3, GetMessageViewAt(3)->notification_id());
  } else {
    // New pinned notification should be added to the start.
    EXPECT_EQ(id3, GetMessageViewAt(0)->notification_id());
  }

  // New non-pinned notification should be added before pinned notifications.
  auto id4 = AddNotification();
  EXPECT_EQ(5u, message_list_view()->children().size());

  if (!features::IsNotificationsRefreshEnabled()) {
    EXPECT_EQ(id1, GetMessageViewAt(0)->notification_id());
    EXPECT_EQ(id2, GetMessageViewAt(1)->notification_id());
    EXPECT_EQ(id4, GetMessageViewAt(2)->notification_id());
    EXPECT_EQ(id0, GetMessageViewAt(3)->notification_id());
    EXPECT_EQ(id3, GetMessageViewAt(4)->notification_id());
  } else {
    EXPECT_EQ(id1, GetMessageViewAt(4)->notification_id());
    EXPECT_EQ(id2, GetMessageViewAt(3)->notification_id());
    EXPECT_EQ(id4, GetMessageViewAt(2)->notification_id());
    EXPECT_EQ(id0, GetMessageViewAt(1)->notification_id());
    EXPECT_EQ(id3, GetMessageViewAt(0)->notification_id());
  }
}

// Tests only with NotificationsRefresh enabled.
class RefreshedNotificationListView : public NotificationListViewTest {
 public:
  RefreshedNotificationListView() = default;
  RefreshedNotificationListView(const RefreshedNotificationListView&) = delete;
  RefreshedNotificationListView& operator=(
      const RefreshedNotificationListView&) = delete;
  ~RefreshedNotificationListView() override = default;

  void SetUp() override {
    scoped_feature_list_ = std::make_unique<base::test::ScopedFeatureList>();
    scoped_feature_list_->InitAndEnableFeature(features::kNotificationsRefresh);
    NotificationListViewTest::SetUp();
  }

  // Start sliding the message view at the given index in the list.
  void StartSliding(size_t index) {
    auto* message_view = GetMessageViewAt(index);
    message_view->set_slide_amount(1);
    message_view->OnSlideChanged(/*in_progress=*/true);
  }

 private:
  std::unique_ptr<base::test::ScopedFeatureList> scoped_feature_list_;
};

// Tests that preferred size changes upon toggle of expand/collapse.
TEST_F(RefreshedNotificationListView, PreferredSizeChangesOnToggle) {
  AddNotification(/*pinned=*/false, /*expandable=*/true);
  AddNotification(/*pinned=*/false, /*expandable=*/true);
  CreateMessageListView();
  auto* message_view = GetMessageViewAt(0);
  ASSERT_TRUE(message_view->IsExpanded());
  gfx::Size old_preferred_size =
      message_list_view()->children()[0]->GetPreferredSize();

  EXPECT_FALSE(IsAnimating());

  message_view->SetExpanded(/*expanded=*/false);

  EXPECT_TRUE(IsAnimating());
  EXPECT_TRUE(message_list_view()->IsAnimatingExpandOrCollapseContainer(
      message_list_view()->children()[0]));
  EXPECT_EQ(old_preferred_size.height(),
            message_list_view()->children()[0]->GetPreferredSize().height());

  old_preferred_size = message_list_view()->children()[0]->GetPreferredSize();
  AnimateToMiddle();

  EXPECT_GT(old_preferred_size.height(),
            message_list_view()->children()[0]->GetPreferredSize().height());

  AnimateToEnd();
  FinishSlideOutAnimation();
  EXPECT_FALSE(IsAnimating());
  EXPECT_FALSE(message_list_view()->IsAnimatingExpandOrCollapseContainer(
      message_list_view()->children()[0]));
}

// Tests that expanding a notification while a different notification is
// expanding is handled gracefully.
TEST_F(RefreshedNotificationListView, TwoExpandsInARow) {
  AddNotification(/*pinned=*/false, /*expandable=*/true);
  AddNotification(/*pinned=*/false, /*expandable=*/true);
  CreateMessageListView();

  // First expand the notification in `first_notification_container`.
  auto* first_notification_container = message_list_view()->children()[1];
  auto* message_view = GetMessageViewAt(1);
  ASSERT_FALSE(message_view->IsExpanded());
  message_view->SetExpanded(/*expanded=*/true);
  AnimateToMiddle();
  const gfx::Size first_notification_middle_of_animation_size =
      first_notification_container->GetPreferredSize();

  // Collapse the second notification as `message_view` is still animating.
  auto* second_notification_container = message_list_view()->children()[0];
  const gfx::Size second_notification_initial_size =
      second_notification_container->GetPreferredSize();
  message_view = GetMessageViewAt(0);
  message_view->SetExpanded(/*expanded=*/false);

  EXPECT_TRUE(IsAnimating());
  EXPECT_FALSE(message_list_view()->IsAnimatingExpandOrCollapseContainer(
      first_notification_container));
  EXPECT_TRUE(message_list_view()->IsAnimatingExpandOrCollapseContainer(
      second_notification_container));
  // The originally animating container should have been snapped to its final
  // bounds.
  EXPECT_LT(first_notification_middle_of_animation_size.height(),
            first_notification_container->GetPreferredSize().height());

  AnimateToEnd();
  FinishSlideOutAnimation();

  // `second_notification_container` should animate to its final bounds.
  EXPECT_GT(second_notification_initial_size.height(),
            second_notification_container->GetPreferredSize().height());
}

// Tests that collapsing/expanding is reversible.
TEST_F(RefreshedNotificationListView, ReverseExpand) {
  AddNotification(/*pinned=*/false, /*expandable=*/true);
  AddNotification(/*pinned=*/false, /*expandable=*/true);
  CreateMessageListView();
  auto* message_view = GetMessageViewAt(0);

  auto* second_notification_container = message_list_view()->children()[0];
  message_view->SetExpanded(/*expanded=*/false);
  AnimateToMiddle();
  const gfx::Size middle_of_collapsed_size =
      second_notification_container->GetPreferredSize();

  // Animate to expanded in the middle of the collapse animation. This should
  // stop the collapse animation and set the view to its final bounds, then
  // animate to expanded.
  message_view->SetExpanded(/*expanded=*/true);
  const gfx::Size final_collapsed_size =
      second_notification_container->GetPreferredSize();
  EXPECT_LT(final_collapsed_size.height(), middle_of_collapsed_size.height());

  // Animate to the end. The container view should be fully expanded.
  AnimateToEnd();
  EXPECT_LT(middle_of_collapsed_size.height(),
            second_notification_container->GetPreferredSize().height());
}

// Tests that destroying during a collapse animation does not crash.
TEST_F(RefreshedNotificationListView, DestroyMessageListViewDuringCollapse) {
  AddNotification(/*pinned=*/false, /*expandable=*/true);
  AddNotification(/*pinned=*/false, /*expandable=*/true);
  CreateMessageListView();
  auto* message_view = GetMessageViewAt(0);
  message_view->SetExpanded(/*expanded=*/false);
  AnimateToMiddle();

  DestroyMessageListView();
}

// Tests that closing a notification while its collapse animation is ongoing
// works properly.
TEST_F(RefreshedNotificationListView, RemoveNotificationDuringCollapse) {
  auto id1 = AddNotification(/*pinned=*/false, /*expandable=*/true);
  CreateMessageListView();
  auto* message_view = GetMessageViewAt(0);
  message_view->SetExpanded(/*expanded=*/false);
  AnimateToMiddle();
  auto* notification_container = message_list_view()->children()[0];
  const gfx::Size middle_of_collapsed_size =
      notification_container->GetPreferredSize();

  // Remove the notification for `message_view`. The view should snap to
  // collapsed bounds, then slide out.
  MessageCenter::Get()->RemoveNotification(id1, /*by_user=*/true);

  EXPECT_LE(notification_container->GetPreferredSize().height(),
            middle_of_collapsed_size.height());
  FinishSlideOutAnimation();
  AnimateUntilIdle();

  EXPECT_EQ(0u, message_list_view()->children().size());
  EXPECT_EQ(0, message_list_view()->GetPreferredSize().height());
}

// Tests that expanding a notification at various stages while it is being
// closed does not result in an animation.
// TODO(crbug.com/1292775): Test is flaky.
TEST_F(RefreshedNotificationListView,
       DISABLED_CollapseDuringCloseResultsInNoCollapseAnimation) {
  auto id1 = AddNotification(/*pinned=*/false, /*expandable=*/true);
  AddNotification(/*pinned=*/false, /*expandable=*/true);
  CreateMessageListView();

  auto* notification_container = message_list_view()->children()[0];
  const gfx::Size pre_remove_size = notification_container->GetPreferredSize();
  // Remove the notification, this should activate the "slide out" animation.
  MessageCenter::Get()->RemoveNotification(id1, /*by_user=*/true);
  EXPECT_EQ(notification_container->GetPreferredSize(), pre_remove_size);
  // Removing the notification does not trigger an animation at the level of
  // NotificationListView
  EXPECT_FALSE(message_list_view()->IsAnimating());
  EXPECT_FALSE(message_list_view()->IsAnimatingExpandOrCollapseContainer(
      notification_container));

  // Trigger the collapse before slide out completes, this should not trigger an
  // animation for NotificationListView, and no animation should occur.
  // SlideOut animation happens at a lower level. Also, size changes should be
  // ignored when being removed.
  GetMessageViewAt(0)->SetExpanded(/*expanded=*/false);
  EXPECT_FALSE(message_list_view()->IsAnimating());
  EXPECT_FALSE(message_list_view()->IsAnimatingExpandOrCollapseContainer(
      notification_container));
  EXPECT_EQ(pre_remove_size, notification_container->GetPreferredSize());

  // Finish the slide out animation. Then an animation should begin to shrink
  // MessageListView to contain the remaining notifications via
  // State::MOVE_DOWN. Only one notification should remain.
  FinishSlideOutAnimation();
  EXPECT_TRUE(message_list_view()->IsAnimating());
  EXPECT_EQ(1u, message_list_view()->children().size());
}

// Tests that collapsing a notification while it is being moved automatically
// completes both animations.
// TODO(crbug.com/1292816): Test is flaky.
TEST_F(RefreshedNotificationListView, DISABLED_CollapseDuringMoveNoAnimation) {
  auto to_be_removed_notification =
      AddNotification(/*pinned=*/false, /*expandable=*/true);
  auto to_be_collapsed_notification =
      AddNotification(/*pinned=*/false, /*expandable=*/true);
  CreateMessageListView();
  auto* to_be_collapsed_message_view_container =
      message_list_view()->children()[1];
  auto* to_be_collapsed_message_view = GetMessageViewAt(1);
  const gfx::Size pre_collapse_size =
      to_be_collapsed_message_view_container->GetPreferredSize();
  ASSERT_TRUE(to_be_collapsed_message_view->IsExpanded());

  // Delete the first notification. This should begin the slide out animation.
  // Let that finish, then State::MOVE_DOWN should begin.
  MessageCenter::Get()->RemoveNotification(to_be_removed_notification,
                                           /*by_user=*/true);
  FinishSlideOutAnimation();
  EXPECT_TRUE(message_list_view()->IsAnimating());
  EXPECT_FALSE(message_list_view()->IsAnimatingExpandOrCollapseContainer(
      to_be_collapsed_message_view_container));

  // Animate to the middle, then attempt to collapse an existing notification.
  // All animations should complete.
  AnimateToMiddle();
  to_be_collapsed_message_view->SetExpanded(false);
  EXPECT_FALSE(message_list_view()->IsAnimating());
  EXPECT_FALSE(message_list_view()->IsAnimatingExpandOrCollapseContainer(
      to_be_collapsed_message_view_container));
  EXPECT_GT(
      pre_collapse_size.height(),
      to_be_collapsed_message_view_container->GetPreferredSize().height());
}

// Tests that moving a notification while it is already collapsing completes
// both animations.
TEST_F(RefreshedNotificationListView, MoveDuringCollapseNoAnimation) {
  auto to_be_removed_notification =
      AddNotification(/*pinned=*/false, /*expandable=*/true);
  auto to_be_collapsed_notification =
      AddNotification(/*pinned=*/false, /*expandable=*/true);
  CreateMessageListView();
  auto* to_be_collapsed_message_view_container =
      message_list_view()->children()[0];
  auto* to_be_collapsed_message_view = GetMessageViewAt(0);
  const gfx::Size pre_collapse_size =
      to_be_collapsed_message_view_container->GetPreferredSize();
  ASSERT_TRUE(to_be_collapsed_message_view->IsExpanded());

  // Collapse the second notification, then delete the first.
  to_be_collapsed_message_view->SetExpanded(false);
  AnimateToMiddle();
  EXPECT_TRUE(message_list_view()->IsAnimatingExpandOrCollapseContainer(
      to_be_collapsed_message_view_container));
  EXPECT_TRUE(message_list_view()->IsAnimating());
  MessageCenter::Get()->RemoveNotification(to_be_removed_notification,
                                           /*by_user=*/true);

  EXPECT_FALSE(message_list_view()->IsAnimatingExpandOrCollapseContainer(
      to_be_collapsed_message_view_container));
  EXPECT_FALSE(message_list_view()->IsAnimating());
  EXPECT_GT(
      pre_collapse_size.height(),
      to_be_collapsed_message_view_container->GetPreferredSize().height());
}

TEST_F(RefreshedNotificationListView, SlideNotification) {
  // Show message list with four notifications.
  auto id0 = AddNotification();
  auto id1 = AddNotification();
  auto id2 = AddNotification();
  auto id3 = AddNotification();
  CreateMessageListView();

  // At first, there should be no fully rounded corners for the middle
  // notification.
  EXPECT_EQ(kMessageCenterNotificationInnerCornerRadius,
            GetMessageViewAt(2)->top_radius());
  EXPECT_EQ(kMessageCenterNotificationInnerCornerRadius,
            GetMessageViewAt(2)->bottom_radius());

  // Start sliding notification 2 away.
  StartSliding(2);
  EXPECT_EQ(kMessageCenterNotificationTopBottomCornerRadius,
            GetMessageViewAt(2)->bottom_radius());

  // Notification 1's bottom corner and notification 3's top corner should also
  // be rounded.
  EXPECT_EQ(kMessageCenterNotificationInnerCornerRadius,
            GetMessageViewAt(1)->top_radius());
  EXPECT_EQ(kMessageCenterNotificationTopBottomCornerRadius,
            GetMessageViewAt(1)->bottom_radius());
  EXPECT_EQ(kMessageCenterNotificationTopBottomCornerRadius,
            GetMessageViewAt(3)->top_radius());
  EXPECT_EQ(kMessageCenterNotificationInnerCornerRadius,
            GetMessageViewAt(3)->bottom_radius());

  // Notification 0 should not change.
  EXPECT_EQ(kMessageCenterNotificationTopBottomCornerRadius,
            GetMessageViewAt(0)->top_radius());
  EXPECT_EQ(kMessageCenterNotificationInnerCornerRadius,
            GetMessageViewAt(0)->bottom_radius());

  // Slide out notification 2, the 3 notifications left should have no rounded
  // corner after slide out done.
  MessageCenter::Get()->RemoveNotification(id2, /*by_user=*/true);
  FinishSlideOutAnimation();
  AnimateUntilIdle();

  EXPECT_EQ(kMessageCenterNotificationInnerCornerRadius,
            GetMessageViewAt(0)->bottom_radius());
  EXPECT_EQ(kMessageCenterNotificationInnerCornerRadius,
            GetMessageViewAt(1)->top_radius());

  // Test with notification 1. Same behavior should happen.
  StartSliding(1);
  EXPECT_EQ(kMessageCenterNotificationTopBottomCornerRadius,
            GetMessageViewAt(1)->top_radius());
  EXPECT_EQ(kMessageCenterNotificationTopBottomCornerRadius,
            GetMessageViewAt(1)->bottom_radius());
  EXPECT_EQ(kMessageCenterNotificationInnerCornerRadius,
            GetMessageViewAt(0)->top_radius());
  EXPECT_EQ(kMessageCenterNotificationTopBottomCornerRadius,
            GetMessageViewAt(0)->bottom_radius());
  EXPECT_EQ(kMessageCenterNotificationTopBottomCornerRadius,
            GetMessageViewAt(2)->top_radius());
  EXPECT_EQ(kMessageCenterNotificationInnerCornerRadius,
            GetMessageViewAt(2)->bottom_radius());

  // Cancel the slide. Everything goes back to normal.
  GetMessageViewAt(1)->OnSlideChanged(/*in_progress=*/false);
  for (int i = 0; i <= 2; i++) {
    EXPECT_EQ(kMessageCenterNotificationInnerCornerRadius,
              GetMessageViewAt(i)->top_radius());
    EXPECT_EQ(kMessageCenterNotificationInnerCornerRadius,
              GetMessageViewAt(i)->bottom_radius());
  }

  // Test with the top notification.
  StartSliding(0);
  EXPECT_EQ(kMessageCenterNotificationTopBottomCornerRadius,
            GetMessageViewAt(0)->top_radius());
  EXPECT_EQ(kMessageCenterNotificationTopBottomCornerRadius,
            GetMessageViewAt(0)->bottom_radius());
  EXPECT_EQ(kMessageCenterNotificationTopBottomCornerRadius,
            GetMessageViewAt(1)->top_radius());
  EXPECT_EQ(kMessageCenterNotificationInnerCornerRadius,
            GetMessageViewAt(1)->bottom_radius());
  EXPECT_EQ(kMessageCenterNotificationInnerCornerRadius,
            GetMessageViewAt(2)->top_radius());
  EXPECT_EQ(kMessageCenterNotificationInnerCornerRadius,
            GetMessageViewAt(2)->bottom_radius());
  GetMessageViewAt(0)->OnSlideChanged(/*in_progress=*/false);

  // Test with the bottom notification.
  StartSliding(2);
  EXPECT_EQ(kMessageCenterNotificationInnerCornerRadius,
            GetMessageViewAt(0)->top_radius());
  EXPECT_EQ(kMessageCenterNotificationInnerCornerRadius,
            GetMessageViewAt(0)->bottom_radius());
  EXPECT_EQ(kMessageCenterNotificationInnerCornerRadius,
            GetMessageViewAt(1)->top_radius());
  EXPECT_EQ(kMessageCenterNotificationTopBottomCornerRadius,
            GetMessageViewAt(1)->bottom_radius());
  EXPECT_EQ(kMessageCenterNotificationTopBottomCornerRadius,
            GetMessageViewAt(2)->top_radius());
  EXPECT_EQ(kMessageCenterNotificationTopBottomCornerRadius,
            GetMessageViewAt(2)->bottom_radius());
  GetMessageViewAt(2)->OnSlideChanged(/*in_progress=*/false);
}

}  // namespace ash
