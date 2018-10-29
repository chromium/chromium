// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/message_center/message_center_view.h"

#include <map>
#include <memory>
#include <utility>

#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/ash_pref_names.h"
#include "ash/session/session_controller.h"
#include "ash/shell.h"
#include "ash/system/message_center/ash_message_center_lock_screen_controller.h"
#include "ash/system/message_center/message_center_button_bar.h"
#include "ash/system/message_center/message_center_style.h"
#include "ash/system/message_center/message_list_view.h"
#include "ash/test/ash_test_base.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "components/prefs/pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/message_center/fake_message_center.h"
#include "ui/message_center/notification_list.h"
#include "ui/message_center/public/cpp/message_center_constants.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_types.h"
#include "ui/message_center/views/notification_view.h"
#include "ui/views/animation/bounds_animator_observer.h"
#include "ui/views/widget/widget.h"

namespace ash {

using message_center::FakeMessageCenter;
using message_center::MessageCenter;
using message_center::MessageView;
using message_center::Notification;
using message_center::NotificationList;
using message_center::NotificationView;
using message_center::NotifierId;
using message_center::NOTIFICATION_TYPE_SIMPLE;

namespace {

const char* kNotificationId1 = "notification id 1";
const char* kNotificationId2 = "notification id 2";

// Plain button bar height (56)
const int kLockedMessageCenterViewHeight = 56;

// Plain button bar height (56) + Empty view (96)
const int kEmptyMessageCenterViewHeight = 152;

/* Types **********************************************************************/

enum CallType { GET_PREFERRED_SIZE, GET_HEIGHT_FOR_WIDTH, LAYOUT };

class DummyEvent : public ui::Event {
 public:
  DummyEvent() : Event(ui::ET_UNKNOWN, base::TimeTicks(), 0) {}
  ~DummyEvent() override = default;
};

/* Instrumented/Mock NotificationView subclass ********************************/

class MockNotificationView : public NotificationView {
 public:
  class Test {
   public:
    virtual void RegisterCall(CallType type) = 0;
  };

  explicit MockNotificationView(const Notification& notification, Test* test);
  ~MockNotificationView() override;

  gfx::Size CalculatePreferredSize() const override;
  int GetHeightForWidth(int w) const override;
  void Layout() override;

 private:
  Test* test_;

  DISALLOW_COPY_AND_ASSIGN(MockNotificationView);
};

MockNotificationView::MockNotificationView(const Notification& notification,
                                           Test* test)
    : NotificationView(notification), test_(test) {}

MockNotificationView::~MockNotificationView() = default;

gfx::Size MockNotificationView::CalculatePreferredSize() const {
  test_->RegisterCall(GET_PREFERRED_SIZE);
  DCHECK(child_count() > 0);
  return NotificationView::CalculatePreferredSize();
}

int MockNotificationView::GetHeightForWidth(int width) const {
  test_->RegisterCall(GET_HEIGHT_FOR_WIDTH);
  DCHECK(child_count() > 0);
  return NotificationView::GetHeightForWidth(width);
}

void MockNotificationView::Layout() {
  test_->RegisterCall(LAYOUT);
  DCHECK(child_count() > 0);
  NotificationView::Layout();
}

class FakeMessageCenterImpl : public FakeMessageCenter {
 public:
  const NotificationList::Notifications& GetVisibleNotifications() override {
    return visible_notifications_;
  }
  void SetVisibleNotifications(NotificationList::Notifications notifications) {
    visible_notifications_ = notifications;
  }
  void RemoveAllNotifications(bool by_user, RemoveType type) override {
    if (type == RemoveType::NON_PINNED)
      remove_all_closable_notification_called_ = true;
  }
  bool remove_all_closable_notification_called_ = false;
  NotificationList::Notifications visible_notifications_;
};

// This is the class we are testing, but we need to override some functions
// in it, hence MockMessageCenterView.
class MockMessageCenterView : public MessageCenterView {
 public:
  MockMessageCenterView(MessageCenter* message_center, int max_height);

  bool SetRepositionTarget() override;

