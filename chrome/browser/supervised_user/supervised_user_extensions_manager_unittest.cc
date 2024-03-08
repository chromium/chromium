// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/supervised_user_extensions_manager.h"

#include <string>
#include <vector>

#include "base/test/gtest_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/supervised_user/supervised_user_test_util.h"
#include "chrome/test/base/testing_profile.h"
#include "components/supervised_user/core/browser/supervised_user_utils.h"
#include "components/supervised_user/core/common/features.h"
#include "components/supervised_user/core/common/pref_names.h"
#include "components/version_info/version_info.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/features/feature_channel.h"
#include "extensions/common/manifest_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

enum class ExtensionsManagingToggle : int {
  /* Extensions are managed by the
     "Permissions for sites, apps and extensions" FL button. */
  kPermissions = 0,
  /* Extensions are managed by the dedicated
    "Skip parent approval to install extensions" FL button. */
  kExtensions = 1
};

using extensions::Extension;

class SupervisedUserExtensionsManagerTest
    : public extensions::ExtensionServiceTestBase,
      public ::testing::WithParamInterface<ExtensionsManagingToggle> {
 public:
  SupervisedUserExtensionsManagerTest() : channel_(version_info::Channel::DEV) {
    std::vector<base::test::FeatureRef> enabled_features;

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
    enabled_features.push_back(
        supervised_user::
            kEnableExtensionsPermissionsForSupervisedUsersOnDesktop);
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

    if (GetParam() == ExtensionsManagingToggle::kExtensions) {
      enabled_features.push_back(
          supervised_user::
              kEnableSupervisedUserSkipParentApprovalToInstallExtensions);
    }
    scoped_feature_list_.InitWithFeatures(enabled_features,
                                          /*disabled_feature=s*/ {});
  }
  ~SupervisedUserExtensionsManagerTest() override = default;

  void SetUp() override {
    ExtensionServiceTestBase::SetUp();
    ExtensionServiceInitParams params;
    params.profile_is_supervised = true;
    InitializeExtensionService(params);
    // Flush the message loop, to ensure that credentials have been loaded in
    // Identity Manager.
    base::RunLoop().RunUntilIdle();
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

  scoped_refptr<const extensions::Extension> MakeExtension(
      std::string name = "Extension") {
    scoped_refptr<const extensions::Extension> extension =
        extensions::ExtensionBuilder(name).Build();
    return extension;
  }

  void MakeSupervisedUserExtensionsManager() {
    manager_ = std::make_unique<extensions::SupervisedUserExtensionsManager>(
        profile());
  }

  extensions::ScopedCurrentChannel channel_;
  std::unique_ptr<extensions::SupervisedUserExtensionsManager> manager_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_P(SupervisedUserExtensionsManagerTest,
       ExtensionManagementPolicyProviderWithoutSUInitiatedInstalls) {
  MakeSupervisedUserExtensionsManager();
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

  scoped_refptr<const extensions::Extension> extension = MakeExtension();
  if (GetParam() == ExtensionsManagingToggle::kPermissions) {
    // Now check a different kind of extension; the supervised user should not
    // be able to load it. It should also not need to remain installed.
    std::u16string error_1;
    EXPECT_FALSE(manager_->UserMayLoad(extension.get(), &error_1));
    EXPECT_FALSE(error_1.empty());

    std::u16string error_2;
    EXPECT_FALSE(manager_->UserMayInstall(extension.get(), &error_2));
    EXPECT_FALSE(error_2.empty());
  } else {
    // Under the "Extensions" switch, installation are always allowed.
    std::u16string error_1;
    EXPECT_TRUE(manager_->UserMayLoad(extension.get(), &error_1));
    EXPECT_TRUE(error_1.empty());

    std::u16string error_2;
    EXPECT_TRUE(manager_->UserMayInstall(extension.get(), &error_2));
    EXPECT_TRUE(error_2.empty());
  }

    std::u16string error_3;
    EXPECT_FALSE(manager_->MustRemainInstalled(extension.get(), &error_3));
    EXPECT_TRUE(error_3.empty());

#if DCHECK_IS_ON()
  EXPECT_FALSE(manager_->GetDebugPolicyProviderName().empty());
#endif
}

TEST_P(SupervisedUserExtensionsManagerTest,
       ExtensionManagementPolicyProviderWithSUInitiatedInstalls) {
  MakeSupervisedUserExtensionsManager();
  if (GetParam() == ExtensionsManagingToggle::kExtensions) {
    // Enable child users to initiate extension installs by simulating the
    // toggling of "Skip parent approval to install extensions" to disabled.
    supervised_user_test_util::SetSkipParentApprovalToInstallExtensionsPref(
        profile(), false);
  } else {
    // Enable child users to initiate extension installs by simulating the
    // toggling of "Permissions for sites, apps and extensions" to enabled.
    supervised_user_test_util::
        SetSupervisedUserExtensionsMayRequestPermissionsPref(profile_.get(),
                                                             true);
  }

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

// Tests that on Desktop (Win/Linux/Mac) platforms, when the feature
// `kEnableSupervisedUserSkipParentApprovalToInstallExtensions` is first
// enabled, present extensions will be marked as locally parent-approved.
TEST_P(SupervisedUserExtensionsManagerTest,
       MigrateExtensionsToLocallyApproved) {
  ASSERT_TRUE(profile_->IsChild());

  // Register two extensions.
  scoped_refptr<const Extension> approved_extn =
      MakeExtension("extension_test_1");
  scoped_refptr<const Extension> locally_approved_extn =
      MakeExtension("extension_test_2");
  service()->AddExtension(approved_extn.get());
  service()->AddExtension(locally_approved_extn.get());

  // Mark one extensions as already parent-approved in the corresponding
  // preference.
  auto* prefs = profile()->GetPrefs();
  CHECK(prefs);
  base::Value::Dict approved_extensions;
  approved_extensions.Set(approved_extn->id(), true);
  prefs->SetDict(prefs::kSupervisedUserApprovedExtensions,
                 std::move(approved_extensions));

  // Create the object under test.
  MakeSupervisedUserExtensionsManager();

  bool has_local_approval_migration_run = false;
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  auto expected_migration_state =
      GetParam() == ExtensionsManagingToggle::kExtensions
          ? supervised_user::LocallyParentApprovedExtensionsMigrationState::
                kComplete
          : supervised_user::LocallyParentApprovedExtensionsMigrationState::
                kNeedToRun;
  has_local_approval_migration_run =
      GetParam() == ExtensionsManagingToggle::kExtensions;

  EXPECT_EQ(
      static_cast<int>(expected_migration_state),
      prefs->GetInteger(prefs::kLocallyParentApprovedExtensionsMigrationState));
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

  // The already approved extension should be allowed and not part of the
  // local-approved list.
  const base::Value::Dict& local_approved_extensions_pref =
      prefs->GetDict(prefs::kSupervisedUserLocallyParentApprovedExtensions);
  EXPECT_FALSE(local_approved_extensions_pref.contains(approved_extn->id()));
  EXPECT_TRUE(manager_->IsExtensionAllowed(*approved_extn));

  // The extensions approved in the migration should be allowed and part of the
  // local-approved list.
  EXPECT_EQ(
      has_local_approval_migration_run,
      local_approved_extensions_pref.contains(locally_approved_extn->id()));
  EXPECT_EQ(has_local_approval_migration_run,
            manager_->IsExtensionAllowed(*locally_approved_extn));
}

// TODO(b/321240030): Add test case for local approval revoking on extension
// uninstalling.

INSTANTIATE_TEST_SUITE_P(
    All,
    SupervisedUserExtensionsManagerTest,
    testing::Values(ExtensionsManagingToggle::kPermissions,
                    ExtensionsManagingToggle::kExtensions));
