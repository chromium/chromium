// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/internal/clients/finds_client.h"

#include <memory>

#include "base/functional/bind.h"
#include "chrome/browser/notifications/scheduler/public/finds_agent.h"
#include "chrome/browser/notifications/scheduler/public/notification_scheduler_client.h"
#include "chrome/browser/notifications/scheduler/public/notification_scheduler_constant.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace notifications {
namespace {

class MockFindsAgent : public FindsAgent {
 public:
  MOCK_METHOD(void, OpenNotificationUrl, (const GURL& url), (override));
};

}  // namespace

class FindsClientTest : public testing::Test {
 public:
  FindsClientTest() = default;

  void SetUp() override {
    auto mock_finds_agent = std::make_unique<MockFindsAgent>();
    mock_finds_agent_ = mock_finds_agent.get();
    finds_client_ = std::make_unique<FindsClient>(std::move(mock_finds_agent),
                                                  &pref_service_);
  }

 protected:
  NotificationSchedulerClient* finds_client() { return finds_client_.get(); }
  MockFindsAgent* mock_finds_agent() { return mock_finds_agent_; }

 private:
  std::unique_ptr<FindsClient> finds_client_;
  raw_ptr<MockFindsAgent> mock_finds_agent_;
  TestingPrefServiceSimple pref_service_;
};

// Verifies that a click action calls the FindsAgent OpenNotificationUrl
// function.
TEST_F(FindsClientTest, OnUserAction_Click) {
  UserActionData action_data(SchedulerClientType::kChromeFinds,
                             UserActionType::kClick, "guid1");
  const char kTestUrl[] = "https://www.google.com/";
  action_data.custom_data[kChromeFindsNotificationsUrl] = kTestUrl;

  EXPECT_CALL(*mock_finds_agent(), OpenNotificationUrl(GURL(kTestUrl)));
  finds_client()->OnUserAction(action_data);
}

}  // namespace notifications
