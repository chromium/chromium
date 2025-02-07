// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/tailored_security/message_retry_handler.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

class Profile;

namespace safe_browsing {

// The amount of time to wait after construction before checking if a retry is
// needed.
static constexpr const base::TimeDelta kRetryAttemptStartupDelay =
    base::Minutes(2);

// The amount of time to wait between retry attempts.
static constexpr const base::TimeDelta kRetryNextAttemptDelay = base::Days(1);

// Length of time that the retry mechanism will wait before running. This
// delay is used for the case where the service can't tell
// if it succeeded in the past.
static constexpr const base::TimeDelta kWaitingPeriodInterval = base::Days(90);

class MessageRetryHandlerTest : public testing::Test {
 public:
  MessageRetryHandlerTest() = default;
  ~MessageRetryHandlerTest() override = default;

  void SetUp() override { profile_ = std::make_unique<TestingProfile>(); }

  std::unique_ptr<MessageRetryHandler> CreateRetryHandler() {
    // We use tailored security prefs to instantiate the handler here. We can
    // use any customized prefs here.
    return std::make_unique<MessageRetryHandler>(
        profile_.get(), prefs::kTailoredSecuritySyncFlowRetryState,
        prefs::kTailoredSecurityNextSyncFlowTimestamp,
        kRetryAttemptStartupDelay, kRetryNextAttemptDelay,
        kWaitingPeriodInterval, base::DoNothing(),
        "SafeBrowsing.TailoredSecurity.ShouldRetryOutcome",
        prefs::kAccountTailoredSecurityUpdateTimestamp,
        prefs::kEnhancedProtectionEnabledViaTailoredSecurity);
  }

  TestingProfile* profile() { return profile_.get(); }

