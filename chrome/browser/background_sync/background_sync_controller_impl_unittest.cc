// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"
#include "chrome/browser/background_sync/background_sync_delegate_impl.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/background_sync/background_sync_controller_impl.h"
#include "components/history/core/browser/history_database_params.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/test/test_history_database.h"
#include "components/site_engagement/content/site_engagement_score.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "components/variations/variations_associated_data.h"
#include "content/public/browser/background_sync_parameters.h"
#include "content/public/browser/background_sync_registration.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/background_sync/background_sync.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/android/background_sync_launcher_android.h"
#endif

namespace {

using content::BackgroundSyncController;

const char kFieldTrialGroup[] = "GroupA";
const char kExampleUrl[] = "https://www.example.com/foo/";
const char kTag[] = "test_tag";

constexpr base::TimeDelta kSmallerThanMinGap = base::Hours(11);
constexpr base::TimeDelta kLargerThanMinGap = base::Hours(13);
constexpr base::TimeDelta kLargerThanMinGapExpectedDelay = base::Hours(24);

std::unique_ptr<KeyedService> BuildTestHistoryService(
    const base::FilePath& file_path,
    content::BrowserContext* context) {
  auto service = std::make_unique<history::HistoryService>();
  service->Init(history::TestHistoryDatabaseParamsForPath(file_path));
  return service;
}

class BackgroundSyncControllerImplTest : public testing::Test {
 public:
  BackgroundSyncControllerImplTest(const BackgroundSyncControllerImplTest&) =
      delete;
  BackgroundSyncControllerImplTest& operator=(
      const BackgroundSyncControllerImplTest&) = delete;

 protected:
  BackgroundSyncControllerImplTest()
      : task_environment_(content::BrowserTaskEnvironment::IO_MAINLOOP) {
    ResetFieldTrialList();
#if BUILDFLAG(IS_ANDROID)
    BackgroundSyncLauncherAndroid::SetPlayServicesVersionCheckDisabledForTests(
        true);
#endif
  }

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    base::FilePath sub_dir =
        temp_dir_.GetPath().AppendASCII("BackgroundSyncTest");
    ASSERT_TRUE(base::CreateDirectory(sub_dir));

    HistoryServiceFactory::GetInstance()->SetTestingFactory(
        &profile_, base::BindRepeating(&BuildTestHistoryService, sub_dir));
    controller_ = std::make_unique<BackgroundSyncControllerImpl>(
        &profile_, std::make_unique<BackgroundSyncDelegateImpl>(&profile_));
  }

  void ResetFieldTrialList() {
    variations::testing::ClearAllVariationParams();
    base::FieldTrialList::CreateFieldTrial(
        BackgroundSyncControllerImpl::kFieldTrialName, kFieldTrialGroup);
  }

  content::BackgroundSyncRegistration MakeBackgroundSyncRegistration(
      int min_interval,
      int num_attempts,
      blink::mojom::BackgroundSyncType sync_type) {
    url::Origin origin = url::Origin::Create(GURL(kExampleUrl));
    content::BackgroundSyncRegistration registration;
    registration.set_origin(origin);

    blink::mojom::SyncRegistrationOptions options(kTag, min_interval);
    *registration.options() = std::move(options);
    registration.set_num_attempts(num_attempts);

    return registration;
  }

  base::ScopedTempDir temp_dir_;
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  std::unique_ptr<BackgroundSyncControllerImpl> controller_;
};

TEST_F(BackgroundSyncControllerImplTest, NoFieldTrial) {
  content::BackgroundSyncParameters original;
  content::BackgroundSyncParameters overrides;
  controller_->GetParameterOverrides(&overrides);
  EXPECT_EQ(original, overrides);
}

