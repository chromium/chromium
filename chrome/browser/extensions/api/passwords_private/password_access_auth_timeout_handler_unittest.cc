// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/passwords_private/password_access_auth_timeout_handler.h"

#include <utility>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/sync/base/features.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::TestWithParam;
using ::testing::Values;

namespace extensions {

using MockTimeoutCallback =
    base::MockCallback<PasswordAccessAuthTimeoutHandler::TimeoutCallback>;

class PasswordAccessAuthTimeoutHandlerTest : public TestWithParam<bool> {
 public:
  PasswordAccessAuthTimeoutHandlerTest() = default;

  base::test::TaskEnvironment& task_environment() { return task_environment_; }
  MockTimeoutCallback& timeout_callback() { return timeout_callback_; }
  PasswordAccessAuthTimeoutHandler& handler() { return handler_; }

 protected:
  void SetUp() override {
    if (GetParam()) {
      feature_list.InitAndEnableFeature(syncer::kPasswordNotesWithBackup);
    }
    handler_.Init(timeout_callback_.Get());
  }

  void TearDown() override { feature_list.Reset(); }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  MockTimeoutCallback timeout_callback_;
  PasswordAccessAuthTimeoutHandler handler_;
  base::test::ScopedFeatureList feature_list;
};

TEST_P(PasswordAccessAuthTimeoutHandlerTest, RestartAuthTimer) {
  // Timeout callback will not be run because the timer was not started.
  handler().RestartAuthTimer();
  EXPECT_CALL(timeout_callback(), Run).Times(0);
  task_environment().FastForwardBy(
      PasswordAccessAuthTimeoutHandler::GetAuthValidityPeriod() +
      base::Seconds(1));

  // Timeout callback will not be run because not enough time has passed.
  handler().start_auth_timer(timeout_callback().Get());
  task_environment().FastForwardBy(
      PasswordAccessAuthTimeoutHandler::GetAuthValidityPeriod() -
      base::Seconds(1));

  // Timeout callback will be run.
  handler().RestartAuthTimer();
  EXPECT_CALL(timeout_callback(), Run).Times(1);
  task_environment().FastForwardBy(
      PasswordAccessAuthTimeoutHandler::GetAuthValidityPeriod());
}

INSTANTIATE_TEST_SUITE_P(,
                         PasswordAccessAuthTimeoutHandlerTest,
                         testing::Bool());

}  // namespace extensions