  void PreferredSizeChanged() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(MockMessageCenterView);
};

MockMessageCenterView::MockMessageCenterView(MessageCenter* message_center,
                                             int max_height)
    : MessageCenterView(message_center, max_height) {}

// Always say that the current reposition session is still active, by
// returning true. Normally the reposition session is set based on where the
// mouse is hovering.
bool MockMessageCenterView::SetRepositionTarget() {
  return true;
}

void MockMessageCenterView::PreferredSizeChanged() {
  SetSize(GetPreferredSize());
  MessageCenterView::PreferredSizeChanged();
}

}  // namespace

/* Test fixture ***************************************************************/

class MessageCenterViewTest : public AshTestBase,
                              public testing::WithParamInterface<bool>,
                              public MockNotificationView::Test,
                              views::BoundsAnimatorObserver {
 public:
  // Expose the private enum class MessageCenter::Mode for this test.
  using Mode = MessageCenterView::Mode;

  MessageCenterViewTest();
  ~MessageCenterViewTest() override;

  void SetUp() override;
  void TearDown() override;

  NotificationList::Notifications Notifications();
  MessageCenterView* GetMessageCenterView();
  MessageListView* GetMessageListView();
  FakeMessageCenterImpl* GetMessageCenter() const;
  MessageView* GetNotificationView(const std::string& id);
  views::BoundsAnimator* GetAnimator();
  int GetNotificationCount();
  int GetCallCount(CallType type);
  int GetCalculatedMessageListViewHeight();
  void SetLockedState(bool locked);
  Mode GetMessageCenterViewInternalMode();
  void AddNotification(std::unique_ptr<Notification> notification);
  void RemoveNotification(const std::string& notification_id, bool by_user);
  void UpdateNotification(const std::string& notification_id,
                          std::unique_ptr<Notification> notification);

  // Overridden from MockNotificationView::Test
  void RegisterCall(CallType type) override;

  // Overridden from views::BoundsAnimatorObserver
  void OnBoundsAnimatorProgressed(views::BoundsAnimator* animator) override{};
  void OnBoundsAnimatorDone(views::BoundsAnimator* animator) override;

  void FireOnMouseExitedEvent();

  void LogBounds(int depth, views::View* view);

  MessageCenterButtonBar* GetButtonBar() const;

  void RemoveDefaultNotifications();

  void WaitForAnimationToFinish();

  void SetLockScreenNotificationsEnabled();

 private:
  views::View* MakeParent(views::View* child1, views::View* child2);

  // The ownership map of notifications; the key is the id.
  std::map<std::string, std::unique_ptr<Notification>> notifications_;
  std::unique_ptr<views::Widget> widget_;
  std::unique_ptr<MockMessageCenterView> message_center_view_;
  std::unique_ptr<FakeMessageCenterImpl> message_center_;
  std::map<CallType, int> callCounts_;

  std::unique_ptr<base::RunLoop> run_loop_;

  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(MessageCenterViewTest);
};

MessageCenterViewTest::MessageCenterViewTest() = default;

MessageCenterViewTest::~MessageCenterViewTest() = default;

void MessageCenterViewTest::SetUp() {
  AshTestBase::SetUp();

  MessageCenterView::disable_animation_for_testing = true;
  message_center_.reset(new FakeMessageCenterImpl());

  // Create a dummy notification.
  std::unique_ptr<Notification> notification1 = std::make_unique<Notification>(
      NOTIFICATION_TYPE_SIMPLE, std::string(kNotificationId1),
      base::UTF8ToUTF16("title"), base::UTF8ToUTF16("message1"), gfx::Image(),
      base::UTF8ToUTF16("display source"), GURL(),
      NotifierId(NotifierId::APPLICATION, "extension_id"),
      message_center::RichNotificationData(), nullptr);

  std::unique_ptr<Notification> notification2 = std::make_unique<Notification>(
      NOTIFICATION_TYPE_SIMPLE, std::string(kNotificationId2),
      base::UTF8ToUTF16("title2"), base::UTF8ToUTF16("message2"), gfx::Image(),
      base::UTF8ToUTF16("display source"), GURL(),
      NotifierId(NotifierId::APPLICATION, "extension_id"),
      message_center::RichNotificationData(), nullptr);

  // ...and a list for it.
  notifications_[std::string(kNotificationId1)] = std::move(notification1);
  notifications_[std::string(kNotificationId2)] = std::move(notification2);
  NotificationList::Notifications notifications = Notifications();
  message_center_->SetVisibleNotifications(notifications);

  // Then create a new MockMessageCenterView with that single notification.
  message_center_view_.reset(
      new MockMessageCenterView(message_center_.get(), 600));
  GetMessageCenterView()->SetBounds(0, 0, 380, 100);
  message_center_view_->SetNotifications(notifications);
  message_center_view_->set_owned_by_client();

  widget_.reset(new views::Widget());
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_POPUP);
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.bounds = gfx::Rect(50, 50, 650, 650);
  params.context = CurrentContext();
  widget_->Init(params);
  views::View* root = widget_->GetRootView();
  root->AddChildView(message_center_view_.get());
  widget_->Show();
  widget_->Activate();

  GetAnimator()->AddObserver(this);

  WaitForAnimationToFinish();
}

void MessageCenterViewTest::TearDown() {
  GetAnimator()->RemoveObserver(this);
  widget_->CloseNow();
  widget_.reset();
  message_center_view_.reset();
  notifications_.clear();
  AshTestBase::TearDown();
}

NotificationList::Notifications MessageCenterViewTest::Notifications() {
  NotificationList::Notifications result;
  for (const auto& notification_pair : notifications_)
    result.insert(notification_pair.second.get());

  return result;
}

MessageCenterView* MessageCenterViewTest::GetMessageCenterView() {
  return message_center_view_.get();
}

MessageListView* MessageCenterViewTest::GetMessageListView() {
  return message_center_view_->message_list_view_.get();
}

FakeMessageCenterImpl* MessageCenterViewTest::GetMessageCenter() const {
  return message_center_.get();
}

MessageView* MessageCenterViewTest::GetNotificationView(const std::string& id) {
  return GetMessageListView()->GetNotificationById(id).second;
}

int MessageCenterViewTest::GetCalculatedMessageListViewHeight() {
  return GetMessageListView()->GetHeightForWidth(GetMessageListView()->width());
}

MessageCenterViewTest::Mode
MessageCenterViewTest::GetMessageCenterViewInternalMode() {
  return GetMessageCenterView()->mode_;
}

views::BoundsAnimator* MessageCenterViewTest::GetAnimator() {
  return &GetMessageListView()->animator_;
}

int MessageCenterViewTest::GetNotificationCount() {
  return 2;
}

int MessageCenterViewTest::GetCallCount(CallType type) {
  return callCounts_[type];
}

void MessageCenterViewTest::SetLockedState(bool locked) {
  GetMessageCenterView()->OnLockStateChanged(locked);
}

void MessageCenterViewTest::AddNotification(
    std::unique_ptr<Notification> notification) {
  std::string notification_id = notification->id();
  notifications_[notification_id] = std::move(notification);
  message_center_->SetVisibleNotifications(Notifications());
  message_center_view_->OnNotificationAdded(notification_id);
}

void MessageCenterViewTest::UpdateNotification(
    const std::string& notification_id,
    std::unique_ptr<Notification> notification) {
  DCHECK_EQ(notification_id, notification->id());
  notifications_[notification_id] = std::move(notification);

  message_center_->SetVisibleNotifications(Notifications());
  message_center_view_->OnNotificationUpdated(notification_id);
}