TEST_F(BackgroundSyncControllerImplTest, SomeParamsSet) {
  std::map<std::string, std::string> field_parameters;
  field_parameters[BackgroundSyncControllerImpl::kDisabledParameterName] =
      "TrUe";
  field_parameters[BackgroundSyncControllerImpl::kInitialRetryParameterName] =
      "100";
  ASSERT_TRUE(base::AssociateFieldTrialParams(
      BackgroundSyncControllerImpl::kFieldTrialName, kFieldTrialGroup,
      field_parameters));

  content::BackgroundSyncParameters original;
  content::BackgroundSyncParameters sync_parameters;
  controller_->GetParameterOverrides(&sync_parameters);
  EXPECT_TRUE(sync_parameters.disable);
  EXPECT_EQ(base::Seconds(100), sync_parameters.initial_retry_delay);

  EXPECT_EQ(original.max_sync_attempts, sync_parameters.max_sync_attempts);
  EXPECT_EQ(original.retry_delay_factor, sync_parameters.retry_delay_factor);
  EXPECT_EQ(original.min_sync_recovery_time,
            sync_parameters.min_sync_recovery_time);
  EXPECT_EQ(original.max_sync_event_duration,
            sync_parameters.max_sync_event_duration);
  EXPECT_EQ(original.min_periodic_sync_events_interval,
            sync_parameters.min_periodic_sync_events_interval);
}

TEST_F(BackgroundSyncControllerImplTest, AllParamsSet) {
  std::map<std::string, std::string> field_parameters;
  field_parameters[BackgroundSyncControllerImpl::kDisabledParameterName] =
      "FALSE";
  field_parameters[BackgroundSyncControllerImpl::kInitialRetryParameterName] =
      "100";
  field_parameters[BackgroundSyncControllerImpl::kMaxAttemptsParameterName] =
      "200";
  field_parameters[BackgroundSyncControllerImpl::
                       kMaxAttemptsWithNotificationPermissionParameterName] =
      "250";
  field_parameters
      [BackgroundSyncControllerImpl::kRetryDelayFactorParameterName] = "300";
  field_parameters[BackgroundSyncControllerImpl::kMinSyncRecoveryTimeName] =
      "400";
  field_parameters[BackgroundSyncControllerImpl::kMaxSyncEventDurationName] =
      "500";
  field_parameters
      [BackgroundSyncControllerImpl::kMinPeriodicSyncEventsInterval] = "43200";
  ASSERT_TRUE(base::AssociateFieldTrialParams(
      BackgroundSyncControllerImpl::kFieldTrialName, kFieldTrialGroup,
      field_parameters));

  content::BackgroundSyncParameters sync_parameters;
  controller_->GetParameterOverrides(&sync_parameters);

  EXPECT_FALSE(sync_parameters.disable);
  EXPECT_EQ(base::Seconds(100), sync_parameters.initial_retry_delay);
  EXPECT_EQ(200, sync_parameters.max_sync_attempts);
  EXPECT_EQ(250,
            sync_parameters.max_sync_attempts_with_notification_permission);
  EXPECT_EQ(300, sync_parameters.retry_delay_factor);
  EXPECT_EQ(base::Seconds(400), sync_parameters.min_sync_recovery_time);
  EXPECT_EQ(base::Seconds(500), sync_parameters.max_sync_event_duration);
  EXPECT_EQ(base::Seconds(43200),
            sync_parameters.min_periodic_sync_events_interval);
}

TEST_F(BackgroundSyncControllerImplTest, OneShotSyncMultipleAttempts) {
  content::BackgroundSyncParameters sync_parameters;
  url::Origin origin = url::Origin::Create(GURL(kExampleUrl));
  site_engagement::SiteEngagementScore::SetParamValuesForTesting();
  site_engagement::SiteEngagementService::Get(&profile_)->ResetBaseScoreForURL(
      GURL(kExampleUrl),
      site_engagement::SiteEngagementScore::GetHighEngagementBoundary());

  base::TimeDelta delay = controller_->GetNextEventDelay(
      MakeBackgroundSyncRegistration(
          /* min_interval= */ -1,
          /* num_attempts= */ 0, blink::mojom::BackgroundSyncType::ONE_SHOT),
      &sync_parameters, base::TimeDelta::Max());
  EXPECT_EQ(delay, base::TimeDelta());

  base::TimeDelta delay_after_attempt1 = controller_->GetNextEventDelay(
      MakeBackgroundSyncRegistration(
          /* min_interval= */ -1,
          /* num_attempts= */ 1, blink::mojom::BackgroundSyncType::ONE_SHOT),
      &sync_parameters, base::TimeDelta::Max());
  EXPECT_EQ(delay_after_attempt1, sync_parameters.initial_retry_delay);

  base::TimeDelta delay_after_attempt2 = controller_->GetNextEventDelay(
      MakeBackgroundSyncRegistration(
          /* min_interval= */ -1,
          /* num_attempts= */ 2, blink::mojom::BackgroundSyncType::ONE_SHOT),
      &sync_parameters, base::TimeDelta::Max());
  EXPECT_LT(delay_after_attempt1, delay_after_attempt2);
}

