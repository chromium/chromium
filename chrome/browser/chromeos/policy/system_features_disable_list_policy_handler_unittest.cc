// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/system_features_disable_list_policy_handler.h"

#include "ash/public/cpp/ash_pref_names.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/values.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {
class SystemFeaturesDisableListPolicyHandlerTest : public testing::Test {
 public:
  SystemFeaturesDisableListPolicyHandlerTest() = default;
  ~SystemFeaturesDisableListPolicyHandlerTest() override = default;

  base::HistogramTester histogram_tester_;
};

// Tests that ApplyList sets the right enum values for the corresponding strings
// & records them via UMA histogram.
TEST_F(SystemFeaturesDisableListPolicyHandlerTest, ApplyListTest) {
  PolicyMap policy_map;
  PrefValueMap prefs;
  base::Value* value = nullptr;
  SystemFeaturesDisableListPolicyHandler policy_handler;
  base::Value features_list(base::Value::Type::LIST);

  features_list.Append("camera");
  features_list.Append("browser_settings");

  policy_map.Set(policy::key::kSystemFeaturesDisableList,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                 policy::POLICY_SOURCE_CLOUD, std::move(features_list),
                 nullptr);
  policy_handler.ApplyPolicySettings(policy_map, &prefs);

  EXPECT_TRUE(prefs.GetValue(ash::prefs::kOsSettingsEnabled, &value));
  EXPECT_TRUE(value->GetBool());

  base::Value expected_list(base::Value::Type::LIST);
  expected_list.Append(SystemFeature::kCamera);
  expected_list.Append(SystemFeature::kBrowserSettings);

  EXPECT_TRUE(prefs.GetValue(policy_prefs::kSystemFeaturesDisableList, &value));
  EXPECT_EQ(expected_list, *value);

  histogram_tester_.ExpectTotalCount(kSystemFeaturesDisableListHistogram, 2);
  histogram_tester_.ExpectBucketCount(kSystemFeaturesDisableListHistogram,
                                      SystemFeature::kCamera,
                                      /*amount*/ 1);
  histogram_tester_.ExpectBucketCount(kSystemFeaturesDisableListHistogram,
                                      SystemFeature::kBrowserSettings,
                                      /*amount*/ 1);
  histogram_tester_.ExpectBucketCount(kSystemFeaturesDisableListHistogram,
                                      SystemFeature::kOsSettings,
                                      /*amount*/ 0);
  histogram_tester_.ExpectBucketCount(kSystemFeaturesDisableListHistogram,
                                      SystemFeature::kScanning,
                                      /*amount*/ 0);
  histogram_tester_.ExpectBucketCount(kSystemFeaturesDisableListHistogram,
                                      SystemFeature::kUnknownSystemFeature,
                                      /*amount*/ 0);
  histogram_tester_.ExpectBucketCount(kSystemFeaturesDisableListHistogram,
                                      SystemFeature::kWebStore,
                                      /*amount*/ 0);
  histogram_tester_.ExpectBucketCount(kSystemFeaturesDisableListHistogram,
                                      SystemFeature::kCanvas,
                                      /*amount*/ 0);
  histogram_tester_.ExpectBucketCount(kSystemFeaturesDisableListHistogram,
                                      SystemFeature::kGoogleNews,
                                      /*amount*/ 0);
  histogram_tester_.ExpectBucketCount(kSystemFeaturesDisableListHistogram,
                                      SystemFeature::kExplore,
                                      /*amount*/ 0);

  features_list.ClearList();
  features_list.Append("camera");
  features_list.Append("os_settings");
  features_list.Append("scanning");
  features_list.Append("gallery");
  features_list.Append("web_store");
  features_list.Append("canvas");
  features_list.Append("google_news");
  features_list.Append("explore");

  policy_map.Set(policy::key::kSystemFeaturesDisableList,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                 policy::POLICY_SOURCE_CLOUD, std::move(features_list),
                 nullptr);
  policy_handler.ApplyPolicySettings(policy_map, &prefs);

  EXPECT_TRUE(prefs.GetValue(ash::prefs::kOsSettingsEnabled, &value));
  EXPECT_FALSE(value->GetBool());

  expected_list.ClearList();
  expected_list.Append(SystemFeature::kCamera);
  expected_list.Append(SystemFeature::kOsSettings);
  expected_list.Append(SystemFeature::kScanning);
  expected_list.Append(SystemFeature::kUnknownSystemFeature);
  expected_list.Append(SystemFeature::kWebStore);
  expected_list.Append(SystemFeature::kCanvas);
  expected_list.Append(SystemFeature::kGoogleNews);
  expected_list.Append(SystemFeature::kExplore);

  EXPECT_TRUE(prefs.GetValue(policy_prefs::kSystemFeaturesDisableList, &value));
  EXPECT_EQ(expected_list, *value);

  histogram_tester_.ExpectTotalCount(kSystemFeaturesDisableListHistogram, 9);
  histogram_tester_.ExpectBucketCount(kSystemFeaturesDisableListHistogram,
                                      SystemFeature::kCamera,
                                      /*amount*/ 1);
  histogram_tester_.ExpectBucketCount(kSystemFeaturesDisableListHistogram,
                                      SystemFeature::kBrowserSettings,
                                      /*amount*/ 1);
  histogram_tester_.ExpectBucketCount(kSystemFeaturesDisableListHistogram,
                                      SystemFeature::kOsSettings,
                                      /*amount*/ 1);
  histogram_tester_.ExpectBucketCount(kSystemFeaturesDisableListHistogram,
                                      SystemFeature::kScanning,
                                      /*amount*/ 1);
  histogram_tester_.ExpectBucketCount(kSystemFeaturesDisableListHistogram,
                                      SystemFeature::kUnknownSystemFeature,
                                      /*amount*/ 1);
  histogram_tester_.ExpectBucketCount(kSystemFeaturesDisableListHistogram,
                                      SystemFeature::kWebStore,
                                      /*amount*/ 1);
  histogram_tester_.ExpectBucketCount(kSystemFeaturesDisableListHistogram,
                                      SystemFeature::kCanvas,
                                      /*amount*/ 1);
  histogram_tester_.ExpectBucketCount(kSystemFeaturesDisableListHistogram,
                                      SystemFeature::kGoogleNews,
                                      /*amount*/ 1);
  histogram_tester_.ExpectBucketCount(kSystemFeaturesDisableListHistogram,
                                      SystemFeature::kExplore,
                                      /*amount*/ 1);
}
}  // namespace policy
