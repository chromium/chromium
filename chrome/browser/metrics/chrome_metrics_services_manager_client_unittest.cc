// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/chrome_metrics_services_manager_client.h"

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/metrics/field_trial.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/metrics/chrome_metrics_service_client.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/metrics/enabled_state_provider.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/metrics_reporting_default_state.h"
#include "components/metrics/metrics_service_accessor.h"
#include "components/metrics/metrics_state_manager.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/lacros/lacros_test_helper.h"
#include "content/public/test/browser_task_environment.h"
#endif

using ::testing::NotNull;

class ChromeMetricsServicesManagerClientTest : public testing::Test {
 public:
  ChromeMetricsServicesManagerClientTest() = default;

  ChromeMetricsServicesManagerClientTest(
      const ChromeMetricsServicesManagerClientTest&) = delete;
  ChromeMetricsServicesManagerClientTest& operator=(
      const ChromeMetricsServicesManagerClientTest&) = delete;

  ~ChromeMetricsServicesManagerClientTest() override = default;

  void SetUp() override {
    // Set up Local State prefs.
    TestingBrowserProcess::GetGlobal()->SetLocalState(&local_state_);
    ChromeMetricsServiceClient::RegisterPrefs(local_state()->registry());
  }

  void TearDown() override {
    TestingBrowserProcess::GetGlobal()->SetLocalState(nullptr);
  }

  TestingPrefServiceSimple* local_state() { return &local_state_; }

 private:
  TestingPrefServiceSimple local_state_;
};

using CreateFallbackSamplingTrialTest = ChromeMetricsServicesManagerClientTest;
using IsClientInSampleTest = ChromeMetricsServicesManagerClientTest;

TEST_F(ChromeMetricsServicesManagerClientTest, ForceTrialsDisablesReporting) {
  // First, test with UMA reporting setting defaulting to off.
  local_state()->registry()->RegisterBooleanPref(
      metrics::prefs::kMetricsReportingEnabled, false);
  // Force the pref to be used, even in unofficial builds.
  ChromeMetricsServiceAccessor::SetForceIsMetricsReportingEnabledPrefLookup(
      true);

  ChromeMetricsServicesManagerClient client(local_state());
  const metrics::EnabledStateProvider& provider =
      client.GetEnabledStateProviderForTesting();
  metrics_services_manager::MetricsServicesManagerClient* base_client = &client;

  // The provider and client APIs should agree.
  EXPECT_EQ(provider.IsConsentGiven(), base_client->IsMetricsConsentGiven());
  EXPECT_EQ(provider.IsReportingEnabled(),
            base_client->IsMetricsReportingEnabled());

  // Both consent and reporting should be false.
  EXPECT_FALSE(provider.IsConsentGiven());
  EXPECT_FALSE(provider.IsReportingEnabled());

  // Set the pref to true.
  local_state()->SetBoolean(metrics::prefs::kMetricsReportingEnabled, true);

  // The provider and client APIs should agree.
  EXPECT_EQ(provider.IsConsentGiven(), base_client->IsMetricsConsentGiven());
  EXPECT_EQ(provider.IsReportingEnabled(),
            base_client->IsMetricsReportingEnabled());

  // Both consent and reporting should be true.
  EXPECT_TRUE(provider.IsConsentGiven());
  EXPECT_TRUE(provider.IsReportingEnabled());

  // Set --force-fieldtrials= command-line flag, which should disable reporting
  // but not consent.
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kForceFieldTrials, "Foo/Bar");

  // The provider and client APIs should agree.
  EXPECT_EQ(provider.IsConsentGiven(), base_client->IsMetricsConsentGiven());
  EXPECT_EQ(provider.IsReportingEnabled(),
            base_client->IsMetricsReportingEnabled());

  // Consent should be true but reporting should be false.
  EXPECT_TRUE(provider.IsConsentGiven());
  EXPECT_FALSE(provider.IsReportingEnabled());
}

TEST_F(ChromeMetricsServicesManagerClientTest, PopulateStartupVisibility) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // Set up ScopedLacrosServiceTestHelper needed for Lacros.
  content::BrowserTaskEnvironment task_environment;
  chromeos::ScopedLacrosServiceTestHelper helper;
#endif

  // Register the kMetricsReportingEnabled pref.
  local_state()->registry()->RegisterBooleanPref(
      metrics::prefs::kMetricsReportingEnabled, false);

  ChromeMetricsServicesManagerClient client(local_state());
  metrics::MetricsStateManager* metrics_state_manager =
      client.GetMetricsStateManagerForTesting();

  // Verify that the MetricsStateManager's StartupVisibility is not unknown.
  EXPECT_TRUE(metrics_state_manager->is_foreground_session() ||
              metrics_state_manager->is_background_session());
}

