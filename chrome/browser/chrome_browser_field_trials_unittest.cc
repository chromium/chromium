// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_browser_field_trials.h"

#include <memory>

#include "base/feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/metrics/chrome_browser_sampling_trials.h"
#include "components/ukm/ukm_recorder_impl.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/device_info.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "third_party/blink/public/common/features.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "base/test/scoped_feature_list.h"
#include "base/test/test_reg_util_win.h"
#include "chrome/browser/metrics/chrome_metrics_services_manager_client.h"
#include "chrome/chrome_elf/nt_registry/nt_registry.h"
#include "chrome/install_static/install_util.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID)
TEST(ChromeBrowserFieldTrialsTest, SamplingTrials) {
  ChromeBrowserFieldTrials chrome_browser_field_trials;

  const char kSamplingTrialName[] = "MetricsAndCrashSampling";
#if BUILDFLAG(IS_ANDROID)
  const char kPostFREFixSamplingTrialName[] =
      "PostFREFixMetricsAndCrashSampling";
#endif  // BUILDFLAG(IS_ANDROID)
  const char kUkmSamplingTrialName[] = "UkmSamplingRate";

  // Verify that initially, sampling trials do not exist.
  EXPECT_FALSE(base::FieldTrialList::TrialExists(kSamplingTrialName));
#if BUILDFLAG(IS_ANDROID)
  EXPECT_FALSE(base::FieldTrialList::TrialExists(kPostFREFixSamplingTrialName));
#endif  // BUILDFLAG(IS_ANDROID)
  EXPECT_FALSE(base::FieldTrialList::TrialExists(kUkmSamplingTrialName));

  // Call SetUpClientSideFieldTrials(), which should create fallback
  // sampling trials since they do not exist yet. Using an empty string for
  // limited_entropy_randomization_source since it's not relevant to the
  // sampling trials that are being tested here.
  variations::EntropyProviders entropy_providers(
      "client_id", {0, 8000},
      /*limited_entropy_randomization_source=*/std::string_view());
  auto feature_list = std::make_unique<base::FeatureList>();
#if BUILDFLAG(IS_ANDROID)
  base::ScopedClosureRunner reset_is_desktop(base::BindOnce(
      &base::android::device_info::reset_is_desktop_for_testing));

  // On Android Desktop, metrics/crash fallback sampling trials should not be
  // created, though the UKM fallback sampling trial should still be created.
  base::android::device_info::set_is_desktop_for_testing(true);
  chrome_browser_field_trials.SetUpClientSideFieldTrials(
      /*has_seed=*/false, entropy_providers, feature_list.get());
  EXPECT_FALSE(base::FieldTrialList::TrialExists(kSamplingTrialName));
  EXPECT_FALSE(base::FieldTrialList::TrialExists(kPostFREFixSamplingTrialName));
  EXPECT_TRUE(base::FieldTrialList::TrialExists(kUkmSamplingTrialName));

  base::android::device_info::set_is_desktop_for_testing(false);
#endif  // BUILDFLAG(IS_ANDROID)
  chrome_browser_field_trials.SetUpClientSideFieldTrials(
      /*has_seed=*/false, entropy_providers, feature_list.get());

  // Verify that the sampling trials were created.
  EXPECT_TRUE(base::FieldTrialList::TrialExists(kSamplingTrialName));
#if BUILDFLAG(IS_ANDROID)
  EXPECT_TRUE(base::FieldTrialList::TrialExists(kPostFREFixSamplingTrialName));
#endif  // BUILDFLAG(IS_ANDROID)
  EXPECT_TRUE(base::FieldTrialList::TrialExists(kUkmSamplingTrialName));

  // Try to create the sampling trials again. This should be a no-op,
  // since the sampling trials already exist. If the trials are created again,
  // a CHECK will be triggered and this will crash.
  metrics::CreateFallbackSamplingTrialsIfNeeded(
      entropy_providers.default_entropy(), feature_list.get());
  metrics::CreateFallbackUkmSamplingTrialIfNeeded(
      entropy_providers.default_entropy(), feature_list.get());
}
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_WIN)
TEST(ChromeBrowserFieldTrialsTest,
     OnVariationsSetupCompleteUpdatesCrashpadSample) {
  registry_util::RegistryOverrideManager override_manager;
  std::wstring hkcu_override_path;
  ASSERT_NO_FATAL_FAILURE(override_manager.OverrideRegistry(
      HKEY_CURRENT_USER, &hkcu_override_path));
  nt::SetTestingOverride(nt::HKCU, hkcu_override_path);
  absl::Cleanup reset_nt_override = [] {
    nt::SetTestingOverride(nt::HKCU, std::wstring());
  };

  ChromeBrowserFieldTrials chrome_browser_field_trials;

  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(
        metrics::internal::kMetricsReportingFeature);
    chrome_browser_field_trials.OnVariationsSetupComplete();
    EXPECT_TRUE(install_static::GetCollectStatsInSample());
  }

  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndDisableFeature(
        metrics::internal::kMetricsReportingFeature);
    chrome_browser_field_trials.OnVariationsSetupComplete();
    EXPECT_FALSE(install_static::GetCollectStatsInSample());
  }
}
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_ANDROID)
TEST(ChromeBrowserFieldTrialsTest, TranslationApiDisabledOnAutomotive) {
  base::ScopedClosureRunner reset_is_automotive(base::BindOnce(
      &base::android::device_info::reset_is_automotive_for_testing));
  ChromeBrowserFieldTrials chrome_browser_field_trials;

  base::android::device_info::set_is_automotive_for_testing(true);
  base::FeatureList automotive_feature_list;
  chrome_browser_field_trials.RegisterFeatureOverrides(
      &automotive_feature_list);
  EXPECT_TRUE(automotive_feature_list.IsFeatureOverriddenFromCommandLine(
      blink::features::kTranslationAPI.name,
      base::FeatureList::OVERRIDE_DISABLE_FEATURE));

  base::android::device_info::set_is_automotive_for_testing(false);
  base::FeatureList non_automotive_feature_list;
  chrome_browser_field_trials.RegisterFeatureOverrides(
      &non_automotive_feature_list);
  EXPECT_FALSE(non_automotive_feature_list.IsFeatureOverridden(
      blink::features::kTranslationAPI.name));
}
#endif  // BUILDFLAG(IS_ANDROID)
