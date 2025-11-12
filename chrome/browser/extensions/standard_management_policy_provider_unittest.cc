// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/standard_management_policy_provider.h"

#include <memory>

#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/extensions/extension_management.h"
#include "chrome/browser/extensions/extension_management_internal.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/external_policy_loader.h"
#include "chrome/test/base/testing_profile.h"
#include "components/policy/core/common/management/scoped_management_service_override_for_testing.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/themes/pref_names.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/blocklist.h"
#include "extensions/browser/blocklist_extension_prefs.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/pref_names.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/extension_urls.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

using extensions::mojom::ManifestLocation;

namespace extensions {

class StandardManagementPolicyProviderTest : public testing::Test {
 public:
  StandardManagementPolicyProviderTest()
      : settings_(std::make_unique<ExtensionManagement>(&profile_)),
        provider_(settings_.get(), &profile_) {}

 protected:
  scoped_refptr<const Extension> CreateExtension(ManifestLocation location) {
    return ExtensionBuilder("test").SetLocation(location).Build();
  }

  content::BrowserTaskEnvironment task_environment_;

  TestingProfile profile_;
  std::unique_ptr<ExtensionManagement> settings_;

  StandardManagementPolicyProvider provider_;
};

// Tests the behavior of the ManagementPolicy provider methods for an
// extension required by policy.
TEST_F(StandardManagementPolicyProviderTest, RequiredExtension) {
  auto extension = CreateExtension(ManifestLocation::kExternalPolicyDownload);

  std::u16string error16;
  EXPECT_TRUE(provider_.UserMayLoad(extension.get(), &error16));
  EXPECT_EQ(std::u16string(), error16);

  // We won't check the exact wording of the error, but it should say
  // something.
  EXPECT_FALSE(provider_.UserMayModifySettings(extension.get(), &error16));
  EXPECT_NE(std::u16string(), error16);
  EXPECT_TRUE(provider_.MustRemainEnabled(extension.get(), &error16));
  EXPECT_NE(std::u16string(), error16);

  // Component/policy extensions can modify and disable policy extensions, while
  // all others cannot.
  auto component = CreateExtension(ManifestLocation::kComponent);
  auto policy = extension;
  auto policy2 = CreateExtension(ManifestLocation::kExternalPolicy);
  auto internal = CreateExtension(ManifestLocation::kInternal);
  EXPECT_TRUE(provider_.ExtensionMayModifySettings(component.get(),
                                                   policy.get(), nullptr));
  EXPECT_TRUE(provider_.ExtensionMayModifySettings(policy2.get(), policy.get(),
                                                   nullptr));
  EXPECT_FALSE(provider_.ExtensionMayModifySettings(internal.get(),
                                                    policy.get(), nullptr));
  // The Webstore hosted app is an exception, in that it is a component
  // extension, but it should not be able to modify policy required extensions.
  // Note: We add to the manifest JSON to build this as a hosted app.
  // Regression test for crbug.com/1363793
  constexpr char kHostedApp[] = R"(
      "app": {
        "launch": {
          "web_url": "https://example.com"
        },
        "urls": [
          "https://example.com"
        ]
      })";
  auto webstore = ExtensionBuilder("webstore hosted app")
                      .AddJSON(kHostedApp)
                      .SetLocation(ManifestLocation::kComponent)
                      .SetID(kWebStoreAppId)
                      .Build();
  EXPECT_FALSE(provider_.ExtensionMayModifySettings(webstore.get(),
                                                    policy.get(), nullptr));
}

