// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/privacy_hub/privacy_hub_notification.h"

#include "ash/public/cpp/test/test_system_tray_client.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/test/ash_test_base.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/bind.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/message_center_observer.h"

namespace ash {
namespace {

static constexpr char kNotificationId[] = "unit.test";

class FakeSensorDisabledNotificationDelegate
    : public SensorDisabledNotificationDelegate {
 public:
  std::vector<std::u16string> GetAppsAccessingSensor(Sensor sensor) override {
    return apps_;
  }

  void LaunchApp(const std::u16string& app_name) {
    apps_.insert(apps_.begin(), app_name);
  }

  void CloseApp(const std::u16string& app_name) {
    auto it = std::find(apps_.begin(), apps_.end(), app_name);
    if (it != apps_.end()) {
      apps_.erase(it);
    }
  }

 private:
  std::vector<std::u16string> apps_;
};

class RemoveNotificationWaiter : public message_center::MessageCenterObserver {
 public:
  RemoveNotificationWaiter() {
    message_center::MessageCenter::Get()->AddObserver(this);
  }
  ~RemoveNotificationWaiter() override {
    message_center::MessageCenter::Get()->RemoveObserver(this);
  }

  void Wait() { run_loop_.Run(); }

  // message_center::MessageCenterObserver:
  void OnNotificationRemoved(const std::string& notification_id,
                             const bool by_user) override {
    if (notification_id == kNotificationId) {
      run_loop_.Quit();
    }
  }

 private:
  base::RunLoop run_loop_;
};

// A waiter class, once `Wait()` is invoked, waits until a pop up of the
// notification with id `kNotificationId` is closed.
class NotificationPopupWaiter : public message_center::MessageCenterObserver {
 public:
  NotificationPopupWaiter() {
    message_center::MessageCenter::Get()->AddObserver(this);
  }
  ~NotificationPopupWaiter() override {
    message_center::MessageCenter::Get()->RemoveObserver(this);
  }
  NotificationPopupWaiter& operator=(const NotificationPopupWaiter&) = delete;
  NotificationPopupWaiter(const NotificationPopupWaiter&) = delete;

  void Wait() { run_loop_.Run(); }

  // message_center::MessageCenterObserver:
  void OnNotificationPopupShown(const std::string& notification_id,
                                bool mark_notification_as_read) override {
    if (notification_id == kNotificationId) {
      run_loop_.Quit();
    }
  }

 private:
  base::RunLoop run_loop_;
};

message_center::Notification* GetNotification() {
  return message_center::MessageCenter::Get()->FindNotificationById(
      kNotificationId);
}

message_center::Notification* GetPopupNotification() {
  return message_center::MessageCenter::Get()->FindPopupNotificationById(
      kNotificationId);
}

}  // namespace

class PrivacyHubNotificationTest : public AshTestBase {
 public:
  PrivacyHubNotificationTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        notification_(
            kNotificationId,
            IDS_PRIVACY_HUB_MICROPHONE_AND_CAMERA_OFF_NOTIFICATION_TITLE,
            {IDS_PRIVACY_HUB_MICROPHONE_AND_CAMERA_OFF_NOTIFICATION_MESSAGE,
             IDS_PRIVACY_HUB_MICROPHONE_AND_CAMERA_OFF_NOTIFICATION_MESSAGE_WITH_ONE_APP_NAME,
             IDS_PRIVACY_HUB_MICROPHONE_AND_CAMERA_OFF_NOTIFICATION_MESSAGE_WITH_TWO_APP_NAMES},
            {SensorDisabledNotificationDelegate::Sensor::kMicrophone},
            base::MakeRefCounted<PrivacyHubNotificationClickDelegate>(
                base::DoNothing()),
            ash::NotificationCatalogName::kTestCatalogName,
            IDS_PRIVACY_HUB_MICROPHONE_AND_CAMERA_OFF_NOTIFICATION_BUTTON) {}
  ~PrivacyHubNotificationTest() override = default;

  PrivacyHubNotification& notification() { return notification_; }

  FakeSensorDisabledNotificationDelegate& sensor_delegate() {
    return sensor_delegate_;
  }

