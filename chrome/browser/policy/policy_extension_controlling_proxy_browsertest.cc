// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/callback_helpers.h"
#include "base/path_service.h"
#include "base/test/gtest_tags.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/chrome_content_verifier_delegate.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/mixin_based_extension_apitest.h"
#include "chrome/browser/policy/extension_force_install_mixin.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/prefs/pref_service.h"
#include "components/proxy_config/proxy_config_dictionary.h"
#include "components/proxy_config/proxy_config_pref_names.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/test_management_policy.h"
#include "extensions/common/extension.h"
#include "extensions/test/extension_test_message_listener.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/win_util.h"
#endif

namespace policy {

namespace {

using extensions::mojom::ManifestLocation;

// A proxy test extension which is setting proxy for chromium primary profile:
const char kProxySettingExtensionId[] = "cmppkjmdihjjoenffkfoejbjdgnlgnbm";
const char kProxySettingExtensionExtensionPath[] =
    "extensions/browsertest/proxy_setting_extension/";
const char kProxySettingExtensionPemPath[] =
    "extensions/browsertest/proxy_setting_extension.pem";

}  // namespace

class PolicyExtensionControllingProxyTest
    : public extensions::MixinBasedExtensionApiTest {
 public:
  PolicyExtensionControllingProxyTest() = default;

  std::string GetKeyFromProxyPrefs(const PrefService::Preference* prefs,
                                   const std::string& key) {
    return *prefs->GetValue()->GetDict().FindString(key);
  }

  const PrefService::Preference* GetOriginalProxyPrefs() {
    PrefService* prefs = browser()->profile()->GetOriginalProfile()->GetPrefs();
    return prefs->FindPreference(proxy_config::prefs::kProxy);
  }

  const PrefService::Preference* GetIncognitoProxyPrefs() {
    PrefService* prefs =
        browser()
            ->profile()
            ->GetOffTheRecordProfile(
                Profile::OTRProfileID::CreateUniqueForTesting(),
                /*create_if_needed=*/true)
            ->GetPrefs();
    return prefs->FindPreference(proxy_config::prefs::kProxy);
  }

  policy::MockConfigurationPolicyProvider* policy_provider() {
    return &mock_policy_provider_;
  }

  ExtensionForceInstallMixin* force_mixin() {
    return &extension_force_install_mixin_;
  }

  base::FilePath GetTestDataDir() {
    return base::PathService::CheckedGet(chrome::DIR_TEST_DATA);
  }

 protected:
  void SetUp() override {
    extensions::ChromeContentVerifierDelegate::SetDefaultModeForTesting(
        extensions::ChromeContentVerifierDelegate::VerifyInfo::Mode::
            ENFORCE_STRICT);
    extensions::MixinBasedExtensionApiTest::SetUp();
  }

  void SetUpInProcessBrowserTestFixture() override {
    extensions::MixinBasedExtensionApiTest::SetUpInProcessBrowserTestFixture();

    mock_policy_provider_.SetDefaultReturns(
        true /* is_initialization_complete_return */,
        true /* is_first_policy_load_complete_return */);
    mock_policy_provider_.SetAutoRefresh();
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(
        &mock_policy_provider_);
  }

  void SetUpOnMainThread() override {
    extensions::MixinBasedExtensionApiTest::SetUpOnMainThread();
    extension_force_install_mixin_.InitWithMockPolicyProvider(
        profile(), policy_provider());
  }

  void AddScreenplayTag() {
    base::AddTagToTestResult("feature_id",
                             "screenplay-3c0007b0-8082-4b7d-bdeb-675a4dc1bbb4");
  }

  ExtensionForceInstallMixin extension_force_install_mixin_{&mixin_host_};

 private:
  testing::NiceMock<policy::MockConfigurationPolicyProvider>
      mock_policy_provider_;
  web_app::OsIntegrationManager::ScopedSuppressForTesting os_hooks_suppress_;
};

// Verifies that proxy which is configured by force-installed extensions
// will not affect incognito profile.
IN_PROC_BROWSER_TEST_F(PolicyExtensionControllingProxyTest,
                       ForcedProxyExtensionHasNoEffectInIncognitoMode) {
  AddScreenplayTag();
#if BUILDFLAG(IS_WIN)
  // Mark as enterprise managed.
  base::win::ScopedDomainStateForTesting scoped_domain(true);
#endif

  {
    // Check that extension is not loaded.
    EXPECT_FALSE(
        force_mixin()->GetInstalledExtension(kProxySettingExtensionId));
    // Check that in main profile default proxy mode is system.
    const PrefService::Preference* proxy_prefs = GetOriginalProxyPrefs();
    ASSERT_TRUE(proxy_prefs);
    EXPECT_FALSE(proxy_prefs->IsExtensionControlled());
    std::string proxy_mode = GetKeyFromProxyPrefs(proxy_prefs, "mode");
    EXPECT_EQ(proxy_mode, "system");

    // Check that in incognito profile default proxy mode is system.
    const PrefService::Preference* incognito_proxy_prefs =
        GetIncognitoProxyPrefs();
    ASSERT_TRUE(incognito_proxy_prefs);
    EXPECT_FALSE(proxy_prefs->IsExtensionControlled());
    std::string incognito_proxy_mode =
        GetKeyFromProxyPrefs(incognito_proxy_prefs, "mode");
    EXPECT_EQ(proxy_mode, "system");
  }

  // Force load extension from the source dir and wait until message "ready"
  // is received.
  // As PEM file is provided, we are expecting same extension ID always.
  EXPECT_TRUE(force_mixin()->ForceInstallFromSourceDir(
      GetTestDataDir().AppendASCII(kProxySettingExtensionExtensionPath),
      GetTestDataDir().AppendASCII(kProxySettingExtensionPemPath),
      ExtensionForceInstallMixin::WaitMode::kReadyMessageReceived));

  // Verify extension is installed and enabled.
  ASSERT_TRUE(force_mixin()->GetInstalledExtension(kProxySettingExtensionId));
  EXPECT_TRUE(force_mixin()->GetEnabledExtension(kProxySettingExtensionId));
  EXPECT_EQ(force_mixin()
                ->GetInstalledExtension(kProxySettingExtensionId)
                ->location(),
            ManifestLocation::kExternalPolicyDownload);

  {
    // Verify extension has changed proxy setting for Main profile.
    const PrefService::Preference* proxy_prefs = GetOriginalProxyPrefs();
    ASSERT_TRUE(proxy_prefs);
    EXPECT_TRUE(proxy_prefs->IsExtensionControlled());
    std::string proxy_mode = GetKeyFromProxyPrefs(proxy_prefs, "mode");
    std::string proxy_server = GetKeyFromProxyPrefs(proxy_prefs, "server");
    std::string proxy_bypass_list =
        GetKeyFromProxyPrefs(proxy_prefs, "bypass_list");

    EXPECT_EQ(proxy_mode, "fixed_servers");
    EXPECT_EQ(proxy_server, "https=google.com:5555");
    EXPECT_EQ(proxy_bypass_list, "127.0.0.1");

    // Verify extension has not changed proxy setting for Incognito profile.
    const PrefService::Preference* incognito_proxy_prefs =
        GetIncognitoProxyPrefs();
    ASSERT_TRUE(proxy_prefs);
    EXPECT_FALSE(incognito_proxy_prefs->IsExtensionControlled());
    std::string incognito_proxy_mode =
        GetKeyFromProxyPrefs(incognito_proxy_prefs, "mode");
    EXPECT_EQ(incognito_proxy_mode, "system");
  }
}

}  // namespace policy
