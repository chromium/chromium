// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_field_trials.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"

namespace android_webview {

namespace {

const char kTestEnabledFeatureName[] = "AwFieldTrialTestEnabledFeature";
const char kTestDisabledFeatureName[] = "AwFieldTrialTestDisabledFeature";

BASE_FEATURE(kTestEnabledFeature,
             kTestEnabledFeatureName,
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kTestDisabledFeature,
             kTestDisabledFeatureName,
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace

class AwFieldTrialsTest : public testing::Test {
 public:
  AwFieldTrialsTest() {
    // Without overrides, the states should correspond to the feature defaults.
    EXPECT_TRUE(base::FeatureList::IsEnabled(kTestEnabledFeature));
    EXPECT_FALSE(base::FeatureList::IsEnabled(kTestDisabledFeature));
    original_feature_list_ = base::FeatureList::ClearInstanceForTesting();
  }

  ~AwFieldTrialsTest() override {
    // Restore feature list.
    if (original_feature_list_) {
      base::FeatureList::ClearInstanceForTesting();
      base::FeatureList::RestoreInstanceForTesting(
          std::move(original_feature_list_));
    }
  }

  AwFieldTrialsTest(const AwFieldTrialsTest&) = delete;
  AwFieldTrialsTest& operator=(const AwFieldTrialsTest&) = delete;

  void SetUpFeatureTrial(base::FeatureList* feature_list,
                         const char* feature_name,
                         base::FeatureList::OverrideState override_state) {
    // Set-up a trial enable/disable test feature. An empty limited entropy
    // randomization is used since only default_entropy() will used on the
    // constructed `entropy_providers` instance.
    variations::EntropyProviders entropy_providers(
        "client_id", {0, 8000},
        /*limited_entropy_randomization_source=*/std::string_view());
    scoped_refptr<base::FieldTrial> trial(
        base::FieldTrialList::FactoryGetFieldTrial(
            feature_name, /*total_probability=*/1000, "Default",
            entropy_providers.default_entropy()));
    feature_list->RegisterFieldTrialOverride(feature_name, override_state,
                                             trial.get());
  }

 private:
  //  Save the present FeatureList and restore it after test finish.
  std::unique_ptr<base::FeatureList> original_feature_list_;
};

TEST_F(AwFieldTrialsTest, TrialEnableFeatureOverrides) {
  AwFieldTrials aw_field_trials;
  auto feature_list = std::make_unique<base::FeatureList>();
  SetUpFeatureTrial(feature_list.get(), kTestDisabledFeatureName,
                    base::FeatureList::OVERRIDE_ENABLE_FEATURE);
  aw_field_trials.RegisterFeatureOverrides(feature_list.get());
  base::FeatureList::SetInstance(std::move(feature_list));

  EXPECT_TRUE(base::FeatureList::IsEnabled(kTestDisabledFeature));
}

TEST_F(AwFieldTrialsTest, TrialDisableFeatureOverrides) {
  AwFieldTrials aw_field_trials;
  auto feature_list = std::make_unique<base::FeatureList>();
  SetUpFeatureTrial(feature_list.get(), kTestDisabledFeatureName,
                    base::FeatureList::OVERRIDE_DISABLE_FEATURE);
  aw_field_trials.RegisterFeatureOverrides(feature_list.get());
  base::FeatureList::SetInstance(std::move(feature_list));

  EXPECT_FALSE(base::FeatureList::IsEnabled(kTestDisabledFeature));
}

TEST_F(AwFieldTrialsTest, CommandLineEnableFeatureOverrides) {
  AwFieldTrials aw_field_trials;
  auto feature_list = std::make_unique<base::FeatureList>();
  feature_list->InitFromCommandLine(kTestDisabledFeatureName, "");
  aw_field_trials.RegisterFeatureOverrides(feature_list.get());
  base::FeatureList::SetInstance(std::move(feature_list));

  EXPECT_TRUE(base::FeatureList::IsEnabled(kTestDisabledFeature));
}

TEST_F(AwFieldTrialsTest, CommandLineDisableFeatureOverrides) {
  AwFieldTrials aw_field_trials;
  auto feature_list = std::make_unique<base::FeatureList>();
  feature_list->InitFromCommandLine("", kTestDisabledFeatureName);
  aw_field_trials.RegisterFeatureOverrides(feature_list.get());
  base::FeatureList::SetInstance(std::move(feature_list));

  EXPECT_FALSE(base::FeatureList::IsEnabled(kTestDisabledFeature));
}

TEST_F(AwFieldTrialsTest, OnlyRegisterFeatureOverrides) {
  AwFieldTrials aw_field_trials;
  auto feature_list = std::make_unique<base::FeatureList>();
  // Register feature override.
  aw_field_trials.RegisterFeatureOverrides(feature_list.get());
  base::FeatureList::SetInstance(std::move(feature_list));

  EXPECT_FALSE(base::FeatureList::IsEnabled(kTestDisabledFeature));
}

TEST_F(AwFieldTrialsTest, AwFeatureOverrides_NoPreviousOverrides) {
  auto feature_list = std::make_unique<base::FeatureList>();
  {
    internal::AwFeatureOverrides aw_feature_overrides(*feature_list);
    aw_feature_overrides.DisableFeature(kTestEnabledFeature);
    aw_feature_overrides.EnableFeature(kTestDisabledFeature);
  }
  base::FeatureList::SetInstance(std::move(feature_list));

  // When there are no previous overrides, the state should match what's set up
  // via AwFeatureOverrides.
  EXPECT_FALSE(base::FeatureList::IsEnabled(kTestEnabledFeature));
  EXPECT_TRUE(base::FeatureList::IsEnabled(kTestDisabledFeature));
}

TEST_F(AwFieldTrialsTest, AwFeatureOverrides_WithPreviousOverrides) {
  auto feature_list = std::make_unique<base::FeatureList>();
  SetUpFeatureTrial(feature_list.get(), kTestDisabledFeatureName,
                    base::FeatureList::OVERRIDE_ENABLE_FEATURE);
  SetUpFeatureTrial(feature_list.get(), kTestEnabledFeatureName,
                    base::FeatureList::OVERRIDE_DISABLE_FEATURE);
  {
    internal::AwFeatureOverrides aw_feature_overrides(*feature_list);
    aw_feature_overrides.DisableFeature(kTestDisabledFeature);
    aw_feature_overrides.EnableFeature(kTestEnabledFeature);
  }
  base::FeatureList::SetInstance(std::move(feature_list));

  // When there are previous overrides (e.g. coming from server-side configs in
  // production), they should take precedence over AwFeatureOverrides.
  EXPECT_TRUE(base::FeatureList::IsEnabled(kTestDisabledFeature));
  EXPECT_FALSE(base::FeatureList::IsEnabled(kTestEnabledFeature));
}

TEST_F(AwFieldTrialsTest, AwFeatureOverrides_WithPreviousUseDefaultOverride) {
  auto feature_list = std::make_unique<base::FeatureList>();
  SetUpFeatureTrial(feature_list.get(), kTestEnabledFeatureName,
                    base::FeatureList::OVERRIDE_USE_DEFAULT);
  {
    internal::AwFeatureOverrides aw_feature_overrides(*feature_list);
    aw_feature_overrides.DisableFeature(kTestEnabledFeature);
  }
  base::FeatureList::SetInstance(std::move(feature_list));

  // Before querying the feature, the trial shouldn't be active.
  EXPECT_FALSE(base::FieldTrialList::IsTrialActive(kTestEnabledFeatureName));
  // OVERRIDE_USE_DEFAULT entries should be overridden by AwFeatureOverrides.
  EXPECT_FALSE(base::FeatureList::IsEnabled(kTestEnabledFeature));
  // The associated trial should have been activated.
  EXPECT_TRUE(base::FieldTrialList::IsTrialActive(kTestEnabledFeatureName));
}

}  // namespace android_webview
