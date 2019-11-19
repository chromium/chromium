// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/message_center/unified_message_list_view.h"

#include "ash/public/cpp/ash_features.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/unified/unified_system_tray_model.h"
#include "ash/test/ash_test_base.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "ui/compositor/layer_animator.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/views/message_view.h"
#include "ui/message_center/views/notification_view_md.h"

using message_center::MessageCenter;
using message_center::MessageView;
using message_center::Notification;

namespace ash {

namespace {

class TestNotificationView : public message_center::NotificationViewMD {
 public:
  TestNotificationView(const message_center::Notification& notification)
      : NotificationViewMD(notification) {
    layer()->GetAnimator()->set_preemption_strategy(
        ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
  }

  ~TestNotificationView() override = default;

  // message_center::NotificationViewMD:
  void UpdateCornerRadius(int top_radius, int bottom_radius) override {
    top_radius_ = top_radius;
    bottom_radius_ = bottom_radius;
    message_center::NotificationViewMD::UpdateCornerRadius(top_radius,
                                                           bottom_radius);
  }

  int top_radius() const { return top_radius_; }
  int bottom_radius() const { return bottom_radius_; }

 private:
  int top_radius_ = 0;
  int bottom_radius_ = 0;

  DISALLOW_COPY_AND_ASSIGN(TestNotificationView);
};

class TestUnifiedMessageListView : public UnifiedMessageListView {
 public:
  explicit TestUnifiedMessageListView(UnifiedSystemTrayModel* model)
      : UnifiedMessageListView(nullptr, model) {}

  ~TestUnifiedMessageListView() override = default;

  void set_stacked_notification_count(int stacked_notification_count) {
    stacked_notifications_.clear();
    for (int i = 0; i < stacked_notification_count; i++) {
      std::string id = base::NumberToString(0);
      auto notification = std::make_unique<Notification>(
          message_center::NOTIFICATION_TYPE_BASE_FORMAT, id,
          base::UTF8ToUTF16("test title"), base::UTF8ToUTF16("test message"),
          gfx::Image(), base::string16() /* display_source */, GURL(),
          message_center::NotifierId(), message_center::RichNotificationData(),
          new message_center::NotificationDelegate());

      stacked_notifications_.push_back(notification.get());
    }
  }

  // UnifiedMessageListView:
  message_center::MessageView* CreateMessageView(
      const message_center::Notification& notification) override {
    auto* view = new TestNotificationView(notification);
    view->SetIsNested();
    return view;
  }

  std::vector<message_center::Notification*> GetStackedNotifications()
      const override {
    return stacked_notifications_;
  }

 private:
  std::vector<message_center::Notification*> stacked_notifications_;

  DISALLOW_COPY_AND_ASSIGN(TestUnifiedMessageListView);
};

}  // namespace

class UnifiedMessageListViewTest : public AshTestBase,
                                   public views::ViewObserver {
 public:
  UnifiedMessageListViewTest() = default;
  ~UnifiedMessageListViewTest() override = default;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();
    model_ = std::make_unique<UnifiedSystemTrayModel>(nullptr);
  }

  void TearDown() override {
    message_list_view_.reset();
    model_.reset();
    AshTestBase::TearDown();
  }

  // views::ViewObserver:
  void OnViewPreferredSizeChanged(views::View* view) override {
    view->SetBoundsRect(gfx::Rect(view->GetPreferredSize()));
    view->Layout();
    ++size_changed_count_;
  }

 protected:
  std::string AddNotification(bool pinned = false) {
    std::string id = base::NumberToString(id_++);
    auto notification = std::make_unique<Notification>(
        message_center::NOTIFICATION_TYPE_BASE_FORMAT, id,
        base::UTF8ToUTF16("test title"), base::UTF8ToUTF16("test message"),
        gfx::Image(), base::string16() /* display_source */, GURL(),
        message_center::NotifierId(), message_center::RichNotificationData(),
        new message_center::NotificationDelegate());
    notification->set_pinned(pinned);
    MessageCenter::Get()->AddNotification(std::move(notification));
    return id;
  }

  void OffsetNotificationTimestamp(const std::string& id,
                                   const int milliseconds) {
    MessageCenter::Get()->FindVisibleNotificationById(id)->set_timestamp(
        base::Time::Now() - base::TimeDelta::FromMilliseconds(milliseconds));
  }

