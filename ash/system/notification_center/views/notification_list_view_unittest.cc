// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/notification_center/views/notification_list_view.h"

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/system_notification_builder.h"
#include "ash/system/notification_center/message_center_constants.h"
#include "ash/system/notification_center/message_center_utils.h"
#include "ash/system/notification_center/notification_center_test_api.h"
#include "ash/system/notification_center/views/ash_notification_view.h"
#include "ash/system/unified/unified_system_tray_model.h"
#include "ash/test/ash_test_base.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "ui/base/models/image_model.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/notification_list.h"
#include "ui/message_center/public/cpp/notifier_id.h"
#include "ui/message_center/views/message_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/test/views_test_utils.h"
#include "url/gurl.h"

using message_center::MessageCenter;
using message_center::MessageView;
using message_center::Notification;

namespace ash {

namespace {

class TestNotificationView : public AshNotificationView {
 public:
  explicit TestNotificationView(
      const message_center::Notification& notification)
      : AshNotificationView(notification, /*shown_in_popup=*/false) {
    layer()->GetAnimator()->set_preemption_strategy(
        ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
  }

  TestNotificationView(const TestNotificationView&) = delete;
  TestNotificationView& operator=(const TestNotificationView&) = delete;

  ~TestNotificationView() override = default;

  // AshNotificationView:
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

  std::optional<float> slide_amount_;
};

class TestNotificationListView : public NotificationListView {
 public:
  TestNotificationListView() : NotificationListView(nullptr) {}

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
  std::unique_ptr<message_center::MessageView> CreateMessageView(
      const message_center::Notification& notification) override {
    auto message_view = std::make_unique<TestNotificationView>(notification);
    ConfigureMessageView(message_view.get());
    return message_view;
  }

  std::vector<raw_ptr<message_center::Notification, VectorExperimental>>
  GetStackedNotifications() const override {
    return stacked_notifications_;
  }

  std::vector<std::string> GetNonVisibleNotificationIdsInViewHierarchy()
      const override {
    return notification_id_list_;
  }

