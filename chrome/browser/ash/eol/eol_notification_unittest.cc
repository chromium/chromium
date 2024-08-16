// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/eol/eol_notification.h"

#include "ash/constants/ash_features.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/test/simple_test_clock.h"
#include "base/time/time.h"
#include "chrome/browser/ash/extended_updates/test/mock_extended_updates_controller.h"
#include "chrome/browser/ash/extended_updates/test/scoped_extended_updates_controller.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/notifications/notification_handler.h"
#include "chrome/browser/notifications/system_notification_helper.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/dbus/concierge/concierge_client.h"
#include "chromeos/ash/components/dbus/update_engine/fake_update_engine_client.h"
#include "chromeos/ash/components/dbus/update_engine/update_engine_client.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/chromeos/devicetype_utils.h"
#include "ui/message_center/public/cpp/notification_types.h"

namespace ash {

class EolNotificationTest : public BrowserWithTestWindowTest,
                            public testing::WithParamInterface<bool> {
 public:
  EolNotificationTest() = default;
  ~EolNotificationTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitWithFeatureState(
        features::kSuppressFirstEolWarning, SuppressFirstWarningEnabled());

    fake_update_engine_client_ = UpdateEngineClient::InitializeFakeForTest();
    ConciergeClient::InitializeFake(/*fake_cicerone_client=*/nullptr);
    BrowserWithTestWindowTest::SetUp();

    TestingBrowserProcess::GetGlobal()->SetSystemNotificationHelper(
        std::make_unique<SystemNotificationHelper>());
    tester_ = std::make_unique<NotificationDisplayServiceTester>(profile());

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
    fake_update_engine_client_ = nullptr;
    BrowserWithTestWindowTest::TearDown();
    ConciergeClient::Shutdown();
    UpdateEngineClient::Shutdown();
  }

