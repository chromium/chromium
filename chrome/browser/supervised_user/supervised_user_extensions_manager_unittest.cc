// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/supervised_user_extensions_manager.h"

#include "base/test/gtest_util.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/supervised_user/supervised_user_test_util.h"
#include "chrome/test/base/testing_profile.h"
#include "components/version_info/version_info.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/features/feature_channel.h"
#include "extensions/common/manifest_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

using extensions::Extension;

class SupervisedUserExtensionsManagerTest
    : public extensions::ExtensionServiceTestBase {
 public:
  SupervisedUserExtensionsManagerTest()
      : channel_(version_info::Channel::DEV) {}
  ~SupervisedUserExtensionsManagerTest() override = default;

  void SetUp() override {
    ExtensionServiceTestBase::SetUp();
    ExtensionServiceInitParams params;
    params.profile_is_supervised = true;
    InitializeExtensionService(params);
    // Flush the message loop, to ensure that credentials have been loaded in
    // Identity Manager.
    base::RunLoop().RunUntilIdle();

    manager_ = std::make_unique<extensions::SupervisedUserExtensionsManager>(
        profile());
  }

  void TearDown() override {
    // Flush the message loop, to ensure all posted tasks run.
    base::RunLoop().RunUntilIdle();
  }

 protected:
  scoped_refptr<const extensions::Extension> MakeThemeExtension() {
    base::Value::Dict source;
    source.Set(extensions::manifest_keys::kName, "Theme");
    source.Set(extensions::manifest_keys::kTheme, base::Value::Dict());
    source.Set(extensions::manifest_keys::kVersion, "1.0");
    extensions::ExtensionBuilder builder;
    scoped_refptr<const extensions::Extension> extension =
        builder.SetManifest(std::move(source)).Build();
    return extension;
  }

  scoped_refptr<const extensions::Extension> MakeExtension() {
    scoped_refptr<const extensions::Extension> extension =
        extensions::ExtensionBuilder("Extension").Build();
    return extension;
  }

  extensions::ScopedCurrentChannel channel_;
  std::unique_ptr<extensions::SupervisedUserExtensionsManager> manager_;
};

TEST_F(SupervisedUserExtensionsManagerTest,
       ExtensionManagementPolicyProviderWithoutSUInitiatedInstalls) {
  supervised_user_test_util::
      SetSupervisedUserExtensionsMayRequestPermissionsPref(profile_.get(),
                                                           false);
  ASSERT_TRUE(profile_->IsChild());

  // Check that a supervised user can install and uninstall a theme even if
  // they are not allowed to install extensions.
  {
    scoped_refptr<const extensions::Extension> theme = MakeThemeExtension();

    std::u16string error_1;
    EXPECT_TRUE(manager_->UserMayLoad(theme.get(), &error_1));
    EXPECT_TRUE(error_1.empty());

    std::u16string error_2;
    EXPECT_FALSE(manager_->MustRemainInstalled(theme.get(), &error_2));
    EXPECT_TRUE(error_2.empty());
  }

  // Now check a different kind of extension; the supervised user should not be
  // able to load it. It should also not need to remain installed.
  {
    scoped_refptr<const extensions::Extension> extension = MakeExtension();

    std::u16string error_1;
    EXPECT_FALSE(manager_->UserMayLoad(extension.get(), &error_1));
    EXPECT_FALSE(error_1.empty());

    std::u16string error_2;
    EXPECT_FALSE(manager_->UserMayInstall(extension.get(), &error_2));
    EXPECT_FALSE(error_2.empty());

    std::u16string error_3;
    EXPECT_FALSE(manager_->MustRemainInstalled(extension.get(), &error_3));
    EXPECT_TRUE(error_3.empty());
  }

#if DCHECK_IS_ON()
  EXPECT_FALSE(manager_->GetDebugPolicyProviderName().empty());
#endif
}

TEST_F(SupervisedUserExtensionsManagerTest,
       ExtensionManagementPolicyProviderWithSUInitiatedInstalls) {
  // Enable child users to initiate extension installs by simulating the
  // toggling of "Permissions for sites, apps and extensions" to enabled.
  supervised_user_test_util::
      SetSupervisedUserExtensionsMayRequestPermissionsPref(profile_.get(),
                                                           true);
  ASSERT_TRUE(profile_->IsChild());

  // The supervised user should be able to load and uninstall the extensions
  // they install.
  {
    scoped_refptr<const extensions::Extension> extension = MakeExtension();

    std::u16string error;
    EXPECT_TRUE(manager_->UserMayLoad(extension.get(), &error));
    EXPECT_TRUE(error.empty());

    std::u16string error_2;
    EXPECT_FALSE(manager_->MustRemainInstalled(extension.get(), &error_2));
    EXPECT_TRUE(error_2.empty());

    std::u16string error_3;
    extensions::disable_reason::DisableReason reason =
        extensions::disable_reason::DISABLE_NONE;
    EXPECT_TRUE(
        manager_->MustRemainDisabled(extension.get(), &reason, &error_3));
    EXPECT_EQ(extensions::disable_reason::DISABLE_CUSTODIAN_APPROVAL_REQUIRED,
              reason);
    EXPECT_FALSE(error_3.empty());

    std::u16string error_4;
    EXPECT_TRUE(manager_->UserMayModifySettings(extension.get(), &error_4));
    EXPECT_TRUE(error_4.empty());

    std::u16string error_5;
    EXPECT_TRUE(manager_->UserMayInstall(extension.get(), &error_5));
    EXPECT_TRUE(error_5.empty());
  }

#if DCHECK_IS_ON()
  EXPECT_FALSE(manager_->GetDebugPolicyProviderName().empty());
#endif
}
