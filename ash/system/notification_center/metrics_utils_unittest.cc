// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/notification_center/metrics_utils.h"

#include <memory>

#include "ash/system/notification_center/views/ash_notification_view.h"
#include "ash/system/notification_center/notification_center_test_api.h"
#include "ash/system/notification_center/views/notification_center_view.h"
#include "ash/system/notification_center/views/notification_list_view.h"
#include "ash/test/ash_test_base.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notifier_id.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/widget/widget_utils.h"

using message_center::Notification;

namespace {

constexpr char kNotificationViewTypeHistogramName[] =
    "Ash.NotificationView.NotificationAdded.Type";

constexpr char kCountInOneGroupHistogramName[] =
    "Ash.Notification.CountOfNotificationsInOneGroup";

constexpr char kGroupNotificationAddedHistogramName[] =
    "Ash.Notification.GroupNotificationAdded";

constexpr char kSystemNotificationAddedHistogramName[] =
    "Ash.NotifierFramework.SystemNotification.Added";

constexpr char kPinnedSystemNotificationAddedHistogramName[] =
    "Ash.NotifierFramework.PinnedSystemNotification.Added";

constexpr char kSystemNotificationClickedOnActionButton[] =
    "Ash.NotifierFramework.SystemNotification.ClickedActionButton.";

constexpr char kSystemNotificationPopupShownHistogramName[] =
    "Ash.NotifierFramework.SystemNotification.Popup.ShownCount";

constexpr char kSystemNotificationPopupUserJourneyTime[] =
    "Ash.NotifierFramework.SystemNotification.Popup.UserJourneyTime";

constexpr char kSystemNotificationPopupDismissedWithin1s[] =
    "Ash.NotifierFramework.SystemNotification.Popup.Dismissed.Within1s";

constexpr char kSystemNotificationPopupDismissedWithin7s[] =
    "Ash.NotifierFramework.SystemNotification.Popup.Dismissed.Within7s";

constexpr char kSystemNotificationPopupDismissedAfter7s[] =
    "Ash.NotifierFramework.SystemNotification.Popup.Dismissed.After7s";

constexpr int kImageSize = 80;

void CheckNotificationViewTypeRecorded(
    std::unique_ptr<Notification> notification,
    ash::metrics_utils::NotificationViewType type) {
  base::HistogramTester histograms;

  // Add the notification. Expect that the corresponding notification type is
  // recorded.
  message_center::MessageCenter::Get()->AddNotification(
      std::move(notification));
  histograms.ExpectBucketCount(kNotificationViewTypeHistogramName, type, 1);
}

// A blocker that blocks a notification with the given ID.
class IdNotificationBlocker : public message_center::NotificationBlocker {
 public:
  explicit IdNotificationBlocker(message_center::MessageCenter* message_center)
      : NotificationBlocker(message_center) {}
  IdNotificationBlocker(const IdNotificationBlocker&) = delete;
  IdNotificationBlocker& operator=(const IdNotificationBlocker&) = delete;
  ~IdNotificationBlocker() override = default;

  void SetTargetIdAndNotifyBlock(const std::string& target_id) {
    target_id_ = target_id;
    NotifyBlockingStateChanged();
  }

  // message_center::NotificationBlocker:
  bool ShouldShowNotification(
      const message_center::Notification& notification) const override {
    return notification.id() != target_id_;
  }

  bool ShouldShowNotificationAsPopup(
      const message_center::Notification& notification) const override {
    return notification.id() != target_id_;
  }

 private:
  std::string target_id_;
};

// Returns true if a notification with a given `id` is showing a notification in
// the message center.
bool NotificationVisible(const std::string& id) {
  return message_center::MessageCenter::Get()->FindVisibleNotificationById(
             id) != nullptr;
}

// Returns true if a notification with a given `id` has a pop-up.
bool PopupVisible(const std::string& id) {
  return message_center::MessageCenter::Get()->FindPopupNotificationById(id) !=
         nullptr;
}

}  // namespace