 private:
  std::vector<raw_ptr<message_center::Notification, VectorExperimental>>
      stacked_notifications_;
  std::vector<std::string> notification_id_list_;
};

// Returns true if the provided `corner_radius` has a value that is used for
// notification views on the edges of `NotificationListView`, or for
// notifications that are sliding out.
bool IsEdge(int corner_radius) {
  return corner_radius == kMessageCenterScrollViewCornerRadius;
}

// Returns true if the provided `corner_radius` has a value that is
// used for inner borders of `NotificationListView` child views.
bool IsInner(int corner_radius) {
  return corner_radius == kMessageCenterNotificationInnerCornerRadius;
}

}  // namespace

// The base test class, has no params so tests with no params can inherit from
// this.
class NotificationListViewTest : public AshTestBase,
                                 public views::ViewObserver,
                                 public testing::WithParamInterface<
                                     /*are_ongoing_processes_enabled=*/bool> {
 public:
  NotificationListViewTest() {
    scoped_feature_list_.InitWithFeatureState(features::kOngoingProcesses,
                                              AreOngoingProcessesEnabled());
  }

  void SetUp() override {
    AshTestBase::SetUp();
    model_ = base::MakeRefCounted<UnifiedSystemTrayModel>(nullptr);

    test_api_ = std::make_unique<NotificationCenterTestApi>();
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

  NotificationCenterTestApi* test_api() { return test_api_.get(); }

  bool AreOngoingProcessesEnabled() const { return GetParam(); }

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
    // Manually trigger observer functions since `NotificationCenterController`,
    // which is used to create ongoing processes is not created in this test.
    if (AreOngoingProcessesEnabled() && notification_list_view_) {
      notification_list_view_->OnNotificationAdded(id);
    }
    return id;
  }

  void RemoveNotification(const std::string& id, bool by_user = true) {
    MessageCenter::Get()->RemoveNotification(id, by_user);
    // Manually trigger observer functions since `NotificationCenterController`,
    // which is used to create ongoing processes is not created in this test.
    if (AreOngoingProcessesEnabled() && notification_list_view_) {
      notification_list_view_->OnNotificationRemoved(id, by_user);
    }
  }

  void OffsetNotificationTimestamp(const std::string& id,
                                   const int milliseconds) {
    MessageCenter::Get()->FindVisibleNotificationById(id)->set_timestamp(
        base::Time::Now() - base::Milliseconds(milliseconds));
  }

  void CreateMessageListView() {
    notification_list_view_ = std::make_unique<TestNotificationListView>();
    if (AreOngoingProcessesEnabled()) {
      notification_list_view_->Init(
          message_center_utils::GetSortedNotificationsWithOwnView());
    } else {
      notification_list_view_->Init();
    }
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

  // Start sliding the message view at the given index in the list.
  void StartSliding(size_t index) {
    auto* message_view = GetMessageViewAt(index);
    message_view->set_slide_amount(1);
    message_view->OnSlideChanged(/*in_progress=*/true);
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
    while (message_list_view()->animation_->is_animating()) {
      message_list_view()->animation_->End();
    }
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

  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<NotificationCenterTestApi> test_api_;
  scoped_refptr<UnifiedSystemTrayModel> model_;
  std::unique_ptr<TestNotificationListView> notification_list_view_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    NotificationListViewTest,
    /*enable_notification_center_controller=*/testing::Bool());

TEST_P(NotificationListViewTest, Open) {
  auto id0 = AddNotification();
  auto id1 = AddNotification();
  auto id2 = AddNotification();
  CreateMessageListView();

  EXPECT_EQ(3u, message_list_view()->children().size());

  EXPECT_EQ(id0, GetMessageViewAt(2)->notification_id());
  EXPECT_EQ(id1, GetMessageViewAt(1)->notification_id());
  EXPECT_EQ(id2, GetMessageViewAt(0)->notification_id());

  EXPECT_FALSE(GetMessageViewAt(2)->IsExpanded());
  EXPECT_FALSE(GetMessageViewAt(1)->IsExpanded());
  EXPECT_TRUE(GetMessageViewAt(0)->IsExpanded());

  // Check the position of notifications for the required extra spacing between
  // notifications.
  EXPECT_EQ(GetMessageViewBounds(0).bottom() + kMessageListNotificationSpacing,
            GetMessageViewBounds(1).y());
  EXPECT_EQ(GetMessageViewBounds(1).bottom() + kMessageListNotificationSpacing,
            GetMessageViewBounds(2).y());

  const int top_bottom_corner_radius = kMessageCenterScrollViewCornerRadius;
  const int inner_corner_radius = kMessageCenterNotificationInnerCornerRadius;
  EXPECT_EQ(top_bottom_corner_radius, GetMessageViewAt(0)->top_radius());
  EXPECT_EQ(inner_corner_radius, GetMessageViewAt(1)->top_radius());
  EXPECT_EQ(inner_corner_radius, GetMessageViewAt(2)->top_radius());

  EXPECT_EQ(inner_corner_radius, GetMessageViewAt(0)->bottom_radius());
  EXPECT_EQ(inner_corner_radius, GetMessageViewAt(1)->bottom_radius());

  EXPECT_EQ(top_bottom_corner_radius, GetMessageViewAt(2)->bottom_radius());

  EXPECT_LT(0, message_list_view()->GetPreferredSize().height());
}

TEST_P(NotificationListViewTest, AddNotifications) {
  CreateMessageListView();
  EXPECT_EQ(0, message_list_view()->GetPreferredSize().height());

  auto id0 = AddNotification();
  EXPECT_EQ(1, size_changed_count());
  EXPECT_EQ(1u, message_list_view()->children().size());
  EXPECT_EQ(id0, GetMessageViewAt(0)->notification_id());

  const int top_bottom_corner_radius = kMessageCenterScrollViewCornerRadius;
  const int inner_corner_radius = kMessageCenterNotificationInnerCornerRadius;
  EXPECT_EQ(top_bottom_corner_radius, GetMessageViewAt(0)->top_radius());
  EXPECT_EQ(top_bottom_corner_radius, GetMessageViewAt(0)->bottom_radius());

  int previous_notification_list_view_height =
      message_list_view()->GetPreferredSize().height();
  EXPECT_LT(0, previous_notification_list_view_height);

  gfx::Rect previous_bounds = GetMessageViewBounds(0);
  auto id1 = AddNotification();
  EXPECT_EQ(2, size_changed_count());
  EXPECT_EQ(2u, message_list_view()->children().size());
  EXPECT_EQ(id1, GetMessageViewAt(0)->notification_id());

  EXPECT_LT(previous_notification_list_view_height,
            message_list_view()->GetPreferredSize().height());

  EXPECT_EQ(previous_bounds, GetMessageViewBounds(0));

  EXPECT_EQ(GetMessageViewBounds(0).bottom() + kMessageListNotificationSpacing,
            GetMessageViewBounds(1).y());

  EXPECT_EQ(top_bottom_corner_radius, GetMessageViewAt(0)->top_radius());
  EXPECT_EQ(inner_corner_radius, GetMessageViewAt(1)->top_radius());

  EXPECT_EQ(top_bottom_corner_radius, GetMessageViewAt(1)->bottom_radius());
}

TEST_P(NotificationListViewTest, RemoveNotification) {
  auto id0 = AddNotification();
  auto id1 = AddNotification();

  CreateMessageListView();
  int previous_height = message_list_view()->GetPreferredSize().height();

  EXPECT_EQ(2u, message_list_view()->children().size());

  const int top_bottom_corner_radius = kMessageCenterScrollViewCornerRadius;
  const int inner_corner_radius = kMessageCenterNotificationInnerCornerRadius;
  EXPECT_EQ(top_bottom_corner_radius, GetMessageViewAt(0)->top_radius());
  EXPECT_EQ(inner_corner_radius, GetMessageViewAt(0)->bottom_radius());

  gfx::Rect previous_bounds = GetMessageViewBounds(0);
  RemoveNotification(id0);
  FinishSlideOutAnimation();
  AnimateUntilIdle();
  EXPECT_EQ(1u, message_list_view()->children().size());
  EXPECT_EQ(previous_bounds.y(), GetMessageViewBounds(0).y());
  EXPECT_LT(0, message_list_view()->GetPreferredSize().height());
  EXPECT_GT(previous_height, message_list_view()->GetPreferredSize().height());

  EXPECT_EQ(top_bottom_corner_radius, GetMessageViewAt(0)->top_radius());
  EXPECT_EQ(top_bottom_corner_radius, GetMessageViewAt(0)->bottom_radius());

  RemoveNotification(id1);
  FinishSlideOutAnimation();
  AnimateUntilIdle();
  EXPECT_EQ(0u, message_list_view()->children().size());
  EXPECT_EQ(0, message_list_view()->GetPreferredSize().height());
}

TEST_P(NotificationListViewTest, CollapseOlderNotifications) {
  AddNotification();
  CreateMessageListView();
  EXPECT_TRUE(GetMessageViewAt(0)->IsExpanded());

  AddNotification();

  EXPECT_FALSE(GetMessageViewAt(1)->IsExpanded());
  EXPECT_TRUE(GetMessageViewAt(0)->IsExpanded());

  AddNotification();

  EXPECT_FALSE(GetMessageViewAt(2)->IsExpanded());
  EXPECT_FALSE(GetMessageViewAt(1)->IsExpanded());
  EXPECT_TRUE(GetMessageViewAt(0)->IsExpanded());

  GetMessageViewAt(1)->SetExpanded(true);
  GetMessageViewAt(1)->SetManuallyExpandedOrCollapsed(
      message_center::ExpandState::USER_EXPANDED);

  AddNotification();

  EXPECT_TRUE(GetMessageViewAt(0)->IsExpanded());
  EXPECT_FALSE(GetMessageViewAt(1)->IsExpanded());
  EXPECT_TRUE(GetMessageViewAt(2)->IsExpanded());
  EXPECT_FALSE(GetMessageViewAt(3)->IsExpanded());
}

TEST_P(NotificationListViewTest, RemovingNotificationAnimation) {
  auto id0 = AddNotification(/*pinned=*/false);
  auto id1 = AddNotification();
  auto id2 = AddNotification();
  CreateMessageListView();
  int previous_height = message_list_view()->GetPreferredSize().height();
  gfx::Rect bounds0 = GetMessageViewBounds(0);
  gfx::Rect bounds1 = GetMessageViewBounds(1);
  gfx::Rect bounds2 = GetMessageViewBounds(2);

  RemoveNotification(id1);
  FinishSlideOutAnimation();
  AnimateToEnd();
  EXPECT_GT(previous_height, message_list_view()->GetPreferredSize().height());
  previous_height = message_list_view()->GetPreferredSize().height();

  EXPECT_EQ(bounds0, GetMessageViewBounds(0));
  EXPECT_EQ(bounds1, GetMessageViewBounds(1));

  RemoveNotification(id2);
  FinishSlideOutAnimation();
  AnimateToEnd();
  EXPECT_GT(previous_height, message_list_view()->GetPreferredSize().height());
  previous_height = message_list_view()->GetPreferredSize().height();
  // The remaining notification keeps the smaller vertical size of the last
  // notification in the original list, but shifts position.
  EXPECT_EQ(bounds2.size(), GetMessageViewBounds(0).size());
  EXPECT_EQ(bounds0.origin(), GetMessageViewBounds(0).origin());

  RemoveNotification(id0);
  FinishSlideOutAnimation();
  AnimateToEnd();
  EXPECT_GT(previous_height, message_list_view()->GetPreferredSize().height());
  previous_height = message_list_view()->GetPreferredSize().height();

  EXPECT_EQ(0, message_list_view()->GetPreferredSize().height());
}

// Flaky: https://crbug.com/1292774.
TEST_P(NotificationListViewTest, DISABLED_ResetAnimation) {
  auto id0 = AddNotification();
  auto id1 = AddNotification();
  CreateMessageListView();

  RemoveNotification(id0);
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

TEST_P(NotificationListViewTest, KeepManuallyExpanded) {
  AddNotification();
  AddNotification();
  CreateMessageListView();

  EXPECT_FALSE(GetMessageViewAt(1)->IsExpanded());
  EXPECT_TRUE(GetMessageViewAt(0)->IsExpanded());

  EXPECT_FALSE(GetMessageViewAt(0)->IsManuallyExpandedOrCollapsed());
  EXPECT_FALSE(GetMessageViewAt(1)->IsManuallyExpandedOrCollapsed());

  // Manually expand the first notification & manually collapse the second one.
  GetMessageViewAt(0)->SetExpanded(true);
  GetMessageViewAt(0)->SetManuallyExpandedOrCollapsed(
      message_center::ExpandState::USER_EXPANDED);
  GetMessageViewAt(1)->SetExpanded(false);
  GetMessageViewAt(1)->SetManuallyExpandedOrCollapsed(
      message_center::ExpandState::USER_COLLAPSED);

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
  EXPECT_FALSE(GetMessageViewAt(2)->IsExpanded());
  EXPECT_TRUE(GetMessageViewAt(1)->IsExpanded());
  EXPECT_TRUE(GetMessageViewAt(0)->IsExpanded());
  EXPECT_TRUE(GetMessageViewAt(2)->IsManuallyExpandedOrCollapsed());
  EXPECT_TRUE(GetMessageViewAt(1)->IsManuallyExpandedOrCollapsed());
  EXPECT_FALSE(GetMessageViewAt(0)->IsManuallyExpandedOrCollapsed());
}

TEST_P(NotificationListViewTest, ClearAllWithOnlyVisibleNotifications) {
  AddNotification();
  AddNotification();
  CreateMessageListView();

  EXPECT_EQ(2u, message_list_view()->children().size());
  int previous_height = message_list_view()->GetPreferredSize().height();
  int removed_view_index = 1;
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

TEST_P(NotificationListViewTest, ClearAllWithStackingNotifications) {
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

TEST_P(NotificationListViewTest, ClearAllClosedInTheMiddle) {
  AddNotification();
  AddNotification();
  AddNotification();
  CreateMessageListView();

  message_list_view()->ClearAllWithAnimation();
  AnimateToMiddle();

  DestroyMessageListView();
  EXPECT_TRUE(MessageCenter::Get()->GetVisibleNotifications().empty());
}

TEST_P(NotificationListViewTest, ClearAllInterrupted) {
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

TEST_P(NotificationListViewTest, ClearAllWithPinnedNotifications) {
  AddNotification(/*pinned=*/true);
  AddNotification();
  AddNotification();
  CreateMessageListView();

  message_list_view()->ClearAllWithAnimation();
  AnimateUntilIdle();
  EXPECT_EQ(1u, message_list_view()->children().size());
}

TEST_P(NotificationListViewTest, ClearAllWithStackingAndPinnedNotifications) {
  AddNotification(/*pinned=*/true);
  AddNotification(/*pinned=*/true);
  AddNotification();
  AddNotification();
  AddNotification();
  CreateMessageListView();

  message_list_view()->set_stacked_notification_count(3);

  message_list_view()->ClearAllWithAnimation();
  AnimateUntilIdle();
  EXPECT_EQ(2u, message_list_view()->children().size());
}

// Flaky: https://crbug.com/1292701.
TEST_P(NotificationListViewTest, DISABLED_UserSwipesAwayNotification) {
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
  RemoveNotification(id1);
  FinishSlideOutAnimation();
  EXPECT_EQ(1u, MessageCenter::Get()->GetVisibleNotifications().size());
  EXPECT_EQ(1u, message_list_view()->children().size());

  // The next and only animation should be the move down animation.
  int previous_height = message_list_view()->GetPreferredSize().height();
  AnimateToEnd();
  EXPECT_GT(previous_height, message_list_view()->GetPreferredSize().height());
  EXPECT_FALSE(message_list_view()->IsAnimating());
}

TEST_P(NotificationListViewTest, InitInSortedOrder) {
  // MessageViews should be ordered, from top down: [ id1, id2, id0 ].
  auto id0 = AddNotification(/*pinned=*/true);
  OffsetNotificationTimestamp(id0, 2000 /* milliseconds */);
  auto id1 = AddNotification();
  OffsetNotificationTimestamp(id1, 1000 /* milliseconds */);
  auto id2 = AddNotification();
  CreateMessageListView();

  EXPECT_EQ(3u, message_list_view()->children().size());
  EXPECT_EQ(id1, GetMessageViewAt(2)->notification_id());
  EXPECT_EQ(id2, GetMessageViewAt(1)->notification_id());
  EXPECT_EQ(id0, GetMessageViewAt(0)->notification_id());
}

TEST_P(NotificationListViewTest, NotificationAddedInSortedOrder) {
  auto id0 = AddNotification(/*pinned=*/true);
  OffsetNotificationTimestamp(id0, 3000 /* milliseconds */);
  auto id1 = AddNotification();
  OffsetNotificationTimestamp(id1, 2000 /* milliseconds */);
  auto id2 = AddNotification();
  OffsetNotificationTimestamp(id2, 1000 /* milliseconds */);
  CreateMessageListView();

  auto id3 = AddNotification(/*pinned=*/true);
  EXPECT_EQ(4u, message_list_view()->children().size());

  // New pinned notification should be added to the start.
  EXPECT_EQ(id3, GetMessageViewAt(0)->notification_id());

  // New non-pinned notification should be added before pinned notifications.
  auto id4 = AddNotification();
  EXPECT_EQ(5u, message_list_view()->children().size());

  EXPECT_EQ(id1, GetMessageViewAt(4)->notification_id());
  EXPECT_EQ(id2, GetMessageViewAt(3)->notification_id());
  EXPECT_EQ(id4, GetMessageViewAt(2)->notification_id());
  EXPECT_EQ(id0, GetMessageViewAt(1)->notification_id());
  EXPECT_EQ(id3, GetMessageViewAt(0)->notification_id());
}

TEST_P(NotificationListViewTest, OnChildNotificationViewUpdated) {
  const std::string source_url = "http://test-url.com";

  std::string id0;
  id0 = test_api()->AddNotificationWithSourceUrl(source_url);
  test_api()->AddNotificationWithSourceUrl(source_url);

  // Get the notification id for the parent notification. Parent notifications
  // are created by copying the oldest notification for a given notifier_id.
  const std::string parent_id =
      test_api()->NotificationIdToParentNotificationId(id0);

  test_api()->ToggleBubble();

  auto* parent_notification_view =
      test_api()->GetNotificationViewForId(parent_id);

  // Ensure id0 exist as child notifications inside the
  // `parent_notification_view` with the correct title.
  auto* child_view = static_cast<AshNotificationView*>(
      parent_notification_view->FindGroupNotificationView(id0));
  EXPECT_TRUE(child_view);
  EXPECT_EQ(u"test_title",
            child_view->GetTitleRowLabelForTest()->GetDisplayTextForTesting());

  std::u16string new_title = u"new title";

  // Update the child notification with a new title.
  std::unique_ptr<Notification> notification = std::make_unique<Notification>(
      message_center::NOTIFICATION_TYPE_SIMPLE, id0, new_title, u"message",
      ui::ImageModel(), u"display source", GURL(source_url),
      message_center::NotifierId(GURL(source_url)),
      message_center::RichNotificationData(),
      new message_center::NotificationDelegate());
  message_center::MessageCenter::Get()->UpdateNotification(
      id0, std::move(notification));

  test_api()->GetNotificationListView()->OnChildNotificationViewUpdated(
      parent_id, /*child_notification_id=*/id0);

  // Make sure the child view has the new title.
  child_view = static_cast<AshNotificationView*>(
      parent_notification_view->FindGroupNotificationView(id0));
  EXPECT_TRUE(child_view);
  EXPECT_EQ(new_title,
            child_view->GetTitleRowLabelForTest()->GetDisplayTextForTesting());
}

// Tests that the view animates when notification contents are updated.
TEST_P(NotificationListViewTest, AnimatesOnNotificationUpdated) {
  // Skip the test when `OngoingProcesses` are enabled since
  // `NotificationCenterController` is not created in this test.
  if (AreOngoingProcessesEnabled()) {
    GTEST_SKIP();
  }

  const std::string id = "id";
  // Create and add a notification without a `message` to the `MessageCenter`.
  auto notification =
      ash::SystemNotificationBuilder()
          .SetId(id)
          .SetCatalogName(NotificationCatalogName::kTestCatalogName)
          .SetTitle(u"Title")
          .BuildPtr(
              /*keep_timestamp=*/false);
  MessageCenter::Get()->AddNotification(std::move(notification));

  // Ensure the notification list does not animate when being created.
  CreateMessageListView();
  EXPECT_FALSE(IsAnimating());

  // Update the notification by attempting to create a new notification with the
  // same id, but it now has a non-empty `message`.
  auto updated_notification =
      ash::SystemNotificationBuilder()
          .SetId(id)
          .SetCatalogName(NotificationCatalogName::kTestCatalogName)
          .SetTitle(u"Title")
          .SetMessage(u"Message")
          .BuildPtr(
              /*keep_timestamp=*/false);
  MessageCenter::Get()->AddNotification(std::move(updated_notification));

  // Ensure the notification list animated to fit the new contents.
  EXPECT_TRUE(IsAnimating());
}

// Tests that preferred size changes upon toggle of expand/collapse.
TEST_P(NotificationListViewTest, PreferredSizeChangesOnToggle) {
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
TEST_P(NotificationListViewTest, TwoExpandsInARow) {
  AddNotification(/*pinned=*/false, /*expandable=*/true);
  AddNotification(/*pinned=*/false, /*expandable=*/true);
  CreateMessageListView();

  // First expand the notification in `first_notification_container`.
  auto* first_notification_container = message_list_view()->children()[1].get();
  auto* message_view = GetMessageViewAt(1);
  ASSERT_FALSE(message_view->IsExpanded());
  message_view->SetExpanded(/*expanded=*/true);
  AnimateToMiddle();
  const gfx::Size first_notification_middle_of_animation_size =
      first_notification_container->GetPreferredSize();

  // Collapse the second notification as `message_view` is still animating.
  auto* second_notification_container =
      message_list_view()->children()[0].get();
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
TEST_P(NotificationListViewTest, ReverseExpand) {
  AddNotification(/*pinned=*/false, /*expandable=*/true);
  AddNotification(/*pinned=*/false, /*expandable=*/true);
  CreateMessageListView();
  auto* message_view = GetMessageViewAt(0);

  auto* second_notification_container =
      message_list_view()->children()[0].get();
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
TEST_P(NotificationListViewTest, DestroyMessageListViewDuringCollapse) {
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
TEST_P(NotificationListViewTest, RemoveNotificationDuringCollapse) {
  auto id1 = AddNotification(/*pinned=*/false, /*expandable=*/true);
  CreateMessageListView();
  auto* message_view = GetMessageViewAt(0);
  message_view->SetExpanded(/*expanded=*/false);
  AnimateToMiddle();
  auto* notification_container = message_list_view()->children()[0].get();
  const gfx::Size middle_of_collapsed_size =
      notification_container->GetPreferredSize();

  // Remove the notification for `message_view`. The view should snap to
  // collapsed bounds, then slide out.
  RemoveNotification(id1);

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
TEST_P(NotificationListViewTest,
       DISABLED_CollapseDuringCloseResultsInNoCollapseAnimation) {
  auto id1 = AddNotification(/*pinned=*/false, /*expandable=*/true);
  AddNotification(/*pinned=*/false, /*expandable=*/true);
  CreateMessageListView();

  auto* notification_container = message_list_view()->children()[0].get();
  const gfx::Size pre_remove_size = notification_container->GetPreferredSize();
  // Remove the notification, this should activate the "slide out" animation.
  RemoveNotification(id1);
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
TEST_P(NotificationListViewTest, DISABLED_CollapseDuringMoveNoAnimation) {
  auto to_be_removed_notification =
      AddNotification(/*pinned=*/false, /*expandable=*/true);
  auto to_be_collapsed_notification =
      AddNotification(/*pinned=*/false, /*expandable=*/true);
  CreateMessageListView();
  auto* to_be_collapsed_message_view_container =
      message_list_view()->children()[1].get();
  auto* to_be_collapsed_message_view = GetMessageViewAt(1);
  const gfx::Size pre_collapse_size =
      to_be_collapsed_message_view_container->GetPreferredSize();
  ASSERT_TRUE(to_be_collapsed_message_view->IsExpanded());

  // Delete the first notification. This should begin the slide out animation.
  // Let that finish, then State::MOVE_DOWN should begin.
  RemoveNotification(to_be_removed_notification);
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
TEST_P(NotificationListViewTest, MoveDuringCollapseNoAnimation) {
  auto to_be_removed_notification =
      AddNotification(/*pinned=*/false, /*expandable=*/true);
  auto to_be_collapsed_notification =
      AddNotification(/*pinned=*/false, /*expandable=*/true);
  CreateMessageListView();
  auto* to_be_collapsed_message_view_container =
      message_list_view()->children()[0].get();
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
  RemoveNotification(to_be_removed_notification);

  EXPECT_FALSE(message_list_view()->IsAnimatingExpandOrCollapseContainer(
      to_be_collapsed_message_view_container));
  EXPECT_FALSE(message_list_view()->IsAnimating());
  EXPECT_GT(
      pre_collapse_size.height(),
      to_be_collapsed_message_view_container->GetPreferredSize().height());
}

TEST_P(NotificationListViewTest, SlideNotification) {
  // Show message list with four notifications.
  auto id0 = AddNotification();
  auto id1 = AddNotification();
  auto id2 = AddNotification();
  auto id3 = AddNotification();
  CreateMessageListView();

  // Ensure corners of all notifications are rounded properly.
  EXPECT_TRUE(IsEdge(GetMessageViewAt(0)->top_radius()));
  EXPECT_TRUE(IsInner(GetMessageViewAt(0)->bottom_radius()));
  for (int i = 1; i <= 2; i++) {
    EXPECT_TRUE(IsInner(GetMessageViewAt(i)->top_radius()));
    EXPECT_TRUE(IsInner(GetMessageViewAt(i)->bottom_radius()));
  }
  EXPECT_TRUE(IsInner(GetMessageViewAt(3)->top_radius()));
  EXPECT_TRUE(IsEdge(GetMessageViewAt(3)->bottom_radius()));

  // Start sliding a middle notification away. The top bottom and radius of the
  // sliding notification should be rounded like an edge.
  StartSliding(2);
  EXPECT_TRUE(IsEdge(GetMessageViewAt(2)->top_radius()));
  EXPECT_TRUE(IsEdge(GetMessageViewAt(2)->bottom_radius()));

  // The adjacent notification corners should also be rounded like an edge.
  EXPECT_TRUE(IsInner(GetMessageViewAt(1)->top_radius()));
  EXPECT_TRUE(IsEdge(GetMessageViewAt(1)->bottom_radius()));
  EXPECT_TRUE(IsEdge(GetMessageViewAt(3)->top_radius()));
  EXPECT_TRUE(IsEdge(GetMessageViewAt(3)->bottom_radius()));

  // The non-adjacent notification should not change.
  EXPECT_TRUE(IsEdge(GetMessageViewAt(0)->top_radius()));
  EXPECT_TRUE(IsInner(GetMessageViewAt(0)->bottom_radius()));

  // Slide out the middle notification.
  RemoveNotification(id2);
  FinishSlideOutAnimation();
  AnimateUntilIdle();

  // The adjacent notification corners should not be rounded like an edge after
  // the notification fully slid out.
  EXPECT_TRUE(IsEdge(GetMessageViewAt(0)->top_radius()));
  EXPECT_TRUE(IsInner(GetMessageViewAt(0)->bottom_radius()));
  EXPECT_TRUE(IsInner(GetMessageViewAt(1)->top_radius()));
  EXPECT_TRUE(IsInner(GetMessageViewAt(1)->bottom_radius()));
  EXPECT_TRUE(IsInner(GetMessageViewAt(2)->top_radius()));
  EXPECT_TRUE(IsEdge(GetMessageViewAt(2)->bottom_radius()));

  // Test with the next middle notification. Same behavior should happen.
  StartSliding(1);
  EXPECT_TRUE(IsEdge(GetMessageViewAt(0)->top_radius()));
  EXPECT_TRUE(IsEdge(GetMessageViewAt(0)->bottom_radius()));
  EXPECT_TRUE(IsEdge(GetMessageViewAt(1)->top_radius()));
  EXPECT_TRUE(IsEdge(GetMessageViewAt(1)->bottom_radius()));
  EXPECT_TRUE(IsEdge(GetMessageViewAt(2)->top_radius()));
  EXPECT_TRUE(IsEdge(GetMessageViewAt(2)->bottom_radius()));

  // Cancel the slide. Everything goes back to normal.
  GetMessageViewAt(1)->OnSlideChanged(/*in_progress=*/false);
  EXPECT_TRUE(IsEdge(GetMessageViewAt(0)->top_radius()));
  EXPECT_TRUE(IsInner(GetMessageViewAt(0)->bottom_radius()));
  EXPECT_TRUE(IsInner(GetMessageViewAt(1)->top_radius()));
  EXPECT_TRUE(IsInner(GetMessageViewAt(1)->bottom_radius()));
  EXPECT_TRUE(IsInner(GetMessageViewAt(2)->top_radius()));
  EXPECT_TRUE(IsEdge(GetMessageViewAt(2)->bottom_radius()));

  // Slide the top notification.
  StartSliding(0);
  EXPECT_TRUE(IsEdge(GetMessageViewAt(0)->top_radius()));
  EXPECT_TRUE(IsEdge(GetMessageViewAt(0)->bottom_radius()));
  EXPECT_TRUE(IsEdge(GetMessageViewAt(1)->top_radius()));
  EXPECT_TRUE(IsInner(GetMessageViewAt(1)->bottom_radius()));
  EXPECT_TRUE(IsInner(GetMessageViewAt(2)->top_radius()));
  EXPECT_TRUE(IsEdge(GetMessageViewAt(2)->bottom_radius()));

  // Remove the top notification.
  RemoveNotification(id0);
  FinishSlideOutAnimation();
  AnimateUntilIdle();

  // Ensure corners are properly rounded for remaining notifications.
  EXPECT_TRUE(IsEdge(GetMessageViewAt(0)->top_radius()));
  EXPECT_TRUE(IsInner(GetMessageViewAt(0)->bottom_radius()));
  EXPECT_TRUE(IsInner(GetMessageViewAt(1)->top_radius()));
  EXPECT_TRUE(IsEdge(GetMessageViewAt(1)->bottom_radius()));

  // Remove previous to last notification.
  RemoveNotification(id1);
  FinishSlideOutAnimation();
  AnimateUntilIdle();

  // All corners of the remaining notification should be edge-rounded.
  EXPECT_TRUE(IsEdge(GetMessageViewAt(0)->top_radius()));
  EXPECT_TRUE(IsEdge(GetMessageViewAt(0)->bottom_radius()));
}

}  // namespace ash