void MessageCenterViewTest::RemoveNotification(
    const std::string& notification_id,
    bool by_user) {
  notifications_.erase(notification_id);

  message_center_->SetVisibleNotifications(Notifications());
  message_center_view_->OnNotificationRemoved(notification_id, by_user);
}

void MessageCenterViewTest::RegisterCall(CallType type) {
  callCounts_[type] += 1;
}

void MessageCenterViewTest::OnBoundsAnimatorDone(
    views::BoundsAnimator* animator) {
  if (run_loop_)
    run_loop_->QuitWhenIdle();
}

void MessageCenterViewTest::FireOnMouseExitedEvent() {
  ui::MouseEvent dummy_event(
      ui::ET_MOUSE_EXITED /* type */, gfx::Point(0, 0) /* location */,
      gfx::Point(0, 0) /* root location */, base::TimeTicks() /* time_stamp */,
      0 /* flags */, 0 /*changed_button_flags */);
  message_center_view_->OnMouseExited(dummy_event);
}

void MessageCenterViewTest::LogBounds(int depth, views::View* view) {
  base::string16 inset;
  for (int i = 0; i < depth; ++i)
    inset.append(base::UTF8ToUTF16("  "));
  gfx::Rect bounds = view->bounds();
  DVLOG(0) << inset << bounds.width() << " x " << bounds.height() << " @ "
           << bounds.x() << ", " << bounds.y();
  for (int i = 0; i < view->child_count(); ++i)
    LogBounds(depth + 1, view->child_at(i));
}

MessageCenterButtonBar* MessageCenterViewTest::GetButtonBar() const {
  return message_center_view_->button_bar_;
}

void MessageCenterViewTest::RemoveDefaultNotifications() {
  RemoveNotification(kNotificationId1, false);
  RemoveNotification(kNotificationId2, false);
}

void MessageCenterViewTest::WaitForAnimationToFinish() {
  while (GetAnimator()->IsAnimating()) {
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
  }
}

void MessageCenterViewTest::SetLockScreenNotificationsEnabled() {
  scoped_feature_list_.InitAndEnableFeature(features::kLockScreenNotifications);

  PrefService* user_prefs =
      Shell::Get()->session_controller()->GetActivePrefService();
  user_prefs->SetString(prefs::kMessageCenterLockScreenMode,
                        prefs::kMessageCenterLockScreenModeShow);

  ASSERT_TRUE(AshMessageCenterLockScreenController::IsEnabled());
}

/* Unit tests *****************************************************************/

TEST_F(MessageCenterViewTest, CallTest) {
  // Verify that this didn't generate more than 2 Layout() call per descendant
  // NotificationView or more than a total of 20 GetPreferredSize() and
  // GetHeightForWidth() calls per descendant NotificationView. 20 is a very
  // large number corresponding to the current reality. That number will be
  // ratcheted down over time as the code improves.
  EXPECT_LE(GetCallCount(LAYOUT), GetNotificationCount() * 2);
  EXPECT_LE(
      GetCallCount(GET_PREFERRED_SIZE) + GetCallCount(GET_HEIGHT_FOR_WIDTH),
      GetNotificationCount() * 20);
}

TEST_F(MessageCenterViewTest, Size) {
  EXPECT_EQ(2, GetMessageListView()->child_count());
  EXPECT_EQ(GetMessageListView()->height(),
            GetCalculatedMessageListViewHeight());

  int width =
      GetMessageListView()->width() - GetMessageListView()->GetInsets().width();
  EXPECT_EQ(
      GetMessageListView()->height(),
      GetNotificationView(kNotificationId1)->GetHeightForWidth(width) +
          message_center::kMarginBetweenItemsInList +
          GetNotificationView(kNotificationId2)->GetHeightForWidth(width) +
          GetMessageListView()->GetInsets().height());
}

// TODO(tetsui): The test is broken because there's no guarantee anymore that
// height would change after setting longer message, as NotificationViewMD
// implements collapse / expand functionality of long message.
TEST_F(MessageCenterViewTest, DISABLED_SizeAfterUpdate) {
  EXPECT_EQ(2, GetMessageListView()->child_count());
  int width =
      GetMessageListView()->width() - GetMessageListView()->GetInsets().width();

  std::unique_ptr<Notification> notification = std::make_unique<Notification>(
      NOTIFICATION_TYPE_SIMPLE, std::string(kNotificationId2),
      base::UTF8ToUTF16("title2"),
      base::UTF8ToUTF16("message\nwhich\nis\nvertically\nlong\n."),
      gfx::Image(), base::UTF8ToUTF16("display source"), GURL(),
      NotifierId(NotifierId::APPLICATION, "extension_id"),
      message_center::RichNotificationData(), nullptr);

  EXPECT_EQ(
      GetMessageListView()->height(),
      GetNotificationView(kNotificationId1)->GetHeightForWidth(width) +
          message_center::kMarginBetweenItemsInList +
          GetNotificationView(kNotificationId2)->GetHeightForWidth(width) +
          GetMessageListView()->GetInsets().height());

  int previous_height = GetMessageListView()->height();

  UpdateNotification(kNotificationId2, std::move(notification));

  WaitForAnimationToFinish();

  EXPECT_EQ(2, GetMessageListView()->child_count());
  EXPECT_EQ(GetMessageListView()->height(),
            GetCalculatedMessageListViewHeight());

  // The size must be changed, since the new string is longer than the old one.
  EXPECT_NE(previous_height, GetMessageListView()->height());

  EXPECT_EQ(
      GetMessageListView()->height(),
      GetNotificationView(kNotificationId1)->GetHeightForWidth(width) +
          message_center::kMarginBetweenItemsInList +
          GetNotificationView(kNotificationId2)->GetHeightForWidth(width) +
          GetMessageListView()->GetInsets().height());
}

