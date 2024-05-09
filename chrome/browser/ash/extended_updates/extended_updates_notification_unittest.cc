// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/extended_updates/extended_updates_notification.h"

#include <algorithm>
#include <optional>

#include "ash/constants/ash_pref_names.h"
#include "ash/system/extended_updates/extended_updates_metrics.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ash/settings/scoped_testing_cros_settings.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/notifications/notification_handler.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/message_center/public/cpp/notification.h"

namespace ash {

namespace {

using IndexedButton = ExtendedUpdatesNotification::IndexedButton;

using ::testing::Eq;

class TestExtendedUpdatesNotification final
    : public ExtendedUpdatesNotification {
 public:
  explicit TestExtendedUpdatesNotification(Profile* profile)
      : ExtendedUpdatesNotification(profile) {}
  TestExtendedUpdatesNotification(const TestExtendedUpdatesNotification&) =
      delete;
  TestExtendedUpdatesNotification& operator=(
      const TestExtendedUpdatesNotification&) = delete;

  MOCK_METHOD(void, ShowExtendedUpdatesDialog, (), (override));
  MOCK_METHOD(void, OpenLearnMoreUrl, (), (override));

 private:
  ~TestExtendedUpdatesNotification() override = default;
};

class ExtendedUpdatesNotificationTest : public testing::Test {
 public:
  ExtendedUpdatesNotificationTest() = default;
  ExtendedUpdatesNotificationTest(const ExtendedUpdatesNotificationTest&) =
      delete;
  ExtendedUpdatesNotificationTest& operator=(
      const ExtendedUpdatesNotificationTest&) = delete;
  ~ExtendedUpdatesNotificationTest() override = default;

 protected:
  scoped_refptr<TestExtendedUpdatesNotification> CreateTestNotification(
      Profile* profile) {
    return base::MakeRefCounted<TestExtendedUpdatesNotification>(profile);
  }

  // Gets the number of notifications that are currently showing.
  int ShowingNotificationCount() {
    return std::ranges::count_if(
        notification_display_service_tester_.GetDisplayedNotificationsForType(
            ExtendedUpdatesNotification::kNotificationType),
        [](const message_center::Notification& note) {
          return note.id() == ExtendedUpdatesNotification::kNotificationId;
        });
  }

  void ClickNotification(std::optional<IndexedButton> button) {
    notification_display_service_tester_.SimulateClick(
        ExtendedUpdatesNotification::kNotificationType,
        std::string(ExtendedUpdatesNotification::kNotificationId),
        button ? std::optional<int>{static_cast<int>(*button)} : std::nullopt,
        /*reply=*/std::nullopt);
  }

  void CloseNotification(bool by_user) {
    notification_display_service_tester_.RemoveNotification(
        ExtendedUpdatesNotification::kNotificationType,
        std::string(ExtendedUpdatesNotification::kNotificationId), by_user,
        /*silent=*/false);
  }

