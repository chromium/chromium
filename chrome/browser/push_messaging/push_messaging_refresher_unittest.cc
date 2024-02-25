// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/push_messaging/push_messaging_refresher.h"

#include <stdint.h>

#include <optional>

#include "base/time/time.h"
#include "chrome/browser/push_messaging/push_messaging_app_identifier.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
namespace {

void ExpectAppIdentifiersEqual(const PushMessagingAppIdentifier& a,
                               const PushMessagingAppIdentifier& b) {
  EXPECT_EQ(a.app_id(), b.app_id());
  EXPECT_EQ(a.origin(), b.origin());
  EXPECT_EQ(a.service_worker_registration_id(),
            b.service_worker_registration_id());
  EXPECT_EQ(a.expiration_time(), b.expiration_time());
}

constexpr char kTestOrigin[] = "https://example.com";
constexpr char kTestSenderId[] = "1234567890";
const int64_t kTestServiceWorkerId = 42;

class PushMessagingRefresherTest : public testing::Test {
 protected:
  void SetUp() override {
    old_app_identifier_ = PushMessagingAppIdentifier::Generate(
        GURL(kTestOrigin), kTestServiceWorkerId);
    new_app_identifier_ = PushMessagingAppIdentifier::Generate(
        GURL(kTestOrigin), kTestServiceWorkerId);
  }

  Profile* profile() { return &profile_; }

  PushMessagingRefresher* refresher() { return &refresher_; }

  std::optional<PushMessagingAppIdentifier> old_app_identifier_;
  std::optional<PushMessagingAppIdentifier> new_app_identifier_;

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  PushMessagingRefresher refresher_;
};

TEST_F(PushMessagingRefresherTest, GotMessageThroughNewSubscription) {
  refresher()->Refresh(old_app_identifier_.value(),
                       new_app_identifier_.value().app_id(), kTestSenderId);
  refresher()->GotMessageFrom(new_app_identifier_.value().app_id());
  auto app_identifier = refresher()->FindActiveAppIdentifier(
      old_app_identifier_.value().app_id());
  EXPECT_FALSE(app_identifier.has_value());
}

TEST_F(PushMessagingRefresherTest, LookupOldSubscription) {
  refresher()->Refresh(old_app_identifier_.value(),
                       new_app_identifier_.value().app_id(), kTestSenderId);
  {
    std::optional<PushMessagingAppIdentifier> found_old_app_identifier =
        refresher()->FindActiveAppIdentifier(
            old_app_identifier_.value().app_id());
    EXPECT_TRUE(found_old_app_identifier.has_value());
    ExpectAppIdentifiersEqual(old_app_identifier_.value(),
                              found_old_app_identifier.value());
  }
  refresher()->OnUnsubscribed(old_app_identifier_.value().app_id());
  {
    std::optional<PushMessagingAppIdentifier> found_after_unsubscribe =
        refresher()->FindActiveAppIdentifier(
            old_app_identifier_.value().app_id());
    EXPECT_FALSE(found_after_unsubscribe.has_value());
  }
  EXPECT_EQ(0u, refresher()->GetCount());
}

}  // namespace