TEST_F(MessageCenterViewTest, SizeAfterUpdateBelowWithRepositionTarget) {
  EXPECT_EQ(2, GetMessageListView()->child_count());
  // Make sure that notification 2 is placed above notification 1.
  EXPECT_LT(GetNotificationView(kNotificationId2)->GetBoundsInScreen().y(),
            GetNotificationView(kNotificationId1)->GetBoundsInScreen().y());

  GetMessageListView()->SetRepositionTarget(
      GetNotificationView(kNotificationId1)->GetBoundsInScreen());

  std::unique_ptr<Notification> notification = std::make_unique<Notification>(
      NOTIFICATION_TYPE_SIMPLE, std::string(kNotificationId2),
      base::UTF8ToUTF16("title2"),
      base::UTF8ToUTF16("message\nwhich\nis\nvertically\nlong\n."),
      gfx::Image(), base::UTF8ToUTF16("display source"), GURL(),
      NotifierId(NotifierId::APPLICATION, "extension_id"),
      message_center::RichNotificationData(), nullptr);
  UpdateNotification(kNotificationId2, std::move(notification));

  WaitForAnimationToFinish();

  int width =
      GetMessageListView()->width() - GetMessageListView()->GetInsets().width();
  EXPECT_EQ(
      GetMessageListView()->height(),
      GetNotificationView(kNotificationId1)->GetHeightForWidth(width) +
          message_center::kMarginBetweenItemsInList +
          GetNotificationView(kNotificationId2)->GetHeightForWidth(width) +
          GetMessageListView()->GetInsets().height());
}

TEST_F(MessageCenterViewTest, SizeAfterUpdateOfRepositionTarget) {
  EXPECT_EQ(2, GetMessageListView()->child_count());
  // Make sure that notification 2 is placed above notification 1.
  EXPECT_LT(GetNotificationView(kNotificationId2)->GetBoundsInScreen().y(),
            GetNotificationView(kNotificationId1)->GetBoundsInScreen().y());

  GetMessageListView()->SetRepositionTarget(
      GetNotificationView(kNotificationId1)->GetBoundsInScreen());

  std::unique_ptr<Notification> notification = std::make_unique<Notification>(
      NOTIFICATION_TYPE_SIMPLE, std::string(kNotificationId1),
      base::UTF8ToUTF16("title2"),
      base::UTF8ToUTF16("message\nwhich\nis\nvertically\nlong\n."),
      gfx::Image(), base::UTF8ToUTF16("display source"), GURL(),
      NotifierId(NotifierId::APPLICATION, "extension_id"),
      message_center::RichNotificationData(), nullptr);
  UpdateNotification(kNotificationId1, std::move(notification));

  WaitForAnimationToFinish();

  int width =
      GetMessageListView()->width() - GetMessageListView()->GetInsets().width();
  EXPECT_EQ(
      GetMessageListView()->height(),
      GetNotificationView(kNotificationId1)->GetHeightForWidth(width) +
          message_center::kMarginBetweenItemsInList +
          GetNotificationView(kNotificationId2)->GetHeightForWidth(width) +
          GetMessageListView()->GetInsets().height());
}

TEST_F(MessageCenterViewTest, SizeAfterRemove) {
  int original_height = GetMessageListView()->height();
  EXPECT_EQ(2, GetMessageListView()->child_count());
  RemoveNotification(kNotificationId1, false);

  WaitForAnimationToFinish();

  EXPECT_EQ(1, GetMessageListView()->child_count());

  EXPECT_FALSE(GetNotificationView(kNotificationId1));
  EXPECT_TRUE(GetNotificationView(kNotificationId2));
  EXPECT_EQ(original_height, GetMessageListView()->height());
}

TEST_F(MessageCenterViewTest, PositionAfterUpdate) {
  // Make sure that the notification 2 is placed above the notification 1.
  EXPECT_LT(GetNotificationView(kNotificationId2)->GetBoundsInScreen().y(),
            GetNotificationView(kNotificationId1)->GetBoundsInScreen().y());

  int previous_vertical_pos_from_bottom =
      GetMessageListView()->height() -
      GetNotificationView(kNotificationId1)->GetBoundsInScreen().y();
  GetMessageListView()->SetRepositionTarget(
      GetNotificationView(kNotificationId1)->parent()->bounds());

  std::unique_ptr<Notification> notification = std::make_unique<Notification>(
      NOTIFICATION_TYPE_SIMPLE, std::string(kNotificationId2),
      base::UTF8ToUTF16("title2"),
      base::UTF8ToUTF16("message\nwhich\nis\nvertically\nlong\n."),
      gfx::Image(), base::UTF8ToUTF16("display source"), GURL(),
      NotifierId(NotifierId::APPLICATION, "extension_id"),
      message_center::RichNotificationData(), nullptr);
  UpdateNotification(kNotificationId2, std::move(notification));

  WaitForAnimationToFinish();

  // The vertical position of the target from bottom should be kept over change.
  int current_vertical_pos_from_bottom =
      GetMessageListView()->height() -
      GetNotificationView(kNotificationId1)->GetBoundsInScreen().y();
  EXPECT_EQ(previous_vertical_pos_from_bottom,
            current_vertical_pos_from_bottom);
}

