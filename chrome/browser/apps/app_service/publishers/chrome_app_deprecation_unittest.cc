// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/publishers/chrome_app_deprecation.h"

#include "base/feature_list.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "extensions/browser/extension_registrar.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/mojom/manifest.mojom-shared.h"
#include "extensions/test/test_extension_dir.h"

using extensions::ChromeTestExtensionLoader;
using extensions::CrxInstaller;
using extensions::Extension;
using extensions::ExtensionBuilder;
using extensions::ExtensionRegistry;
using extensions::TestExtensionDir;
using extensions::mojom::ManifestLocation;

namespace apps::chrome_app_deprecation {

class DeprecationControllerTest : public extensions::ExtensionServiceTestBase {
 protected:
  void SetUp() override {
    ExtensionServiceTestBase::SetUp();

    InitializeEmptyExtensionService();

    app_ = InstallTestApp(profile());
    ASSERT_TRUE(app_);
    ASSERT_TRUE(registrar()->IsExtensionEnabled(app_->id()));
  }

  void TearDown() override {
    app_.release()->Release();

    ExtensionServiceTestBase::TearDown();
  }

  scoped_refptr<const Extension> InstallTestApp(Profile* profile) {
    // Build a simple Chrome App.
    base::Value::Dict manifest =
        base::Value::Dict()
            .Set("name", "Test app")
            .Set("version", "1.0.0")
            .Set("manifest_version", 3)
            .Set("description", "an extension")
            .Set("app", base::Value::Dict().Set(
                            "launch", base::Value::Dict().Set("local_path",
                                                              "test.html")));

    TestExtensionDir good_extension_dir;
    good_extension_dir.WriteManifest(manifest);

    ChromeTestExtensionLoader loader(profile);
    loader.set_pack_extension(false);
    return loader.LoadExtension(good_extension_dir.UnpackedPath());
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_refptr<const Extension> app_;
};

TEST_F(DeprecationControllerTest, HandleDeprecationDefaultFeatureFlag) {
  scoped_feature_list_.InitWithEmptyFeatureAndFieldTrialLists();
  ASSERT_TRUE(base::FeatureList::IsEnabled(kAllowUserInstalledChromeApps));

  EXPECT_EQ(HandleDeprecation(app_->id(), profile()),
            DeprecationStatus::kLaunchAllowed);

  EXPECT_EQ(HandleDeprecation(app_->id(), profile()),
            DeprecationStatus::kLaunchAllowed);
}

TEST_F(DeprecationControllerTest, HandleDeprecationDisabledFeatureFlag) {
  scoped_feature_list_.InitAndDisableFeature(kAllowUserInstalledChromeApps);
  ASSERT_FALSE(base::FeatureList::IsEnabled(kAllowUserInstalledChromeApps));

  EXPECT_EQ(HandleDeprecation(app_->id(), profile()),
            DeprecationStatus::kLaunchBlocked);
}

TEST_F(DeprecationControllerTest, HandleDeprecationEnabledFeatureFlag) {
  scoped_feature_list_.InitAndEnableFeature(kAllowUserInstalledChromeApps);
  ASSERT_TRUE(base::FeatureList::IsEnabled(kAllowUserInstalledChromeApps));

  EXPECT_EQ(HandleDeprecation(app_->id(), profile()),
            DeprecationStatus::kLaunchAllowed);
}

class DeprecationControllerAllowlistTest : public DeprecationControllerTest {
 protected:
  void SetUp() override {
    DeprecationControllerTest::SetUp();

    AddAppToAllowlistForTesting(app_->id());
  }

  void TearDown() override {
    ResetAllowlistForTesting();

    DeprecationControllerTest::TearDown();
  }
};

TEST_F(DeprecationControllerAllowlistTest,
       HandleDeprecationDefaultFeatureFlag) {
  scoped_feature_list_.InitWithEmptyFeatureAndFieldTrialLists();
  ASSERT_TRUE(base::FeatureList::IsEnabled(kAllowUserInstalledChromeApps));

  EXPECT_EQ(HandleDeprecation(app_->id(), profile()),
            DeprecationStatus::kLaunchAllowed);
}

TEST_F(DeprecationControllerAllowlistTest,
       HandleDeprecationDisabledFeatureFlag) {
  scoped_feature_list_.InitAndDisableFeature(kAllowUserInstalledChromeApps);
  ASSERT_FALSE(base::FeatureList::IsEnabled(kAllowUserInstalledChromeApps));

  EXPECT_EQ(HandleDeprecation(app_->id(), profile()),
            DeprecationStatus::kLaunchAllowed);
}

TEST_F(DeprecationControllerAllowlistTest,
       HandleDeprecationEnabledFeatureFlag) {
  scoped_feature_list_.InitAndEnableFeature(kAllowUserInstalledChromeApps);
  ASSERT_TRUE(base::FeatureList::IsEnabled(kAllowUserInstalledChromeApps));

  EXPECT_EQ(HandleDeprecation(app_->id(), profile()),
            DeprecationStatus::kLaunchAllowed);
}
}  // namespace apps::chrome_app_deprecation
