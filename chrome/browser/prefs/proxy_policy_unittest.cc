// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/prefs/chrome_command_line_pref_store.h"
#include "chrome/common/chrome_switches.h"
#include "components/policy/core/common/external_data_fetcher.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_service_impl.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/proxy_config/proxy_config_dictionary.h"
#include "components/proxy_config/proxy_config_pref_names.h"
#include "components/sync_preferences/pref_service_mock_factory.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/ash/components/install_attributes/stub_install_attributes.h"
#endif

using ::testing::Return;
using ::testing::_;

namespace policy {

namespace {

void assertProxyMode(const ProxyConfigDictionary& dict,
                     ProxyPrefs::ProxyMode expected_mode) {
  ProxyPrefs::ProxyMode actual_mode;
  ASSERT_TRUE(dict.GetMode(&actual_mode));
  EXPECT_EQ(expected_mode, actual_mode);
}

void assertProxyServer(const ProxyConfigDictionary& dict,
                       const std::string& expected) {
  std::string actual;
  if (!expected.empty()) {
    ASSERT_TRUE(dict.GetProxyServer(&actual));
    EXPECT_EQ(expected, actual);
  } else {
    EXPECT_FALSE(dict.GetProxyServer(&actual));
  }
}

void assertPacUrl(const ProxyConfigDictionary& dict,
                  const std::string& expected) {
  std::string actual;
  if (!expected.empty()) {
    ASSERT_TRUE(dict.GetPacUrl(&actual));
    EXPECT_EQ(expected, actual);
  } else {
    EXPECT_FALSE(dict.GetPacUrl(&actual));
  }
}

void assertBypassList(const ProxyConfigDictionary& dict,
                      const std::string& expected) {
  std::string actual;
  if (!expected.empty()) {
    ASSERT_TRUE(dict.GetBypassList(&actual));
    EXPECT_EQ(expected, actual);
  } else {
    EXPECT_FALSE(dict.GetBypassList(&actual));
  }
}

void assertProxyModeWithoutParams(const ProxyConfigDictionary& dict,
                                  ProxyPrefs::ProxyMode proxy_mode) {
  assertProxyMode(dict, proxy_mode);
  assertProxyServer(dict, std::string());
  assertPacUrl(dict, std::string());
  assertBypassList(dict, std::string());
}

}  // namespace

class ProxyPolicyTest : public testing::Test {
 protected:
  ProxyPolicyTest() : command_line_(base::CommandLine::NO_PROGRAM) {}

  void SetUp() override {
    provider_.SetDefaultReturns(
        /*is_initialization_complete_return=*/true,
        /*is_first_policy_load_complete_return=*/true);

    PolicyServiceImpl::Providers providers;
    providers.push_back(&provider_);
    policy_service_ = std::make_unique<PolicyServiceImpl>(std::move(providers));
    provider_.Init();
  }

  void TearDown() override { provider_.Shutdown(); }

  std::unique_ptr<PrefService> CreatePrefService(bool with_managed_policies) {
    sync_preferences::PrefServiceMockFactory factory;
    factory.set_command_line_prefs(
        new ChromeCommandLinePrefStore(&command_line_));
    if (with_managed_policies) {
      factory.SetManagedPolicies(policy_service_.get(),
                                 g_browser_process->browser_policy_connector());
    }

    scoped_refptr<user_prefs::PrefRegistrySyncable> registry(
        new user_prefs::PrefRegistrySyncable);
    std::unique_ptr<sync_preferences::PrefServiceSyncable> prefs =
        factory.CreateSyncable(registry.get());
    RegisterUserProfilePrefs(registry.get());
    return std::move(prefs);
  }

