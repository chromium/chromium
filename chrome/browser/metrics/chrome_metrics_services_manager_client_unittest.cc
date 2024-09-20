// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/chrome_metrics_services_manager_client.h"

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/metrics/chrome_metrics_service_client.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/metrics/enabled_state_provider.h"
#include "components/metrics/metrics_pref_names.h"
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
      client.GetEnabledStateProvider();

  // Both consent and reporting should be false.
  EXPECT_FALSE(provider.IsConsentGiven());
  EXPECT_FALSE(provider.IsReportingEnabled());

  // Set the pref to true.
  local_state()->SetBoolean(metrics::prefs::kMetricsReportingEnabled, true);

  // Both consent and reporting should be true.
  EXPECT_TRUE(provider.IsConsentGiven());
  EXPECT_TRUE(provider.IsReportingEnabled());

  // Set --force-fieldtrials= command-line flag, which should disable reporting
  // but not consent.
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kForceFieldTrials, "Foo/Bar");

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

// Verifies that IsClientInSampleForMetrics() uses the "MetricsReporting"
// feature to determine sampling if the |kUsePostFREFixSamplingTrial| pref is
// not set. This is the case if 1) this is a non-Android platform, or 2) this is
// an Android client that is not using the new sampling trial.
TEST_F(IsClientInSampleTest, UsesMetricsReportingFeature) {
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndDisableFeature(
        metrics::internal::kMetricsReportingFeature);
    // The client should not be considered sampled in.
    EXPECT_FALSE(
        ChromeMetricsServicesManagerClient::IsClientInSampleForMetrics());
  }

  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(
        metrics::internal::kMetricsReportingFeature);
    // The client should be considered sampled in.
    EXPECT_TRUE(
        ChromeMetricsServicesManagerClient::IsClientInSampleForMetrics());
  }
}

#if BUILDFLAG(IS_ANDROID)
// Verifies that IsClientInSampleForMetrics() uses the post-FRE-fix sampling
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
    EXPECT_FALSE(
        ChromeMetricsServicesManagerClient::IsClientInSampleForMetrics());
  }

  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(
        metrics::internal::kPostFREFixMetricsReportingFeature);
    // The client should be considered sampled in.
    EXPECT_TRUE(
        ChromeMetricsServicesManagerClient::IsClientInSampleForMetrics());
  }
}
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID)
TEST_F(IsClientInSampleTest, IsClientInSampleForCrashesTest) {
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(
        metrics::internal::kMetricsReportingFeature);
    EXPECT_TRUE(
        ChromeMetricsServicesManagerClient::IsClientInSampleForCrashes());
  }

  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeatureWithParameters(
        metrics::internal::kMetricsReportingFeature,
        {{"disable_crashes", "true"}});
    EXPECT_FALSE(
        ChromeMetricsServicesManagerClient::IsClientInSampleForCrashes());
  }

#if BUILDFLAG(IS_ANDROID)
  // Set the |kUsePostFREFixSamplingTrial| pref to true.
  local_state()->SetBoolean(metrics::prefs::kUsePostFREFixSamplingTrial, true);

  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(
        metrics::internal::kPostFREFixMetricsReportingFeature);
    EXPECT_TRUE(
        ChromeMetricsServicesManagerClient::IsClientInSampleForCrashes());
  }

  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeatureWithParameters(
        metrics::internal::kPostFREFixMetricsReportingFeature,
        {{"disable_crashes", "true"}});
    EXPECT_FALSE(
        ChromeMetricsServicesManagerClient::IsClientInSampleForCrashes());
  }
#endif  // BUILDFLAG(IS_ANDROID)
}
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID)
