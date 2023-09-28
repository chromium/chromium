// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_field_trials.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"

namespace android_webview {

namespace {
const char kUserAgentClientHintFeatureName[] = "UserAgentClientHint";
const char kTestTrialName[] = "TestTrial";
}  // namespace

class AwFieldTrialsTest : public testing::Test {
 public:
  AwFieldTrialsTest() {
    // Default overrides takes effect before testing other override.
    EXPECT_TRUE(
        base::FeatureList::IsEnabled(blink::features::kUserAgentClientHint));
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
                         base::FeatureList::OverrideState override_state) {
    // Set-up a trial enable/disable user-agent client hint feature.
    variations::EntropyProviders entropy_providers("client_id", {0, 8000});
    scoped_refptr<base::FieldTrial> trial(
        base::FieldTrialList::FactoryGetFieldTrial(
            kTestTrialName, /*total_probability=*/1000, "Default",
            entropy_providers.default_entropy()));
    feature_list->RegisterFieldTrialOverride(kUserAgentClientHintFeatureName,
                                             override_state, trial.get());
  }

 private:
  //  Save the present FeatureList and restore it after test finish.
  std::unique_ptr<base::FeatureList> original_feature_list_;
};

TEST_F(AwFieldTrialsTest, TrialEnableFeatureOverrides) {
  AwFieldTrials aw_field_trials;
  auto feature_list = std::make_unique<base::FeatureList>();
  SetUpFeatureTrial(feature_list.get(),
                    base::FeatureList::OVERRIDE_ENABLE_FEATURE);
  aw_field_trials.RegisterFeatureOverrides(feature_list.get());
  base::FeatureList::SetInstance(std::move(feature_list));

  EXPECT_TRUE(
      base::FeatureList::IsEnabled(blink::features::kUserAgentClientHint));
}

TEST_F(AwFieldTrialsTest, TrialDisableFeatureOverrides) {
  AwFieldTrials aw_field_trials;
  auto feature_list = std::make_unique<base::FeatureList>();
  SetUpFeatureTrial(feature_list.get(),
                    base::FeatureList::OVERRIDE_DISABLE_FEATURE);
  aw_field_trials.RegisterFeatureOverrides(feature_list.get());
  base::FeatureList::SetInstance(std::move(feature_list));

  EXPECT_FALSE(
      base::FeatureList::IsEnabled(blink::features::kUserAgentClientHint));
}

TEST_F(AwFieldTrialsTest, CommandLineEnableFeatureOverrides) {
  AwFieldTrials aw_field_trials;
  auto feature_list = std::make_unique<base::FeatureList>();
  feature_list->InitializeFromCommandLine(kUserAgentClientHintFeatureName, "");
  aw_field_trials.RegisterFeatureOverrides(feature_list.get());
  base::FeatureList::SetInstance(std::move(feature_list));

  EXPECT_TRUE(
      base::FeatureList::IsEnabled(blink::features::kUserAgentClientHint));
}

TEST_F(AwFieldTrialsTest, CommandLineDisableFeatureOverrides) {
  AwFieldTrials aw_field_trials;
  auto feature_list = std::make_unique<base::FeatureList>();
  feature_list->InitializeFromCommandLine("", kUserAgentClientHintFeatureName);
  aw_field_trials.RegisterFeatureOverrides(feature_list.get());
  base::FeatureList::SetInstance(std::move(feature_list));

  EXPECT_FALSE(
      base::FeatureList::IsEnabled(blink::features::kUserAgentClientHint));
}

TEST_F(AwFieldTrialsTest, OnlyRegisterFeatureOverrides) {
  AwFieldTrials aw_field_trials;
  auto feature_list = std::make_unique<base::FeatureList>();
  // Register feature override.
  aw_field_trials.RegisterFeatureOverrides(feature_list.get());
  base::FeatureList::SetInstance(std::move(feature_list));

  EXPECT_TRUE(
      base::FeatureList::IsEnabled(blink::features::kUserAgentClientHint));

  EXPECT_FALSE(base::FeatureList::IsEnabled(
      blink::features::kReduceUserAgentMinorVersion));
}

}  // namespace android_webview
