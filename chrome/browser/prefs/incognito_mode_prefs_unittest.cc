// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/incognito_mode_prefs.h"

#include <optional>

#include "base/test/gtest_util.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/test/base/testing_profile.h"
#include "components/enterprise/isolated_mode/isolated_mode_features.h"
#include "components/enterprise/isolated_mode/prefs.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

class IncognitoModePrefsTest : public testing::Test {
 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
};

TEST_F(IncognitoModePrefsTest, IntToAvailability) {
  ASSERT_EQ(0, static_cast<int>(policy::IncognitoModeAvailability::kEnabled));
  ASSERT_EQ(1, static_cast<int>(policy::IncognitoModeAvailability::kDisabled));
  ASSERT_EQ(2, static_cast<int>(policy::IncognitoModeAvailability::kForced));

  policy::IncognitoModeAvailability incognito;
  EXPECT_TRUE(IncognitoModePrefs::IntToAvailability(0, &incognito));
  EXPECT_EQ(policy::IncognitoModeAvailability::kEnabled, incognito);
  EXPECT_TRUE(IncognitoModePrefs::IntToAvailability(1, &incognito));
  EXPECT_EQ(policy::IncognitoModeAvailability::kDisabled, incognito);
  EXPECT_TRUE(IncognitoModePrefs::IntToAvailability(2, &incognito));
  EXPECT_EQ(policy::IncognitoModeAvailability::kForced, incognito);

  EXPECT_FALSE(IncognitoModePrefs::IntToAvailability(10, &incognito));
  EXPECT_EQ(IncognitoModePrefs::kDefaultAvailability, incognito);
  EXPECT_FALSE(IncognitoModePrefs::IntToAvailability(-1, &incognito));
  EXPECT_EQ(IncognitoModePrefs::kDefaultAvailability, incognito);
}

TEST_F(IncognitoModePrefsTest, GetAvailability) {
  profile_.GetTestingPrefService()->SetUserPref(
      policy::policy_prefs::kIncognitoModeAvailability,
      std::make_unique<base::Value>(
          static_cast<int>(policy::IncognitoModeAvailability::kEnabled)));
  EXPECT_EQ(policy::IncognitoModeAvailability::kEnabled,
            IncognitoModePrefs::GetAvailability(&profile_));

  profile_.GetTestingPrefService()->SetUserPref(
      policy::policy_prefs::kIncognitoModeAvailability,
      std::make_unique<base::Value>(
          static_cast<int>(policy::IncognitoModeAvailability::kDisabled)));
  EXPECT_EQ(policy::IncognitoModeAvailability::kDisabled,
            IncognitoModePrefs::GetAvailability(&profile_));

  profile_.GetTestingPrefService()->SetUserPref(
      policy::policy_prefs::kIncognitoModeAvailability,
      std::make_unique<base::Value>(
          static_cast<int>(policy::IncognitoModeAvailability::kForced)));
  EXPECT_EQ(policy::IncognitoModeAvailability::kForced,
            IncognitoModePrefs::GetAvailability(&profile_));
}

// Tests that the Enterprise Isolated Mode setting has higher priority than the
// Incognito mode availability preference.
TEST_F(IncognitoModePrefsTest, IsolatedModeHasHigherPriority) {
  base::test::ScopedFeatureList scoped_feature_list(
      enterprise_isolated_mode::kEnableEnterpriseIsolatedMode);
  profile_.GetTestingPrefService()->SetInteger(
      enterprise_isolated_mode::kEnterpriseIsolatedModeSettings,
      static_cast<int>(
          enterprise_isolated_mode::IsolatedModeSetting::kEnabled));
  profile_.GetTestingPrefService()->SetInteger(
      policy::policy_prefs::kIncognitoModeAvailability,
      static_cast<int>(policy::IncognitoModeAvailability::kDisabled));

  EXPECT_EQ(policy::IncognitoModeAvailability::kEnabled,
            IncognitoModePrefs::GetAvailability(&profile_));
}

struct TypeTestCase {
  const char* test_name;
  enterprise_isolated_mode::IsolatedModeSetting isolated_mode_setting;
  policy::IncognitoModeAvailability incognito_mode_availability;
  IncognitoModePrefs::IncognitoModeType expected_type;
};

class IncognitoModePrefsTypeTest
    : public IncognitoModePrefsTest,
      public testing::WithParamInterface<TypeTestCase> {};

TEST_P(IncognitoModePrefsTypeTest, GetType) {
  const TypeTestCase& test_case = GetParam();

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      enterprise_isolated_mode::kEnableEnterpriseIsolatedMode);

  profile_.GetTestingPrefService()->SetInteger(
      enterprise_isolated_mode::kEnterpriseIsolatedModeSettings,
      static_cast<int>(test_case.isolated_mode_setting));

  profile_.GetTestingPrefService()->SetInteger(
      policy::policy_prefs::kIncognitoModeAvailability,
      static_cast<int>(test_case.incognito_mode_availability));

  EXPECT_EQ(test_case.expected_type,
            IncognitoModePrefs::GetIncognitoModeType(&profile_));
  EXPECT_EQ(
      test_case.expected_type != IncognitoModePrefs::IncognitoModeType::kNone,
      IncognitoModePrefs::IsIncognitoAllowed(&profile_));
  EXPECT_TRUE(IncognitoModePrefs::IsIncognitoTypeAllowed(
      &profile_, test_case.expected_type));
}

const TypeTestCase kTypeTestCases[] = {
    {
        .test_name = "IncognitoAndIsolatedModeDisabled",
        .isolated_mode_setting =
            enterprise_isolated_mode::IsolatedModeSetting::kDisabled,
        .incognito_mode_availability =
            policy::IncognitoModeAvailability::kDisabled,
        .expected_type = IncognitoModePrefs::IncognitoModeType::kNone,
    },
    {
        .test_name = "OnlyIncognitoEnabled",
        .isolated_mode_setting =
            enterprise_isolated_mode::IsolatedModeSetting::kDisabled,
        .incognito_mode_availability =
            policy::IncognitoModeAvailability::kEnabled,
        .expected_type = IncognitoModePrefs::IncognitoModeType::kStandard,
    },
    {
        .test_name = "OnlyIsolatedModeEnabled",
        .isolated_mode_setting =
            enterprise_isolated_mode::IsolatedModeSetting::kEnabled,
        .incognito_mode_availability =
            policy::IncognitoModeAvailability::kEnabled,
        .expected_type = IncognitoModePrefs::IncognitoModeType::kEnterprise,
    },
    {
        .test_name = "IncognitoForcedIsolatedModeDisabled",
        .isolated_mode_setting =
            enterprise_isolated_mode::IsolatedModeSetting::kDisabled,
        .incognito_mode_availability =
            policy::IncognitoModeAvailability::kForced,
        .expected_type = IncognitoModePrefs::IncognitoModeType::kStandard,
    },
    {
        .test_name = "IsolatedModeEnabledIncognitoDisabled",
        .isolated_mode_setting =
            enterprise_isolated_mode::IsolatedModeSetting::kEnabled,
        .incognito_mode_availability =
            policy::IncognitoModeAvailability::kDisabled,
        .expected_type = IncognitoModePrefs::IncognitoModeType::kEnterprise,
    },
};

INSTANTIATE_TEST_SUITE_P(
    All,
    IncognitoModePrefsTypeTest,
    testing::ValuesIn(kTypeTestCases),
    [](const testing::TestParamInfo<TypeTestCase>& test_param_info) {
      return test_param_info.param.test_name;
    });