namespace ash {

// This serves as an unit test class for all metrics recording in
// notification/message center.
class MessageCenterMetricsUtilsTest : public AshTestBase {
 public:
  MessageCenterMetricsUtilsTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  MessageCenterMetricsUtilsTest(const MessageCenterMetricsUtilsTest&) = delete;
  MessageCenterMetricsUtilsTest& operator=(
      const MessageCenterMetricsUtilsTest&) = delete;
  ~MessageCenterMetricsUtilsTest() override = default;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();
    test_delegate_ =
        base::MakeRefCounted<message_center::NotificationDelegate>();
    notification_center_test_api_ =
        std::make_unique<NotificationCenterTestApi>();
  }

  // Create a test notification. Noted that the notifications are using the same
  // url and profile id so that they are grouped together.
  std::unique_ptr<Notification> CreateTestNotification() {
    message_center::RichNotificationData data;
    data.settings_button_handler =
        message_center::SettingsButtonHandler::INLINE;
    message_center::NotifierId notifier_id;
    notifier_id.profile_id = "a@b.com";
    notifier_id.type = message_center::NotifierType::WEB_PAGE;
    return std::make_unique<Notification>(
        message_center::NOTIFICATION_TYPE_SIMPLE,
        base::NumberToString(current_id_++), u"title", u"message",
        ui::ImageModel(), u"display source", GURL(u"http://test-url.com"),
        notifier_id, data,
        /*delegate=*/nullptr);
  }

  // Create a system notification with given `catalog_name`.
  std::unique_ptr<Notification> CreateNotificationWithCatalogName(
      NotificationCatalogName catalog_name) {
    const std::string id =
        "id" + base::NumberToString(static_cast<int>(catalog_name));
    message_center::RichNotificationData data;
    return std::make_unique<Notification>(
        message_center::NOTIFICATION_TYPE_SIMPLE, id, u"title", u"message",
        ui::ImageModel(), u"display source", GURL(u"http://test-url.com"),
        message_center::NotifierId(
            message_center::NotifierType::SYSTEM_COMPONENT, id, catalog_name),
        data, /*delegate=*/nullptr);
  }

  // Create a pinned system notification with given `catalog_name`.
  std::unique_ptr<Notification> CreatePinnedNotificationWithCatalogName(
      NotificationCatalogName catalog_name) {
    auto notification = CreateNotificationWithCatalogName(catalog_name);
    notification->set_pinned(true);
    return notification;
  }

  // Get the notification view from message center associated with `id`.
  views::View* GetNotificationViewFromMessageCenter(const std::string& id) {
    return notification_center_test_api_->GetNotificationViewForId(id);
  }

  // Get the popup notification view associated with `id`.
  AshNotificationView* GetPopupNotificationView(const std::string& id) {
    return static_cast<AshNotificationView*>(
        notification_center_test_api_->GetPopupViewForId(id)->message_view());
  }

  views::View* GetActionButtonFromIndex(AshNotificationView* notification_view,
                                        unsigned int index) {
    CHECK_LT(index, notification_view->action_buttons().size());
    return notification_view->action_buttons()[index];
  }

  void ClickView(views::View* view) {
    ui::test::EventGenerator generator(GetRootWindow(view->GetWidget()));
    gfx::Point cursor_location = view->GetBoundsInScreen().CenterPoint();
    generator.MoveMouseTo(cursor_location);
    generator.ClickLeftButton();
  }

  void HoverOnView(views::View* view) {
    ui::test::EventGenerator generator(GetRootWindow(view->GetWidget()));
    gfx::Point cursor_location = view->GetBoundsInScreen().CenterPoint();
    generator.MoveMouseTo(cursor_location);
  }

  scoped_refptr<message_center::NotificationDelegate> test_delegate() {
    return test_delegate_;
  }

  NotificationCenterTestApi* notification_center_test_api() {
    return notification_center_test_api_.get();
  }

 private:
  scoped_refptr<message_center::NotificationDelegate> test_delegate_;
  std::unique_ptr<NotificationCenterTestApi> notification_center_test_api_;

  // Used to create test notification. This represents the current available
  // number that we can use to create the next test notification. This id will
  // be incremented whenever we create a new test notification.
  int current_id_ = 0;
};