// Verifies that CreateFallBackSamplingTrial() uses the MetricsAndCrashSampling
// sampling trial if the |kUsePostFREFixSamplingTrial| pref is not set. This is
// the case if 1) this is a non-Android platform, or 2) this is an Android
// client that is not using the new sampling trial. This also verifies that the
// param |kRateParamName| is correctly set, depending on which group we are
// assigned to during the test (not deterministic).
TEST_F(CreateFallbackSamplingTrialTest, UsesMetricsAndCrashSamplingTrial) {
#if BUILDFLAG(IS_ANDROID)
  ASSERT_FALSE(
      local_state()->GetBoolean(metrics::prefs::kUsePostFREFixSamplingTrial));
#endif

  // Initially, neither sampling trial should exist.
  ASSERT_FALSE(base::FieldTrialList::TrialExists("MetricsAndCrashSampling"));
  ASSERT_FALSE(
      base::FieldTrialList::TrialExists("PostFREFixMetricsAndCrashSampling"));

  // Create the fallback sampling trial.
  auto feature_list = std::make_unique<base::FeatureList>();
  ChromeMetricsServicesManagerClient::CreateFallbackSamplingTrial(
      version_info::Channel::STABLE, feature_list.get());

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatureList(std::move(feature_list));

  // Since the |kUsePostFREFixSamplingTrial| pref was not set to true, the
  // MetricsAndCrashSampling sampling trial should be registered.
  EXPECT_TRUE(base::FieldTrialList::TrialExists("MetricsAndCrashSampling"));
  EXPECT_FALSE(
      base::FieldTrialList::TrialExists("PostFREFixMetricsAndCrashSampling"));

  // The MetricsReporting feature should be associated with the
  // MetricsAndCrashSampling trial, and its state should be overridden.
  base::FieldTrial* associated_trial = base::FeatureList::GetFieldTrial(
      metrics::internal::kMetricsReportingFeature);
  EXPECT_THAT(associated_trial, NotNull());
  EXPECT_EQ("MetricsAndCrashSampling", associated_trial->trial_name());
  EXPECT_TRUE(base::FeatureList::GetStateIfOverridden(
                  metrics::internal::kMetricsReportingFeature)
                  .has_value());

  // Verify that we are either in the "OutOfReportingSample" or
  // "InReportingSample" group, and verify that the sampling rate is what we
  // expect (10% sampled in rate or 90% sampled out rate for Stable).
  // TODO(crbug/1322904): Maybe make
  // ChromeMetricsServicesManagerClient::GetSamplingRatePerMille return the
  // sampling rate even when sampled out, so that we can replace the code below
  // with ChromeMetricsServicesManagerClient::GetSamplingRatePerMille.
  const std::string group_name = associated_trial->group_name();
  ASSERT_TRUE(group_name == "OutOfReportingSample" ||
              group_name == "InReportingSample");
  base::FieldTrialParams params;
  ASSERT_TRUE(
      base::GetFieldTrialParams(associated_trial->trial_name(), &params));
  ASSERT_TRUE(base::Contains(params, metrics::internal::kRateParamName));
  int sampling_rate_per_mille;
  ASSERT_TRUE(base::StringToInt(params[metrics::internal::kRateParamName],
                                &sampling_rate_per_mille));
  EXPECT_EQ(group_name == "OutOfReportingSample" ? 900 : 100,
            sampling_rate_per_mille);
}