TEST_F(BackgroundSyncControllerImplTest, PeriodicSyncMultipleAttempts) {
  content::BackgroundSyncParameters sync_parameters;
  url::Origin origin = url::Origin::Create(GURL(kExampleUrl));
  site_engagement::SiteEngagementScore::SetParamValuesForTesting();
  site_engagement::SiteEngagementService::Get(&profile_)->ResetBaseScoreForURL(
      GURL(kExampleUrl),
      site_engagement::SiteEngagementScore::GetHighEngagementBoundary());

  base::TimeDelta delay = controller_->GetNextEventDelay(
      MakeBackgroundSyncRegistration(
          /* min_interval= */ kSmallerThanMinGap.InMilliseconds(),
          /* num_attempts= */ 0, blink::mojom::BackgroundSyncType::PERIODIC),
      &sync_parameters, base::TimeDelta::Max());
  EXPECT_EQ(delay, sync_parameters.min_periodic_sync_events_interval);

  base::TimeDelta delay_after_attempt1 = controller_->GetNextEventDelay(
      MakeBackgroundSyncRegistration(
          /* min_interval= */ kSmallerThanMinGap.InMilliseconds(),
          /* num_attempts= */ 1, blink::mojom::BackgroundSyncType::PERIODIC),
      &sync_parameters, base::TimeDelta::Max());
  EXPECT_EQ(delay_after_attempt1, sync_parameters.initial_retry_delay);

  base::TimeDelta delay_after_attempt2 = controller_->GetNextEventDelay(
      MakeBackgroundSyncRegistration(
          /* min_interval= */ kSmallerThanMinGap.InMilliseconds(),
          /* num_attempts= */ 2, blink::mojom::BackgroundSyncType::PERIODIC),
      &sync_parameters, base::TimeDelta::Max());
  EXPECT_LT(delay_after_attempt1, delay_after_attempt2);
}

TEST_F(BackgroundSyncControllerImplTest,
       GetNextEventDelayWithSiteEngagementPenalty) {
  content::BackgroundSyncParameters sync_parameters;
  int64_t min_gap_between_periodic_sync_events_ms =
      sync_parameters.min_periodic_sync_events_interval.InMilliseconds();
  content::BackgroundSyncRegistration registration =
      MakeBackgroundSyncRegistration(
          min_gap_between_periodic_sync_events_ms,
          /*num_attempts= */ 0, blink::mojom::BackgroundSyncType::PERIODIC);

  site_engagement::SiteEngagementScore::SetParamValuesForTesting();
  site_engagement::SiteEngagementService::Get(&profile_)->ResetBaseScoreForURL(
      GURL(kExampleUrl),
      site_engagement::SiteEngagementScore::GetMediumEngagementBoundary());

  // Medium engagement.
  base::TimeDelta delay = controller_->GetNextEventDelay(
      registration, &sync_parameters, base::TimeDelta::Max());
  EXPECT_EQ(
      delay,
      base::Milliseconds(
          min_gap_between_periodic_sync_events_ms *
          BackgroundSyncDelegateImpl::kEngagementLevelLowOrMediumPenalty));

  // Low engagement.
  site_engagement::SiteEngagementService::Get(&profile_)->ResetBaseScoreForURL(
      GURL(kExampleUrl),
      site_engagement::SiteEngagementScore::GetMediumEngagementBoundary() - 1);
  delay = controller_->GetNextEventDelay(registration, &sync_parameters,
                                         base::TimeDelta::Max());
  EXPECT_EQ(
      delay,
      base::Milliseconds(
          min_gap_between_periodic_sync_events_ms *
          BackgroundSyncDelegateImpl::kEngagementLevelLowOrMediumPenalty));

  // Minimal engagement.
  site_engagement::SiteEngagementService::Get(&profile_)->ResetBaseScoreForURL(
      GURL(kExampleUrl), 0.5);
  delay = controller_->GetNextEventDelay(registration, &sync_parameters,
                                         base::TimeDelta::Max());
  EXPECT_EQ(delay,
            base::Milliseconds(
                min_gap_between_periodic_sync_events_ms *
                BackgroundSyncDelegateImpl::kEngagementLevelMinimalPenalty));

  // No engagement.
  site_engagement::SiteEngagementService::Get(&profile_)->ResetBaseScoreForURL(
      GURL(kExampleUrl), 0);
  delay = controller_->GetNextEventDelay(registration, &sync_parameters,
                                         base::TimeDelta::Max());
  EXPECT_EQ(delay, base::TimeDelta::Max());
}

