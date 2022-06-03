// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/standard_management_policy_provider.h"

#include <memory>

#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "chrome/browser/extensions/blocklist.h"
#include "chrome/browser/extensions/extension_management.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

using extensions::mojom::ManifestLocation;

namespace extensions {

class StandardManagementPolicyProviderTest : public testing::Test {
 public:
  StandardManagementPolicyProviderTest()
      : settings_(std::make_unique<ExtensionManagement>(&profile_)),
        provider_(settings_.get()) {}

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

// Tests the behavior of the ManagementPolicy provider methods for a theme
// extension with and without a set policy theme.
TEST_F(StandardManagementPolicyProviderTest, ThemeExtension) {
  auto extension =
      ExtensionBuilder("testTheme")
          .SetLocation(ManifestLocation::kInternal)
          .SetManifestKey("theme", std::make_unique<base::DictionaryValue>())
          .Build();
  std::u16string error16;

  EXPECT_EQ(extension->GetType(), Manifest::TYPE_THEME);
  EXPECT_TRUE(provider_.UserMayLoad(extension.get(), &error16));
  EXPECT_EQ(std::u16string(), error16);

  // Setting policy theme prevents users from loading an extension theme.
  profile_.GetTestingPrefService()->SetManagedPref(
      prefs::kPolicyThemeColor, std::make_unique<base::Value>(100));

  EXPECT_FALSE(provider_.UserMayLoad(extension.get(), &error16));
  EXPECT_NE(std::u16string(), error16);

  // Unsetting policy theme allows users to load an extension theme.
  profile_.GetTestingPrefService()->RemoveManagedPref(prefs::kPolicyThemeColor);

  EXPECT_TRUE(provider_.UserMayLoad(extension.get(), &error16));
}

}  // namespace extensions