TEST_F(MessageCenterViewTest, PositionAfterRemove) {
  // Make sure that the notification 2 is placed above the notification 1.
  EXPECT_LT(GetNotificationView(kNotificationId2)->GetBoundsInScreen().y(),
            GetNotificationView(kNotificationId1)->GetBoundsInScreen().y());

  GetMessageListView()->SetRepositionTarget(
      GetNotificationView(kNotificationId2)->parent()->bounds());
  int previous_height = GetMessageListView()->height();
  int previous_notification2_y =
      GetNotificationView(kNotificationId2)->GetBoundsInScreen().y();

  EXPECT_EQ(2, GetMessageListView()->child_count());
  RemoveNotification(kNotificationId2, false);

  WaitForAnimationToFinish();

  EXPECT_EQ(1, GetMessageListView()->child_count());

  // Confirm that notification 1 is moved up to the place on which the
  // notification 2 was.
  EXPECT_EQ(previous_notification2_y,
            GetNotificationView(kNotificationId1)->GetBoundsInScreen().y());
  // The size should be kept.
  EXPECT_EQ(previous_height, GetMessageListView()->height());

  // Notification 2 is successfully removed.
  EXPECT_FALSE(GetNotificationView(kNotificationId2));
  EXPECT_TRUE(GetNotificationView(kNotificationId1));

  // Mouse cursor moves out of the message center. This resets the reposition
  // target in the message list.
  FireOnMouseExitedEvent();

  // The height should be kept even after the cursor moved out.
  EXPECT_EQ(previous_height, GetMessageListView()->height());
}

TEST_F(MessageCenterViewTest, CloseButton) {
  views::Button* close_button = GetButtonBar()->GetCloseAllButtonForTest();
  EXPECT_NE(nullptr, close_button);

  GetButtonBar()->ButtonPressed(close_button, DummyEvent());
  WaitForAnimationToFinish();
  EXPECT_TRUE(GetMessageCenter()->remove_all_closable_notification_called_);
}

TEST_F(MessageCenterViewTest, CloseButtonEnablity) {
  views::Button* close_button = GetButtonBar()->GetCloseAllButtonForTest();
  EXPECT_NE(nullptr, close_button);

  // There should be 2 non-pinned notifications.
  EXPECT_EQ(2u, GetMessageCenter()->GetVisibleNotifications().size());
  EXPECT_TRUE(close_button->enabled());

  RemoveNotification(kNotificationId1, false);
  WaitForAnimationToFinish();

  // There should be 1 non-pinned notification.
  EXPECT_EQ(1u, GetMessageCenter()->GetVisibleNotifications().size());
  EXPECT_TRUE(close_button->enabled());

  RemoveNotification(kNotificationId2, false);
  WaitForAnimationToFinish();

  // There should be no notification.
  EXPECT_EQ(0u, GetMessageCenter()->GetVisibleNotifications().size());
  EXPECT_FALSE(close_button->enabled());

  // Non-pinned version of notification #1
  Notification normal_notification1(
      NOTIFICATION_TYPE_SIMPLE, std::string(kNotificationId1),
      base::UTF8ToUTF16("title2"),
      base::UTF8ToUTF16("message\nwhich\nis\nvertically\nlong\n."),
      gfx::Image(), base::UTF8ToUTF16("display source"), GURL(),
      NotifierId(NotifierId::APPLICATION, "extension_id"),
      message_center::RichNotificationData(), NULL);

  // Pinned version of notification #1
  Notification pinned_notification1(
      NOTIFICATION_TYPE_SIMPLE, std::string(kNotificationId1),
      base::UTF8ToUTF16("title2"),
      base::UTF8ToUTF16("message\nwhich\nis\nvertically\nlong\n."),
      gfx::Image(), base::UTF8ToUTF16("display source"), GURL(),
      NotifierId(NotifierId::APPLICATION, "extension_id"),
      message_center::RichNotificationData(), NULL);
  pinned_notification1.set_pinned(true);

  // Pinned notification #2
  Notification pinned_notification2(
      NOTIFICATION_TYPE_SIMPLE, std::string(kNotificationId2),
      base::UTF8ToUTF16("title2"),
      base::UTF8ToUTF16("message\nwhich\nis\nvertically\nlong\n."),
      gfx::Image(), base::UTF8ToUTF16("display source"), GURL(),
      NotifierId(NotifierId::APPLICATION, "extension_id"),
      message_center::RichNotificationData(), NULL);
  pinned_notification2.set_pinned(true);

  AddNotification(std::make_unique<Notification>(normal_notification1));

  // There should be 1 non-pinned notification.
  EXPECT_EQ(1u, GetMessageCenter()->GetVisibleNotifications().size());
  EXPECT_TRUE(close_button->enabled());

  UpdateNotification(kNotificationId1,
                     std::make_unique<Notification>(pinned_notification1));

  // There should be 1 pinned notification.
  EXPECT_EQ(1u, GetMessageCenter()->GetVisibleNotifications().size());
  EXPECT_FALSE(close_button->enabled());

  // Adds 1 pinned notification.
  AddNotification(std::make_unique<Notification>(pinned_notification2));

  // There should be 1 pinned notification.
  EXPECT_EQ(2u, GetMessageCenter()->GetVisibleNotifications().size());
  EXPECT_FALSE(close_button->enabled());

  UpdateNotification(kNotificationId1,
                     std::make_unique<Notification>(normal_notification1));

  // There should be 1 normal notification and 1 pinned notification.
  EXPECT_EQ(2u, GetMessageCenter()->GetVisibleNotifications().size());
  EXPECT_TRUE(close_button->enabled());

  RemoveNotification(kNotificationId1, false);

  // There should be 1 pinned notification.
  EXPECT_EQ(1u, GetMessageCenter()->GetVisibleNotifications().size());
  EXPECT_FALSE(close_button->enabled());

  RemoveNotification(kNotificationId2, false);

  // There should be no notification.
  EXPECT_EQ(0u, GetMessageCenter()->GetVisibleNotifications().size());
  EXPECT_FALSE(close_button->enabled());

  AddNotification(std::make_unique<Notification>(pinned_notification2));

  // There should be 1 pinned notification.
  EXPECT_EQ(1u, GetMessageCenter()->GetVisibleNotifications().size());
  EXPECT_FALSE(close_button->enabled());
}