  void CreateMessageListView() {
    message_list_view_ =
        std::make_unique<TestUnifiedMessageListView>(model_.get());
    message_list_view_->Init();
    message_list_view_->AddObserver(this);
    OnViewPreferredSizeChanged(message_list_view_.get());
    size_changed_count_ = 0;
  }

  void DestroyMessageListView() { message_list_view_.reset(); }

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

  void AnimateToEnd() {
    message_list_view()->animation_->End();
  }

  void AnimateUntilIdle() {
    while (message_list_view()->animation_->is_animating())
      message_list_view()->animation_->End();
  }

  bool IsAnimating() { return message_list_view()->animation_->is_animating(); }

  TestUnifiedMessageListView* message_list_view() const {
    return message_list_view_.get();
  }

  int size_changed_count() const { return size_changed_count_; }

  ui::LayerAnimator* LayerAnimatorAt(int i) {
    return GetMessageViewAt(i)->layer()->GetAnimator();
  }

 private:
  int id_ = 0;
  int size_changed_count_ = 0;

  std::unique_ptr<UnifiedSystemTrayModel> model_;
  std::unique_ptr<TestUnifiedMessageListView> message_list_view_;

  DISALLOW_COPY_AND_ASSIGN(UnifiedMessageListViewTest);
};

TEST_F(UnifiedMessageListViewTest, Open) {
  auto id0 = AddNotification();
  auto id1 = AddNotification();
  auto id2 = AddNotification();
  CreateMessageListView();

  EXPECT_EQ(3u, message_list_view()->children().size());
  EXPECT_EQ(id0, GetMessageViewAt(0)->notification_id());
  EXPECT_EQ(id1, GetMessageViewAt(1)->notification_id());
  EXPECT_EQ(id2, GetMessageViewAt(2)->notification_id());

  EXPECT_FALSE(GetMessageViewAt(0)->IsExpanded());
  EXPECT_FALSE(GetMessageViewAt(1)->IsExpanded());
  EXPECT_TRUE(GetMessageViewAt(2)->IsExpanded());

  EXPECT_EQ(GetMessageViewBounds(0).bottom(), GetMessageViewBounds(1).y());
  EXPECT_EQ(GetMessageViewBounds(1).bottom(), GetMessageViewBounds(2).y());

  EXPECT_EQ(0, GetMessageViewAt(0)->top_radius());
  EXPECT_EQ(0, GetMessageViewAt(1)->top_radius());
  EXPECT_EQ(0, GetMessageViewAt(2)->top_radius());

  EXPECT_EQ(0, GetMessageViewAt(0)->bottom_radius());
  EXPECT_EQ(0, GetMessageViewAt(1)->bottom_radius());
  EXPECT_EQ(kUnifiedTrayCornerRadius, GetMessageViewAt(2)->bottom_radius());

  EXPECT_LT(0, message_list_view()->GetPreferredSize().height());
}

TEST_F(UnifiedMessageListViewTest, AddNotifications) {
  CreateMessageListView();
  EXPECT_EQ(0, message_list_view()->GetPreferredSize().height());

  auto id0 = AddNotification();
  EXPECT_EQ(1, size_changed_count());
  EXPECT_EQ(1u, message_list_view()->children().size());
  EXPECT_EQ(id0, GetMessageViewAt(0)->notification_id());

  EXPECT_EQ(kUnifiedTrayCornerRadius, GetMessageViewAt(0)->top_radius());
  EXPECT_EQ(kUnifiedTrayCornerRadius, GetMessageViewAt(0)->bottom_radius());

  int previous_height = message_list_view()->GetPreferredSize().height();
  EXPECT_LT(0, previous_height);

  gfx::Rect previous_bounds = GetMessageViewBounds(0);

  auto id1 = AddNotification();
  EXPECT_EQ(2, size_changed_count());
  EXPECT_EQ(2u, message_list_view()->children().size());
  EXPECT_EQ(id1, GetMessageViewAt(1)->notification_id());

  EXPECT_LT(previous_height, message_list_view()->GetPreferredSize().height());
  // 1dip larger because now it has separator border.
  previous_bounds.Inset(gfx::Insets(0, 0, -1, 0));
  EXPECT_EQ(previous_bounds, GetMessageViewBounds(0));
  EXPECT_EQ(GetMessageViewBounds(0).bottom(), GetMessageViewBounds(1).y());

  // The top radius is zero because the stacking bar is shown.
  EXPECT_EQ(0, GetMessageViewAt(0)->top_radius());
  EXPECT_EQ(0, GetMessageViewAt(1)->top_radius());

  EXPECT_EQ(0, GetMessageViewAt(0)->bottom_radius());
  EXPECT_EQ(kUnifiedTrayCornerRadius, GetMessageViewAt(1)->bottom_radius());
}

TEST_F(UnifiedMessageListViewTest, RemoveNotification) {
  auto id0 = AddNotification();
  auto id1 = AddNotification();

  CreateMessageListView();
  int previous_height = message_list_view()->GetPreferredSize().height();

  EXPECT_EQ(2u, message_list_view()->children().size());
  EXPECT_EQ(0, GetMessageViewAt(0)->top_radius());
  EXPECT_EQ(0, GetMessageViewAt(0)->bottom_radius());

  gfx::Rect previous_bounds = GetMessageViewBounds(0);
  MessageCenter::Get()->RemoveNotification(id0, true /* by_user */);
  FinishSlideOutAnimation();
  AnimateUntilIdle();
  EXPECT_EQ(1u, message_list_view()->children().size());
  EXPECT_EQ(previous_bounds.y(), GetMessageViewBounds(0).y());
  EXPECT_LT(0, message_list_view()->GetPreferredSize().height());
  EXPECT_GT(previous_height, message_list_view()->GetPreferredSize().height());

  EXPECT_EQ(kUnifiedTrayCornerRadius, GetMessageViewAt(0)->top_radius());
  EXPECT_EQ(kUnifiedTrayCornerRadius, GetMessageViewAt(0)->bottom_radius());

  MessageCenter::Get()->RemoveNotification(id1, true /* by_user */);
  FinishSlideOutAnimation();
  AnimateUntilIdle();
  EXPECT_EQ(0u, message_list_view()->children().size());
  EXPECT_EQ(0, message_list_view()->GetPreferredSize().height());
}

TEST_F(UnifiedMessageListViewTest, CollapseOlderNotifications) {
  AddNotification();
  CreateMessageListView();
  EXPECT_TRUE(GetMessageViewAt(0)->IsExpanded());

  AddNotification();
  EXPECT_FALSE(GetMessageViewAt(0)->IsExpanded());
  EXPECT_TRUE(GetMessageViewAt(1)->IsExpanded());

  AddNotification();
  EXPECT_FALSE(GetMessageViewAt(0)->IsExpanded());
  EXPECT_FALSE(GetMessageViewAt(1)->IsExpanded());
  EXPECT_TRUE(GetMessageViewAt(2)->IsExpanded());

  GetMessageViewAt(1)->SetExpanded(true);
  GetMessageViewAt(1)->SetManuallyExpandedOrCollapsed(true);

  AddNotification();
  EXPECT_FALSE(GetMessageViewAt(0)->IsExpanded());
  EXPECT_TRUE(GetMessageViewAt(1)->IsExpanded());
  EXPECT_FALSE(GetMessageViewAt(2)->IsExpanded());
  EXPECT_TRUE(GetMessageViewAt(3)->IsExpanded());
}

TEST_F(UnifiedMessageListViewTest, RemovingNotificationAnimation) {
  auto id0 = AddNotification();
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
  // Now it lost separator border.
  bounds1.Inset(gfx::Insets(0, 0, 1, 0));
  EXPECT_EQ(bounds0, GetMessageViewBounds(0));
  EXPECT_EQ(bounds1, GetMessageViewBounds(1));

  MessageCenter::Get()->RemoveNotification(id2, true /* by_user */);
  FinishSlideOutAnimation();
  AnimateToEnd();
  EXPECT_GT(previous_height, message_list_view()->GetPreferredSize().height());
  previous_height = message_list_view()->GetPreferredSize().height();
  // Now it lost separator border.
  bounds0.Inset(gfx::Insets(0, 0, 1, 0));
  EXPECT_EQ(bounds0, GetMessageViewBounds(0));

  MessageCenter::Get()->RemoveNotification(id0, true /* by_user */);
  FinishSlideOutAnimation();
  AnimateToEnd();
  EXPECT_GT(previous_height, message_list_view()->GetPreferredSize().height());
  previous_height = message_list_view()->GetPreferredSize().height();

  EXPECT_EQ(0, message_list_view()->GetPreferredSize().height());
}

TEST_F(UnifiedMessageListViewTest, ResetAnimation) {
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

TEST_F(UnifiedMessageListViewTest, KeepManuallyExpanded) {
  AddNotification();
  AddNotification();
  CreateMessageListView();

  EXPECT_FALSE(GetMessageViewAt(0)->IsExpanded());
  EXPECT_TRUE(GetMessageViewAt(1)->IsExpanded());
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
  AddNotification();
  CreateMessageListView();

  // Confirm the new notification isn't affected & others are still kept.
  EXPECT_TRUE(GetMessageViewAt(0)->IsExpanded());
  EXPECT_FALSE(GetMessageViewAt(1)->IsExpanded());
  EXPECT_TRUE(GetMessageViewAt(2)->IsExpanded());
  EXPECT_TRUE(GetMessageViewAt(0)->IsManuallyExpandedOrCollapsed());
  EXPECT_TRUE(GetMessageViewAt(1)->IsManuallyExpandedOrCollapsed());
  EXPECT_FALSE(GetMessageViewAt(2)->IsManuallyExpandedOrCollapsed());
}

TEST_F(UnifiedMessageListViewTest, ClearAllWithOnlyVisibleNotifications) {
  AddNotification();
  AddNotification();
  CreateMessageListView();

  EXPECT_EQ(2u, message_list_view()->children().size());
  int previous_height = message_list_view()->GetPreferredSize().height();
  gfx::Rect previous_bounds = GetMessageViewBounds(0);

  message_list_view()->ClearAllWithAnimation();
  AnimateToMiddle();
  EXPECT_LT(previous_bounds.x(), GetMessageViewBounds(0).x());
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

TEST_F(UnifiedMessageListViewTest, ClearAllWithStackingNotifications) {
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

TEST_F(UnifiedMessageListViewTest, ClearAllClosedInTheMiddle) {
  AddNotification();
  AddNotification();
  AddNotification();
  CreateMessageListView();

  message_list_view()->ClearAllWithAnimation();
  AnimateToMiddle();

  DestroyMessageListView();
  EXPECT_TRUE(MessageCenter::Get()->GetVisibleNotifications().empty());
}

TEST_F(UnifiedMessageListViewTest, ClearAllInterrupted) {
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

TEST_F(UnifiedMessageListViewTest, ClearAllWithPinnedNotifications) {
  AddNotification(true /* pinned */);
  AddNotification();
  AddNotification();
  CreateMessageListView();

  message_list_view()->ClearAllWithAnimation();
  AnimateUntilIdle();
  EXPECT_EQ(1u, message_list_view()->children().size());
}

TEST_F(UnifiedMessageListViewTest, UserSwipesAwayNotification) {
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

TEST_F(UnifiedMessageListViewTest, InitInSortedOrder) {
  // MessageViews should be ordered, from top down: [ id1, id2, id0 ].
  auto id0 = AddNotification(true /* pinned */);
  OffsetNotificationTimestamp(id0, 2000 /* milliseconds */);
  auto id1 = AddNotification();
  OffsetNotificationTimestamp(id1, 1000 /* milliseconds */);
  auto id2 = AddNotification();
  CreateMessageListView();

  EXPECT_EQ(3u, message_list_view()->children().size());
  EXPECT_EQ(id1, GetMessageViewAt(0)->notification_id());
  EXPECT_EQ(id2, GetMessageViewAt(1)->notification_id());
  EXPECT_EQ(id0, GetMessageViewAt(2)->notification_id());
}

TEST_F(UnifiedMessageListViewTest, NotificationAddedInSortedOrder) {
  auto id0 = AddNotification(true /* pinned */);
  OffsetNotificationTimestamp(id0, 3000 /* milliseconds */);
  auto id1 = AddNotification();
  OffsetNotificationTimestamp(id1, 2000 /* milliseconds */);
  auto id2 = AddNotification();
  OffsetNotificationTimestamp(id2, 1000 /* milliseconds */);
  CreateMessageListView();

  // New pinned notification should be added to the end.
  auto id3 = AddNotification(true /* pinned */);
  EXPECT_EQ(4u, message_list_view()->children().size());
  EXPECT_EQ(id3, GetMessageViewAt(3)->notification_id());

  // New non-pinned notification should be added before pinned notifications.
  auto id4 = AddNotification();
  EXPECT_EQ(5u, message_list_view()->children().size());

  EXPECT_EQ(id1, GetMessageViewAt(0)->notification_id());
  EXPECT_EQ(id2, GetMessageViewAt(1)->notification_id());
  EXPECT_EQ(id4, GetMessageViewAt(2)->notification_id());
  EXPECT_EQ(id0, GetMessageViewAt(3)->notification_id());
  EXPECT_EQ(id3, GetMessageViewAt(4)->notification_id());
}

}  // namespace ash