TEST_F(MessageCenterMetricsUtilsTest, RecordBadClicks) {
  base::HistogramTester histograms;
  auto notification = CreateTestNotification();

  // Add the notification and get its view in message center.
  message_center::MessageCenter::Get()->AddNotification(
      std::make_unique<message_center::Notification>(*notification));
  notification_center_test_api()->ToggleBubble();

  // A click to a notification without a delegate should record a bad click.
  ClickView(GetNotificationViewFromMessageCenter(notification->id()));
  histograms.ExpectTotalCount("Notifications.Cros.Actions.ClickedBody.BadClick",
                              1);
  histograms.ExpectTotalCount(
      "Notifications.Cros.Actions.ClickedBody.GoodClick", 0);
}

TEST_F(MessageCenterMetricsUtilsTest, RecordGoodClicks) {
  base::HistogramTester histograms;
  auto notification = CreateTestNotification();
  notification->set_delegate(test_delegate());

  // Add the notification and get its view in message center.
  message_center::MessageCenter::Get()->AddNotification(
      std::make_unique<message_center::Notification>(*notification));
  notification_center_test_api()->ToggleBubble();

  // A click to a notification with a delegate should record a good click.
  ClickView(GetNotificationViewFromMessageCenter(notification->id()));
  histograms.ExpectTotalCount(
      "Notifications.Cros.Actions.ClickedBody.GoodClick", 1);
  histograms.ExpectTotalCount("Notifications.Cros.Actions.ClickedBody.BadClick",
                              0);
}

TEST_F(MessageCenterMetricsUtilsTest, RecordHover) {
  base::HistogramTester histograms;
  auto notification = CreateTestNotification();

  // Add the notification and get its view in message center.
  message_center::MessageCenter::Get()->AddNotification(
      std::make_unique<message_center::Notification>(*notification));

  auto* popup =
      notification_center_test_api()->GetPopupViewForId(notification->id());
  // Move the mouse hover on the popup notification view, expect hover action
  // recorded.
  HoverOnView(popup);
  histograms.ExpectTotalCount("Notifications.Cros.Actions.Popup.Hover", 1);

  notification_center_test_api()->ToggleBubble();
  auto* notification_view =
      GetNotificationViewFromMessageCenter(notification->id());

  // Move the mouse hover on the notification view, expect hover action
  // recorded.
  HoverOnView(notification_view);
  histograms.ExpectTotalCount("Notifications.Cros.Actions.Tray.Hover", 1);
}

TEST_F(MessageCenterMetricsUtilsTest, RecordNotificationViewTypeSimple) {
  message_center::MessageCenter::Get()->RemoveAllNotifications(
      /*by_user=*/true, message_center::MessageCenter::RemoveType::ALL);

  auto simple_notification = CreateTestNotification();
  CheckNotificationViewTypeRecorded(
      std::move(simple_notification),
      metrics_utils::NotificationViewType::SIMPLE);

  auto grouped_simple_notification = CreateTestNotification();
  CheckNotificationViewTypeRecorded(
      std::move(grouped_simple_notification),
      metrics_utils::NotificationViewType::GROUPED_SIMPLE);
}

TEST_F(MessageCenterMetricsUtilsTest, RecordNotificationViewTypeImage) {
  message_center::MessageCenter::Get()->RemoveAllNotifications(
      /*by_user=*/true, message_center::MessageCenter::RemoveType::ALL);

  auto image_notification = CreateTestNotification();
  image_notification->SetImage(gfx::test::CreateImage(kImageSize));
  CheckNotificationViewTypeRecorded(
      std::move(image_notification),
      metrics_utils::NotificationViewType::HAS_IMAGE);

  auto grouped_image_notification = CreateTestNotification();
  grouped_image_notification->SetImage(gfx::test::CreateImage(kImageSize));
  CheckNotificationViewTypeRecorded(
      std::move(grouped_image_notification),
      metrics_utils::NotificationViewType::GROUPED_HAS_IMAGE);
}

TEST_F(MessageCenterMetricsUtilsTest, RecordNotificationViewTypeActionButtons) {
  message_center::MessageCenter::Get()->RemoveAllNotifications(
      /*by_user=*/true, message_center::MessageCenter::RemoveType::ALL);

  auto notification = CreateTestNotification();
  notification->set_buttons({message_center::ButtonInfo(u"Test button")});
  CheckNotificationViewTypeRecorded(
      std::move(notification), metrics_utils::NotificationViewType::HAS_ACTION);

  auto grouped_notification = CreateTestNotification();
  grouped_notification->set_buttons(
      {message_center::ButtonInfo(u"Test button")});
  CheckNotificationViewTypeRecorded(
      std::move(grouped_notification),
      metrics_utils::NotificationViewType::GROUPED_HAS_ACTION);
}