TEST_F(MessageCenterViewTest, CheckModeWithSettingsVisibleAndHidden) {
  // Check the initial state.
  EXPECT_EQ(Mode::NOTIFICATIONS, GetMessageCenterViewInternalMode());
  // Show the settings.
  GetMessageCenterView()->SetSettingsVisible(true);
  EXPECT_EQ(Mode::SETTINGS, GetMessageCenterViewInternalMode());
  // Hide the settings.
  GetMessageCenterView()->SetSettingsVisible(false);
  EXPECT_EQ(Mode::NOTIFICATIONS, GetMessageCenterViewInternalMode());
}

TEST_F(MessageCenterViewTest, CheckModeWithRemovingAndAddingNotifications) {
  // Check the initial state.
  EXPECT_EQ(Mode::NOTIFICATIONS, GetMessageCenterViewInternalMode());

  // Remove notifications.
  RemoveDefaultNotifications();
  EXPECT_EQ(Mode::NO_NOTIFICATIONS, GetMessageCenterViewInternalMode());

  // Add a notification.
  AddNotification(std::make_unique<Notification>(
      NOTIFICATION_TYPE_SIMPLE, std::string(kNotificationId1),
      base::UTF8ToUTF16("title2"),
      base::UTF8ToUTF16("message\nwhich\nis\nvertically\nlong\n."),
      gfx::Image(), base::UTF8ToUTF16("display source"), GURL(),
      NotifierId(NotifierId::APPLICATION, "extension_id"),
      message_center::RichNotificationData(), nullptr));
  EXPECT_EQ(Mode::NOTIFICATIONS, GetMessageCenterViewInternalMode());
}

TEST_F(MessageCenterViewTest, CheckModeWithSettingsVisibleAndHiddenOnEmpty) {
  // Set up by removing all existing notifications.
  RemoveDefaultNotifications();

  // Check the initial state.
  EXPECT_EQ(Mode::NO_NOTIFICATIONS, GetMessageCenterViewInternalMode());
  // Show the settings.
  GetMessageCenterView()->SetSettingsVisible(true);
  EXPECT_EQ(Mode::SETTINGS, GetMessageCenterViewInternalMode());
  // Hide the settings.
  GetMessageCenterView()->SetSettingsVisible(false);
  EXPECT_EQ(Mode::NO_NOTIFICATIONS, GetMessageCenterViewInternalMode());
}

TEST_F(MessageCenterViewTest,
       CheckModeWithRemovingNotificationDuringSettingsVisible) {
  // Check the initial state.
  EXPECT_EQ(Mode::NOTIFICATIONS, GetMessageCenterViewInternalMode());

  // Show the settings.
  GetMessageCenterView()->SetSettingsVisible(true);
  EXPECT_EQ(Mode::SETTINGS, GetMessageCenterViewInternalMode());

  // Remove a notification during settings is visible.
  RemoveDefaultNotifications();
  EXPECT_EQ(Mode::SETTINGS, GetMessageCenterViewInternalMode());

  // Hide the settings.
  GetMessageCenterView()->SetSettingsVisible(false);
  EXPECT_EQ(Mode::NO_NOTIFICATIONS, GetMessageCenterViewInternalMode());
}

TEST_F(MessageCenterViewTest,
       CheckModeWithAddingNotificationDuringSettingsVisible) {
  // Set up by removing all existing notifications.
  RemoveDefaultNotifications();

  // Check the initial state.
  EXPECT_EQ(Mode::NO_NOTIFICATIONS, GetMessageCenterViewInternalMode());

  // Show the settings.
  GetMessageCenterView()->SetSettingsVisible(true);
  EXPECT_EQ(Mode::SETTINGS, GetMessageCenterViewInternalMode());

  // Add a notification during settings is visible.
  AddNotification(std::make_unique<Notification>(
      NOTIFICATION_TYPE_SIMPLE, std::string(kNotificationId1),
      base::UTF8ToUTF16("title2"),
      base::UTF8ToUTF16("message\nwhich\nis\nvertically\nlong\n."),
      gfx::Image(), base::UTF8ToUTF16("display source"), GURL(),
      NotifierId(NotifierId::APPLICATION, "extension_id"),
      message_center::RichNotificationData(), nullptr));
  EXPECT_EQ(Mode::SETTINGS, GetMessageCenterViewInternalMode());

  // Hide the settings.
  GetMessageCenterView()->SetSettingsVisible(false);
  EXPECT_EQ(Mode::NOTIFICATIONS, GetMessageCenterViewInternalMode());
}

TEST_F(MessageCenterViewTest, CheckModeWithLockingAndUnlocking) {
  // Check the initial state.
  EXPECT_EQ(Mode::NOTIFICATIONS, GetMessageCenterViewInternalMode());

  // Lock!
  SetLockedState(true);
  EXPECT_EQ(Mode::LOCKED, GetMessageCenterViewInternalMode());

  // Unlock!
  SetLockedState(false);
  EXPECT_EQ(Mode::NOTIFICATIONS, GetMessageCenterViewInternalMode());

  // Remove all existing notifications.
  RemoveDefaultNotifications();
  EXPECT_EQ(Mode::NO_NOTIFICATIONS, GetMessageCenterViewInternalMode());

  // Lock!
  SetLockedState(true);
  EXPECT_EQ(Mode::LOCKED, GetMessageCenterViewInternalMode());

  // Unlock!
  SetLockedState(false);
  EXPECT_EQ(Mode::NO_NOTIFICATIONS, GetMessageCenterViewInternalMode());
}

