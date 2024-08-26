// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_feature_entries.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "base/test/scoped_feature_list.h"
#include "components/flags_ui/feature_entry.h"
#include "components/flags_ui/feature_entry_macros.h"
#include "components/flags_ui/flags_state.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace android_webview {

namespace {

BASE_FEATURE(kTestFeature,
             "AwFeatureEntriesTest",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kTestFeature2,
             "AwFeatureEntriesTest2",
             base::FEATURE_DISABLED_BY_DEFAULT);

const flags_ui::FeatureEntry::FeatureParam kForceDark_SimpleHsl[] = {
    {"inversion_method", "hsl_based"},
    {"image_behavior", "none"},
    {"foreground_lightness_threshold", "255"},
    {"background_lightness_threshold", "0"}};

const flags_ui::FeatureEntry::FeatureParam kForceDark_SimpleCielab[] = {
    {"inversion_method", "cielab_based"},
    {"image_behavior", "none"},
    {"foreground_lightness_threshold", "255"},
    {"background_lightness_threshold", "0"}};

const flags_ui::FeatureEntry::FeatureParam kForceDark_SimpleRgb[] = {
    {"inversion_method", "rgb_based"},
    {"image_behavior", "none"},
    {"foreground_lightness_threshold", "255"},
    {"background_lightness_threshold", "0"}};

const flags_ui::FeatureEntry::FeatureVariation kForceDarkVariations[] = {
    {"with simple HSL-based inversion", kForceDark_SimpleHsl,
     std::size(kForceDark_SimpleHsl), nullptr},
    {"with simple CIELAB-based inversion", kForceDark_SimpleCielab,
     std::size(kForceDark_SimpleCielab), nullptr},
    {"with simple RGB-based inversion", kForceDark_SimpleRgb,
     std::size(kForceDark_SimpleRgb), nullptr}};

// Not for display, set the descriptions to empty.
flags_ui::FeatureEntry kForceDark = {
    "enable-force-dark", "", "", flags_ui::kOsWebView,
    FEATURE_WITH_PARAMS_VALUE_TYPE(kTestFeature,
                                   kForceDarkVariations,
                                   "ForceDarkVariations")};

flags_ui::FeatureEntry kForceDarkDisabled = {
    "enable-force-dark2", "", "", flags_ui::kOsWebView,
    FEATURE_WITH_PARAMS_VALUE_TYPE(kTestFeature2,
                                   kForceDarkVariations,
                                   "ForceDarkVariations")};

flags_ui::FeatureEntry kWebViewTestFeatureEntries[] = {
    kForceDark,
    kForceDarkDisabled,
};

void VerifyFeatureParameters(
    const flags_ui::FeatureEntry::FeatureVariation& variation) {
  for (int i = 0; i < variation.num_params; i++) {
    base::FeatureParam<std::string> param{&kTestFeature,
                                          variation.params[i].param_name, ""};
    EXPECT_EQ(variation.params[i].param_value, param.Get());
  }
}

}  // namespace

TEST(AwFeatureEntriesTest, ToEnabledEntry) {
  EXPECT_EQ("enable-force-dark@2",
            aw_feature_entries::internal::ToEnabledEntry(kForceDark, 0));
  EXPECT_EQ("enable-force-dark@4",
            aw_feature_entries::internal::ToEnabledEntry(kForceDark, 2));
}

TEST(AwFeatureEntriesTest, RegisterEnabledFeatureEntries) {
  std::set<std::string> enabled_entries;
  enabled_entries.insert(
      aw_feature_entries::internal::ToEnabledEntry(kForceDark, 2));
  auto feature_list = std::make_unique<base::FeatureList>();
  flags_ui::FlagsState::RegisterEnabledFeatureVariationParameters(
      kWebViewTestFeatureEntries, enabled_entries, "webview_dev_ui",
      feature_list.get());
  EXPECT_FALSE(feature_list->IsFeatureOverridden(kTestFeature2.name));
  EXPECT_TRUE(feature_list->IsFeatureOverridden(kTestFeature.name));

  // FeatureList can only be queried when the initialization is done, replaces
  // the global one to set the initialization being finished.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatureList(std::move(feature_list));
  base::FieldTrial* field_trial =
      base::FeatureList::GetInstance()->GetEnabledFieldTrialByFeatureName(
          kForceDark.feature.feature->name);
  EXPECT_TRUE(field_trial);
  EXPECT_EQ("webview_dev_ui", field_trial->group_name());
  EXPECT_EQ("ForceDarkVariations", field_trial->trial_name());
  // Verify the enabled variation kForceDark_SimpleRgb (index = 2) setup
  // correctly.
  VerifyFeatureParameters(kForceDark.feature.feature_variations[2]);
}

}  // namespace android_webview