  content::BrowserTaskEnvironment task_environment_;
  ScopedTestingCrosSettings cros_settings_;
  TestingProfile profile_;
  NotificationDisplayServiceTester notification_display_service_tester_{
      &profile_};
};

}  // namespace

TEST_F(ExtendedUpdatesNotificationTest, ProfileDestroyedBeforeShow) {
  auto profile = std::make_unique<TestingProfile>();
  auto note = CreateTestNotification(profile.get());

  profile.reset();
  ExtendedUpdatesNotification::Show(std::move(note));
}

TEST_F(ExtendedUpdatesNotificationTest, ProfileDestroyedAfterShow) {
  auto profile = std::make_unique<TestingProfile>();
  auto note = CreateTestNotification(profile.get());

  ExtendedUpdatesNotification::Show(note);

  profile.reset();
  note->Close(/*by_user=*/true);
}

TEST_F(ExtendedUpdatesNotificationTest, ClickNoButton) {
  ExtendedUpdatesNotification::Show(CreateTestNotification(&profile_));
  EXPECT_THAT(ShowingNotificationCount(), Eq(1));

  ClickNotification(std::nullopt);
  EXPECT_THAT(ShowingNotificationCount(), Eq(1));

  CloseNotification(/*by_user=*/false);
  EXPECT_THAT(ShowingNotificationCount(), Eq(0));
}

TEST_F(ExtendedUpdatesNotificationTest, ShowExtendedUpdatesDialog) {
  base::HistogramTester histogram_tester;
  auto note = CreateTestNotification(&profile_);
  EXPECT_CALL(*note, ShowExtendedUpdatesDialog()).Times(1);

  ExtendedUpdatesNotification::Show(std::move(note));
  EXPECT_THAT(ShowingNotificationCount(), Eq(1));
  histogram_tester.ExpectBucketCount(
      kExtendedUpdatesEntryPointEventMetric,
      ExtendedUpdatesEntryPointEvent::kNoArcNotificationShown, 1);

  ClickNotification(IndexedButton::kSetUp);
  EXPECT_THAT(ShowingNotificationCount(), Eq(0));
  EXPECT_FALSE(ExtendedUpdatesNotification::IsNotificationDismissed(&profile_));
  histogram_tester.ExpectBucketCount(
      kExtendedUpdatesEntryPointEventMetric,
      ExtendedUpdatesEntryPointEvent::kNoArcNotificationClicked, 1);
}

TEST_F(ExtendedUpdatesNotificationTest, OpenLearnMoreUrl) {
  auto note = CreateTestNotification(&profile_);
  EXPECT_CALL(*note, OpenLearnMoreUrl()).Times(1);

  ExtendedUpdatesNotification::Show(std::move(note));
  EXPECT_THAT(ShowingNotificationCount(), Eq(1));

  ClickNotification(IndexedButton::kLearnMore);
  EXPECT_THAT(ShowingNotificationCount(), Eq(0));
  EXPECT_FALSE(ExtendedUpdatesNotification::IsNotificationDismissed(&profile_));
}

TEST_F(ExtendedUpdatesNotificationTest, UserDismiss) {
  ExtendedUpdatesNotification::Show(CreateTestNotification(&profile_));
  EXPECT_THAT(ShowingNotificationCount(), Eq(1));

  CloseNotification(/*by_user=*/true);
  EXPECT_THAT(ShowingNotificationCount(), Eq(0));
  EXPECT_TRUE(ExtendedUpdatesNotification::IsNotificationDismissed(&profile_));
}

TEST_F(ExtendedUpdatesNotificationTest, NonUserDismiss) {
  ExtendedUpdatesNotification::Show(CreateTestNotification(&profile_));
  EXPECT_THAT(ShowingNotificationCount(), Eq(1));

  CloseNotification(/*by_user=*/false);
  EXPECT_THAT(ShowingNotificationCount(), Eq(0));
  EXPECT_FALSE(ExtendedUpdatesNotification::IsNotificationDismissed(&profile_));
}

TEST_F(ExtendedUpdatesNotificationTest, DismissNotificationAfterOptIn) {
  ExtendedUpdatesNotification::Show(CreateTestNotification(&profile_));
  EXPECT_THAT(ShowingNotificationCount(), Eq(1));

  cros_settings_.device_settings()->SetBoolean(kDeviceExtendedAutoUpdateEnabled,
                                               true);
  EXPECT_THAT(ShowingNotificationCount(), Eq(0));
}

TEST_F(ExtendedUpdatesNotificationTest,
       DismissNotificationAfterOptInWithNoNotificationShowing) {
  ExtendedUpdatesNotification::Show(CreateTestNotification(&profile_));
  EXPECT_THAT(ShowingNotificationCount(), Eq(1));

  CloseNotification(/*by_user=*/false);
  EXPECT_THAT(ShowingNotificationCount(), Eq(0));

  cros_settings_.device_settings()->SetBoolean(kDeviceExtendedAutoUpdateEnabled,
                                               true);
  EXPECT_THAT(ShowingNotificationCount(), Eq(0));
}

}  // namespace ash