  sync_preferences::TestingPrefServiceSyncable* prefs() {
    return profile_->GetTestingPrefService();
  }

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<TestingProfile> profile_;
};

TEST_F(MessageRetryHandlerTest, TailoredSecurityUpdateTimeNotSetDoesNotRetry) {
  base::HistogramTester tester;
  auto retry_handler = CreateRetryHandler();
  profile()->GetPrefs()->SetTime(prefs::kAccountTailoredSecurityUpdateTimestamp,
                                 base::Time());
  SetSafeBrowsingState(profile()->GetPrefs(),
                       SafeBrowsingState::STANDARD_PROTECTION);
  profile()->GetPrefs()->SetInteger(prefs::kTailoredSecuritySyncFlowRetryState,
                                    safe_browsing::RETRY_NEEDED);
  profile()->GetPrefs()->SetTime(prefs::kTailoredSecurityNextSyncFlowTimestamp,
                                 base::Time::Now());
  retry_handler->StartRetryTimer();
  task_environment_.FastForwardBy(kRetryAttemptStartupDelay);
  // Check that the "ShouldRetryOutcome" histogram has not recorded any
  // outcome.
  tester.ExpectTotalCount("SafeBrowsing.TailoredSecurity.ShouldRetryOutcome",
                          0);
}

TEST_F(MessageRetryHandlerTest,
       WhenRetryNeededButNotEnoughTimeHasPassedDoesNotRetry) {
  auto retry_handler = CreateRetryHandler();
  profile()->GetPrefs()->SetTime(prefs::kAccountTailoredSecurityUpdateTimestamp,
                                 base::Time::Now());
  SetSafeBrowsingState(profile()->GetPrefs(),
                       SafeBrowsingState::STANDARD_PROTECTION);
  profile()->GetPrefs()->SetInteger(prefs::kTailoredSecuritySyncFlowRetryState,
                                    safe_browsing::RETRY_NEEDED);
  // set next sync flow to after when the retry check will happen.
  profile()->GetPrefs()->SetTime(
      prefs::kTailoredSecurityNextSyncFlowTimestamp,
      base::Time::Now() + kRetryAttemptStartupDelay + base::Seconds(1));
  retry_handler->StartRetryTimer();
  base::HistogramTester tester;
  task_environment_.FastForwardBy(kRetryAttemptStartupDelay);
  tester.ExpectBucketCount(
      "SafeBrowsing.TailoredSecurity.ShouldRetryOutcome",
      MessageRetryHandler::ShouldRetryOutcome::kRetryNeededKeepWaiting, 1);
}

TEST_F(MessageRetryHandlerTest, WhenRetryNeededAndEnoughTimeHasPassedRetries) {
  auto retry_handler = CreateRetryHandler();
  profile()->GetPrefs()->SetTime(prefs::kAccountTailoredSecurityUpdateTimestamp,
                                 base::Time::Now());
  SetSafeBrowsingState(profile()->GetPrefs(),
                       SafeBrowsingState::STANDARD_PROTECTION);
  profile()->GetPrefs()->SetInteger(prefs::kTailoredSecuritySyncFlowRetryState,
                                    safe_browsing::RETRY_NEEDED);
  profile()->GetPrefs()->SetTime(
      prefs::kTailoredSecurityNextSyncFlowTimestamp,
      base::Time::Now() + kRetryAttemptStartupDelay - base::Seconds(1));
  retry_handler->StartRetryTimer();
  base::HistogramTester tester;
  task_environment_.FastForwardBy(kRetryAttemptStartupDelay);
  tester.ExpectBucketCount(
      "SafeBrowsing.TailoredSecurity.ShouldRetryOutcome",
      MessageRetryHandler::ShouldRetryOutcome::kRetryNeededDoRetry, 1);
}

TEST_F(
    MessageRetryHandlerTest,
    WhenRetryNeededAndEnoughTimeHasPassedUpdatesNextSyncFlowTimestampByNextAttemptDelay) {
  auto retry_handler = CreateRetryHandler();
  prefs()->SetTime(prefs::kAccountTailoredSecurityUpdateTimestamp,
                   base::Time::Now());
  SetSafeBrowsingState(prefs(), SafeBrowsingState::STANDARD_PROTECTION);
  prefs()->SetInteger(prefs::kTailoredSecuritySyncFlowRetryState,
                      safe_browsing::RETRY_NEEDED);

  prefs()->SetTime(
      prefs::kTailoredSecurityNextSyncFlowTimestamp,
      base::Time::Now() + kRetryAttemptStartupDelay - base::Seconds(1));
  retry_handler->StartRetryTimer();
  task_environment_.FastForwardBy(kRetryAttemptStartupDelay);

  EXPECT_EQ(prefs()->GetTime(prefs::kTailoredSecurityNextSyncFlowTimestamp),
            base::Time::Now() + kRetryNextAttemptDelay);
}

TEST_F(
    MessageRetryHandlerTest,
    WhenRetryNotSetAndEnhancedProtectionEnabledViaTailoredSecurityDoesNotSetNextSyncFlowTimestamp) {
  auto retry_handler = CreateRetryHandler();
  prefs()->SetTime(prefs::kAccountTailoredSecurityUpdateTimestamp,
                   base::Time::Now());
  SetSafeBrowsingState(prefs(), SafeBrowsingState::STANDARD_PROTECTION);

  prefs()->SetTime(prefs::kTailoredSecurityNextSyncFlowTimestamp, base::Time());
  prefs()->SetInteger(prefs::kTailoredSecuritySyncFlowRetryState,
                      safe_browsing::UNSET);
  prefs()->SetBoolean(prefs::kEnhancedProtectionEnabledViaTailoredSecurity,
                      true);
  retry_handler->StartRetryTimer();
  base::HistogramTester tester;
  task_environment_.FastForwardBy(kRetryAttemptStartupDelay);
  EXPECT_EQ(prefs()->GetTime(prefs::kTailoredSecurityNextSyncFlowTimestamp),
            base::Time());

  tester.ExpectBucketCount(
      "SafeBrowsing.TailoredSecurity.ShouldRetryOutcome",
      MessageRetryHandler::ShouldRetryOutcome::kUnsetInitializeWaitingPeriod,
      0);
}

TEST_F(
    MessageRetryHandlerTest,
    WhenRetryNotSetAndNextSyncFlowNotSetSetsNextSyncFlowToWaitingIntervalFromNow) {
  auto retry_handler = CreateRetryHandler();
  prefs()->SetTime(prefs::kAccountTailoredSecurityUpdateTimestamp,
                   base::Time::Now());
  SetSafeBrowsingState(prefs(), SafeBrowsingState::STANDARD_PROTECTION);

  prefs()->SetInteger(prefs::kTailoredSecuritySyncFlowRetryState,
                      safe_browsing::UNSET);
  prefs()->SetTime(prefs::kTailoredSecurityNextSyncFlowTimestamp, base::Time());
  retry_handler->StartRetryTimer();
  base::HistogramTester tester;
  task_environment_.FastForwardBy(kRetryAttemptStartupDelay);
  EXPECT_EQ(prefs()->GetTime(prefs::kTailoredSecurityNextSyncFlowTimestamp),
            base::Time::Now() + kWaitingPeriodInterval);

  tester.ExpectBucketCount(
      "SafeBrowsing.TailoredSecurity.ShouldRetryOutcome",
      MessageRetryHandler::ShouldRetryOutcome::kUnsetInitializeWaitingPeriod,
      1);
}

TEST_F(MessageRetryHandlerTest,
       WhenRetryNotSetAndNextSyncFlowHasPassedRunsRetry) {
  auto retry_handler = CreateRetryHandler();
  prefs()->SetTime(prefs::kAccountTailoredSecurityUpdateTimestamp,
                   base::Time::Now());
  SetSafeBrowsingState(prefs(), SafeBrowsingState::STANDARD_PROTECTION);

  prefs()->SetInteger(prefs::kTailoredSecuritySyncFlowRetryState,
                      safe_browsing::UNSET);
  prefs()->SetTime(prefs::kTailoredSecurityNextSyncFlowTimestamp,
                   base::Time::Now());
  retry_handler->StartRetryTimer();
  base::HistogramTester tester;
  task_environment_.FastForwardBy(kRetryAttemptStartupDelay);

  tester.ExpectBucketCount(
      "SafeBrowsing.TailoredSecurity.ShouldRetryOutcome",
      MessageRetryHandler::ShouldRetryOutcome::kUnsetRetryBecauseDoneWaiting,
      1);
}

TEST_F(MessageRetryHandlerTest,
       WhenRetryNotSetAndNextSyncFlowHasPassedSetsNextSyncFlowToTomorrow) {
  auto retry_handler = CreateRetryHandler();
  prefs()->SetTime(prefs::kAccountTailoredSecurityUpdateTimestamp,
                   base::Time::Now());
  SetSafeBrowsingState(prefs(), SafeBrowsingState::STANDARD_PROTECTION);

  prefs()->SetInteger(prefs::kTailoredSecuritySyncFlowRetryState,
                      safe_browsing::UNSET);
  prefs()->SetTime(prefs::kTailoredSecurityNextSyncFlowTimestamp,
                   base::Time::Now());
  retry_handler->StartRetryTimer();
  task_environment_.FastForwardBy(kRetryAttemptStartupDelay);

  // Next sync flow time should be tomorrow.
  EXPECT_EQ(prefs()->GetTime(prefs::kTailoredSecurityNextSyncFlowTimestamp),
            base::Time::Now() + kRetryNextAttemptDelay);
}

TEST_F(MessageRetryHandlerTest,
       WhenRetryNotSetAndNextSyncFlowHasNotPassedDoesNotRunRetryLogic) {
  auto retry_handler = CreateRetryHandler();
  prefs()->SetTime(prefs::kAccountTailoredSecurityUpdateTimestamp,
                   base::Time::Now());
  SetSafeBrowsingState(prefs(), SafeBrowsingState::STANDARD_PROTECTION);

  prefs()->SetInteger(prefs::kTailoredSecuritySyncFlowRetryState,
                      safe_browsing::UNSET);
  // Set the next flow time to tomorrow. The logic should not run until then.
  prefs()->SetTime(prefs::kTailoredSecurityNextSyncFlowTimestamp,
                   base::Time::Now() + base::Days(1));
  retry_handler->StartRetryTimer();
  base::HistogramTester tester;
  task_environment_.FastForwardBy(kRetryAttemptStartupDelay);

  tester.ExpectBucketCount(
      "SafeBrowsing.TailoredSecurity.ShouldRetryOutcome",
      MessageRetryHandler::ShouldRetryOutcome::kUnsetStillWaiting, 1);
}

TEST_F(MessageRetryHandlerTest, WhenNoRetryNeededDoesNotRetry) {
  auto retry_handler = CreateRetryHandler();
  prefs()->SetTime(prefs::kAccountTailoredSecurityUpdateTimestamp,
                   base::Time::Now());
  SetSafeBrowsingState(prefs(), SafeBrowsingState::STANDARD_PROTECTION);

  prefs()->SetInteger(prefs::kTailoredSecuritySyncFlowRetryState,
                      safe_browsing::NO_RETRY_NEEDED);
  retry_handler->StartRetryTimer();
  base::HistogramTester tester;
  task_environment_.FastForwardBy(kRetryAttemptStartupDelay);
  // Check that the "ShouldRetryOutcome" histogram has recorded the expected
  // outcome which is 0 because we don't retry here.
  tester.ExpectBucketCount(
      "SafeBrowsing.TailoredSecurity.ShouldRetryOutcome",
      MessageRetryHandler::ShouldRetryOutcome::kRetryNeededDoRetry, 0);
  tester.ExpectBucketCount(
      "SafeBrowsing.TailoredSecurity.ShouldRetryOutcome",
      MessageRetryHandler::ShouldRetryOutcome::kRetryNeededKeepWaiting, 0);
  tester.ExpectBucketCount(
      "SafeBrowsing.TailoredSecurity.ShouldRetryOutcome",
      MessageRetryHandler::ShouldRetryOutcome::kUnsetInitializeWaitingPeriod,
      0);
  tester.ExpectBucketCount(
      "SafeBrowsing.TailoredSecurity.ShouldRetryOutcome",
      MessageRetryHandler::ShouldRetryOutcome::kUnsetRetryBecauseDoneWaiting,
      0);
  tester.ExpectBucketCount(
      "SafeBrowsing.TailoredSecurity.ShouldRetryOutcome",
      MessageRetryHandler::ShouldRetryOutcome::kUnsetStillWaiting, 0);
}
}  // namespace safe_browsing
