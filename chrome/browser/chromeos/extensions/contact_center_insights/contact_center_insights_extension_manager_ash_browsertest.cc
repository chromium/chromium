// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/test/cryptohome_mixin.h"
#include "chrome/browser/ash/login/test/user_auth_config.h"
#include "chrome/browser/ash/policy/affiliation/affiliation_mixin.h"
#include "chrome/browser/ash/policy/affiliation/affiliation_test_helper.h"
#include "chrome/browser/ash/policy/core/device_policy_cros_browser_test.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/pref_names.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_launcher.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace {

// Browser test that validates extension installation status when the
// `InsightsExtensionEnabled` policy is set/unset. Inheriting from
// DevicePolicyCrosBrowserTest enables use of AffiliationMixin for setting up
// profile/device affiliation. Only available in Ash.
class ContactCenterInsightsExtensionManagerBrowserTest
    : public ::policy::DevicePolicyCrosBrowserTest {
 protected:
  ContactCenterInsightsExtensionManagerBrowserTest() {
    crypto_home_mixin_.MarkUserAsExisting(affiliation_mixin_.account_id());
    crypto_home_mixin_.ApplyAuthConfig(
        affiliation_mixin_.account_id(),
        ash::test::UserAuthConfig::Create(ash::test::kDefaultAuthSetup));
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ::policy::AffiliationTestHelper::AppendCommandLineSwitchesForLoginManager(
        command_line);
    ::policy::DevicePolicyCrosBrowserTest::SetUpCommandLine(command_line);
  }

  void SetUpOnMainThread() override {
    ::policy::DevicePolicyCrosBrowserTest::SetUpOnMainThread();
    if (content::IsPreTest()) {
      // Preliminary setup - set up affiliated user
      ::policy::AffiliationTestHelper::PreLoginUser(
          affiliation_mixin_.account_id());
      return;
    }

    // Login as affiliated user otherwise
    ::policy::AffiliationTestHelper::LoginUser(affiliation_mixin_.account_id());
  }

  Profile* profile() const {
    return ash::ProfileHelper::Get()->GetProfileByAccountId(
        affiliation_mixin_.account_id());
  }

  void SetPrefValue(bool value) {
    profile()->GetPrefs()->SetBoolean(::prefs::kInsightsExtensionEnabled,
                                      value);
  }

  ::policy::DevicePolicyCrosTestHelper test_helper_;
  ::policy::AffiliationMixin affiliation_mixin_{&mixin_host_, &test_helper_};
  ash::CryptohomeMixin crypto_home_mixin_{&mixin_host_};
};

IN_PROC_BROWSER_TEST_F(ContactCenterInsightsExtensionManagerBrowserTest,
                       PRE_ExtensionUnloadedByDefault) {
  // Dummy case that sets up the affiliated user.
}

IN_PROC_BROWSER_TEST_F(ContactCenterInsightsExtensionManagerBrowserTest,
                       ExtensionUnloadedByDefault) {
  auto* const extension_registry =
      ::extensions::ExtensionRegistry::Get(profile());
  EXPECT_FALSE(extension_registry->GetInstalledExtension(
      ::extension_misc::kContactCenterInsightsExtensionId));
}

IN_PROC_BROWSER_TEST_F(ContactCenterInsightsExtensionManagerBrowserTest,
                       PRE_InstallExtensionWhenPrefSet) {
  // Dummy case that sets up the affiliated user.
}

IN_PROC_BROWSER_TEST_F(ContactCenterInsightsExtensionManagerBrowserTest,
                       InstallExtensionWhenPrefSet) {
  SetPrefValue(true);
  auto* const extension_registry =
      ::extensions::ExtensionRegistry::Get(profile());
  EXPECT_TRUE(extension_registry->enabled_extensions().GetByID(
      ::extension_misc::kContactCenterInsightsExtensionId));
}

IN_PROC_BROWSER_TEST_F(ContactCenterInsightsExtensionManagerBrowserTest,
                       PRE_ExtensionUninstalledWhenPrefUnset) {
  // Dummy case that sets up the affiliated user.
}

IN_PROC_BROWSER_TEST_F(ContactCenterInsightsExtensionManagerBrowserTest,
                       ExtensionUninstalledWhenPrefUnset) {
  auto* const extension_registry =
      ::extensions::ExtensionRegistry::Get(profile());

  // Set pref to enable extension.
  SetPrefValue(true);
  EXPECT_TRUE(extension_registry->enabled_extensions().GetByID(
      ::extension_misc::kContactCenterInsightsExtensionId));

  // Unset pref and verify extension is unloaded
  SetPrefValue(false);
  EXPECT_FALSE(extension_registry->GetInstalledExtension(
      ::extension_misc::kContactCenterInsightsExtensionId));
}

}  // namespace
}  // namespace chromeos
