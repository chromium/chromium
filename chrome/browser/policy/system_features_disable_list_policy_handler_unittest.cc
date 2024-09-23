// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/system_features_disable_list_policy_handler.h"

#include <asm-generic/errno-base.h>

#include "base/test/metrics/histogram_tester.h"
#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_pref_names.h"
#endif

namespace policy {
class SystemFeaturesDisableListPolicyHandlerTest : public testing::Test {
 public:
  SystemFeaturesDisableListPolicyHandlerTest() = default;
  ~SystemFeaturesDisableListPolicyHandlerTest() override = default;

  base::HistogramTester histogram_tester_;
  PrefValueMap prefs_;
  SystemFeaturesDisableListPolicyHandler policy_handler_;

  void ApplyPolicySettings(std::vector<std::string> list) {
    PolicyMap policy_map;
    base::Value::List features_list;
    for (auto& i : list) {
      features_list.Append(i);
    }
    policy_map.Set(policy::key::kSystemFeaturesDisableList,
                   policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                   policy::POLICY_SOURCE_CLOUD,
                   base::Value(std::move(features_list)), nullptr);
    policy_handler_.ApplyPolicySettings(policy_map, &prefs_);
  }

  void VerifyPrefList(std::vector<SystemFeature> expected) {
    base::Value::List expected_list;
    for (auto& i : expected) {
      expected_list.Append(static_cast<int>(i));
    }

    base::Value* value = nullptr;
    EXPECT_TRUE(
        prefs_.GetValue(policy_prefs::kSystemFeaturesDisableList, &value));
    EXPECT_EQ(expected_list, *value);
  }
};

TEST_F(SystemFeaturesDisableListPolicyHandlerTest, ShouldHandleSomeSettings) {
  ApplyPolicySettings({"camera", "browser_settings"});

  VerifyPrefList({SystemFeature::kCamera, SystemFeature::kBrowserSettings});

  std::vector<base::Bucket> expected_histogram{
      base::Bucket(static_cast<int>(SystemFeature::kCamera), 1),
      base::Bucket(static_cast<int>(SystemFeature::kBrowserSettings), 1)};

  EXPECT_EQ(
      histogram_tester_.GetAllSamples(kSystemFeaturesDisableListHistogram),
      expected_histogram);
}

TEST_F(SystemFeaturesDisableListPolicyHandlerTest, ShouldHandleAllSettings) {
  ApplyPolicySettings({"camera", "os_settings", "browser_settings", "scanning",
                       "web_store", "canvas", "explore", "crosh", "terminal",
                       "gallery", "print_jobs", "key_shortcuts", "recorder"});

  VerifyPrefList({SystemFeature::kCamera, SystemFeature::kOsSettings,
                  SystemFeature::kBrowserSettings, SystemFeature::kScanning,
                  SystemFeature::kWebStore, SystemFeature::kCanvas,
                  SystemFeature::kExplore, SystemFeature::kCrosh,
                  SystemFeature::kTerminal, SystemFeature::kGallery,
                  SystemFeature::kPrintJobs, SystemFeature::kKeyShortcuts,
                  SystemFeature::kRecorder});

  std::vector<base::Bucket> expected_histogram{
      base::Bucket(static_cast<int>(SystemFeature::kCamera), 1),
      base::Bucket(static_cast<int>(SystemFeature::kBrowserSettings), 1),
      base::Bucket(static_cast<int>(SystemFeature::kOsSettings), 1),
      base::Bucket(static_cast<int>(SystemFeature::kScanning), 1),
      base::Bucket(static_cast<int>(SystemFeature::kWebStore), 1),
      base::Bucket(static_cast<int>(SystemFeature::kCanvas), 1),
      base::Bucket(static_cast<int>(SystemFeature::kExplore), 1),
      base::Bucket(static_cast<int>(SystemFeature::kCrosh), 1),
      base::Bucket(static_cast<int>(SystemFeature::kTerminal), 1),
      base::Bucket(static_cast<int>(SystemFeature::kGallery), 1),
      base::Bucket(static_cast<int>(SystemFeature::kPrintJobs), 1),
      base::Bucket(static_cast<int>(SystemFeature::kKeyShortcuts), 1),
      base::Bucket(static_cast<int>(SystemFeature::kRecorder), 1)};

  EXPECT_EQ(
      histogram_tester_.GetAllSamples(kSystemFeaturesDisableListHistogram),
      expected_histogram);
}

TEST_F(SystemFeaturesDisableListPolicyHandlerTest,
       ShouldHandleLongerListAfterShorterList) {
  ApplyPolicySettings({"camera", "os_settings"});
  ApplyPolicySettings({"camera", "os_settings", "browser_settings", "scanning",
                       "web_store", "canvas", "explore"});

  VerifyPrefList({SystemFeature::kCamera, SystemFeature::kOsSettings,
                  SystemFeature::kBrowserSettings, SystemFeature::kScanning,
                  SystemFeature::kWebStore, SystemFeature::kCanvas,
                  SystemFeature::kExplore});

  std::vector<base::Bucket> expected_histogram{
      base::Bucket(static_cast<int>(SystemFeature::kCamera), 1),
      base::Bucket(static_cast<int>(SystemFeature::kBrowserSettings), 1),
      base::Bucket(static_cast<int>(SystemFeature::kOsSettings), 1),
      base::Bucket(static_cast<int>(SystemFeature::kScanning), 1),
      base::Bucket(static_cast<int>(SystemFeature::kWebStore), 1),
      base::Bucket(static_cast<int>(SystemFeature::kCanvas), 1),
      base::Bucket(static_cast<int>(SystemFeature::kExplore), 1)};

  EXPECT_EQ(
      histogram_tester_.GetAllSamples(kSystemFeaturesDisableListHistogram),
      expected_histogram);
}

TEST_F(SystemFeaturesDisableListPolicyHandlerTest,
       ShouldHandleUnknownFeatureName) {
  ApplyPolicySettings({"unknown-feature"});

  VerifyPrefList({SystemFeature::kUnknownSystemFeature});

  std::vector<base::Bucket> expected_histogram{
      base::Bucket(static_cast<int>(SystemFeature::kUnknownSystemFeature), 1)};

  EXPECT_EQ(
      histogram_tester_.GetAllSamples(kSystemFeaturesDisableListHistogram),
      expected_histogram);
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(SystemFeaturesDisableListPolicyHandlerTest,
       ShouldDisableOsSettingsWhenSet) {
  ApplyPolicySettings({"os_settings"});

  base::Value* value = nullptr;
  EXPECT_TRUE(prefs_.GetValue(ash::prefs::kOsSettingsEnabled, &value));
  EXPECT_FALSE(value->GetBool());
}

TEST_F(SystemFeaturesDisableListPolicyHandlerTest,
       ShouldEnableOsSettingsWhenUnsest) {
  ApplyPolicySettings({"os_settings"});
  ApplyPolicySettings({});

  base::Value* value = nullptr;
  EXPECT_TRUE(prefs_.GetValue(ash::prefs::kOsSettingsEnabled, &value));
  EXPECT_TRUE(value->GetBool());
}
#endif

}  // namespace policy