TEST_F(MessageCenterViewTest,
       CheckModeWithLockingAndUnlockingWithLockScreenNotificationEnabled) {
  SetLockScreenNotificationsEnabled();

  // Check the initial state.
  EXPECT_EQ(Mode::NOTIFICATIONS, GetMessageCenterViewInternalMode());

  // Lock!
  SetLockedState(true);
  EXPECT_EQ(Mode::NOTIFICATIONS, GetMessageCenterViewInternalMode());

  // Unlock!
  SetLockedState(false);
  EXPECT_EQ(Mode::NOTIFICATIONS, GetMessageCenterViewInternalMode());

  // Remove all existing notifications.
  RemoveDefaultNotifications();
  EXPECT_EQ(Mode::NO_NOTIFICATIONS, GetMessageCenterViewInternalMode());

  // Lock!
  SetLockedState(true);
  EXPECT_EQ(Mode::NO_NOTIFICATIONS, GetMessageCenterViewInternalMode());

  // Unlock!
  SetLockedState(false);
  EXPECT_EQ(Mode::NO_NOTIFICATIONS, GetMessageCenterViewInternalMode());
}

TEST_F(MessageCenterViewTest, CheckModeWithRemovingNotificationDuringLock) {
  // Check the initial state.
  EXPECT_EQ(Mode::NOTIFICATIONS, GetMessageCenterViewInternalMode());

  // Lock!
  SetLockedState(true);
  EXPECT_EQ(Mode::LOCKED, GetMessageCenterViewInternalMode());

  // Remove all existing notifications.
  RemoveDefaultNotifications();
  EXPECT_EQ(Mode::LOCKED, GetMessageCenterViewInternalMode());

  // Unlock!
  SetLockedState(false);

  EXPECT_EQ(Mode::NO_NOTIFICATIONS, GetMessageCenterViewInternalMode());
}

TEST_F(
    MessageCenterViewTest,
    CheckModeWithRemovingNotificationDuringLockWithLockScreenNotificationEnabled) {
  SetLockScreenNotificationsEnabled();

  // Check the initial state.
  EXPECT_EQ(Mode::NOTIFICATIONS, GetMessageCenterViewInternalMode());

  // Lock!
  SetLockedState(true);
  EXPECT_EQ(Mode::NOTIFICATIONS, GetMessageCenterViewInternalMode());

  // Remove all existing notifications.
  RemoveDefaultNotifications();
  EXPECT_EQ(Mode::NO_NOTIFICATIONS, GetMessageCenterViewInternalMode());

  // Unlock!
  SetLockedState(false);

  EXPECT_EQ(Mode::NO_NOTIFICATIONS, GetMessageCenterViewInternalMode());
}

TEST_F(MessageCenterViewTest, LockScreen) {
  EXPECT_TRUE(GetNotificationView(kNotificationId1)->IsDrawn());
  EXPECT_TRUE(GetNotificationView(kNotificationId2)->IsDrawn());

  views::Button* close_button = GetButtonBar()->GetCloseAllButtonForTest();
  ASSERT_TRUE(close_button);
  views::Button* quiet_mode_button =
      GetButtonBar()->GetQuietModeButtonForTest();
  ASSERT_TRUE(quiet_mode_button);
  views::Button* settings_button = GetButtonBar()->GetSettingsButtonForTest();
  ASSERT_TRUE(settings_button);
  views::Button* collapse_button = GetButtonBar()->GetCollapseButtonForTest();
  ASSERT_TRUE(collapse_button);

  EXPECT_TRUE(close_button->visible());
  EXPECT_TRUE(quiet_mode_button->visible());
  EXPECT_TRUE(settings_button->visible());

  // Lock!
  SetLockedState(true);

  EXPECT_FALSE(GetNotificationView(kNotificationId1)->IsDrawn());
  EXPECT_FALSE(GetNotificationView(kNotificationId2)->IsDrawn());

  GetMessageCenterView()->SizeToPreferredSize();
  EXPECT_EQ(kLockedMessageCenterViewHeight, GetMessageCenterView()->height());

  RemoveNotification(kNotificationId1, false);

  GetMessageCenterView()->SizeToPreferredSize();
  EXPECT_EQ(kLockedMessageCenterViewHeight, GetMessageCenterView()->height());

  RemoveNotification(kNotificationId2, false);

  GetMessageCenterView()->SizeToPreferredSize();
  EXPECT_EQ(kLockedMessageCenterViewHeight, GetMessageCenterView()->height());

  AddNotification(std::make_unique<Notification>(
      NOTIFICATION_TYPE_SIMPLE, std::string(kNotificationId1),
      base::UTF8ToUTF16("title1"), base::UTF8ToUTF16("message"), gfx::Image(),
      base::UTF8ToUTF16("display source"), GURL(),
      NotifierId(NotifierId::APPLICATION, "extension_id"),
      message_center::RichNotificationData(), nullptr));
  EXPECT_FALSE(GetNotificationView(kNotificationId1)->IsDrawn());

  GetMessageCenterView()->SizeToPreferredSize();
  EXPECT_EQ(kLockedMessageCenterViewHeight, GetMessageCenterView()->height());

  EXPECT_FALSE(close_button->visible());
  EXPECT_FALSE(quiet_mode_button->visible());
  EXPECT_FALSE(settings_button->visible());
  EXPECT_FALSE(collapse_button->visible());

  // Unlock!
  SetLockedState(false);

  EXPECT_TRUE(GetNotificationView(kNotificationId1)->IsDrawn());

  GetMessageCenterView()->SizeToPreferredSize();
  EXPECT_NE(kLockedMessageCenterViewHeight, GetMessageCenterView()->height());
  EXPECT_NE(kEmptyMessageCenterViewHeight, GetMessageCenterView()->height());

  EXPECT_TRUE(close_button->visible());
  EXPECT_TRUE(quiet_mode_button->visible());
  EXPECT_TRUE(settings_button->visible());

  // Lock!
  SetLockedState(true);

  EXPECT_FALSE(GetNotificationView(kNotificationId1)->IsDrawn());

  GetMessageCenterView()->SizeToPreferredSize();
  EXPECT_EQ(kLockedMessageCenterViewHeight, GetMessageCenterView()->height());

  EXPECT_FALSE(close_button->visible());
  EXPECT_FALSE(quiet_mode_button->visible());
  EXPECT_FALSE(settings_button->visible());
  EXPECT_FALSE(collapse_button->visible());
}