// Tests the behavior of the ManagementPolicy provider methods for extensions
// installed by sys-admin policies in low-trust environments.
TEST_F(StandardManagementPolicyProviderTest,
       ExternalPolicyExtensionsInLowTrustEnvironment) {
  // Mark enterprise management authority for platform as NONE to simulate an
  // un-trusted environment.
  policy::ScopedManagementServiceOverrideForTesting platform_management(
      policy::ManagementServiceFactory::GetForPlatform(),
      policy::EnterpriseManagementAuthority::NONE);

  // Dummy CWS extension not installed from the store
  auto extension = ExtensionBuilder("CWSPolicyInstalledExtension")
                       .SetVersion("1.0")
                       .SetLocation(ManifestLocation::kExternalPolicy)
                       .SetManifestKey("update_url",
                                       extension_urls::kChromeWebstoreUpdateURL)
                       .AddFlags(Extension::MAY_BE_UNTRUSTED)
                       .Build();

  std::u16string error16;
  EXPECT_TRUE(provider_.UserMayLoad(extension.get(), &error16));
  EXPECT_EQ(std::u16string(), error16);

  EXPECT_FALSE(provider_.UserMayModifySettings(extension.get(), &error16));
  EXPECT_NE(std::u16string(), error16);

  // CWS extensions should remain enabled when installed by external policy.
  EXPECT_FALSE(provider_.MustRemainDisabled(extension.get(), nullptr));
  EXPECT_TRUE(provider_.MustRemainEnabled(extension.get(), &error16));
  EXPECT_NE(std::u16string(), error16);
}

// Tests the behavior of the ManagementPolicy provider methods for greylisted
// extensions force-installed in low-trust environments.
TEST_F(StandardManagementPolicyProviderTest,
       GreylistedForceInstalledExtensionsInLowTrustEnvironment) {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  base::test::ScopedFeatureList feature_list(
      kDisableForceInstalledExtensionsInLowTrustEnviromentWhenGreylisted);
  bool expected = true;
#else
  bool expected = false;
#endif

  // Mark enterprise management authority for platform as NONE to simulate an
  // un-trusted environment.
  policy::ScopedManagementServiceOverrideForTesting platform_management(
      policy::ManagementServiceFactory::GetForPlatform(),
      policy::EnterpriseManagementAuthority::NONE);

  // Force-install a CWS extension.
  auto extension = ExtensionBuilder("CWSPolicyInstalledExtension")
                       .SetVersion("1.0")
                       .SetLocation(ManifestLocation::kExternalPolicy)
                       .SetManifestKey("update_url",
                                       extension_urls::kChromeWebstoreUpdateURL)
                       .AddFlags(Extension::FROM_WEBSTORE)
                       .Build();
  base::Value::Dict forced_list_pref;
  ExternalPolicyLoader::AddExtension(forced_list_pref, extension->id(),
                                     extension_urls::kChromeWebstoreUpdateURL);
  profile_.GetTestingPrefService()->SetManagedPref(
      pref_names::kInstallForceList, forced_list_pref.Clone());

  // Greylist the extension.
  blocklist_prefs::SetSafeBrowsingExtensionBlocklistState(
      extension->id(), BitMapBlocklistState::BLOCKLISTED_POTENTIALLY_UNWANTED,
      ExtensionPrefs::Get(&profile_));

  EXPECT_EQ(expected,
            provider_.UserMayModifySettings(extension.get(), nullptr));
  EXPECT_EQ(expected, provider_.ExtensionMayModifySettings(
                          nullptr, extension.get(), nullptr));
  EXPECT_NE(expected, provider_.MustRemainEnabled(extension.get(), nullptr));
}

TEST_F(StandardManagementPolicyProviderTest, UnsupportedDeveloperExtension) {
  base::test::ScopedFeatureList feature_list(
      extensions_features::kExtensionDisableUnsupportedDeveloper);
  // Disable developer mode.
  util::SetDeveloperModeForProfile(&profile_, false);
  auto extension = CreateExtension(ManifestLocation::kUnpacked);

  std::u16string error16;
  EXPECT_TRUE(provider_.MustRemainDisabled(extension.get(), nullptr));
}

// Tests the behavior of the ManagementPolicy provider methods for a component
// extension.
TEST_F(StandardManagementPolicyProviderTest, ComponentExtension) {
  auto extension = CreateExtension(ManifestLocation::kComponent);

  std::u16string error16;
  EXPECT_TRUE(provider_.UserMayLoad(extension.get(), &error16));
  EXPECT_EQ(std::u16string(), error16);

  EXPECT_FALSE(provider_.UserMayModifySettings(extension.get(), &error16));
  EXPECT_NE(std::u16string(), error16);
  EXPECT_TRUE(provider_.MustRemainEnabled(extension.get(), &error16));
  EXPECT_NE(std::u16string(), error16);

  // No extension can modify or disable component extensions.
  auto component = extension;
  auto component2 = CreateExtension(ManifestLocation::kComponent);
  auto policy = CreateExtension(ManifestLocation::kExternalPolicy);
  auto internal = CreateExtension(ManifestLocation::kInternal);
  EXPECT_FALSE(provider_.ExtensionMayModifySettings(component2.get(),
                                                    component.get(), nullptr));
  EXPECT_FALSE(provider_.ExtensionMayModifySettings(policy.get(),
                                                    component.get(), nullptr));
  EXPECT_FALSE(provider_.ExtensionMayModifySettings(internal.get(),
                                                    component.get(), nullptr));
}