TEST_F(MessageCenterMetricsUtilsTest, RecordNotificationViewTypeInlineReply) {
  message_center::MessageCenter::Get()->RemoveAllNotifications(
      /*by_user=*/true, message_center::MessageCenter::RemoveType::ALL);

  auto create_inline_reply_button = []() {
    message_center::ButtonInfo button(u"Test button");
    button.placeholder = std::u16string();
    return button;
  };
  auto notification = CreateTestNotification();
  notification->set_buttons({create_inline_reply_button()});

  CheckNotificationViewTypeRecorded(
      std::move(notification),
      metrics_utils::NotificationViewType::HAS_INLINE_REPLY);

  auto grouped_notification = CreateTestNotification();
  grouped_notification->set_buttons({create_inline_reply_button()});
  CheckNotificationViewTypeRecorded(
      std::move(grouped_notification),
      metrics_utils::NotificationViewType::GROUPED_HAS_INLINE_REPLY);
}

TEST_F(MessageCenterMetricsUtilsTest,
       RecordNotificationViewTypeImageActionButtons) {
  message_center::MessageCenter::Get()->RemoveAllNotifications(
      /*by_user=*/true, message_center::MessageCenter::RemoveType::ALL);

  auto notification = CreateTestNotification();
  notification->SetImage(gfx::test::CreateImage(kImageSize));
  notification->set_buttons({message_center::ButtonInfo(u"Test button")});
  CheckNotificationViewTypeRecorded(
      std::move(notification),
      metrics_utils::NotificationViewType::HAS_IMAGE_AND_ACTION);

  auto grouped_notification = CreateTestNotification();
  grouped_notification->SetImage(gfx::test::CreateImage(kImageSize));
  grouped_notification->set_buttons(
      {message_center::ButtonInfo(u"Test button")});
  CheckNotificationViewTypeRecorded(
      std::move(grouped_notification),
      metrics_utils::NotificationViewType::GROUPED_HAS_IMAGE_AND_ACTION);
}

TEST_F(MessageCenterMetricsUtilsTest,
       RecordNotificationViewTypeImageInlineReply) {
  message_center::MessageCenter::Get()->RemoveAllNotifications(
      /*by_user=*/true, message_center::MessageCenter::RemoveType::ALL);

  auto create_inline_reply_button = []() {
    message_center::ButtonInfo button(u"Test button");
    button.placeholder = std::u16string();
    return button;
  };
  auto notification = CreateTestNotification();
  notification->SetImage(gfx::test::CreateImage(kImageSize));
  notification->set_buttons({create_inline_reply_button()});

  CheckNotificationViewTypeRecorded(
      std::move(notification),
      metrics_utils::NotificationViewType::HAS_IMAGE_AND_INLINE_REPLY);

  auto grouped_notification = CreateTestNotification();
  grouped_notification->SetImage(gfx::test::CreateImage(kImageSize));
  grouped_notification->set_buttons({create_inline_reply_button()});
  CheckNotificationViewTypeRecorded(
      std::move(grouped_notification),
      metrics_utils::NotificationViewType::GROUPED_HAS_IMAGE_AND_INLINE_REPLY);
}

TEST_F(MessageCenterMetricsUtilsTest, RecordCountOfNotificationsInOneGroup) {
  base::HistogramTester histograms;

  auto notification1 = CreateTestNotification();
  std::string id1 = notification1->id();
  auto notification2 = CreateTestNotification();
  std::string id2 = notification2->id();
  auto notification3 = CreateTestNotification();

  message_center::MessageCenter::Get()->AddNotification(
      std::move(notification1));
  message_center::MessageCenter::Get()->AddNotification(
      std::move(notification2));

  histograms.ExpectBucketCount(kCountInOneGroupHistogramName, 2, 1);

  message_center::MessageCenter::Get()->AddNotification(
      std::move(notification3));
  histograms.ExpectBucketCount(kCountInOneGroupHistogramName, 3, 1);

  message_center::MessageCenter::Get()->RemoveNotification(id1,
                                                           /*by_user=*/true);
  histograms.ExpectBucketCount(kCountInOneGroupHistogramName, 2, 2);
}