 private:
  FakeSensorDisabledNotificationDelegate sensor_delegate_;
  PrivacyHubNotification notification_;
};

using PrivacyHubNotificationClickDelegateTest = AshTestBase;

TEST_F(PrivacyHubNotificationClickDelegateTest, Click) {
  size_t button_clicked = 0;
  size_t message_clicked = 0;
  scoped_refptr<PrivacyHubNotificationClickDelegate> delegate =
      base::MakeRefCounted<PrivacyHubNotificationClickDelegate>(
          base::BindLambdaForTesting(
              [&button_clicked]() { button_clicked++; }));

  ASSERT_EQ(GetSystemTrayClient()->show_os_settings_privacy_hub_count(), 0);

  // Without callback only Privacy Hub should be opened when clicking on the
  // message.
  delegate->Click(absl::nullopt, absl::nullopt);

  EXPECT_EQ(GetSystemTrayClient()->show_os_settings_privacy_hub_count(), 1);
  EXPECT_EQ(button_clicked, 0u);
  EXPECT_EQ(message_clicked, 0u);

  // Click the button.
  delegate->Click(0, absl::nullopt);

  EXPECT_EQ(GetSystemTrayClient()->show_os_settings_privacy_hub_count(), 1);
  EXPECT_EQ(button_clicked, 1u);
  EXPECT_EQ(message_clicked, 0u);

  // Add a message callback.
  delegate->SetMessageClickCallback(
      base::BindLambdaForTesting([&message_clicked]() { message_clicked++; }));

  // When clicking the button, only the button callback should be executed.
  delegate->Click(0, absl::nullopt);

  EXPECT_EQ(GetSystemTrayClient()->show_os_settings_privacy_hub_count(), 1);
  EXPECT_EQ(button_clicked, 2u);
  EXPECT_EQ(message_clicked, 0u);

  // Clicking the message should open Privacy Hub and execute the message
  // callback.
  delegate->Click(absl::nullopt, absl::nullopt);

  EXPECT_EQ(GetSystemTrayClient()->show_os_settings_privacy_hub_count(), 2);
  EXPECT_EQ(button_clicked, 2u);
  EXPECT_EQ(message_clicked, 1u);
}

TEST_F(PrivacyHubNotificationTest, ShowAndHide) {
  EXPECT_FALSE(GetNotification());

  notification().Show();

  EXPECT_TRUE(GetNotification());

  notification().Hide();

  EXPECT_TRUE(GetNotification());

  RemoveNotificationWaiter waiter;

  waiter.Wait();

  EXPECT_FALSE(GetNotification());
}

TEST_F(PrivacyHubNotificationTest, UpdateNotification) {
  // No notification initially.
  EXPECT_FALSE(GetNotification());
  EXPECT_FALSE(GetPopupNotification());

  notification().Show();
  // The notification should pop up.
  EXPECT_TRUE(GetPopupNotification());

  // Wait until pop up of the notification is closed.
  NotificationPopupWaiter waiter;
  waiter.Wait();
  // The notification pop up should close by now. But the notification should
  // stay in the message center.
  EXPECT_TRUE(GetNotification());
  EXPECT_FALSE(GetPopupNotification());

  notification().Update();
  // The update should be silent. The notification should not pop up but stay in
  // the message center.
  EXPECT_TRUE(GetNotification());
  EXPECT_FALSE(GetPopupNotification());
}

TEST_F(PrivacyHubNotificationTest, WithApps) {
  // No apps -> generic notification text.
  notification().Show();

  message_center::Notification* notification_ptr = GetNotification();
  ASSERT_TRUE(notification_ptr);
  EXPECT_EQ(
      notification_ptr->message(),
      l10n_util::GetStringUTF16(
          IDS_PRIVACY_HUB_MICROPHONE_AND_CAMERA_OFF_NOTIFICATION_MESSAGE));

  // Launch a single app -> notification with message for one app.
  const std::u16string app1 = u"test1";
  sensor_delegate().LaunchApp(app1);
  notification().Show();

  notification_ptr = GetNotification();
  ASSERT_TRUE(notification_ptr);
  EXPECT_EQ(
      notification_ptr->message(),
      l10n_util::GetStringFUTF16(
          IDS_PRIVACY_HUB_MICROPHONE_AND_CAMERA_OFF_NOTIFICATION_MESSAGE_WITH_ONE_APP_NAME,
          app1));

  // Launch a second app -> notification with message for two apps.
  const std::u16string app2 = u"test2";
  sensor_delegate().LaunchApp(app2);
  notification().Show();

  notification_ptr = GetNotification();
  ASSERT_TRUE(notification_ptr);
  EXPECT_EQ(
      l10n_util::GetStringFUTF16(
          IDS_PRIVACY_HUB_MICROPHONE_AND_CAMERA_OFF_NOTIFICATION_MESSAGE_WITH_TWO_APP_NAMES,
          app2, app1),
      notification_ptr->message());

  // More than two apps -> generic notification text.
  const std::u16string app3 = u"test3";
  sensor_delegate().LaunchApp(app3);
  notification().Show();

  notification_ptr = GetNotification();
  ASSERT_TRUE(notification_ptr);
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_PRIVACY_HUB_MICROPHONE_AND_CAMERA_OFF_NOTIFICATION_MESSAGE),
            notification_ptr->message());

  // Close one of the applications -> notification with message for two apps.
  sensor_delegate().CloseApp(app2);
  notification().Update();

  notification_ptr = GetNotification();
  ASSERT_TRUE(notification_ptr);
  EXPECT_EQ(
      l10n_util::GetStringFUTF16(
          IDS_PRIVACY_HUB_MICROPHONE_AND_CAMERA_OFF_NOTIFICATION_MESSAGE_WITH_TWO_APP_NAMES,
          app3, app1),
      notification_ptr->message());
}

}  // namespace ash
