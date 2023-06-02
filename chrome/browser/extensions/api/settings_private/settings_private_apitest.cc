// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <tuple>

#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/task/current_thread.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/extensions/api/settings_private/settings_private_delegate.h"
#include "chrome/browser/extensions/api/settings_private/settings_private_delegate_factory.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/common/extensions/api/settings_private.h"
#include "chrome/common/pref_names.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/test/content_settings_mock_provider.h"
#include "components/content_settings/core/test/content_settings_test_utils.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "components/supervised_user/core/common/features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "extensions/common/switches.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/settings/scoped_testing_cros_settings.h"
#endif

using testing::Mock;
using testing::Return;
using testing::_;

namespace extensions {

namespace {

using ContextType = ExtensionBrowserTest::ContextType;

class SettingsPrivateApiTest
    : public ExtensionApiTest,
      public testing::WithParamInterface<std::tuple<ContextType, bool>> {
 public:
  SettingsPrivateApiTest() : ExtensionApiTest(std::get<0>(GetParam())) {}
  ~SettingsPrivateApiTest() override = default;
  SettingsPrivateApiTest(const SettingsPrivateApiTest&) = delete;
  SettingsPrivateApiTest& operator=(const SettingsPrivateApiTest&) = delete;

  void SetUpInProcessBrowserTestFixture() override {
    bool enable_supervised_prefs_flag = std::get<1>(GetParam());

    if (enable_supervised_prefs_flag) {
      feature_list_.InitWithFeatures(
          {supervised_user::kSupervisedPrefsControlledBySupervisedStore}, {});
    } else {
      feature_list_.InitWithFeatures(
          {}, {supervised_user::kSupervisedPrefsControlledBySupervisedStore});
    }

    provider_.SetDefaultReturns(
        /*is_initialization_complete_return=*/true,
        /*is_first_policy_load_complete_return=*/true);
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(&provider_);
    ExtensionApiTest::SetUpInProcessBrowserTestFixture();
  }

 protected:
  bool RunSettingsSubtest(const std::string& subtest) {
    return RunExtensionTest("settings_private", {.custom_arg = subtest.c_str()},
                            {.load_as_component = true});
  }

  void SetPrefPolicy(const std::string& key, policy::PolicyLevel level) {
    policy::PolicyMap policies;
    policies.Set(key, level, policy::POLICY_SCOPE_USER,
                 policy::POLICY_SOURCE_CLOUD, base::Value(true), nullptr);
    provider_.UpdateChromePolicy(policies);
    DCHECK(base::CurrentThread::Get());
    base::RunLoop loop;
    loop.RunUntilIdle();
  }

 private:
  testing::NiceMock<policy::MockConfigurationPolicyProvider> provider_;
  base::test::ScopedFeatureList feature_list_;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  ash::ScopedTestingCrosSettings scoped_testing_cros_settings_;
#endif
};

INSTANTIATE_TEST_SUITE_P(
    PersistentBackground,
    SettingsPrivateApiTest,
    ::testing::Combine(::testing::Values(ContextType::kPersistentBackground),
                       ::testing::Bool()));
INSTANTIATE_TEST_SUITE_P(
    ServiceWorker,
    SettingsPrivateApiTest,
    ::testing::Combine(::testing::Values(ContextType::kPersistentBackground),
                       ::testing::Bool()));

}  // namespace

IN_PROC_BROWSER_TEST_P(SettingsPrivateApiTest, SetPref) {
  EXPECT_TRUE(RunSettingsSubtest("setPref")) << message_;
}

IN_PROC_BROWSER_TEST_P(SettingsPrivateApiTest, GetPref) {
  EXPECT_TRUE(RunSettingsSubtest("getPref")) << message_;
}

IN_PROC_BROWSER_TEST_P(SettingsPrivateApiTest, GetEnforcedPref) {
  SetPrefPolicy(policy::key::kHomepageIsNewTabPage,
                policy::POLICY_LEVEL_MANDATORY);
  EXPECT_TRUE(RunSettingsSubtest("getEnforcedPref")) << message_;
}

IN_PROC_BROWSER_TEST_P(SettingsPrivateApiTest, GetRecommendedPref) {
  SetPrefPolicy(policy::key::kHomepageIsNewTabPage,
                policy::POLICY_LEVEL_RECOMMENDED);
  EXPECT_TRUE(RunSettingsSubtest("getRecommendedPref")) << message_;
}

IN_PROC_BROWSER_TEST_P(SettingsPrivateApiTest, GetDisabledPref) {
  HostContentSettingsMapFactory::GetForProfile(profile())
      ->SetDefaultContentSetting(ContentSettingsType::COOKIES,
                                 ContentSetting::CONTENT_SETTING_BLOCK);
  EXPECT_TRUE(RunSettingsSubtest("getDisabledPref")) << message_;
}

IN_PROC_BROWSER_TEST_P(SettingsPrivateApiTest, GetPartiallyManagedPref) {
  auto provider = std::make_unique<content_settings::MockProvider>();
  provider->SetWebsiteSetting(
      ContentSettingsPattern::Wildcard(), ContentSettingsPattern::Wildcard(),
      ContentSettingsType::COOKIES,
      base::Value(ContentSetting::CONTENT_SETTING_ALLOW));
  content_settings::TestUtils::OverrideProvider(
      HostContentSettingsMapFactory::GetForProfile(profile()),
      std::move(provider), HostContentSettingsMap::POLICY_PROVIDER);
  EXPECT_TRUE(RunSettingsSubtest("getPartiallyManagedPref")) << message_;
}

IN_PROC_BROWSER_TEST_P(SettingsPrivateApiTest, GetAllPrefs) {
  EXPECT_TRUE(RunSettingsSubtest("getAllPrefs")) << message_;
}

IN_PROC_BROWSER_TEST_P(SettingsPrivateApiTest, OnPrefsChanged) {
  EXPECT_TRUE(RunSettingsSubtest("onPrefsChanged")) << message_;
}

IN_PROC_BROWSER_TEST_P(SettingsPrivateApiTest, GetManagedByParentPref) {
  auto provider = std::make_unique<content_settings::MockProvider>();
  provider->SetWebsiteSetting(
      ContentSettingsPattern::Wildcard(), ContentSettingsPattern::Wildcard(),
      ContentSettingsType::COOKIES,
      base::Value(ContentSetting::CONTENT_SETTING_BLOCK));
  content_settings::TestUtils::OverrideProvider(
      HostContentSettingsMapFactory::GetForProfile(profile()),
      std::move(provider), HostContentSettingsMap::SUPERVISED_PROVIDER);
  EXPECT_TRUE(RunSettingsSubtest("getManagedByParentPref")) << message_;
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
IN_PROC_BROWSER_TEST_P(SettingsPrivateApiTest, GetPref_CrOSSetting) {
  EXPECT_TRUE(RunSettingsSubtest("getPref_CrOSSetting")) << message_;
}

IN_PROC_BROWSER_TEST_P(SettingsPrivateApiTest, SetPref_CrOSSetting) {
  EXPECT_TRUE(RunSettingsSubtest("setPref_CrOSSetting")) << message_;
}

IN_PROC_BROWSER_TEST_P(SettingsPrivateApiTest, OnPrefsChanged_CrOSSetting) {
  EXPECT_TRUE(RunSettingsSubtest("onPrefsChanged_CrOSSetting")) << message_;
}
#endif

}  // namespace extensions
