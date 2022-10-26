// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/pref_names.h"
#include "chromeos/startup/browser_init_params.h"
#include "components/policy/core/common/policy_loader_lacros.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_main_parts.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/extension_registry.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace {

constexpr char kFakeDMToken[] = "fake-dm-token";
constexpr char kFakeProfileClientId[] = "fake-profile-client-id";
constexpr char kAffiliationId[] = "affiliation-id";
constexpr char kDomain[] = "domain.com";

void SetupUserDeviceAffiliation() {
  ::enterprise_management::PolicyData profile_policy_data;
  profile_policy_data.add_user_affiliation_ids(kAffiliationId);
  profile_policy_data.set_managed_by(kDomain);
  profile_policy_data.set_device_id(kFakeProfileClientId);
  profile_policy_data.set_request_token(kFakeDMToken);
  ::policy::PolicyLoaderLacros::set_main_user_policy_data_for_testing(
      std::move(profile_policy_data));

  ::crosapi::mojom::BrowserInitParamsPtr init_params =
      ::crosapi::mojom::BrowserInitParams::New();
  init_params->device_properties = crosapi::mojom::DeviceProperties::New();
  init_params->device_properties->device_dm_token = kFakeDMToken;
  init_params->device_properties->device_affiliation_ids = {kAffiliationId};
  BrowserInitParams::SetInitParamsForTests(std::move(init_params));
}

// Browser test that validates extension installation status when the
// `InsightsExtensionEnabled` policy is set/unset on Lacros.
class ContactCenterInsightsExtensionManagerBrowserTest
    : public ::extensions::ExtensionBrowserTest {
 protected:
  void CreatedBrowserMainParts(
      content::BrowserMainParts* browser_parts) override {
    // We need to set up user device affiliation before the
    // `ContactCenterInsightsExtensionManager` is initialized, which happens
    // right after profile init.
    SetupUserDeviceAffiliation();
    ::extensions::ExtensionBrowserTest::CreatedBrowserMainParts(browser_parts);
  }

  void SetPrefValue(bool value) {
    profile()->GetPrefs()->SetBoolean(::prefs::kInsightsExtensionEnabled,
                                      value);
  }
};

IN_PROC_BROWSER_TEST_F(ContactCenterInsightsExtensionManagerBrowserTest,
                       ExtensionUnloadedByDefault) {
  EXPECT_FALSE(extension_registry()->GetInstalledExtension(
      ::extension_misc::kContactCenterInsightsExtensionId));
}

IN_PROC_BROWSER_TEST_F(ContactCenterInsightsExtensionManagerBrowserTest,
                       InstallExtensionWhenPrefSet) {
  SetPrefValue(true);
  EXPECT_TRUE(extension_registry()->enabled_extensions().GetByID(
      ::extension_misc::kContactCenterInsightsExtensionId));
}

IN_PROC_BROWSER_TEST_F(ContactCenterInsightsExtensionManagerBrowserTest,
                       ExtensionUninstalledWhenPrefUnset) {
  // Set pref to enable extension.
  SetPrefValue(true);
  EXPECT_TRUE(extension_registry()->enabled_extensions().GetByID(
      ::extension_misc::kContactCenterInsightsExtensionId));

  // Unset pref and verify extension is unloaded
  SetPrefValue(false);
  EXPECT_FALSE(extension_registry()->GetInstalledExtension(
      ::extension_misc::kContactCenterInsightsExtensionId));
}

}  // namespace
}  // namespace chromeos
