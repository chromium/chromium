// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/standard_management_policy_provider.h"

#include <memory>

#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "chrome/browser/extensions/blacklist.h"
#include "chrome/browser/extensions/extension_management.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/manifest_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

class StandardManagementPolicyProviderTest : public testing::Test {
 public:
  StandardManagementPolicyProviderTest()
      : settings_(std::make_unique<ExtensionManagement>(&profile_)),
        provider_(settings_.get()) {}

 protected:
  scoped_refptr<const Extension> CreateExtension(Manifest::Location location) {
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
  auto extension = CreateExtension(Manifest::EXTERNAL_POLICY_DOWNLOAD);

  base::string16 error16;
  EXPECT_TRUE(provider_.UserMayLoad(extension.get(), &error16));
  EXPECT_EQ(base::string16(), error16);

  // We won't check the exact wording of the error, but it should say
  // something.
  EXPECT_FALSE(provider_.UserMayModifySettings(extension.get(), &error16));
  EXPECT_NE(base::string16(), error16);
  EXPECT_TRUE(provider_.MustRemainEnabled(extension.get(), &error16));
  EXPECT_NE(base::string16(), error16);

  // Component/policy extensions can modify and disable policy extensions, while
  // all others cannot.
  auto component = CreateExtension(Manifest::COMPONENT);
  auto policy = extension;
  auto policy2 = CreateExtension(Manifest::EXTERNAL_POLICY);
  auto internal = CreateExtension(Manifest::INTERNAL);
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
  auto extension = CreateExtension(Manifest::COMPONENT);

  base::string16 error16;
  EXPECT_TRUE(provider_.UserMayLoad(extension.get(), &error16));
  EXPECT_EQ(base::string16(), error16);

  EXPECT_FALSE(provider_.UserMayModifySettings(extension.get(), &error16));
  EXPECT_NE(base::string16(), error16);
  EXPECT_TRUE(provider_.MustRemainEnabled(extension.get(), &error16));
  EXPECT_NE(base::string16(), error16);

  // No extension can modify or disable component extensions.
  auto component = extension;
  auto component2 = CreateExtension(Manifest::COMPONENT);
  auto policy = CreateExtension(Manifest::EXTERNAL_POLICY);
  auto internal = CreateExtension(Manifest::INTERNAL);
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
  auto extension = CreateExtension(Manifest::INTERNAL);

  base::string16 error16;
  EXPECT_TRUE(provider_.UserMayLoad(extension.get(), &error16));
  EXPECT_EQ(base::string16(), error16);
  EXPECT_TRUE(provider_.UserMayModifySettings(extension.get(), &error16));
  EXPECT_EQ(base::string16(), error16);
  EXPECT_FALSE(provider_.MustRemainEnabled(extension.get(), &error16));
  EXPECT_EQ(base::string16(), error16);

  // All extension types can modify or disable internal extensions.
  auto component = CreateExtension(Manifest::COMPONENT);
  auto policy = CreateExtension(Manifest::EXTERNAL_POLICY);
  auto internal = extension;
  auto external_pref = CreateExtension(Manifest::EXTERNAL_PREF);
  EXPECT_TRUE(provider_.ExtensionMayModifySettings(component.get(),
                                                   internal.get(), nullptr));
  EXPECT_TRUE(provider_.ExtensionMayModifySettings(policy.get(), internal.get(),
                                                   nullptr));
  EXPECT_TRUE(provider_.ExtensionMayModifySettings(external_pref.get(),
                                                   internal.get(), nullptr));
}

}  // namespace extensions