TEST_F(MessageCenterMetricsUtilsTest, RecordGroupNotificationAddedType) {
  base::HistogramTester histograms;

  auto notification1 = CreateTestNotification();
  auto notification2 = CreateTestNotification();
  auto notification3 = CreateTestNotification();

  message_center::MessageCenter::Get()->AddNotification(
      std::move(notification1));
  message_center::MessageCenter::Get()->AddNotification(
      std::move(notification2));

  // There should be 1 group parent that contains 2 group child notifications.
  histograms.ExpectBucketCount(
      kGroupNotificationAddedHistogramName,
      metrics_utils::GroupNotificationType::GROUP_PARENT, 1);
  histograms.ExpectBucketCount(
      kGroupNotificationAddedHistogramName,
      metrics_utils::GroupNotificationType::GROUP_CHILD, 2);

  message_center::MessageCenter::Get()->AddNotification(
      std::move(notification3));
  histograms.ExpectBucketCount(
      kGroupNotificationAddedHistogramName,
      metrics_utils::GroupNotificationType::GROUP_PARENT, 1);
  histograms.ExpectBucketCount(
      kGroupNotificationAddedHistogramName,
      metrics_utils::GroupNotificationType::GROUP_CHILD, 3);
}

TEST_F(MessageCenterMetricsUtilsTest, RecordSystemNotificationAdded) {
  base::HistogramTester histograms;

  // Create system notifications with a valid catalog name, one for a non-pinned
  // notification and one for a pinned one (e.g. Full Restore and Caps Lock).
  const NotificationCatalogName catalog_name =
      NotificationCatalogName::kFullRestore;
  const NotificationCatalogName pinned_catalog_name =
      NotificationCatalogName::kCapsLock;
  auto notification = CreateNotificationWithCatalogName(catalog_name);
  auto pinned_notification =
      CreatePinnedNotificationWithCatalogName(pinned_catalog_name);

  // Add notifications to message center.
  auto* message_center = message_center::MessageCenter::Get();
  message_center->AddNotification(
      std::make_unique<message_center::Notification>(*notification));
  message_center->AddNotification(
      std::make_unique<message_center::Notification>(*pinned_notification));

  // Expect metric to be recorded for valid catalog names.
  histograms.ExpectBucketCount(kSystemNotificationAddedHistogramName,
                               catalog_name, 1);
  histograms.ExpectBucketCount(kPinnedSystemNotificationAddedHistogramName,
                               pinned_catalog_name, 1);
}

TEST_F(MessageCenterMetricsUtilsTest,
       RecordSystemNotificationAddedWithNotificationBlockers) {
  base::HistogramTester histograms;

  // Create a system notification with a valid catalog name (e.g. Full Restore).
  const NotificationCatalogName catalog_name =
      NotificationCatalogName::kFullRestore;
  auto notification = CreateNotificationWithCatalogName(catalog_name);

  // Add notification to message center.
  auto* message_center = message_center::MessageCenter::Get();
  message_center->AddNotification(
      std::make_unique<message_center::Notification>(*notification));

  // Verify notification was shown.
  EXPECT_TRUE(PopupVisible(notification->id()));
  EXPECT_TRUE(NotificationVisible(notification->id()));

  // Expect `Added` and `PopupShown` metrics to be recorded.
  histograms.ExpectBucketCount(kSystemNotificationAddedHistogramName,
                               catalog_name, 1);
  histograms.ExpectBucketCount(kSystemNotificationPopupShownHistogramName,
                               catalog_name, 1);

  // Apply a notification blocker.
  IdNotificationBlocker blocker(message_center);
  blocker.Init();
  blocker.SetTargetIdAndNotifyBlock(notification->id());

  // Add more notification instances to the message center.
  message_center->RemoveNotification(notification->id(), /*by_user=*/false);
  message_center->AddNotification(
      std::make_unique<message_center::Notification>(*notification));

  // Verify notification wasn't shown with notification blockers.
  EXPECT_FALSE(PopupVisible(notification->id()));
  EXPECT_FALSE(NotificationVisible(notification->id()));

  // Verify the `Added` metric is recorded even with notification blockers.
  histograms.ExpectBucketCount(kSystemNotificationAddedHistogramName,
                               catalog_name, 2);

  // Verify `Popup Shown` metric is not recorded with notification blockers.
  histograms.ExpectBucketCount(kSystemNotificationPopupShownHistogramName,
                               catalog_name, 1);
}

