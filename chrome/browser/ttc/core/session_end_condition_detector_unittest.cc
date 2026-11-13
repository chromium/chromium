// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/core/session_end_condition_detector.h"

#include <memory>
#include <vector>

#include "base/functional/bind.h"
#include "base/test/power_monitor_test.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/test/base/testing_browser_process.h"
#include "components/account_id/account_id_literal.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/session_manager_types.h"
#include "components/session_manager/test/user_session_test_environment.h"
#include "google_apis/gaia/gaia_id.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace ttc {
namespace {

using ::testing::ElementsAre;
using ::testing::IsEmpty;

#if BUILDFLAG(IS_CHROMEOS)
constexpr AccountId::Literal kPrimaryAccountId =
    AccountId::Literal::FromUserEmailGaiaId("primary@example.com",
                                            GaiaId::Literal("primary-gaia"));
constexpr AccountId::Literal kSecondaryAccountId =
    AccountId::Literal::FromUserEmailGaiaId("secondary@example.com",
                                            GaiaId::Literal("secondary-gaia"));
#endif  // BUILDFLAG(IS_CHROMEOS)

class SessionEndConditionDetectorTest : public testing::Test {
 protected:
  using Reason = SessionEndConditionDetector::Reason;

  base::RepeatingCallback<void(Reason)> RecordReasons() {
    return base::BindRepeating(
        [](std::vector<Reason>* reasons, Reason reason) {
          reasons->push_back(reason);
        },
        &reasons_);
  }

  base::test::TaskEnvironment task_environment_;
  base::test::ScopedPowerMonitorTestSource power_monitor_source_;
  std::vector<Reason> reasons_;
};

TEST_F(SessionEndConditionDetectorTest, SuspendRunsCallback) {
  SessionEndConditionDetector detector(RecordReasons());
  power_monitor_source_.GenerateSuspendEvent();
  EXPECT_THAT(reasons_, ElementsAre(Reason::kSuspend));
}

#if !BUILDFLAG(IS_CHROMEOS)
TEST_F(SessionEndConditionDetectorTest, ScreenLockRunsCallback) {
  SessionEndConditionDetector detector(RecordReasons());
  detector.OnScreenLockChanged(true);
  EXPECT_THAT(reasons_, ElementsAre(Reason::kScreenLock));
}

TEST_F(SessionEndConditionDetectorTest, ScreenUnlockDoesNotRunCallback) {
  SessionEndConditionDetector detector(RecordReasons());
  detector.OnScreenLockChanged(false);
  EXPECT_THAT(reasons_, IsEmpty());
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

TEST_F(SessionEndConditionDetectorTest, SuspendAfterDestructionIsIgnored) {
  {
    SessionEndConditionDetector detector(RecordReasons());
  }
  power_monitor_source_.GenerateSuspendEvent();
  EXPECT_THAT(reasons_, IsEmpty());
}

#if BUILDFLAG(IS_CHROMEOS)
class SessionEndConditionDetectorUserSwitchTest
    : public SessionEndConditionDetectorTest {
 protected:
  void SetUp() override {
    SessionEndConditionDetectorTest::SetUp();
    user_session_test_environment_ =
        std::make_unique<ash::test::UserSessionTestEnvironment>(
            TestingBrowserProcess::GetGlobal()->local_state());
    ASSERT_TRUE(
        user_session_test_environment_->AddRegularUser(kPrimaryAccountId));
    ASSERT_TRUE(
        user_session_test_environment_->AddRegularUser(kSecondaryAccountId));
    user_session_test_environment_->LogIn(kPrimaryAccountId);
  }

  void TearDown() override {
    user_session_test_environment_.reset();
    SessionEndConditionDetectorTest::TearDown();
  }

  std::unique_ptr<ash::test::UserSessionTestEnvironment>
      user_session_test_environment_;
};

TEST_F(SessionEndConditionDetectorUserSwitchTest,
       ActiveUserChangedRunsCallback) {
  user_session_test_environment_->LogIn(kSecondaryAccountId);
  SessionEndConditionDetector detector(RecordReasons());

  session_manager::SessionManager::Get()->SwitchActiveSession(
      kSecondaryAccountId);
  EXPECT_THAT(reasons_, ElementsAre(Reason::kUserSwitch));
}

TEST_F(SessionEndConditionDetectorUserSwitchTest,
       SecondaryLoginScreenRunsCallback) {
  SessionEndConditionDetector detector(RecordReasons());

  session_manager::SessionManager::Get()->SetSessionState(
      session_manager::SessionState::LOGIN_SECONDARY);
  EXPECT_THAT(reasons_, ElementsAre(Reason::kUserSwitch));
}

TEST_F(SessionEndConditionDetectorUserSwitchTest,
       SessionStateLockedRunsScreenLockCallback) {
  SessionEndConditionDetector detector(RecordReasons());

  session_manager::SessionManager::Get()->SetSessionState(
      session_manager::SessionState::LOCKED);
  EXPECT_THAT(reasons_, ElementsAre(Reason::kScreenLock));
}

TEST_F(SessionEndConditionDetectorUserSwitchTest,
       UserSwitchAfterDestructionIsIgnored) {
  user_session_test_environment_->LogIn(kSecondaryAccountId);
  {
    SessionEndConditionDetector detector(RecordReasons());
  }

  session_manager::SessionManager::Get()->SwitchActiveSession(
      kSecondaryAccountId);
  EXPECT_THAT(reasons_, IsEmpty());
}
#endif  // BUILDFLAG(IS_CHROMEOS)

}  // namespace
}  // namespace ttc
