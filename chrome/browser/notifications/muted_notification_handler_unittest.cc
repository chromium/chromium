// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/muted_notification_handler.h"

#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browser_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

class MockMutedNotificationHandlerDelegate
    : public MutedNotificationHandler::Delegate {
 public:
  MockMutedNotificationHandlerDelegate() = default;
  MockMutedNotificationHandlerDelegate(
      const MockMutedNotificationHandlerDelegate&) = delete;
  MockMutedNotificationHandlerDelegate& operator=(
      const MockMutedNotificationHandlerDelegate&) = delete;
  ~MockMutedNotificationHandlerDelegate() override = default;

  // MutedNotificationHandler::Delegate:
  MOCK_METHOD(void, OnAction, (MutedNotificationHandler::Action), (override));
};

class MutedNotificationHandlerTest : public testing::TestWithParam<bool> {
 public:
  MutedNotificationHandlerTest() {
    if (GetParam()) {
      feature_list_.InitAndEnableFeature(
          features::kMuteNotificationSnoozeAction);
    } else {
      feature_list_.InitAndDisableFeature(
          features::kMuteNotificationSnoozeAction);
    }
  }

  ~MutedNotificationHandlerTest() override = default;

  MockMutedNotificationHandlerDelegate& delegate() { return delegate_; }

  MutedNotificationHandler& handler() { return handler_; }

 private:
  base::test::ScopedFeatureList feature_list_;
  MockMutedNotificationHandlerDelegate delegate_;
  MutedNotificationHandler handler_{&delegate_};
};

TEST_P(MutedNotificationHandlerTest, OnUserClose) {
  base::MockCallback<base::OnceClosure> callback;
  EXPECT_CALL(callback, Run());
  EXPECT_CALL(delegate(),
              OnAction(MutedNotificationHandler::Action::kUserClose));
  handler().OnClose(/*profile=*/nullptr, GURL(),
                    /*notification_id=*/std::string(), /*by_user=*/true,
                    callback.Get());
}

TEST_P(MutedNotificationHandlerTest, OnNonUserClose) {
  base::MockCallback<base::OnceClosure> callback;
  EXPECT_CALL(callback, Run());
  EXPECT_CALL(delegate(), OnAction).Times(0);
  handler().OnClose(/*profile=*/nullptr, GURL(),
                    /*notification_id=*/std::string(), /*by_user=*/false,
                    callback.Get());
}

TEST_P(MutedNotificationHandlerTest, OnClickBody) {
  base::MockCallback<base::OnceClosure> callback;
  EXPECT_CALL(callback, Run());
  EXPECT_CALL(delegate(),
              OnAction(MutedNotificationHandler::Action::kBodyClick));
  handler().OnClick(
      /*profile=*/nullptr, GURL(), /*notification_id=*/std::string(),
      /*action_index=*/std::nullopt, /*reply=*/std::nullopt, callback.Get());
}

TEST_P(MutedNotificationHandlerTest, OnClickSnooze) {
  if (!GetParam())
    return;

  base::MockCallback<base::OnceClosure> callback;
  EXPECT_CALL(callback, Run());
  EXPECT_CALL(delegate(),
              OnAction(MutedNotificationHandler::Action::kSnoozeClick));
  handler().OnClick(
      /*profile=*/nullptr, GURL(), /*notification_id=*/std::string(),
      /*action_index=*/0, /*reply=*/std::nullopt, callback.Get());
}

TEST_P(MutedNotificationHandlerTest, OnClickShow) {
  base::MockCallback<base::OnceClosure> callback;
  EXPECT_CALL(callback, Run());
  EXPECT_CALL(delegate(),
              OnAction(MutedNotificationHandler::Action::kShowClick));
  handler().OnClick(
      /*profile=*/nullptr, GURL(), /*notification_id=*/std::string(),
      /*action_index=*/GetParam() ? 1 : 0, /*reply=*/std::nullopt,
      callback.Get());
}

INSTANTIATE_TEST_SUITE_P(, MutedNotificationHandlerTest, testing::Bool());
