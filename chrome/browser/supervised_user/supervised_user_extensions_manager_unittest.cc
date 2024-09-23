// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/supervised_user_extensions_manager.h"

#include <string>
#include <vector>

#include "base/test/gtest_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/supervised_user/supervised_user_extensions_metrics_recorder.h"
#include "chrome/browser/supervised_user/supervised_user_test_util.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
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

class SupervisedUserExtensionsManagerTestBase
    : public extensions::ExtensionServiceTestBase {
 public:
  SupervisedUserExtensionsManagerTestBase()
      : channel_(version_info::Channel::DEV) {}
  ~SupervisedUserExtensionsManagerTestBase() override = default;

  void SetUp() override {
    ExtensionServiceTestBase::SetUp();
    ExtensionServiceInitParams params;
    params.profile_is_supervised = true;
    InitializeExtensionService(std::move(params));
    // Flush the message loop, to ensure that credentials have been loaded in
    // Identity Manager.
    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override {
    // Flush the message loop, to ensure all posted tasks run.
    base::RunLoop().RunUntilIdle();
  }

  void CheckLocalApprovalMigrationForDesktopState(
      supervised_user::LocallyParentApprovedExtensionsMigrationState
          expected_migration_state) {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
    auto* prefs = profile()->GetPrefs();
    CHECK(prefs);
    EXPECT_EQ(static_cast<int>(expected_migration_state),
              prefs->GetInteger(
                  prefs::kLocallyParentApprovedExtensionsMigrationState));
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  }

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
};

class SupervisedUserExtensionsManagerTest
    : public SupervisedUserExtensionsManagerTestBase,
      public ::testing::WithParamInterface<ExtensionsManagingToggle> {
 public:
  SupervisedUserExtensionsManagerTest() {
    std::vector<base::test::FeatureRef> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
    enabled_features.push_back(
        supervised_user::
            kEnableExtensionsPermissionsForSupervisedUsersOnDesktop);
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

    if (GetExtensionsManagingToggle() ==
        ExtensionsManagingToggle::kExtensions) {
      enabled_features.push_back(
          supervised_user::
              kEnableSupervisedUserSkipParentApprovalToInstallExtensions);
    } else {
      disabled_features.push_back(
          supervised_user::
              kEnableSupervisedUserSkipParentApprovalToInstallExtensions);
    }
    scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }

  ExtensionsManagingToggle GetExtensionsManagingToggle() { return GetParam(); }

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
  if (GetExtensionsManagingToggle() == ExtensionsManagingToggle::kPermissions) {
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
  if (GetExtensionsManagingToggle() == ExtensionsManagingToggle::kExtensions) {
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
// enabled, present extensions will be marked as locally parent-approved
// when the SupervisedUserExtensionsManager is created for a supervised user.
TEST_P(SupervisedUserExtensionsManagerTest,
       MigrateExtensionsToLocallyApproved) {
  ASSERT_TRUE(profile_->IsChild());
  base::HistogramTester histogram_tester;

  // Register the extensions.
  scoped_refptr<const Extension> approved_extn =
      MakeExtension("extension_test_1");
  scoped_refptr<const Extension> locally_approved_extn =
      MakeExtension("local_extension_test_1");
  service()->AddExtension(approved_extn.get());
  service()->AddExtension(locally_approved_extn.get());

  // Mark one extension as already parent-approved in the corresponding
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
      GetExtensionsManagingToggle() == ExtensionsManagingToggle::kExtensions
          ? supervised_user::LocallyParentApprovedExtensionsMigrationState::
                kComplete
          : supervised_user::LocallyParentApprovedExtensionsMigrationState::
                kNeedToRun;
  has_local_approval_migration_run =
      GetExtensionsManagingToggle() == ExtensionsManagingToggle::kExtensions;

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

  // The extensions approved in the migration should be allowed and part
  // of the local-approved list.
  int approved_extensions_count = has_local_approval_migration_run ? 1 : 0;
  EXPECT_EQ(
      has_local_approval_migration_run,
      local_approved_extensions_pref.contains(locally_approved_extn->id()));
  EXPECT_EQ(has_local_approval_migration_run,
            manager_->IsExtensionAllowed(*locally_approved_extn));
  histogram_tester.ExpectBucketCount(
      SupervisedUserExtensionsMetricsRecorder::kExtensionsHistogramName,
      SupervisedUserExtensionsMetricsRecorder::UmaExtensionState::
          kLocalApprovalGranted,
      approved_extensions_count);
  histogram_tester.ExpectTotalCount(
      extensions::kInitialLocallyApprovedExtensionCountWinLinuxMacHistogramName,
      approved_extensions_count);
}

// Tests that extensions missing parent approval are granted parent approval
// on their installation, when the extensions are managed by the Extensions
// toggle and the toggle is ON. If extensions are managed by the Permissions
// toggle, the extensions remain disabled and pending approval.
TEST_P(SupervisedUserExtensionsManagerTest,
       GrantParentApprovalOnInstallationWhenExtensionsToggleOn) {
  ASSERT_TRUE(profile_->IsChild());
  base::HistogramTester histogram_tester;

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  // Mark the migration done to avoid any interference with the one-off
  // migration.
  auto* prefs = profile()->GetPrefs();
  CHECK(prefs);
  prefs->SetInteger(
      prefs::kLocallyParentApprovedExtensionsMigrationState,
      static_cast<int>(
          supervised_user::LocallyParentApprovedExtensionsMigrationState::
              kComplete));
#endif

  // Create the object under test.
  MakeSupervisedUserExtensionsManager();

  // Set the Extensions switch to OFF. The extensions installed on this mode,
  // should be pending approval and disabled.
  supervised_user_test_util::SetSkipParentApprovalToInstallExtensionsPref(
      profile(), false);

  // Install an extension.
  scoped_refptr<const Extension> extn_with_switch_off =
      MakeExtension("extension_test_1");
  service()->OnExtensionInstalled(extn_with_switch_off.get(),
                                  /*page_ordinal=*/syncer::StringOrdinal());

  extensions::disable_reason::DisableReason reason;
  std::u16string error;
  EXPECT_FALSE(manager_->IsExtensionAllowed(*extn_with_switch_off.get()));
  EXPECT_TRUE(manager_->MustRemainDisabled(extn_with_switch_off.get(), &reason,
                                           &error));

  histogram_tester.ExpectTotalCount(
      extensions::kExtensionApprovalsCountOnExtensionToggleHistogramName, 0);
  // Set the Extensions switch to ON. Install another extension which should be
  // granted parental approval by the end of the installation, if the Extensions
  // switch manages them.
  supervised_user_test_util::SetSkipParentApprovalToInstallExtensionsPref(
      profile(), true);

  // Toggling the extensions results in granting approval to the existing
  // extension if the Extensions switch manages them.
  int approved_extensions_count =
      GetExtensionsManagingToggle() == ExtensionsManagingToggle::kExtensions
          ? 1
          : 0;
  histogram_tester.ExpectBucketCount(
      SupervisedUserExtensionsMetricsRecorder::kExtensionsHistogramName,
      SupervisedUserExtensionsMetricsRecorder::UmaExtensionState::
          kApprovalGrantedByDefault,
      approved_extensions_count);
  // The entry point of the implicit approval is recorded.
  histogram_tester.ExpectBucketCount(
      SupervisedUserExtensionsMetricsRecorder::
          kImplicitParentApprovalGrantEntryPointHistogramName,
      SupervisedUserExtensionsMetricsRecorder::
          ImplicitExtensionApprovalEntryPoint::
              kOnExtensionsSwitchFlippedToEnabled,
      approved_extensions_count);
  // The number of auto-approved extensions is reco
  histogram_tester.ExpectTotalCount(
      extensions::kExtensionApprovalsCountOnExtensionToggleHistogramName,
      approved_extensions_count);

  // Install an extension.
  scoped_refptr<const Extension> extn_with_switch_on =
      MakeExtension("extension_test_2");
  service()->OnExtensionInstalled(extn_with_switch_on.get(),
                                  /*page_ordinal=*/syncer::StringOrdinal());

  bool is_extension_approved =
      GetExtensionsManagingToggle() == ExtensionsManagingToggle::kExtensions;
  EXPECT_EQ(is_extension_approved,
            manager_->IsExtensionAllowed(*extn_with_switch_on.get()));
  EXPECT_EQ(is_extension_approved,
            !manager_->MustRemainDisabled(extn_with_switch_on.get(), &reason,
                                          &error));
  EXPECT_EQ(is_extension_approved,
            profile()
                ->GetPrefs()
                ->GetDict(prefs::kSupervisedUserApprovedExtensions)
                .contains(extn_with_switch_on->id()));

  histogram_tester.ExpectBucketCount(
      SupervisedUserExtensionsMetricsRecorder::kExtensionsHistogramName,
      SupervisedUserExtensionsMetricsRecorder::UmaExtensionState::
          kApprovalGrantedByDefault,
      approved_extensions_count + (is_extension_approved ? 1 : 0));
  // The migration to locally approved extensions has occurred before we
  // installed any extensions, so not local approvals have been granted.
  histogram_tester.ExpectBucketCount(
      SupervisedUserExtensionsMetricsRecorder::kExtensionsHistogramName,
      SupervisedUserExtensionsMetricsRecorder::UmaExtensionState::
          kLocalApprovalGranted,
      0);
  histogram_tester.ExpectTotalCount(
      extensions::kInitialLocallyApprovedExtensionCountWinLinuxMacHistogramName,
      0);
  // The entry point of the implicit approval is recorded.
  histogram_tester.ExpectBucketCount(
      SupervisedUserExtensionsMetricsRecorder::
          kImplicitParentApprovalGrantEntryPointHistogramName,
      SupervisedUserExtensionsMetricsRecorder::
          ImplicitExtensionApprovalEntryPoint::
              OnExtensionInstallationWithExtensionsSwitchEnabled,
      is_extension_approved ? 1 : 0);
  histogram_tester.ExpectTotalCount(
      SupervisedUserExtensionsMetricsRecorder::
          kImplicitParentApprovalGrantEntryPointHistogramName,
      approved_extensions_count + (is_extension_approved ? 1 : 0));
}

// Tests that extensions missing parent approval are granted parent approval
// when the extensions are managed by the Extensions toggle and the toggle is
// flipped to ON.
// If extensions are managed by the Permissions toggle, the extensions remain
// disabled and pending approval.
TEST_P(SupervisedUserExtensionsManagerTest,
       GrantParentApprovalOnExtensionsWhenExtensionsToggleSetToOn) {
  ASSERT_TRUE(profile_->IsChild());

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  // Mark the migration done to avoid any interference with the one-off
  // migration.
  auto* prefs = profile()->GetPrefs();
  CHECK(prefs);
  prefs->SetInteger(
      prefs::kLocallyParentApprovedExtensionsMigrationState,
      static_cast<int>(
          supervised_user::LocallyParentApprovedExtensionsMigrationState::
              kComplete));
#endif

  // Create the object under test.
  MakeSupervisedUserExtensionsManager();

  // Set the Extensions switch to OFF. The extensions installed on this mode,
  // should be pending approval and disabled.
  supervised_user_test_util::SetSkipParentApprovalToInstallExtensionsPref(
      profile(), false);

  // Install an extension.
  scoped_refptr<const Extension> extn_with_switch_off =
      MakeExtension("extension_test_1");
  service()->OnExtensionInstalled(extn_with_switch_off.get(),
                                  /*page_ordinal=*/syncer::StringOrdinal());

  extensions::disable_reason::DisableReason reason;
  std::u16string error;
  EXPECT_FALSE(manager_->IsExtensionAllowed(*extn_with_switch_off.get()));
  EXPECT_TRUE(manager_->MustRemainDisabled(extn_with_switch_off.get(), &reason,
                                           &error));

  // Set the Extensions switch to ON. The extension should have been granted
  // parent approval when the SkipParentApprovalToInstallExtension preference is
  // flipped.
  supervised_user_test_util::SetSkipParentApprovalToInstallExtensionsPref(
      profile(), true);

  bool is_extension_approved =
      GetExtensionsManagingToggle() == ExtensionsManagingToggle::kExtensions;
  EXPECT_EQ(is_extension_approved,
            manager_->IsExtensionAllowed(*extn_with_switch_off.get()));
  EXPECT_EQ(is_extension_approved,
            !manager_->MustRemainDisabled(extn_with_switch_off.get(), &reason,
                                          &error));
  EXPECT_EQ(is_extension_approved,
            profile()
                ->GetPrefs()
                ->GetDict(prefs::kSupervisedUserApprovedExtensions)
                .contains(extn_with_switch_off->id()));
}

// Tests the local approval is revoked on uninstalling the extension or
// when the extension gains normal parental approval.
TEST_P(SupervisedUserExtensionsManagerTest, RevokeLocalApproval) {
  ASSERT_TRUE(profile_->IsChild());

  scoped_refptr<const Extension> locally_approved_extn1 =
      MakeExtension("extension_test_1");
  service()->AddExtension(locally_approved_extn1.get());
  scoped_refptr<const Extension> locally_approved_extn2 =
      MakeExtension("extension_test_2");
  service()->AddExtension(locally_approved_extn2.get());

  // Create the object under test.
  MakeSupervisedUserExtensionsManager();

  bool has_local_approval_migration_run = false;
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  has_local_approval_migration_run =
      GetExtensionsManagingToggle() == ExtensionsManagingToggle::kExtensions;
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

  auto* prefs = profile()->GetPrefs();
  CHECK(prefs);
  const base::Value::Dict& local_approved_extensions_pref =
      prefs->GetDict(prefs::kSupervisedUserLocallyParentApprovedExtensions);
  EXPECT_EQ(
      has_local_approval_migration_run,
      local_approved_extensions_pref.contains(locally_approved_extn1->id()));
  EXPECT_EQ(
      has_local_approval_migration_run,
      registry()->enabled_extensions().Contains(locally_approved_extn1->id()));

  // Uninstalling the extension also removes the local approval.
  ASSERT_TRUE(service()->UninstallExtension(
      locally_approved_extn1->id(), extensions::UNINSTALL_REASON_FOR_TESTING,
      nullptr));
  EXPECT_FALSE(
      local_approved_extensions_pref.contains(locally_approved_extn1->id()));

  // Granting parent approval (typically from another client) removes the local
  // approval. The extension remains allowed.
  manager_->AddExtensionApproval(*locally_approved_extn2.get());
  EXPECT_FALSE(
      local_approved_extensions_pref.contains(locally_approved_extn2->id()));
  EXPECT_TRUE(manager_->IsExtensionAllowed(*locally_approved_extn2));
}

INSTANTIATE_TEST_SUITE_P(All,
                         SupervisedUserExtensionsManagerTest,
                         testing::Values(ExtensionsManagingToggle::kPermissions,
                                         ExtensionsManagingToggle::kExtensions),
                         [](const auto& info) {
                           return std::string(
                               info.param ==
                                       ExtensionsManagingToggle::kExtensions
                                   ? "ManagedByExtensions"
                                   : "ManagedByPermissions");
                         });

// Whether the local approval extension migration for
// Win/Mac/Linux platforms has already run at the beginning of
// each test case.
enum class LocalApprovalMigrationForDesktopState : int {
  kExecuted = 0,
  kPending = 1
};

// Tests the managed extensions' state when an existing profile becomes
// supervised.
class AddingSupervisionTest
    : public SupervisedUserExtensionsManagerTestBase,
      public ::testing::WithParamInterface<
          std::tuple<ExtensionsManagingToggle,
                     LocalApprovalMigrationForDesktopState>> {
 public:
  AddingSupervisionTest() {
    std::vector<base::test::FeatureRef> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
    enabled_features.push_back(
        supervised_user::
            kEnableExtensionsPermissionsForSupervisedUsersOnDesktop);
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

    if (GetExtensionsManagingToggle() ==
        ExtensionsManagingToggle::kExtensions) {
      enabled_features.push_back(
          supervised_user::
              kEnableSupervisedUserSkipParentApprovalToInstallExtensions);
    } else {
      disabled_features.push_back(
          supervised_user::
              kEnableSupervisedUserSkipParentApprovalToInstallExtensions);
    }
    scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }

  ExtensionsManagingToggle GetExtensionsManagingToggle() {
    return std::get<0>(GetParam());
  }

  LocalApprovalMigrationForDesktopState
  GetInitialLocalApprovalMigrationForDesktopState() {
    return std::get<1>(GetParam());
  }

  void MaybeMarkLocalApprovalMigrationDone() {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
    if (GetInitialLocalApprovalMigrationForDesktopState() ==
            LocalApprovalMigrationForDesktopState::kExecuted &&
        GetExtensionsManagingToggle() ==
            ExtensionsManagingToggle::kExtensions) {
      auto* prefs = profile()->GetPrefs();
      CHECK(prefs);
      prefs->SetInteger(
          prefs::kLocallyParentApprovedExtensionsMigrationState,
          static_cast<int>(
              supervised_user::LocallyParentApprovedExtensionsMigrationState::
                  kComplete));
    }
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that on Desktop (Win/Linux/Mac) platforms, when:
// 1) the feature `kEnableSupervisedUserSkipParentApprovalToInstallExtensions`
// is first enabled and
// 2) a supervised user sings-in on an existing unsupervised profile or
//    the existing profile becomes supervised (Gellerized) then
// the existing extensions remain disabled and pending approval.
// The states of the present extensions should not be impacted by the local
// extension approval migration that is executed on feature release for Desktop
// platforms.
TEST_P(AddingSupervisionTest,
       DisableExistingExtensionsOnProfileBecomingSupervised) {
  MaybeMarkLocalApprovalMigrationDone();
  profile()->AsTestingProfile()->SetIsSupervisedProfile(false);
  ASSERT_TRUE(!profile_->IsChild());

  scoped_refptr<const Extension> existing_extension =
      MakeExtension("extension_test_2");
  service()->AddExtension(existing_extension.get());

  supervised_user::LocallyParentApprovedExtensionsMigrationState
      expected_migragtion_state = supervised_user::
          LocallyParentApprovedExtensionsMigrationState::kNeedToRun;
  if (GetExtensionsManagingToggle() == ExtensionsManagingToggle::kExtensions &&
      GetInitialLocalApprovalMigrationForDesktopState() ==
          LocalApprovalMigrationForDesktopState::kExecuted) {
    expected_migragtion_state = supervised_user::
        LocallyParentApprovedExtensionsMigrationState::kComplete;
  }
  CheckLocalApprovalMigrationForDesktopState(expected_migragtion_state);

  // Create the object under test.
  MakeSupervisedUserExtensionsManager();

  if (GetExtensionsManagingToggle() == ExtensionsManagingToggle::kExtensions) {
    expected_migragtion_state = supervised_user::
        LocallyParentApprovedExtensionsMigrationState::kComplete;
  }
  CheckLocalApprovalMigrationForDesktopState(expected_migragtion_state);

  auto* prefs = profile()->GetPrefs();
  CHECK(prefs);
  const base::Value::Dict& local_approved_extensions_pref =
      prefs->GetDict(prefs::kSupervisedUserLocallyParentApprovedExtensions);
  EXPECT_FALSE(
      local_approved_extensions_pref.contains(existing_extension->id()));
  EXPECT_TRUE(
      registry()->enabled_extensions().Contains(existing_extension->id()));

  // Make the user supervised.
  profile()->AsTestingProfile()->SetIsSupervisedProfile(true);
  ASSERT_TRUE(profile_->IsChild());

  const base::Value::Dict& local_approved_extensions_pref_post_migr =
      prefs->GetDict(prefs::kSupervisedUserLocallyParentApprovedExtensions);
  // Check that the pre-existing extensions should have been be excluded from
  // the local approval migration.
  EXPECT_FALSE(local_approved_extensions_pref_post_migr.contains(
      existing_extension->id()));
  EXPECT_FALSE(manager_->IsExtensionAllowed(*existing_extension));

  EXPECT_TRUE(
      registry()->disabled_extensions().Contains(existing_extension->id()));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    AddingSupervisionTest,
    testing::Combine(
        testing::Values(ExtensionsManagingToggle::kPermissions,
                        ExtensionsManagingToggle::kExtensions),
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
        testing::Values(LocalApprovalMigrationForDesktopState::kExecuted,
                        LocalApprovalMigrationForDesktopState::kPending)),
#else   // ChromeOS case
        // The local approval migration is not applicable for ChromeOS.
        // The test just needs to be executed once, the value of
        // LocalApprovalMigrationForDesktopState does not matter.
        testing::Values(LocalApprovalMigrationForDesktopState::kExecuted)),
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
    [](const auto& info) {
      return std::string(std::get<0>(info.param) ==
                                 ExtensionsManagingToggle::kExtensions
                             ? "ManagedByExtensions"
                             : "ManagedByPermissions") +
             std::string(
                 std::get<1>(info.param) ==
                         LocalApprovalMigrationForDesktopState::kExecuted
                     ? "PostReleaseSkipParentApprovalFeature"
                     : "PreReleaseSkipParentApprovalFeature");
    });
