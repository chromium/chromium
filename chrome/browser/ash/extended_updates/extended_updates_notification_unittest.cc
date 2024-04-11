// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/extended_updates/extended_updates_notification.h"

#include <memory>
#include <optional>

#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/message_center/public/cpp/notification.h"

namespace ash {

namespace {

using IndexedButton = ExtendedUpdatesNotification::IndexedButton;

using ::testing::ElementsAre;

class TestExtendedUpdatesNotification
    : public ExtendedUpdatesNotification,
      public base::SupportsWeakPtr<TestExtendedUpdatesNotification> {
 public:
  explicit TestExtendedUpdatesNotification(Profile* profile)
      : ExtendedUpdatesNotification(profile) {}
  TestExtendedUpdatesNotification(const TestExtendedUpdatesNotification&) =
      delete;
  TestExtendedUpdatesNotification& operator=(
      const TestExtendedUpdatesNotification&) = delete;
  ~TestExtendedUpdatesNotification() override = default;

  MOCK_METHOD(void, ShowExtendedUpdatesDialog, (), (override));
  MOCK_METHOD(void, OpenLearnMoreUrl, (), (override));
};

class ExtendedUpdatesNotificationTest
    : public testing::Test,
      public NotificationDisplayService::Observer {
 public:
  ExtendedUpdatesNotificationTest() = default;
  ExtendedUpdatesNotificationTest(const ExtendedUpdatesNotificationTest&) =
      delete;
  ExtendedUpdatesNotificationTest& operator=(
      const ExtendedUpdatesNotificationTest&) = delete;
  ~ExtendedUpdatesNotificationTest() override = default;

  void SetUp() override {
    testing::Test::SetUp();

    obs_.Observe(NotificationDisplayService::GetForProfile(&profile_));
  }

  // NotificationDisplayService::Observer overrides.
  void OnNotificationDisplayed(
      const message_center::Notification& notification,
      const NotificationCommon::Metadata* const metadata) override {
    displayed_notifications_.push_back(notification.id());
  }
  void OnNotificationClosed(const std::string& notification_id) override {}
  void OnNotificationDisplayServiceDestroyed(
      NotificationDisplayService* service) override {}

 protected:
  base::WeakPtr<TestExtendedUpdatesNotification> CreateTestNotification(
      Profile* profile) {
    return (new TestExtendedUpdatesNotification(profile))->AsWeakPtr();
  }

  void ExpectNotificationShown() {
    EXPECT_THAT(displayed_notifications_,
                ElementsAre(ExtendedUpdatesNotification::kNotificationId));
  }

  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  base::ScopedObservation<NotificationDisplayService,
                          NotificationDisplayService::Observer>
      obs_{this};

  std::vector<std::string> displayed_notifications_;
};

}  // namespace

TEST_F(ExtendedUpdatesNotificationTest, ProfileDestroyedBeforeShow) {
  auto profile = std::make_unique<TestingProfile>();
  auto note = CreateTestNotification(profile.get());
  EXPECT_TRUE(note);

  profile.reset();
  EXPECT_FALSE(note);
}

TEST_F(ExtendedUpdatesNotificationTest, ProfileDestroyedAfterShow) {
  auto profile = std::make_unique<TestingProfile>();
  auto note = CreateTestNotification(profile.get());

  note->Show();
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(note);

  profile.reset();
  EXPECT_FALSE(note);
}

TEST_F(ExtendedUpdatesNotificationTest, ClickNoButton) {
  auto note = CreateTestNotification(&profile_);

  note->Show();
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(note);
  ExpectNotificationShown();

  note->Click(std::nullopt, std::nullopt);
  EXPECT_TRUE(note);

  note->Close(/*by_user=*/false);
  EXPECT_FALSE(note);
}

TEST_F(ExtendedUpdatesNotificationTest, ShowExtendedUpdatesDialog) {
  auto note = CreateTestNotification(&profile_);
  EXPECT_CALL(*note, ShowExtendedUpdatesDialog()).Times(1);

  note->Show();
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(note);
  ExpectNotificationShown();

  note->Click(static_cast<int>(IndexedButton::kSetUp), std::nullopt);
  EXPECT_TRUE(note);

  note->Close(/*by_user=*/true);
  EXPECT_FALSE(note);
}

TEST_F(ExtendedUpdatesNotificationTest, OpenLearnMoreUrl) {
  auto note = CreateTestNotification(&profile_);
  EXPECT_CALL(*note, OpenLearnMoreUrl()).Times(1);

  note->Show();
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(note);
  ExpectNotificationShown();

  note->Click(static_cast<int>(IndexedButton::kLearnMore), std::nullopt);
  EXPECT_TRUE(note);

  note->Close(/*by_user=*/true);
  EXPECT_FALSE(note);
}

}  // namespace ash
