// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tpcd/experiment/eligibility_service.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_settings_factory.h"
#include "chrome/browser/privacy_sandbox/tracking_protection_onboarding_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tpcd/experiment/eligibility_service_factory.h"
#include "chrome/browser/tpcd/experiment/mock_experiment_manager.h"
#include "chrome/browser/tpcd/experiment/tpcd_experiment_features.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/privacy_sandbox/privacy_sandbox_settings.h"
#include "components/privacy_sandbox/privacy_sandbox_test_util.h"
#include "components/privacy_sandbox/tpcd_experiment_eligibility.h"
#include "components/privacy_sandbox/tracking_protection_onboarding.h"
#include "components/privacy_sandbox/tracking_protection_prefs.h"
#include "components/version_info/channel.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace tpcd::experiment {

namespace {

using ::testing::_;
using ::testing::Return;
using TpcdExperimentEligibility = privacy_sandbox::TpcdExperimentEligibility;

constexpr char kReasonForEligibilityStoredInPrefsHistogram[] =
    "PrivacySandbox.CookieDeprecationFacilitatedTesting."
    "ReasonForEligibilityStoredInPrefs";

constexpr char kReasonForComputedEligibilityForProfileHistogram[] =
    "PrivacySandbox.CookieDeprecationFacilitatedTesting."
    "ReasonForComputedEligibilityForProfile";

}  // namespace

class EligibilityServiceTestBase : public testing::Test {
 public:
  EligibilityServiceTestBase()
      : local_state_(TestingBrowserProcess::GetGlobal()) {}

  void SetUp() override {
    experiment_manager_ = std::make_unique<MockExperimentManager>();
    ON_CALL(*experiment_manager_, DidVersionChange)
        .WillByDefault(Return(false));

    privacy_sandbox_settings_ =
        PrivacySandboxSettingsFactory::GetForProfile(&profile_);
    auto privacy_sandbox_delegate = std::make_unique<
        privacy_sandbox_test_util::MockPrivacySandboxSettingsDelegate>();
    privacy_sandbox_delegate_ = privacy_sandbox_delegate.get();
    privacy_sandbox_delegate_
        ->SetUpGetCookieDeprecationExperimentCurrentEligibility(
            TpcdExperimentEligibility::Reason::kEligible);
    privacy_sandbox_settings_->SetDelegateForTesting(
        std::move(privacy_sandbox_delegate));

    onboarding_service_ =
        TrackingProtectionOnboardingFactory::GetForProfile(&profile_);
  }

 protected:
  content::BrowserTaskEnvironment browser_task_environment_;
  ScopedTestingLocalState local_state_;
  TestingProfile profile_;
  std::unique_ptr<MockExperimentManager> experiment_manager_;
  raw_ptr<privacy_sandbox::PrivacySandboxSettings> privacy_sandbox_settings_;
  raw_ptr<privacy_sandbox_test_util::MockPrivacySandboxSettingsDelegate>
      privacy_sandbox_delegate_;
  raw_ptr<privacy_sandbox::TrackingProtectionOnboarding> onboarding_service_;
};

