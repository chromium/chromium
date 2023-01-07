// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/onboarding_user_activity_counter.h"
#include <memory>
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/ash/login/login_pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/session_manager_types.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

class OnboardingUserActivityCounterTest : public ::testing::Test {
 public:
  void SetUp() override {
    OnboardingUserActivityCounter::RegisterProfilePrefs(prefs_.registry());
    prefs_.registry()->RegisterTimePref(prefs::kOobeOnboardingTime,
                                        base::Time());
  }

 protected:
  void SetUpCounter() {
    base::TimeDelta pref_activity_time = base::Minutes(30);
    base::TimeDelta required_activity_time = pref_activity_time * 2;

    // Mark for start.
    prefs_.SetTimeDelta(prefs::kActivityTimeAfterOnboarding,
                        pref_activity_time);

    session_manager_.SetSessionState(
        session_manager::SessionState::LOGIN_PRIMARY);

    counter_ = std::make_unique<OnboardingUserActivityCounter>(
        &prefs_, required_activity_time,
        base::BindLambdaForTesting([&]() { callback_called_ = true; }),
        env_.GetMockTickClock());

    activity_time_left_ = required_activity_time - pref_activity_time;
  }

  base::test::TaskEnvironment env_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  TestingPrefServiceSimple prefs_;
  session_manager::SessionManager session_manager_;

  std::unique_ptr<OnboardingUserActivityCounter> counter_;
  base::TimeDelta activity_time_left_;
  bool callback_called_ = false;
};

TEST_F(OnboardingUserActivityCounterTest, NoShouldStart) {
  EXPECT_FALSE(OnboardingUserActivityCounter::ShouldStart(&prefs_));
}

TEST_F(OnboardingUserActivityCounterTest, ShouldStart) {
  prefs_.SetTimeDelta(prefs::kActivityTimeAfterOnboarding, base::Minutes(10));
  EXPECT_TRUE(OnboardingUserActivityCounter::ShouldStart(&prefs_));
}

// Do not start the counter if the onboarding happened too early in the past.
TEST_F(OnboardingUserActivityCounterTest, ExpiredAfterOnboarding) {
  prefs_.SetTimeDelta(prefs::kActivityTimeAfterOnboarding, base::Minutes(10));

  base::Time now = base::Time::Now();
  prefs_.SetTime(prefs::kOobeOnboardingTime, now - base::Days(2));
  EXPECT_FALSE(OnboardingUserActivityCounter::ShouldStart(&prefs_));
}

TEST_F(OnboardingUserActivityCounterTest, HappyPath) {
  SetUpCounter();

  EXPECT_FALSE(callback_called_);
  // Should not be started because session is not active.
  env_.FastForwardBy(activity_time_left_);
  EXPECT_FALSE(callback_called_);

  session_manager_.SetSessionState(session_manager::SessionState::ACTIVE);
  env_.FastForwardBy(activity_time_left_);
  EXPECT_TRUE(callback_called_);

  // Should not be marked for start.
  EXPECT_FALSE(OnboardingUserActivityCounter::ShouldStart(&prefs_));
}

TEST_F(OnboardingUserActivityCounterTest, LockScreen) {
  SetUpCounter();
  session_manager_.SetSessionState(session_manager::SessionState::ACTIVE);
  base::TimeDelta pref_activity_time =
      prefs_.GetTimeDelta(prefs::kActivityTimeAfterOnboarding);
  env_.FastForwardBy(activity_time_left_ / 2);

  session_manager_.SetSessionState(session_manager::SessionState::LOCKED);

  // Prefs should be updated on the screen lock.
  EXPECT_EQ(prefs_.GetTimeDelta(prefs::kActivityTimeAfterOnboarding),
            pref_activity_time + activity_time_left_ / 2);

  // Locked state should not be taken into account.
  env_.FastForwardBy(activity_time_left_);
  EXPECT_FALSE(callback_called_);

  session_manager_.SetSessionState(session_manager::SessionState::ACTIVE);
  env_.FastForwardBy(activity_time_left_ / 2);
  EXPECT_TRUE(callback_called_);
  EXPECT_FALSE(OnboardingUserActivityCounter::ShouldStart(&prefs_));
}

TEST_F(OnboardingUserActivityCounterTest, UpdatePrefsOnShutdown) {
  SetUpCounter();
  session_manager_.SetSessionState(session_manager::SessionState::ACTIVE);
  base::TimeDelta pref_activity_time =
      prefs_.GetTimeDelta(prefs::kActivityTimeAfterOnboarding);
  env_.FastForwardBy(activity_time_left_ / 2);
  counter_.reset();

  EXPECT_FALSE(callback_called_);

  EXPECT_EQ(prefs_.GetTimeDelta(prefs::kActivityTimeAfterOnboarding),
            pref_activity_time + activity_time_left_ / 2);
}

TEST_F(OnboardingUserActivityCounterTest,
       ExpiredAfterOnboardingDuringLockedState) {
  SetUpCounter();
  session_manager_.SetSessionState(session_manager::SessionState::LOCKED);

  base::Time now = base::Time::Now();
  prefs_.SetTime(prefs::kOobeOnboardingTime, now - base::Days(2));

  session_manager_.SetSessionState(session_manager::SessionState::ACTIVE);

  env_.FastForwardBy(activity_time_left_);
  EXPECT_FALSE(callback_called_);
  EXPECT_FALSE(OnboardingUserActivityCounter::ShouldStart(&prefs_));
}

}  // namespace ash