  content::BrowserTaskEnvironment task_environment_;
  base::CommandLine command_line_;
  testing::NiceMock<MockConfigurationPolicyProvider> provider_;
  std::unique_ptr<PolicyServiceImpl> policy_service_;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  ash::ScopedStubInstallAttributes test_install_attributes_;
#endif
};

TEST_F(ProxyPolicyTest, OverridesCommandLineOptions) {
  command_line_.AppendSwitchASCII(switches::kProxyBypassList, "123");
  command_line_.AppendSwitchASCII(switches::kProxyServer, "789");
  base::Value mode_name(ProxyPrefs::kFixedServersProxyModeName);
  PolicyMap policy;
  policy.Set(key::kProxyMode, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
             POLICY_SOURCE_CLOUD, std::move(mode_name), nullptr);
  policy.Set(key::kProxyBypassList, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
             POLICY_SOURCE_CLOUD, base::Value("abc"), nullptr);
  policy.Set(key::kProxyServer, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
             POLICY_SOURCE_CLOUD, base::Value("ghi"), nullptr);
  provider_.UpdateChromePolicy(policy);

  // First verify that command-line options are set correctly when
  // there is no policy in effect.
  std::unique_ptr<PrefService> prefs(CreatePrefService(false));
  ProxyConfigDictionary dict(
      prefs->GetDict(proxy_config::prefs::kProxy).Clone());
  assertProxyMode(dict, ProxyPrefs::MODE_FIXED_SERVERS);
  assertProxyServer(dict, "789");
  assertPacUrl(dict, std::string());
  assertBypassList(dict, "123");

  // Try a second time time with the managed PrefStore in place, the
  // manual proxy policy should have removed all traces of the command
  // line and replaced them with the policy versions.
  prefs = CreatePrefService(true);
  ProxyConfigDictionary dict2(
      prefs->GetDict(proxy_config::prefs::kProxy).Clone());
  assertProxyMode(dict2, ProxyPrefs::MODE_FIXED_SERVERS);
  assertProxyServer(dict2, "ghi");
  assertPacUrl(dict2, std::string());
  assertBypassList(dict2, "abc");
}

TEST_F(ProxyPolicyTest, OverridesUnrelatedCommandLineOptions) {
  command_line_.AppendSwitchASCII(switches::kProxyBypassList, "123");
  command_line_.AppendSwitchASCII(switches::kProxyServer, "789");
  base::Value mode_name(ProxyPrefs::kAutoDetectProxyModeName);
  PolicyMap policy;
  policy.Set(key::kProxyMode, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
             POLICY_SOURCE_CLOUD, std::move(mode_name), nullptr);
  provider_.UpdateChromePolicy(policy);

  // First verify that command-line options are set correctly when
  // there is no policy in effect.
  std::unique_ptr<PrefService> prefs = CreatePrefService(false);
  ProxyConfigDictionary dict(
      prefs->GetDict(proxy_config::prefs::kProxy).Clone());
  assertProxyMode(dict, ProxyPrefs::MODE_FIXED_SERVERS);
  assertProxyServer(dict, "789");
  assertPacUrl(dict, std::string());
  assertBypassList(dict, "123");

  // Try a second time time with the managed PrefStore in place, the
  // no proxy policy should have removed all traces of the command
  // line proxy settings, even though they were not the specific one
  // set in policy.
  prefs = CreatePrefService(true);
  ProxyConfigDictionary dict2(
      prefs->GetDict(proxy_config::prefs::kProxy).Clone());
  assertProxyModeWithoutParams(dict2, ProxyPrefs::MODE_AUTO_DETECT);
}

TEST_F(ProxyPolicyTest, OverridesCommandLineNoProxy) {
  command_line_.AppendSwitch(switches::kNoProxyServer);
  base::Value mode_name(ProxyPrefs::kAutoDetectProxyModeName);
  PolicyMap policy;
  policy.Set(key::kProxyMode, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
             POLICY_SOURCE_CLOUD, std::move(mode_name), nullptr);
  provider_.UpdateChromePolicy(policy);

  // First verify that command-line options are set correctly when
  // there is no policy in effect.
  std::unique_ptr<PrefService> prefs = CreatePrefService(false);
  ProxyConfigDictionary dict(
      prefs->GetDict(proxy_config::prefs::kProxy).Clone());
  assertProxyModeWithoutParams(dict, ProxyPrefs::MODE_DIRECT);

  // Try a second time time with the managed PrefStore in place, the
  // auto-detect should be overridden. The default pref store must be
  // in place with the appropriate default value for this to work.
  prefs = CreatePrefService(true);
  ProxyConfigDictionary dict2(
      prefs->GetDict(proxy_config::prefs::kProxy).Clone());
  assertProxyModeWithoutParams(dict2, ProxyPrefs::MODE_AUTO_DETECT);
}

TEST_F(ProxyPolicyTest, OverridesCommandLineAutoDetect) {
  command_line_.AppendSwitch(switches::kProxyAutoDetect);
  base::Value mode_name(ProxyPrefs::kDirectProxyModeName);
  PolicyMap policy;
  policy.Set(key::kProxyMode, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
             POLICY_SOURCE_CLOUD, std::move(mode_name), nullptr);
  provider_.UpdateChromePolicy(policy);

  // First verify that the auto-detect is set if there is no managed
  // PrefStore.
  std::unique_ptr<PrefService> prefs = CreatePrefService(false);
  ProxyConfigDictionary dict(
      prefs->GetDict(proxy_config::prefs::kProxy).Clone());
  assertProxyModeWithoutParams(dict, ProxyPrefs::MODE_AUTO_DETECT);

  // Try a second time time with the managed PrefStore in place, the
  // auto-detect should be overridden. The default pref store must be
  // in place with the appropriate default value for this to work.
  prefs = CreatePrefService(true);
  ProxyConfigDictionary dict2(
      prefs->GetDict(proxy_config::prefs::kProxy).Clone());
  assertProxyModeWithoutParams(dict2, ProxyPrefs::MODE_DIRECT);
}

}  // namespace policy