#if BUILDFLAG(IS_ANDROID)
// Verifies that CreateFallBackSamplingTrial() uses the post-FRE-fix sampling
// trial (PostFREFixMetricsAndCrashSampling) if the
// |kUsePostFREFixSamplingTrial| pref is set. This also verifies that the param
// |kRateParamName| is correctly set, depending on which group we are assigned
// to during the test (not deterministic).
TEST_F(CreateFallbackSamplingTrialTest, UsesPostFREFixTrialWhenPrefSet) {
  // Set the |kUsePostFREFixSamplingTrial| pref to true.
  local_state()->SetBoolean(metrics::prefs::kUsePostFREFixSamplingTrial, true);

  // Initially, neither sampling trial should exist.
  ASSERT_FALSE(base::FieldTrialList::TrialExists("MetricsAndCrashSampling"));
  ASSERT_FALSE(
      base::FieldTrialList::TrialExists("PostFREFixMetricsAndCrashSampling"));

  // Create the fallback sampling trial.
  auto feature_list = std::make_unique<base::FeatureList>();
  ChromeMetricsServicesManagerClient::CreateFallbackSamplingTrial(
      version_info::Channel::STABLE, feature_list.get());

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatureList(std::move(feature_list));

  // Since the |kUsePostFREFixSamplingTrial| pref was set to true, the
  // post-FRE-fix sampling trial (PostFREFixMetricsAndCrashSampling) should be
  // registered.
  EXPECT_FALSE(base::FieldTrialList::TrialExists("MetricsAndCrashSampling"));
  EXPECT_TRUE(
      base::FieldTrialList::TrialExists("PostFREFixMetricsAndCrashSampling"));

  // The PostFREFixMetricsReporting feature should be associated with the
  // PostFREFixMetricsAndCrashSampling trial, and its state should be
  // overridden.
  base::FieldTrial* associated_trial = base::FeatureList::GetFieldTrial(
      metrics::internal::kPostFREFixMetricsReportingFeature);
  EXPECT_THAT(associated_trial, NotNull());
  EXPECT_EQ("PostFREFixMetricsAndCrashSampling",
            associated_trial->trial_name());
  EXPECT_TRUE(base::FeatureList::GetStateIfOverridden(
                  metrics::internal::kPostFREFixMetricsReportingFeature)
                  .has_value());

  // Verify that we are either in the "OutOfReportingSample" or
  // "InReportingSample" group, and verify that the sampling rate is what we
  // expect (19% sampled in rate or 81% sampled out rate for Stable).
  // TODO(crbug/1322904): Maybe make
  // ChromeMetricsServicesManagerClient::GetSamplingRatePerMille return the
  // sampling rate even when sampled out, so that we can replace the code below
  // with ChromeMetricsServicesManagerClient::GetSamplingRatePerMille.
  const std::string group_name = associated_trial->group_name();
  ASSERT_TRUE(group_name == "OutOfReportingSample" ||
              group_name == "InReportingSample");
  base::FieldTrialParams params;
  ASSERT_TRUE(
      base::GetFieldTrialParams(associated_trial->trial_name(), &params));
  ASSERT_TRUE(base::Contains(params, metrics::internal::kRateParamName));
  int sampling_rate_per_mille;
  ASSERT_TRUE(base::StringToInt(params[metrics::internal::kRateParamName],
                                &sampling_rate_per_mille));
  EXPECT_EQ(group_name == "OutOfReportingSample" ? 810 : 190,
            sampling_rate_per_mille);
}
#endif  // BUILDFLAG(IS_ANDROID)

// Verifies that IsClientInSample() uses the "MetricsReporting" sampling
// feature to determine sampling if the |kUsePostFREFixSamplingTrial| pref is
// not set. This is the case if 1) this is a non-Android platform, or 2) this is
// an Android client that is not using the new sampling trial.
TEST_F(IsClientInSampleTest, UsesMetricsReportingFeature) {
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndDisableFeature(
        metrics::internal::kMetricsReportingFeature);
    // The client should not be considered sampled in.
    EXPECT_FALSE(ChromeMetricsServicesManagerClient::IsClientInSample());
  }

  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(
        metrics::internal::kMetricsReportingFeature);
    // The client should be considered sampled in.
    EXPECT_TRUE(ChromeMetricsServicesManagerClient::IsClientInSample());
  }
}

#if BUILDFLAG(IS_ANDROID)
// Verifies that IsClientInSample() uses the post-FRE-fix sampling
// feature to determine sampling if the |kUsePostFREFixSamplingTrial| pref is
// set.
TEST_F(IsClientInSampleTest, UsesPostFREFixFeatureWhenPrefSet) {
  // Set the |kUsePostFREFixSamplingTrial| pref to true.
  local_state()->SetBoolean(metrics::prefs::kUsePostFREFixSamplingTrial, true);

  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndDisableFeature(
        metrics::internal::kPostFREFixMetricsReportingFeature);
    // The client should not be considered sampled in.
    EXPECT_FALSE(ChromeMetricsServicesManagerClient::IsClientInSample());
  }

  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(
        metrics::internal::kPostFREFixMetricsReportingFeature);
    // The client should be considered sampled in.
    EXPECT_TRUE(ChromeMetricsServicesManagerClient::IsClientInSample());
  }
}
#endif  // BUILDFLAG(IS_ANDROID)
