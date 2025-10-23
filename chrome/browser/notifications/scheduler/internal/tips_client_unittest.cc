// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/internal/tips_client.h"

#include "base/strings/string_number_conversions.h"
#include "chrome/browser/notifications/scheduler/public/notification_scheduler_client.h"
#include "chrome/browser/notifications/scheduler/public/notification_scheduler_constant.h"
#include "chrome/browser/notifications/scheduler/public/tips_agent.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace notifications {
namespace {

const char kGuid1[] = "guid1";

class MockTipsAgent : public TipsAgent {
 public:
  MOCK_METHOD(void,
              ShowTipsPromo,
              (TipsNotificationsFeatureType feature_type),
              (override));
};

}  // namespace

class TipsClientTest : public testing::Test {
 public:
  void SetUp() override {
    auto mock_tips_agent = std::make_unique<MockTipsAgent>();
    mock_tips_agent_ = mock_tips_agent.get();
    tips_client_ = std::make_unique<TipsClient>(std::move(mock_tips_agent));
  }

 protected:
  NotificationSchedulerClient* tips_client() { return tips_client_.get(); }
  MockTipsAgent* mock_tips_agent() { return mock_tips_agent_; }

 private:
  std::unique_ptr<TipsClient> tips_client_;
  raw_ptr<MockTipsAgent> mock_tips_agent_;
};

// Verifies that a dismiss action is ignored.
TEST_F(TipsClientTest, OnUserActionDismiss) {
  UserActionData action_data(SchedulerClientType::kTips,
                             UserActionType::kDismiss, kGuid1);
  EXPECT_CALL(*mock_tips_agent(), ShowTipsPromo(testing::_)).Times(0);
  tips_client()->OnUserAction(action_data);
}

// Verifies that a valid feature tip is shown.
TEST_F(TipsClientTest, OnUserActionShowFeatureTip) {
  UserActionData action_data(SchedulerClientType::kTips, UserActionType::kClick,
                             kGuid1);
  action_data.custom_data[kTipsNotificationsFeatureType] = base::NumberToString(
      static_cast<int>(TipsNotificationsFeatureType::kEnhancedSafeBrowsing));
  EXPECT_CALL(
      *mock_tips_agent(),
      ShowTipsPromo(TipsNotificationsFeatureType::kEnhancedSafeBrowsing));
  tips_client()->OnUserAction(action_data);
}

}  // namespace notifications