  void DismissNotification() {
    eol_notification_->Click(EolNotification::ButtonIndex::BUTTON_DISMISS,
                             std::nullopt);
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

  bool SuppressFirstWarningEnabled() { return GetParam(); }

 protected:
  raw_ptr<FakeUpdateEngineClient> fake_update_engine_client_;
  std::unique_ptr<NotificationDisplayServiceTester> tester_;
  std::unique_ptr<EolNotification> eol_notification_;
  std::unique_ptr<base::SimpleTestClock> clock_;

  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All, EolNotificationTest, testing::Bool());

TEST_P(EolNotificationTest, TestNoNotifciationBeforeEol) {
  SetCurrentTimeToUtc("1 January 2019");
  SetEolDateUtc("1 December 2019");

  CheckEolInfo();
  auto notification = tester_->GetNotification("chrome://product_eol");
  ASSERT_FALSE(notification);
}

TEST_P(EolNotificationTest, TestFirstWarningNotification) {
  SetCurrentTimeToUtc("1 August 2019");
  SetEolDateUtc("1 December 2019");

  CheckEolInfo();
  auto notification = tester_->GetNotification("chrome://product_eol");

  if (SuppressFirstWarningEnabled()) {
    ASSERT_FALSE(notification);
  } else {
    ASSERT_TRUE(notification);
    std::u16string expected_title = u"Updates end December 2019";
    std::u16string expected_message =
        u"You'll still be able to use this Chrome device after that time, but "
        u"it will no longer get automatic software and security updates";
    EXPECT_EQ(notification->title(), expected_title);
    EXPECT_EQ(notification->message(), expected_message);

    DismissNotification();
  }

  SetCurrentTimeToUtc("15 August 2019");
  CheckEolInfo();
  notification = tester_->GetNotification("chrome://product_eol");
  ASSERT_FALSE(notification);
}

TEST_P(EolNotificationTest, TestSecondWarningNotification) {
  SetCurrentTimeToUtc("1 November 2019");
  SetEolDateUtc("1 December 2019");

  CheckEolInfo();
  auto notification = tester_->GetNotification("chrome://product_eol");
  ASSERT_TRUE(notification);

  std::u16string expected_title = u"Updates end December 2019";
  std::u16string expected_message =
      u"You'll still be able to use this Chrome device after that time, but it "
      u"will no longer get automatic software and security updates";
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

TEST_P(EolNotificationTest, TestFinalEolNotification) {
  SetEolDateUtc("1 December 2019");
  SetCurrentTimeToUtc("2 December 2019");

  CheckEolInfo();
  auto notification = tester_->GetNotification("chrome://product_eol");
  ASSERT_TRUE(notification);

  std::u16string expected_title = u"Final software update";
  std::u16string expected_message =
      u"This is the last automatic software and security update for this "
      u"Chrome device. To get future updates, upgrade to a newer model.";
  EXPECT_EQ(notification->title(), expected_title);
  EXPECT_EQ(notification->message(), expected_message);

  DismissNotification();

  SetCurrentTimeToUtc("15 December 2019");
  CheckEolInfo();
  notification = tester_->GetNotification("chrome://product_eol");
  ASSERT_FALSE(notification);
}

TEST_P(EolNotificationTest, TestOnEolDateChangeBeforeFirstWarning) {
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

TEST_P(EolNotificationTest, TestOnEolDateChangeBeforeSecondWarning) {
  SetCurrentTimeToUtc("1 August 2019");
  SetEolDateUtc("1 December 2019");
  CheckEolInfo();
  auto notification = tester_->GetNotification("chrome://product_eol");
  if (SuppressFirstWarningEnabled()) {
    ASSERT_FALSE(notification);
  } else {
    ASSERT_TRUE(notification);

    // Dismiss first warning notification.
    DismissNotification();
  }

  CheckEolInfo();
  notification = tester_->GetNotification("chrome://product_eol");
  ASSERT_FALSE(notification);

  // In practice, such a small change in date should not happen.
  SetEolDateUtc("2 December 2019");
  CheckEolInfo();
  notification = tester_->GetNotification("chrome://product_eol");

  if (SuppressFirstWarningEnabled()) {
    ASSERT_FALSE(notification);
  } else {
    ASSERT_TRUE(notification);
  }
}

TEST_P(EolNotificationTest, TestOnEolDateChangeBeforeFinalWarning) {
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

TEST_P(EolNotificationTest, TestOnEolDateChangedAfterFinalWarning) {
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

TEST_P(EolNotificationTest, TestNotificationUpdatesProperlyWithoutDismissal) {
  SetCurrentTimeToUtc("1 August 2019");
  SetEolDateUtc("1 December 2019");
  CheckEolInfo();
  auto notification = tester_->GetNotification("chrome://product_eol");

  if (SuppressFirstWarningEnabled()) {
    ASSERT_FALSE(notification);
  } else {
    ASSERT_TRUE(notification);
    std::u16string expected_title = u"Updates end December 2019";
    std::u16string expected_message =
        u"You'll still be able to use this Chrome device after that time, but "
        u"it will no longer get automatic software and security updates";
    EXPECT_EQ(notification->title(), expected_title);
    EXPECT_EQ(notification->message(), expected_message);
  }

  // EOL date arrives and the user has not dismissed the notification.
  SetCurrentTimeToUtc("1 December 2019");
  CheckEolInfo();
  notification = tester_->GetNotification("chrome://product_eol");
  ASSERT_TRUE(notification);
  std::u16string expected_title = u"Final software update";
  std::u16string expected_message =
      u"This is the last automatic software and security update for this "
      u"Chrome device. To get future updates, upgrade to a newer model.";
  EXPECT_EQ(notification->title(), expected_title);
  EXPECT_EQ(notification->message(), expected_message);

  DismissNotification();
  notification = tester_->GetNotification("chrome://product_eol");
  ASSERT_FALSE(notification);
}

TEST_P(EolNotificationTest, TestBackwardsCompatibilityFinalUpdateAlreadyShown) {
  SetEolDateUtc("1 December 2019");
  SetCurrentTimeToUtc("2 December 2019");

  // User dismissed Final Update notification prior to the addition of
  // first and second warning notifications.
  profile()->GetPrefs()->SetBoolean(prefs::kEolNotificationDismissed, true);

  CheckEolInfo();
  auto notification = tester_->GetNotification("chrome://product_eol");
  ASSERT_FALSE(notification);
}

TEST_P(EolNotificationTest, TestOnEolInfoCallsExtendedUpdatesController) {
  SetCurrentTimeToUtc("10 October 2024");

  base::Time eol_date, extended_date;
  ASSERT_TRUE(base::Time::FromUTCString("25 December 2025", &eol_date));
  ASSERT_TRUE(base::Time::FromUTCString("8 June 2024", &extended_date));
  const UpdateEngineClient::EolInfo eol_info{
      .eol_date = eol_date,
      .extended_date = extended_date,
      .extended_opt_in_required = true,
  };
  fake_update_engine_client_->set_eol_info(eol_info);

  ::testing::StrictMock<MockExtendedUpdatesController> mock_controller;
  EXPECT_CALL(mock_controller, OnEolInfo(profile(), eol_info)).Times(1);

  ScopedExtendedUpdatesController scoped_controller(&mock_controller);

  CheckEolInfo();
  auto notification = tester_->GetNotification("chrome://product_eol");
  EXPECT_FALSE(notification);
}

class EolIncentiveNotificationTest : public EolNotificationTest {
 public:
  void SetUp() override {
    EolNotificationTest::SetUp();

    // Set the profile creation date to be at least 6 months before the current
    // time set in these unittests, to correctly show the incentive.
    base::Time creation_time;
    ASSERT_TRUE(base::Time::FromUTCString("1 February 2023", &creation_time));
    profile()->SetCreationTimeForTesting(creation_time);

    scoped_feature_list_.InitAndEnableFeature(ash::features::kEolIncentive);
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All, EolIncentiveNotificationTest, testing::Bool());

TEST_P(EolIncentiveNotificationTest, TestIncentiveFarBeforeEolDate) {
  SetCurrentTimeToUtc("1 January 2023");
  SetEolDateUtc("1 December 2023");

  CheckEolInfo();
  auto notification = tester_->GetNotification("chrome://product_eol");
  ASSERT_FALSE(notification);
}

TEST_P(EolIncentiveNotificationTest, TestIncentiveBeforeEolDate) {
  SetCurrentTimeToUtc("1 November 2023");
  SetEolDateUtc("1 December 2023");

  CheckEolInfo();
  auto notification = tester_->GetNotification("chrome://product_eol");
  ASSERT_TRUE(notification);

  DismissNotification();
  ASSERT_TRUE(profile()->GetPrefs()->GetBoolean(
      prefs::kEolApproachingIncentiveNotificationDismissed));
}

TEST_P(EolIncentiveNotificationTest, TestIncentiveOnEolDate) {
  SetCurrentTimeToUtc("1 December 2023");
  SetEolDateUtc("1 December 2023");

  CheckEolInfo();
  auto notification = tester_->GetNotification("chrome://product_eol");
  ASSERT_TRUE(notification);

  DismissNotification();
  ASSERT_TRUE(profile()->GetPrefs()->GetBoolean(
      prefs::kEolPassedFinalIncentiveDismissed));
}

TEST_P(EolIncentiveNotificationTest, TestIncentiveAfterEolDate) {
  SetCurrentTimeToUtc("3 December 2023");
  SetEolDateUtc("1 December 2023");

  CheckEolInfo();
  auto notification = tester_->GetNotification("chrome://product_eol");
  ASSERT_TRUE(notification);

  DismissNotification();
  ASSERT_TRUE(profile()->GetPrefs()->GetBoolean(
      prefs::kEolPassedFinalIncentiveDismissed));
}

TEST_P(EolIncentiveNotificationTest, TestIncentiveFarAfterEolDate) {
  SetCurrentTimeToUtc("20 December 2023");
  SetEolDateUtc("1 December 2023");

  profile()->GetPrefs()->SetBoolean(prefs::kEolPassedFinalIncentiveDismissed,
                                    true);

  CheckEolInfo();
  auto notification = tester_->GetNotification("chrome://product_eol");
  // Check that no notification is shown far ater EOL date if the final
  // incentive was dismissed.
  ASSERT_FALSE(notification);
}

TEST_P(EolIncentiveNotificationTest,
       TestIncentiveFarAfterEolDateIncentiveNotShown) {
  SetCurrentTimeToUtc("20 December 2023");
  SetEolDateUtc("1 December 2023");

  CheckEolInfo();
  auto notification = tester_->GetNotification("chrome://product_eol");
  // Check that a notification is shown far after EOL date if the final
  // incentive was not dismissed.
  ASSERT_TRUE(notification);

  DismissNotification();
  ASSERT_TRUE(
      profile()->GetPrefs()->GetBoolean(prefs::kEolNotificationDismissed));
}

}  // namespace ash
