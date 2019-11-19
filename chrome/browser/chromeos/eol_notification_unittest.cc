// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/eol_notification.h"

#include "base/test/simple_test_clock.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/notifications/notification_handler.h"
#include "chrome/browser/notifications/system_notification_helper.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_update_engine_client.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/chromeos/devicetype_utils.h"
#include "ui/message_center/public/cpp/notification_types.h"

namespace chromeos {

class EolNotificationTest : public BrowserWithTestWindowTest {
 public:
  EolNotificationTest() = default;
  ~EolNotificationTest() override = default;

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();

    TestingBrowserProcess::GetGlobal()->SetSystemNotificationHelper(
        std::make_unique<SystemNotificationHelper>());
    tester_ = std::make_unique<NotificationDisplayServiceTester>(profile());

    fake_update_engine_client_ = new FakeUpdateEngineClient();
    DBusThreadManager::GetSetterForTesting()->SetUpdateEngineClient(
        base::WrapUnique<UpdateEngineClient>(fake_update_engine_client_));

    eol_notification_ = std::make_unique<EolNotification>(profile());
    clock_ = std::make_unique<base::SimpleTestClock>();
    eol_notification_->clock_ = clock_.get();
  }

  void CheckEolInfo() {
    // The callback passed from |eol_notification_| to
    // |fake_update_engine_client_| should be invoked before a notification can
    // appear.
    eol_notification_->CheckEolInfo();
    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override {
    eol_notification_.reset();
    tester_.reset();
    BrowserWithTestWindowTest::TearDown();
  }

  void DismissNotification() {
    eol_notification_->Click(EolNotification::ButtonIndex::BUTTON_DISMISS,
                             base::nullopt);
  }

  void SetCurrentTimeToUtc(const char* utc_date_string) {
    base::Time utc_time;
    ASSERT_TRUE(base::Time::FromUTCString(utc_date_string, &utc_time));
    clock_->SetNow(utc_time);
  }

  void SetEolDateUtc(const char* utc_date_string) {
    base::Time utc_date;
    ASSERT_TRUE(base::Time::FromUTCString(utc_date_string, &utc_date));
    fake_update_engine_client_->set_eol_date(utc_date);
  }

 protected:
  FakeUpdateEngineClient* fake_update_engine_client_;
  std::unique_ptr<NotificationDisplayServiceTester> tester_;
  std::unique_ptr<EolNotification> eol_notification_;
  std::unique_ptr<base::SimpleTestClock> clock_;
};

TEST_F(EolNotificationTest, TestNoNotifciationBeforeEol) {
  SetCurrentTimeToUtc("1 January 2019");
  SetEolDateUtc("1 December 2019");

  CheckEolInfo();
  auto notification = tester_->GetNotification("chrome://product_eol");
  ASSERT_FALSE(notification);
}

TEST_F(EolNotificationTest, TestFirstWarningNotification) {
  SetCurrentTimeToUtc("1 August 2019");
  SetEolDateUtc("1 December 2019");

  CheckEolInfo();
  auto notification = tester_->GetNotification("chrome://product_eol");
  ASSERT_TRUE(notification);

  base::string16 expected_title =
      base::ASCIIToUTF16("Updates end December 2019");
  base::string16 expected_message = base::ASCIIToUTF16(
      "You'll still be able to use this Chrome device after that time, but it "
      "will no longer get automatic software and security updates");
  EXPECT_EQ(notification->title(), expected_title);
  EXPECT_EQ(notification->message(), expected_message);

  DismissNotification();

  SetCurrentTimeToUtc("15 August 2019");
  CheckEolInfo();
  notification = tester_->GetNotification("chrome://product_eol");
  ASSERT_FALSE(notification);
}

TEST_F(EolNotificationTest, TestSecondWarningNotification) {
  SetCurrentTimeToUtc("1 November 2019");
  SetEolDateUtc("1 December 2019");

  CheckEolInfo();
  auto notification = tester_->GetNotification("chrome://product_eol");
  ASSERT_TRUE(notification);

  base::string16 expected_title =
      base::ASCIIToUTF16("Updates end December 2019");
  base::string16 expected_message = base::ASCIIToUTF16(
      "You'll still be able to use this Chrome device after that time, but it "
      "will no longer get automatic software and security updates");
  EXPECT_EQ(notification->title(), expected_title);
  EXPECT_EQ(notification->message(), expected_message);

  SetCurrentTimeToUtc("1 October 2019");
  CheckEolInfo();
  notification = tester_->GetNotification("chrome://product_eol");
  ASSERT_TRUE(notification);

  DismissNotification();

  SetCurrentTimeToUtc("15 November 2019");
  CheckEolInfo();
  notification = tester_->GetNotification("chrome://product_eol");
  ASSERT_FALSE(notification);
}

TEST_F(EolNotificationTest, TestFinalEolNotification) {
  SetEolDateUtc("1 December 2019");
  SetCurrentTimeToUtc("2 December 2019");

  CheckEolInfo();
  auto notification = tester_->GetNotification("chrome://product_eol");
  ASSERT_TRUE(notification);

  base::string16 expected_title = base::ASCIIToUTF16("Final software update");
  base::string16 expected_message = base::ASCIIToUTF16(
      "This is the last automatic software and security update for this Chrome "
      "device. To get future updates, upgrade to a newer model.");
  EXPECT_EQ(notification->title(), expected_title);
  EXPECT_EQ(notification->message(), expected_message);

  DismissNotification();

  SetCurrentTimeToUtc("15 December 2019");
  CheckEolInfo();
  notification = tester_->GetNotification("chrome://product_eol");
  ASSERT_FALSE(notification);
}

TEST_F(EolNotificationTest, TestOnEolDateChangeBeforeFirstWarning) {
  SetCurrentTimeToUtc("1 January 2019");
  SetEolDateUtc("1 December 2019");
  CheckEolInfo();
  auto notification = tester_->GetNotification("chrome://product_eol");
  ASSERT_FALSE(notification);

  SetCurrentTimeToUtc("1 January 2019");
  SetEolDateUtc("1 November 2019");
  CheckEolInfo();
  notification = tester_->GetNotification("chrome://product_eol");
  ASSERT_FALSE(notification);
}

TEST_F(EolNotificationTest, TestOnEolDateChangeBeforeSecondWarning) {
  SetCurrentTimeToUtc("1 August 2019");
  SetEolDateUtc("1 December 2019");
  CheckEolInfo();
  auto notification = tester_->GetNotification("chrome://product_eol");
  ASSERT_TRUE(notification);

  // Dismiss first warning notification.
  DismissNotification();
  CheckEolInfo();
  notification = tester_->GetNotification("chrome://product_eol");
  ASSERT_FALSE(notification);

  // In practice, such a small change in date should not happen.
  SetEolDateUtc("2 December 2019");
  CheckEolInfo();
  notification = tester_->GetNotification("chrome://product_eol");
  ASSERT_TRUE(notification);
}

TEST_F(EolNotificationTest, TestOnEolDateChangeBeforeFinalWarning) {
  SetCurrentTimeToUtc("1 November 2019");
  SetEolDateUtc("1 December 2019");
  CheckEolInfo();
  auto notification = tester_->GetNotification("chrome://product_eol");
  ASSERT_TRUE(notification);

  // Dismiss first warning notification.
  DismissNotification();
  CheckEolInfo();
  notification = tester_->GetNotification("chrome://product_eol");
  ASSERT_FALSE(notification);

  // In practice, such a small change in date should not happen.
  SetEolDateUtc("2 December 2019");
  CheckEolInfo();
  notification = tester_->GetNotification("chrome://product_eol");
  ASSERT_TRUE(notification);
}

TEST_F(EolNotificationTest, TestOnEolDateChangedAfterFinalWarning) {
  SetEolDateUtc("1 December 2019");
  SetCurrentTimeToUtc("3 December 2019");
  CheckEolInfo();
  auto notification = tester_->GetNotification("chrome://product_eol");
  ASSERT_TRUE(notification);

  // Dismiss first warning notification.
  DismissNotification();
  CheckEolInfo();
  notification = tester_->GetNotification("chrome://product_eol");
  ASSERT_FALSE(notification);

  // Refuse to show notification as eol date is still in the past.
  SetEolDateUtc("2 December 2019");
  CheckEolInfo();
  notification = tester_->GetNotification("chrome://product_eol");
  ASSERT_FALSE(notification);

  // Show as eol date is in the future and within first warning range.
  SetEolDateUtc("4 December 2019");
  CheckEolInfo();
  notification = tester_->GetNotification("chrome://product_eol");
  ASSERT_TRUE(notification);
}

TEST_F(EolNotificationTest, TestNotificationUpdatesProperlyWithoutDismissal) {
  SetCurrentTimeToUtc("1 August 2019");
  SetEolDateUtc("1 December 2019");
  CheckEolInfo();
  auto notification = tester_->GetNotification("chrome://product_eol");
  ASSERT_TRUE(notification);

  base::string16 expected_title =
      base::ASCIIToUTF16("Updates end December 2019");
  base::string16 expected_message = base::ASCIIToUTF16(
      "You'll still be able to use this Chrome device after that time, but it "
      "will no longer get automatic software and security updates");
  EXPECT_EQ(notification->title(), expected_title);
  EXPECT_EQ(notification->message(), expected_message);

  // EOL date arrives and the user has not dismissed the notification.
  SetCurrentTimeToUtc("1 December 2019");
  CheckEolInfo();
  notification = tester_->GetNotification("chrome://product_eol");
  ASSERT_TRUE(notification);
  expected_title = base::ASCIIToUTF16("Final software update");
  expected_message = base::ASCIIToUTF16(
      "This is the last automatic software and security update for this Chrome "
      "device. To get future updates, upgrade to a newer model.");
  EXPECT_EQ(notification->title(), expected_title);
  EXPECT_EQ(notification->message(), expected_message);

  DismissNotification();
  notification = tester_->GetNotification("chrome://product_eol");
  ASSERT_FALSE(notification);
}

TEST_F(EolNotificationTest, TestBackwardsCompatibilityFinalUpdateAlreadyShown) {
  SetEolDateUtc("1 December 2019");
  SetCurrentTimeToUtc("2 December 2019");

  // User dismissed Final Update notification prior to the addition of
  // first and second warning notifications.
  profile()->GetPrefs()->SetBoolean(prefs::kEolNotificationDismissed, true);

  CheckEolInfo();
  auto notification = tester_->GetNotification("chrome://product_eol");
  ASSERT_FALSE(notification);
}

}  // namespace chromeos