TEST_F(MessageCenterMetricsUtilsTest, RecordPopupUserJourneyTime) {
  base::HistogramTester histograms;

  // Create a non-pinned system notification with a valid catalog name.
  const NotificationCatalogName catalog_name =
      NotificationCatalogName::kFullRestore;
  auto notification = CreateNotificationWithCatalogName(catalog_name);

  // Add notification to message center. Use the normal duration for adding the
  // notification so that the recorded popup duration is expected.
  auto* message_center = message_center::MessageCenter::Get();
  std::optional<ui::ScopedAnimationDurationScaleMode> mode(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);
  message_center->AddNotification(
      std::make_unique<message_center::Notification>(*notification));

  // Wait for notification popup to time out.
  constexpr base::TimeDelta kPopupTimeOutDuration(base::Seconds(7));
  task_environment()->FastForwardBy(kPopupTimeOutDuration);

  // Expect user journey time metric to record the popup duration due to timeout
  // (value is between 6 and 7 seconds).
  auto buckets =
      histograms.GetAllSamples(kSystemNotificationPopupUserJourneyTime);
  EXPECT_TRUE(buckets[0].min >= 6000 && buckets[0].min <= 7000);
  histograms.ExpectBucketCount(kSystemNotificationPopupDismissedWithin7s,
                               catalog_name, 1);
  mode.reset();
  message_center->RemoveNotification(notification->id(), /*by_user=*/true);

  // Dismiss popup within 1s.
  notification->set_timestamp(base::Time::Now());
  message_center->AddNotification(
      std::make_unique<message_center::Notification>(*notification));
  message_center->RemoveNotification(notification->id(), /*by_user=*/true);
  task_environment()->FastForwardBy(kPopupTimeOutDuration);
  histograms.ExpectBucketCount(kSystemNotificationPopupDismissedWithin1s,
                               catalog_name, 1);

  // Dismiss "never timeout" popup after 7s.
  notification->set_timestamp(base::Time::Now());
  notification->SetSystemPriority();
  message_center->AddNotification(
      std::make_unique<message_center::Notification>(*notification));
  task_environment()->FastForwardBy(kPopupTimeOutDuration + base::Seconds(1));
  message_center->RemoveNotification(notification->id(), /*by_user=*/true);
  task_environment()->FastForwardBy(kPopupTimeOutDuration);
  histograms.ExpectBucketCount(kSystemNotificationPopupDismissedAfter7s,
                               catalog_name, 1);
}

TEST_F(MessageCenterMetricsUtilsTest, RecordCataloguedClickActionButton) {
  base::HistogramTester histograms;
  auto buttons = {message_center::ButtonInfo(u"Button 1"),
                  message_center::ButtonInfo(u"Button 2"),
                  message_center::ButtonInfo(u"Button 3"),
                  message_center::ButtonInfo(u"Button 4")};

  // Create a non-pinned system notification with a valid catalog name.
  const NotificationCatalogName catalog_name =
      NotificationCatalogName::kTestCatalogName;
  auto notification = CreateNotificationWithCatalogName(catalog_name);
  notification->set_buttons(buttons);

  auto* message_center = message_center::MessageCenter::Get();
  message_center->AddNotification(
      std::make_unique<message_center::Notification>(*notification));

  auto* notification_view = GetPopupNotificationView(notification->id());

  for (unsigned int index = 0; index < buttons.size(); index++) {
    ClickView(GetActionButtonFromIndex(notification_view, index));
    histograms.ExpectBucketCount(kSystemNotificationClickedOnActionButton +
                                     base::NumberToString(index + 1),
                                 catalog_name, 1);
  }
}

}  // namespace ash
