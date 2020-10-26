// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/muted_notification_handler.h"

#include <string>

#include "base/callback.h"
#include "base/optional.h"
#include "base/test/mock_callback.h"
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

class MutedNotificationHandlerTest : public testing::Test {
 public:
  MutedNotificationHandlerTest() = default;
  ~MutedNotificationHandlerTest() override = default;

  MockMutedNotificationHandlerDelegate& delegate() { return delegate_; }

  MutedNotificationHandler& handler() { return handler_; }

 private:
  MockMutedNotificationHandlerDelegate delegate_;
  MutedNotificationHandler handler_{&delegate_};
};

TEST_F(MutedNotificationHandlerTest, OnUserClose) {
  base::MockCallback<base::OnceClosure> callback;
  EXPECT_CALL(callback, Run());
  EXPECT_CALL(delegate(),
              OnAction(MutedNotificationHandler::Action::kUserClose));
  handler().OnClose(/*profile=*/nullptr, GURL(),
                    /*notification_id=*/std::string(), /*by_user=*/true,
                    callback.Get());
}

TEST_F(MutedNotificationHandlerTest, OnNonUserClose) {
  base::MockCallback<base::OnceClosure> callback;
  EXPECT_CALL(callback, Run());
  EXPECT_CALL(delegate(), OnAction).Times(0);
  handler().OnClose(/*profile=*/nullptr, GURL(),
                    /*notification_id=*/std::string(), /*by_user=*/false,
                    callback.Get());
}

TEST_F(MutedNotificationHandlerTest, OnClickBody) {
  base::MockCallback<base::OnceClosure> callback;
  EXPECT_CALL(callback, Run());
  EXPECT_CALL(delegate(),
              OnAction(MutedNotificationHandler::Action::kBodyClick));
  handler().OnClick(
      /*profile=*/nullptr, GURL(), /*notification_id=*/std::string(),
      /*action_index=*/base::nullopt, /*reply=*/base::nullopt, callback.Get());
}

TEST_F(MutedNotificationHandlerTest, OnClickShow) {
  base::MockCallback<base::OnceClosure> callback;
  EXPECT_CALL(callback, Run());
  EXPECT_CALL(delegate(),
              OnAction(MutedNotificationHandler::Action::kShowClick));
  handler().OnClick(
      /*profile=*/nullptr, GURL(), /*notification_id=*/std::string(),
      /*action_index=*/0, /*reply=*/base::nullopt, callback.Get());
}