TEST_F(MessageCenterViewTest, LockScreenWithLockScreenNotificationEnabled) {
  SetLockScreenNotificationsEnabled();

  EXPECT_TRUE(GetNotificationView(kNotificationId1)->IsDrawn());
  EXPECT_TRUE(GetNotificationView(kNotificationId2)->IsDrawn());

  views::Button* close_button = GetButtonBar()->GetCloseAllButtonForTest();
  ASSERT_TRUE(close_button);
  views::Button* quiet_mode_button =
      GetButtonBar()->GetQuietModeButtonForTest();
  ASSERT_TRUE(quiet_mode_button);
  views::Button* settings_button = GetButtonBar()->GetSettingsButtonForTest();
  ASSERT_TRUE(settings_button);
  views::Button* collapse_button = GetButtonBar()->GetCollapseButtonForTest();
  ASSERT_TRUE(collapse_button);

  EXPECT_TRUE(close_button->visible());
  EXPECT_TRUE(quiet_mode_button->visible());
  EXPECT_TRUE(settings_button->visible());

  // Lock!
  SetLockedState(true);

  EXPECT_TRUE(GetNotificationView(kNotificationId1)->IsDrawn());
  EXPECT_TRUE(GetNotificationView(kNotificationId2)->IsDrawn());

  GetMessageCenterView()->SizeToPreferredSize();
  EXPECT_NE(kLockedMessageCenterViewHeight, GetMessageCenterView()->height());
  EXPECT_NE(kEmptyMessageCenterViewHeight, GetMessageCenterView()->height());

  RemoveNotification(kNotificationId1, false);

  GetMessageCenterView()->SizeToPreferredSize();
  EXPECT_NE(kLockedMessageCenterViewHeight, GetMessageCenterView()->height());
  EXPECT_NE(kEmptyMessageCenterViewHeight, GetMessageCenterView()->height());

  RemoveNotification(kNotificationId2, false);

  GetMessageCenterView()->SizeToPreferredSize();
  EXPECT_EQ(kEmptyMessageCenterViewHeight, GetMessageCenterView()->height());

  AddNotification(std::make_unique<Notification>(
      NOTIFICATION_TYPE_SIMPLE, std::string(kNotificationId1),
      base::UTF8ToUTF16("title1"), base::UTF8ToUTF16("message"), gfx::Image(),
      base::UTF8ToUTF16("display source"), GURL(),
      NotifierId(NotifierId::APPLICATION, "extension_id"),
      message_center::RichNotificationData(), nullptr));
  EXPECT_TRUE(GetNotificationView(kNotificationId1)->IsDrawn());

  GetMessageCenterView()->SizeToPreferredSize();
  EXPECT_NE(kLockedMessageCenterViewHeight, GetMessageCenterView()->height());
  EXPECT_NE(kEmptyMessageCenterViewHeight, GetMessageCenterView()->height());

  EXPECT_TRUE(close_button->visible());
  EXPECT_TRUE(quiet_mode_button->visible());
  EXPECT_FALSE(settings_button->visible());
  EXPECT_FALSE(collapse_button->visible());

  // Unlock!
  SetLockedState(false);

  EXPECT_TRUE(GetNotificationView(kNotificationId1)->IsDrawn());

  GetMessageCenterView()->SizeToPreferredSize();
  EXPECT_NE(kLockedMessageCenterViewHeight, GetMessageCenterView()->height());
  EXPECT_NE(kEmptyMessageCenterViewHeight, GetMessageCenterView()->height());

  EXPECT_TRUE(close_button->visible());
  EXPECT_TRUE(quiet_mode_button->visible());
  EXPECT_TRUE(settings_button->visible());

  // Lock!
  SetLockedState(true);

  EXPECT_TRUE(GetNotificationView(kNotificationId1)->IsDrawn());

  GetMessageCenterView()->SizeToPreferredSize();
  EXPECT_NE(kLockedMessageCenterViewHeight, GetMessageCenterView()->height());
  EXPECT_NE(kEmptyMessageCenterViewHeight, GetMessageCenterView()->height());

  EXPECT_TRUE(close_button->visible());
  EXPECT_TRUE(quiet_mode_button->visible());
  EXPECT_FALSE(settings_button->visible());
  EXPECT_FALSE(collapse_button->visible());
}

TEST_F(MessageCenterViewTest, NoNotification) {
  GetMessageCenterView()->SizeToPreferredSize();
  EXPECT_NE(kEmptyMessageCenterViewHeight, GetMessageCenterView()->height());
  RemoveNotification(kNotificationId1, false);
  GetMessageCenterView()->SizeToPreferredSize();
  EXPECT_NE(kEmptyMessageCenterViewHeight, GetMessageCenterView()->height());
  RemoveNotification(kNotificationId2, false);
  GetMessageCenterView()->SizeToPreferredSize();
  EXPECT_EQ(kEmptyMessageCenterViewHeight, GetMessageCenterView()->height());

  AddNotification(std::make_unique<Notification>(
      NOTIFICATION_TYPE_SIMPLE, std::string(kNotificationId1),
      base::UTF8ToUTF16("title1"), base::UTF8ToUTF16("message"), gfx::Image(),
      base::UTF8ToUTF16("display source"), GURL(),
      NotifierId(NotifierId::APPLICATION, "extension_id"),
      message_center::RichNotificationData(), nullptr));

  GetMessageCenterView()->SizeToPreferredSize();
  EXPECT_NE(kEmptyMessageCenterViewHeight, GetMessageCenterView()->height());
}

}  // namespace ash
