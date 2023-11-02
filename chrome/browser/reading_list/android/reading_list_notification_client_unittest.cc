// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/reading_list/android/reading_list_notification_client.h"

#include <memory>
#include <utility>

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "chrome/browser/reading_list/android/reading_list_notification_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using NotificationDataCallback =
    ReadingListNotificationService::NotificationDataCallback;
using ThrottleConfigCallback =
    notifications::NotificationSchedulerClient::ThrottleConfigCallback;
using testing::_;

namespace {

constexpr notifications::SchedulerClientType kNotificationType =
    notifications::SchedulerClientType::kReadingList;

class MockReadingListNotificationService
    : public ReadingListNotificationService {
 public:
  MockReadingListNotificationService() = default;
  ~MockReadingListNotificationService() override = default;
  MOCK_METHOD(void, OnStart, (), (override));
  MOCK_METHOD(void,
              BeforeShowNotification,
              (std::unique_ptr<notifications::NotificationData>,
               NotificationDataCallback),
              (override));
  MOCK_METHOD(void, OnClick, (), (override));
};

class ReadingListNotificationClientTest : public testing::Test {
 public:
  ReadingListNotificationClientTest() = default;
  ~ReadingListNotificationClientTest() override = default;

  void SetUp() override {
    auto service_getter = base::BindLambdaForTesting([&]() {
      return static_cast<ReadingListNotificationService*>(&mock_service_);
    });
    client_ = std::make_unique<ReadingListNotificationClient>(service_getter);
  }

 protected:
  ReadingListNotificationClient* client() { return client_.get(); }
  MockReadingListNotificationService* mock_service() { return &mock_service_; }

 private:
  base::test::TaskEnvironment task_environment_;
  MockReadingListNotificationService mock_service_;
  std::unique_ptr<ReadingListNotificationClient> client_;
};

TEST_F(ReadingListNotificationClientTest, BeforeShowNotification) {
  EXPECT_CALL(*mock_service(), BeforeShowNotification(_, _));
  client()->BeforeShowNotification(
      std::make_unique<notifications::NotificationData>(),
      NotificationDataCallback());
}

TEST_F(ReadingListNotificationClientTest, OnSchedulerInitialized) {
  client()->OnSchedulerInitialized(true, std::set<std::string>());
}

TEST_F(ReadingListNotificationClientTest, OnUserAction) {
  EXPECT_CALL(*mock_service(), OnClick());
  notifications::UserActionData action(
      kNotificationType, notifications::UserActionType::kClick, "1234");
  client()->OnUserAction(action);
}

TEST_F(ReadingListNotificationClientTest, GetThrottleConfig) {
  base::RunLoop loop;
  auto callback = base::BindLambdaForTesting(
      [&](std::unique_ptr<notifications::ThrottleConfig> config) {
        EXPECT_EQ(nullptr, config);
        loop.Quit();
      });
  client()->GetThrottleConfig(callback);
  loop.Run();
}

}  // namespace