TEST_F(BackgroundSyncControllerImplTest, MaxFrequencyForOrigin) {
  content::BackgroundSyncParameters sync_parameters;
  url::Origin origin = url::Origin::Create(GURL(kExampleUrl));
  site_engagement::SiteEngagementScore::SetParamValuesForTesting();
  site_engagement::SiteEngagementService::Get(&profile_)->ResetBaseScoreForURL(
      GURL(kExampleUrl),
      site_engagement::SiteEngagementScore::GetHighEngagementBoundary());

  // Periodic Sync: zero attempts.
  // |min_interval| < kMinGapBetweenPeriodicSyncEvents.
  base::TimeDelta delay = controller_->GetNextEventDelay(
      MakeBackgroundSyncRegistration(
          /* min_interval= */ kSmallerThanMinGap.InMilliseconds(),
          /* num_attempts= */ 0, blink::mojom::BackgroundSyncType::PERIODIC),
      &sync_parameters, base::TimeDelta::Max());
  EXPECT_EQ(delay, sync_parameters.min_periodic_sync_events_interval);

  // Periodic Sync: zero attempts.
  // |min_interval| > kMinGapBetweenPeriodicSyncEvents.
  delay = controller_->GetNextEventDelay(
      MakeBackgroundSyncRegistration(
          /* min_interval= */ kLargerThanMinGap.InMilliseconds(),
          /* num_attempts= */ 0, blink::mojom::BackgroundSyncType::PERIODIC),
      &sync_parameters, base::TimeDelta::Max());
  EXPECT_EQ(delay, kLargerThanMinGapExpectedDelay);

  // Periodic Sync: zero attempts.
  // |min_interval| a multiple of kMinGapBetweenPeriodicSyncEvents.
  delay = controller_->GetNextEventDelay(
      MakeBackgroundSyncRegistration(
          /* min_interval= */ 2 *
              kLargerThanMinGapExpectedDelay.InMilliseconds(),
          /* num_attempts= */ 0, blink::mojom::BackgroundSyncType::PERIODIC),
      &sync_parameters, base::TimeDelta::Max());
  EXPECT_EQ(delay, 2 * kLargerThanMinGapExpectedDelay);

  // Periodic Sync: zero attempts.
  // |min_interval| is zero.
  delay = controller_->GetNextEventDelay(
      MakeBackgroundSyncRegistration(
          /* min_interval= */ 0,
          /* num_attempts= */ 0, blink::mojom::BackgroundSyncType::PERIODIC),
      &sync_parameters, base::TimeDelta::Max());
  EXPECT_EQ(delay, sync_parameters.min_periodic_sync_events_interval);
}