// Tests the behavior of the ManagementPolicy provider methods for a regular
// extension.
TEST_F(StandardManagementPolicyProviderTest, NotRequiredExtension) {
  auto extension = CreateExtension(ManifestLocation::kInternal);

  std::u16string error16;
  EXPECT_TRUE(provider_.UserMayLoad(extension.get(), &error16));
  EXPECT_EQ(std::u16string(), error16);
  EXPECT_TRUE(provider_.UserMayModifySettings(extension.get(), &error16));
  EXPECT_EQ(std::u16string(), error16);
  EXPECT_FALSE(provider_.MustRemainEnabled(extension.get(), &error16));
  EXPECT_EQ(std::u16string(), error16);

  // All extension types can modify or disable internal extensions.
  auto component = CreateExtension(ManifestLocation::kComponent);
  auto policy = CreateExtension(ManifestLocation::kExternalPolicy);
  auto internal = extension;
  auto external_pref = CreateExtension(ManifestLocation::kExternalPref);
  EXPECT_TRUE(provider_.ExtensionMayModifySettings(component.get(),
                                                   internal.get(), nullptr));
  EXPECT_TRUE(provider_.ExtensionMayModifySettings(policy.get(), internal.get(),
                                                   nullptr));
  EXPECT_TRUE(provider_.ExtensionMayModifySettings(external_pref.get(),
                                                   internal.get(), nullptr));
}

#if BUILDFLAG(ENABLE_EXTENSIONS)
// Tests the behavior of the ManagementPolicy provider methods for a theme
// extension with and without a set policy theme.
TEST_F(StandardManagementPolicyProviderTest, ThemeExtension) {
  auto extension = ExtensionBuilder("testTheme")
                       .SetLocation(ManifestLocation::kInternal)
                       .SetManifestKey("theme", base::Value::Dict())
                       .Build();
  std::u16string error16;

  EXPECT_EQ(extension->GetType(), Manifest::TYPE_THEME);
  EXPECT_TRUE(provider_.UserMayLoad(extension.get(), &error16));
  EXPECT_EQ(std::u16string(), error16);

  // Setting policy theme prevents users from loading an extension theme.
  profile_.GetTestingPrefService()->SetManagedPref(
      themes::prefs::kPolicyThemeColor, std::make_unique<base::Value>(100));

  EXPECT_FALSE(provider_.UserMayLoad(extension.get(), &error16));
  EXPECT_NE(std::u16string(), error16);

  // Unsetting policy theme allows users to load an extension theme.
  profile_.GetTestingPrefService()->RemoveManagedPref(
      themes::prefs::kPolicyThemeColor);

  EXPECT_TRUE(provider_.UserMayLoad(extension.get(), &error16));
}
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

// Tests the behavior of the ManagementPolicy provider methods for an extension
// which manifest version is controlled by policy.
TEST_F(StandardManagementPolicyProviderTest, ManifestVersion) {
  auto extension = ExtensionBuilder("testManifestVersion")
                       .SetLocation(ManifestLocation::kExternalPolicyDownload)
                       .SetManifestVersion(2)
                       .Build();

  std::u16string error16;
  EXPECT_TRUE(provider_.UserMayLoad(extension.get(), &error16));
  EXPECT_TRUE(error16.empty());

  profile_.GetTestingPrefService()->SetManagedPref(
      pref_names::kManifestV2Availability,
      std::make_unique<base::Value>(static_cast<int>(
          internal::GlobalSettings::ManifestV2Setting::kDisabled)));

  EXPECT_FALSE(provider_.UserMayLoad(extension.get(), &error16));
  EXPECT_EQ(
      u"The administrator of this machine requires testManifestVersion "
      "to have a minimum manifest version of 3.",
      error16);
}

}  // namespace extensions
