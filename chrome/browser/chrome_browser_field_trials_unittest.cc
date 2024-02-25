// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_browser_field_trials.h"

#include <memory>

#include "base/feature_list.h"
#include "components/prefs/testing_pref_service.h"
#include "components/ukm/ukm_recorder_impl.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID)
TEST(ChromeBrowserFieldTrialsTest, SamplingTrials) {
  TestingPrefServiceSimple local_state;
  ChromeBrowserFieldTrials chrome_browser_field_trials(&local_state);

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
  chrome_browser_field_trials.SetUpClientSideFieldTrials(
      /*has_seed=*/false, entropy_providers, feature_list.get());

  // Verify that the sampling trials were created.
  EXPECT_TRUE(base::FieldTrialList::TrialExists(kSamplingTrialName));
#if BUILDFLAG(IS_ANDROID)
  EXPECT_TRUE(base::FieldTrialList::TrialExists(kPostFREFixSamplingTrialName));
#endif  // BUILDFLAG(IS_ANDROID)
  EXPECT_TRUE(base::FieldTrialList::TrialExists(kUkmSamplingTrialName));

  // Call SetUpClientSideFieldTrials() again. This should be a no-op,
  // since the sampling trials already exist. If the trials are created again,
  // a CHECK will be triggered and this will crash.
  chrome_browser_field_trials.SetUpClientSideFieldTrials(
      /*has_seed=*/false, entropy_providers, feature_list.get());
}
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID)