class EligibilityServiceTest : public EligibilityServiceTestBase {
 public:
  EligibilityServiceTest() {
    feature_list_.InitAndEnableFeature(
        features::kCookieDeprecationFacilitatedTesting);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(EligibilityServiceTest, ClientEligibilityKnown_ClientEligibilityNotSet) {
  base::HistogramTester histograms;

  EXPECT_CALL(*experiment_manager_, IsClientEligible).WillOnce(Return(false));
  EXPECT_CALL(*experiment_manager_, SetClientEligibility).Times(0);

  EligibilityService eligibility_service(&profile_, onboarding_service_,
                                         privacy_sandbox_settings_,
                                         experiment_manager_.get());

  histograms.ExpectTotalCount(kReasonForEligibilityStoredInPrefsHistogram, 0);
  histograms.ExpectUniqueSample(
      kReasonForComputedEligibilityForProfileHistogram,
      /*sample=*/TpcdExperimentEligibility::Reason::kEligible,
      /*expected_bucket_count=*/1);
}

TEST_F(EligibilityServiceTest,
       ClientEligibilityUnknownProfileIneligible_ClientEligibilitySet) {
  base::HistogramTester histograms;

  EXPECT_CALL(*experiment_manager_, IsClientEligible)
      .WillOnce(Return(std::nullopt));

  EXPECT_CALL(*privacy_sandbox_delegate_,
              GetCookieDeprecationExperimentCurrentEligibility)
      .WillOnce(Return(TpcdExperimentEligibility(
          TpcdExperimentEligibility::Reason::k3pCookiesBlocked)));

  EXPECT_CALL(*experiment_manager_, SetClientEligibility(false, _))
      .WillOnce(base::test::RunOnceCallback<1>(false));

  EligibilityService eligibility_service(&profile_, onboarding_service_,
                                         privacy_sandbox_settings_,
                                         experiment_manager_.get());

  histograms.ExpectUniqueSample(
      kReasonForEligibilityStoredInPrefsHistogram,
      /*sample=*/TpcdExperimentEligibility::Reason::k3pCookiesBlocked,
      /*expected_bucket_count=*/1);
  histograms.ExpectUniqueSample(
      kReasonForComputedEligibilityForProfileHistogram,
      /*sample=*/TpcdExperimentEligibility::Reason::k3pCookiesBlocked,
      /*expected_bucket_count=*/1);
}

TEST_F(EligibilityServiceTest,
       ClientEligibilityUnknownProfileEligible_ClientEligibilitySet) {
  base::HistogramTester histograms;

  EXPECT_CALL(*experiment_manager_, IsClientEligible)
      .WillOnce(Return(std::nullopt));

  EXPECT_CALL(*privacy_sandbox_delegate_,
              GetCookieDeprecationExperimentCurrentEligibility)
      .WillOnce(Return(TpcdExperimentEligibility(
          TpcdExperimentEligibility::Reason::kEligible)));

  EXPECT_CALL(*experiment_manager_, SetClientEligibility(true, _))
      .WillOnce(base::test::RunOnceCallback<1>(true));

  EligibilityService eligibility_service(&profile_, onboarding_service_,
                                         privacy_sandbox_settings_,
                                         experiment_manager_.get());

  histograms.ExpectUniqueSample(
      kReasonForEligibilityStoredInPrefsHistogram,
      /*sample=*/TpcdExperimentEligibility::Reason::kEligible,
      /*expected_bucket_count=*/1);
  histograms.ExpectUniqueSample(
      kReasonForComputedEligibilityForProfileHistogram,
      /*sample=*/TpcdExperimentEligibility::Reason::kEligible,
      /*expected_bucket_count=*/1);
}

class EligibilityServiceOTRProfileTest
    : public EligibilityServiceTestBase,
      public testing::WithParamInterface<bool> {
 public:
  EligibilityServiceOTRProfileTest() {
    feature_list_.InitAndEnableFeatureWithParameters(
        features::kCookieDeprecationFacilitatedTesting,
        {{"enable_otr_profiles", GetParam() ? "true" : "false"}});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_P(EligibilityServiceOTRProfileTest, Creation) {
  const bool enable_otr_profiles = GetParam();

  auto* eligibility_service =
      EligibilityServiceFactory::GetForProfile(profile_.GetOffTheRecordProfile(
          Profile::OTRProfileID::CreateUniqueForTesting(),
          /*create_if_needed=*/true));
  EXPECT_EQ(eligibility_service != nullptr, enable_otr_profiles);

// Android does not have guest profiles.
#if !BUILDFLAG(IS_ANDROID)
  auto guest_profile = TestingProfile::Builder().SetGuestSession().Build();
  eligibility_service =
      EligibilityServiceFactory::GetForProfile(guest_profile.get());
  EXPECT_EQ(eligibility_service != nullptr, enable_otr_profiles);
#endif  // !BUILDFLAG(IS_ANDROID)
}

INSTANTIATE_TEST_SUITE_P(All,
                         EligibilityServiceOTRProfileTest,
                         testing::Bool());

struct EligibilityServiceHistogramTestCase {
  bool is_client_eligible = true;
  bool is_profile_eligible = true;
  ProfileEligibilityMismatch expected_histogram_enum;
};

const EligibilityServiceHistogramTestCase kTestCases[] = {
    {
        .expected_histogram_enum =
            ProfileEligibilityMismatch::kEligibleProfileInExperiment,
    },
    {
        .is_client_eligible = false,
        .is_profile_eligible = false,
        .expected_histogram_enum =
            ProfileEligibilityMismatch::kIneligibleProfileNotInExperiment,
    },
    {
        .is_profile_eligible = false,
        .expected_histogram_enum =
            ProfileEligibilityMismatch::kIneligibleProfileInExperiment,
    },
    {
        .is_client_eligible = false,
        .expected_histogram_enum =
            ProfileEligibilityMismatch::kEligibleProfileNotInExperiment,
    }};

class EligibilityServiceHistogramTest
    : public EligibilityServiceTest,
      public testing::WithParamInterface<EligibilityServiceHistogramTestCase> {
 public:
  EligibilityServiceHistogramTest() = default;

  const base::HistogramTester& histograms() const { return histogram_tester_; }

 protected:
  base::HistogramTester histogram_tester_;
};

TEST_P(EligibilityServiceHistogramTest, ProfileEligibilityMismatch) {
  const EligibilityServiceHistogramTestCase& test_case = GetParam();
  // Client eligibility already set, and is not eligible, but current profile is
  // eligible.
  EXPECT_CALL(*experiment_manager_, IsClientEligible)
      .WillOnce(Return(test_case.is_client_eligible));
  EXPECT_CALL(*privacy_sandbox_delegate_,
              GetCookieDeprecationExperimentCurrentEligibility)
      .WillOnce(Return(TpcdExperimentEligibility(
          test_case.is_profile_eligible
              ? TpcdExperimentEligibility::Reason::kEligible
              : TpcdExperimentEligibility::Reason::kHasNotSeenNotice)));

  EligibilityService eligibility_service(&profile_, onboarding_service_,
                                         privacy_sandbox_settings_,
                                         experiment_manager_.get());

  // Expect mismatch value recorded in histogram.
  histograms().ExpectBucketCount(ProfileEligibilityMismatchHistogramName,
                                 test_case.expected_histogram_enum, 1);
}

INSTANTIATE_TEST_SUITE_P(EligibilityServiceHistogramTests,
                         EligibilityServiceHistogramTest,
                         testing::ValuesIn(kTestCases));

}  // namespace tpcd::experiment
