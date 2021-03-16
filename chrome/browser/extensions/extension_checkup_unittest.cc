// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_checkup.h"

#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

class ExtensionCheckupTest : public ExtensionServiceTestBase,
                             public testing::WithParamInterface<const char*> {
 public:
  ExtensionCheckupTest() {}
  ~ExtensionCheckupTest() override {}

  void SetUp() override {
    feature_list_.InitAndEnableFeatureWithParameters(
        extensions_features::kExtensionsCheckup,
        {{extensions_features::kExtensionsCheckupEntryPointParameter,
          GetParam()}});
    ExtensionServiceTestBase::SetUp();
    InitializeEmptyExtensionService();
    service()->Init();
  }

  // Adds a user installed extension.
  void AddUserInstalledExtension() {
    scoped_refptr<const Extension> extension = ExtensionBuilder("foo").Build();
    service()->AddExtension(extension.get());
  }

  void AddExemptExtensions() {
    // Install policy extension.
    scoped_refptr<const Extension> policy_extension =
        ExtensionBuilder("policy")
            .SetLocation(mojom::ManifestLocation::kExternalPolicy)
            .Build();
    service()->AddExtension(policy_extension.get());
    // Install component extension.
    scoped_refptr<const Extension> component_extension =
        ExtensionBuilder("component")
            .SetLocation(mojom::ManifestLocation::kComponent)
            .Build();
    service()->AddExtension(component_extension.get());
    // Load a default installed extension.
    int creation_flags = Extension::WAS_INSTALLED_BY_DEFAULT;
    scoped_refptr<const Extension> default_install_extension =
        ExtensionBuilder("default").AddFlags(creation_flags).Build();
    service()->AddExtension(default_install_extension.get());
    ASSERT_TRUE(default_install_extension);
  }

  // Verify the extensions checkup behavior.
  bool ShouldShowExperimentCheckup() {
    if (GetParam() == extensions_features::kNtpPromoEntryPoint)
      return ShouldShowExtensionsCheckupPromo(browser_context());
    return ShouldShowExtensionsCheckupOnStartup(browser_context());
  }

  // Verify that the opposite entry point will always return false (i.e if the
  // user is supposed to see the middle slot promo entry point they should never
  // see the extensions checkup upon startup).
  void VerifyNonExperimentCheckupDisabled() {
    if (GetParam() == extensions_features::kNtpPromoEntryPoint)
      EXPECT_FALSE(ShouldShowExtensionsCheckupOnStartup(browser_context()));
    else
      EXPECT_FALSE(ShouldShowExtensionsCheckupPromo(browser_context()));
  }

  DISALLOW_COPY_AND_ASSIGN(ExtensionCheckupTest);
};

TEST_P(ExtensionCheckupTest, NoInstalledExtensions) {
  VerifyNonExperimentCheckupDisabled();
  EXPECT_FALSE(ShouldShowExperimentCheckup());
}

TEST_P(ExtensionCheckupTest, NoUserInstalledExtensions) {
  AddExemptExtensions();
  VerifyNonExperimentCheckupDisabled();
  EXPECT_FALSE(ShouldShowExperimentCheckup());
}

// Checkup is shown if at least one non policy extension is installed.
TEST_P(ExtensionCheckupTest, OnlyOneUserInstalledExtension) {
  AddUserInstalledExtension();
  VerifyNonExperimentCheckupDisabled();
  EXPECT_TRUE(ShouldShowExperimentCheckup());
}

TEST_P(ExtensionCheckupTest, UserAndNonUserInstalledExtensions) {
  AddUserInstalledExtension();
  AddExemptExtensions();
  VerifyNonExperimentCheckupDisabled();
  EXPECT_TRUE(ShouldShowExperimentCheckup());
}

INSTANTIATE_TEST_SUITE_P(
    All,
    ExtensionCheckupTest,
    ::testing::Values(extensions_features::kNtpPromoEntryPoint,
                      extensions_features::kStartupEntryPoint));
}  // namespace extensions