TEST_F(BackgroundSyncControllerImplTest, CrossRegistrationLimitsForOrigin) {
  content::BackgroundSyncParameters sync_parameters;
  url::Origin origin = url::Origin::Create(GURL(kExampleUrl));
  site_engagement::SiteEngagementScore::SetParamValuesForTesting();
  site_engagement::SiteEngagementService::Get(&profile_)->ResetBaseScoreForURL(
      GURL(kExampleUrl),
      site_engagement::SiteEngagementScore::GetHighEngagementBoundary());

  // Periodic Sync: zero attempts.
  // |min_interval| < kMinGapBetweenPeriodicSyncEvents.
  // No other event scheduled.
  base::TimeDelta delay = controller_->GetNextEventDelay(
      MakeBackgroundSyncRegistration(
          /* min_interval= */ kSmallerThanMinGap.InMilliseconds(),
          /* num_attempts= */ 0, blink::mojom::BackgroundSyncType::PERIODIC),
      &sync_parameters, base::TimeDelta::Max());
  EXPECT_EQ(delay, sync_parameters.min_periodic_sync_events_interval);

  // Periodic Sync: zero attempts.
  // |min_interval| < kMinGapBetweenPeriodicSyncEvents.
  // Time till next event is zero.
  delay = controller_->GetNextEventDelay(
      MakeBackgroundSyncRegistration(
          /* min_interval= */ kSmallerThanMinGap.InMilliseconds(),
          /* num_attempts= */ 0, blink::mojom::BackgroundSyncType::PERIODIC),
      &sync_parameters, base::TimeDelta());
  EXPECT_EQ(delay, sync_parameters.min_periodic_sync_events_interval);

  // Periodic Sync: zero attempts.
  // |min_interval| < kMinGapBetweenPeriodicSyncEvents.
  // Time till next event > kMinGapBetweenPeriodicSyncEvents.
  delay = controller_->GetNextEventDelay(
      MakeBackgroundSyncRegistration(
          /* min_interval= */ kSmallerThanMinGap.InMilliseconds(),
          /* num_attempts= */ 0, blink::mojom::BackgroundSyncType::PERIODIC),
      &sync_parameters, kLargerThanMinGap);
  EXPECT_EQ(delay, kLargerThanMinGap);

  // Periodic Sync: zero attempts.
  // |min_interval| < kMinGapBetweenPeriodicSyncEvents.
  // delay < time_till_next_event - min_gap_for_origin
  delay = controller_->GetNextEventDelay(
      MakeBackgroundSyncRegistration(
          /* min_interval= */ kSmallerThanMinGap.InMilliseconds(),
          /* num_attempts= */ 0, blink::mojom::BackgroundSyncType::PERIODIC),
      &sync_parameters, base::Hours(24));
  EXPECT_EQ(delay, sync_parameters.min_periodic_sync_events_interval);

  // Periodic Sync: zero attempts.
  // |min_interval| < kMinGapBetweenPeriodicSyncEvents.
  // time_till_next_event - min_gap_for_origin < delay < time_till_next_event +
  // min_gap_for_origin
  delay = controller_->GetNextEventDelay(
      MakeBackgroundSyncRegistration(
          /* min_interval= */ kSmallerThanMinGap.InMilliseconds(),
          /* num_attempts= */ 0, blink::mojom::BackgroundSyncType::PERIODIC),
      &sync_parameters, base::Hours(1));
  EXPECT_EQ(delay,
            sync_parameters.min_periodic_sync_events_interval + base::Hours(1));

  // Periodic Sync: zero attempts.
  // |min_interval| < kMinGapBetweenPeriodicSyncEvents.
  // delay > time_till_next_event + min_gap_for_origin
  delay = controller_->GetNextEventDelay(
      MakeBackgroundSyncRegistration(
          /* min_interval= */ kLargerThanMinGap.InMilliseconds(),
          /* num_attempts= */ 0, blink::mojom::BackgroundSyncType::PERIODIC),
      &sync_parameters, base::Hours(1));
  EXPECT_EQ(delay, kLargerThanMinGapExpectedDelay);
}

}  // namespace
