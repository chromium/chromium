// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <tuple>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/extensions/api/webstore_private/webstore_private_api.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_with_install.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/supervised_user/supervised_user_extensions_delegate_impl.h"
#include "chrome/browser/supervised_user/supervised_user_extensions_metrics_recorder.h"
#include "chrome/browser/supervised_user/supervised_user_test_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/supervised_user/core/common/features.h"
#include "components/supervised_user/core/common/pref_names.h"
#include "extensions/browser/api_test_utils.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registrar.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/supervised_user_extensions_delegate.h"
#include "extensions/common/extension_builder.h"
#include "testing/gtest/include/gtest/gtest.h"

using extensions::api_test_utils::RunFunctionAndReturnSingleResult;

namespace {
const char good_crx[] = "ldnnhddmnhbkjipkidpdiheffobcpfmf";
const char autoupdate[] = "ogjcoiohnmldgjemafoockdghcjciccf";
const char permissions_increase[] = "pgdpcfcocojkjfbgpiianjngphoopgmo";
}  // namespace

namespace extensions {

// Base class for the extension parental controls tests for supervised users.
class SupervisedUserExtensionTestBase : public ExtensionServiceTestWithInstall {
 public:
  void InitServices(bool profile_is_supervised) {
    ExtensionServiceInitParams params;
    params.profile_is_supervised = profile_is_supervised;
    InitializeExtensionService(std::move(params));
    CreateExtensionManager();
  }

  void CreateExtensionManager() {
    supervised_user_extensions_delegate_ =
        std::make_unique<SupervisedUserExtensionsDelegateImpl>(profile());
  }

  ExtensionServiceTestWithInstall::InstallState GetDefaultInstalledState() {
    // Extensions will be installed but disabled until custodian approvals
    // are performed in the default extensions parental control mode.
    return INSTALL_WITHOUT_LOAD;
  }

  const Extension* InstallPermissionsTestExtension(
      ExtensionServiceTestWithInstall::InstallState install_state) {
    const Extension* extension = InstallTestExtension(
        permissions_increase, dir_path("1"), pem_path(), install_state);
    return extension;
  }

  void UpdatePermissionsTestExtension(const std::string& id,
                                      const std::string& version,
                                      UpdateState expected_state) {
    UpdateTestExtension(dir_path(version), pem_path(), id, version,
                        expected_state);
  }

  const Extension* InstallNoPermissionsTestExtension(
      ExtensionServiceTestWithInstall::InstallState install_state) {
    base::FilePath base_path = data_dir().AppendASCII("autoupdate");
    base::FilePath pem_path = base_path.AppendASCII("key.pem");
    base::FilePath dir_path = base_path.AppendASCII("v1");

    return InstallTestExtension(autoupdate, dir_path, pem_path, install_state);
  }

  void UpdateNoPermissionsTestExtension(const std::string& id,
                                        const std::string& version,
                                        UpdateState expected_state) {
    base::FilePath base_path = data_dir().AppendASCII("autoupdate");
    base::FilePath pem_path = base_path.AppendASCII("key.pem");
    base::FilePath dir_path = base_path.AppendASCII("v" + version);

    UpdateTestExtension(dir_path, pem_path, id, version, expected_state);
  }

  void UpdateTestExtension(const base::FilePath& dir_path,
                           const base::FilePath& pem_path,
                           const std::string& id,
                           const std::string& version,
                           const UpdateState& expected_state) {
    PackCRXAndUpdateExtension(id, dir_path, pem_path, expected_state);
    const Extension* extension = registry()->GetInstalledExtension(id);
    ASSERT_TRUE(extension);
    // The version should have been updated.
    EXPECT_EQ(base::Version(version), extension->version());
  }

  const Extension* CheckEnabled(const std::string& extension_id) {
    EXPECT_TRUE(registry()->enabled_extensions().Contains(extension_id));
    EXPECT_FALSE(IsPendingCustodianApproval(extension_id));
    ExtensionPrefs* extension_prefs = ExtensionPrefs::Get(profile());
    EXPECT_TRUE(extension_prefs->GetDisableReasons(extension_id).empty());
    return registry()->enabled_extensions().GetByID(extension_id);
  }

  const Extension* CheckDisabledForCustodianApproval(
      const std::string& extension_id) {
    EXPECT_TRUE(registry()->disabled_extensions().Contains(extension_id));
    EXPECT_TRUE(IsPendingCustodianApproval(extension_id));
    ExtensionPrefs* extension_prefs = ExtensionPrefs::Get(profile());
    EXPECT_TRUE(extension_prefs->HasDisableReason(
        extension_id, disable_reason::DISABLE_CUSTODIAN_APPROVAL_REQUIRED));
    return registry()->disabled_extensions().GetByID(extension_id);
  }

  const Extension* CheckDisabledForPermissionsIncrease(
      const std::string& extension_id) {
    EXPECT_TRUE(registry()->disabled_extensions().Contains(extension_id));
    EXPECT_TRUE(IsPendingCustodianApproval(extension_id));
    ExtensionPrefs* extension_prefs = ExtensionPrefs::Get(profile());
    EXPECT_TRUE(extension_prefs->HasDisableReason(
        extension_id, disable_reason::DISABLE_PERMISSIONS_INCREASE));
    return registry()->disabled_extensions().GetByID(extension_id);
  }

  SupervisedUserExtensionsDelegate* supervised_user_extensions_delegate() {
    return supervised_user_extensions_delegate_.get();
  }

  const Extension* InstallTestExtension(
      const std::string& id,
      const base::FilePath& dir_path,
      const base::FilePath& pem_path,
      ExtensionServiceTestWithInstall::InstallState install_state) {
    const Extension* extension =
        PackAndInstallCRX(dir_path, pem_path, install_state);
    // The extension must now be installed.
    EXPECT_TRUE(extension);
    EXPECT_EQ(extension->id(), id);
    if (install_state != INSTALL_NEW) {
      CheckDisabledForCustodianApproval(id);
    } else {
      CheckEnabled(id);
    }
    EXPECT_EQ(base::Version("1"), extension->version());
    return extension;
  }

  void SetDefaultParentalControlSettings() {
    supervised_user_test_util::
        SetSupervisedUserExtensionsMayRequestPermissionsPref(profile(), true);
  }

 private:
  // Returns true if the extension has disable reason permissions_increase or
  // custodian_approval_required. Tests the Webstore Private Api.
  bool IsPendingCustodianApproval(const std::string& extension_id) {
    auto function = base::MakeRefCounted<
        WebstorePrivateIsPendingCustodianApprovalFunction>();

    std::optional<base::Value> result(RunFunctionAndReturnSingleResult(
        function.get(), "[\"" + extension_id + "\"]", browser_context()));
    return result->GetBool();
  }

  base::FilePath base_path() const {
    return data_dir().AppendASCII("permissions_increase");
  }
  base::FilePath dir_path(const std::string& version) const {
    return base_path().AppendASCII("v" + version);
  }
  base::FilePath pem_path() const {
    return base_path().AppendASCII("permissions.pem");
  }

  std::unique_ptr<SupervisedUserExtensionsDelegateImpl>
      supervised_user_extensions_delegate_;
};

class SupervisedUserExtensionTest : public SupervisedUserExtensionTestBase {};

// Tests that regular users are not affecting supervised user UMA metrics.
TEST_F(SupervisedUserExtensionTest,
       RegularUsersNotAffectingSupervisedUserMetrics) {
  InitServices(/*profile_is_supervised=*/false);

  base::HistogramTester histogram_tester;

  base::FilePath path = data_dir().AppendASCII("good.crx");
  // The extensions will be installed and enabled.
  const Extension* extension = InstallCRX(path, INSTALL_NEW);
  ASSERT_TRUE(extension);

  supervised_user_extensions_delegate()->AddExtensionApproval(*extension);

  histogram_tester.ExpectTotalCount(
      SupervisedUserExtensionsMetricsRecorder::kExtensionsHistogramName, 0);

  supervised_user_extensions_delegate()->RemoveExtensionApproval(*extension);

  histogram_tester.ExpectTotalCount(
      SupervisedUserExtensionsMetricsRecorder::kExtensionsHistogramName, 0);
}

// Tests that simulating custodian approval for regular users doesn't cause any
// unexpected behavior.
TEST_F(SupervisedUserExtensionTest,
       CustodianApprovalDoesNotAffectRegularUsers) {
  InitServices(/*profile_is_supervised=*/false);
  supervised_user_test_util::
      SetSupervisedUserExtensionsMayRequestPermissionsPref(profile(), false);
  supervised_user_test_util::SetSkipParentApprovalToInstallExtensionsPref(
      profile(), false);

  // Install an extension, it should be enabled because this is a regular user.
  base::FilePath path = data_dir().AppendASCII("good.crx");
  const Extension* extension = InstallCRX(path, INSTALL_NEW);
  std::string id = extension->id();

  CheckEnabled(id);

  // Simulate custodian approval and removal.
  supervised_user_extensions_delegate()->AddExtensionApproval(*extension);
  supervised_user_extensions_delegate()->RemoveExtensionApproval(*extension);
  // The extension should still be enabled.
  CheckEnabled(id);
}

// Tests that adding supervision to a regular account (Gellerization)
// disables previously installed extensions.
TEST_F(SupervisedUserExtensionTest, ExtensionsStateAfterGellerization) {
  InitServices(/*profile_is_supervised=*/false);
  SetDefaultParentalControlSettings();

  // Install an extension, it should be enabled because this is a regular user.
  base::FilePath path = data_dir().AppendASCII("good.crx");
  const Extension* extension = InstallCRX(path, INSTALL_NEW);
  ASSERT_TRUE(extension);
  std::string id = extension->id();
  CheckEnabled(id);

  // Now make the profile supervised.
  profile()->AsTestingProfile()->SetIsSupervisedProfile();

    // The extension should be disabled now pending custodian approval.
    CheckDisabledForCustodianApproval(id);

    // Grant parent approval.
    supervised_user_extensions_delegate()->AddExtensionApproval(*extension);

    // The extension should be enabled now.
    CheckEnabled(id);

    // Remove parent approval.
    supervised_user_extensions_delegate()->RemoveExtensionApproval(*extension);

    // The extension should be disabled again now.
    CheckDisabledForCustodianApproval(id);
}

// Tests that extensions that are disabled pending parent approval
// for supervised users, become re-enabled if the user becomes unsupervised.
TEST_F(SupervisedUserExtensionTest, ExtensionsStateAfterGraduation) {
  InitServices(/*profile_is_supervised=*/true);
  SetDefaultParentalControlSettings();

  // The extensions will be installed, but will be disabled until the custodian
  // approval.
  auto install_state = INSTALL_WITHOUT_LOAD;
  base::FilePath path = data_dir().AppendASCII("good.crx");
  const Extension* extension = InstallCRX(path, install_state);
  ASSERT_TRUE(extension);
  std::string id = extension->id();
    // This extension is a supervised user initiated install and should remain
    // disabled.
    CheckDisabledForCustodianApproval(id);

  // Make the profile un-supervised.
  profile()->AsTestingProfile()->SetIsSupervisedProfile(false);

  // The extension should become enabled.
  CheckEnabled(id);
}

// Tests that a child user is allowed to install extensions under the default
// value of the Family Link "Extensions" toggles.
// The newly-installed extensions are disabled until approved by the parent.
TEST_F(SupervisedUserExtensionTest, InstallAllowedForSupervisedUser) {
  InitServices(/*profile_is_supervised=*/true);
  SetDefaultParentalControlSettings();

  auto install_state = GetDefaultInstalledState();
  base::FilePath path = data_dir().AppendASCII("good.crx");
  const Extension* extension = InstallCRX(path, install_state);
  ASSERT_TRUE(extension);
  std::string id = extension->id();

    // This extension is a supervised user initiated install and should remain
    // disabled.
    CheckDisabledForCustodianApproval(id);

    // Grant parent approval.
    supervised_user_extensions_delegate()->AddExtensionApproval(*extension);
    // The extension is now enabled.
    CheckEnabled(id);

    // Remove parent approval.
    supervised_user_extensions_delegate()->RemoveExtensionApproval(*extension);

    // The extension should be disabled again now.
    CheckDisabledForCustodianApproval(id);
}

// Tests that supervised users may approve permission updates without parent
// approval under the default values of the Family Link "Extensions" toggle.
TEST_F(SupervisedUserExtensionTest, UpdateWithPermissionsIncrease) {
  InitServices(true);
  SetDefaultParentalControlSettings();

  // Preconditions.
  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount(
      SupervisedUserExtensionsMetricsRecorder::kExtensionsHistogramName, 0);
  base::UserActionTester user_action_tester;
  EXPECT_EQ(
      0,
      user_action_tester.GetActionCount(
          SupervisedUserExtensionsMetricsRecorder::kApprovalGrantedActionName));
  EXPECT_EQ(
      0,
      user_action_tester.GetActionCount(
          SupervisedUserExtensionsMetricsRecorder::kApprovalRemovedActionName));

  std::string id =
      InstallPermissionsTestExtension(GetDefaultInstalledState())->id();
    // Simulate parent approval.
    supervised_user_extensions_delegate()->AddExtensionApproval(
        *registry()->GetInstalledExtension(id));

    // Should see 1 kApprovalGranted metric count.
    histogram_tester.ExpectUniqueSample(
        SupervisedUserExtensionsMetricsRecorder::kExtensionsHistogramName,
        SupervisedUserExtensionsMetricsRecorder::UmaExtensionState::
            kApprovalGranted,
        1);
    histogram_tester.ExpectTotalCount(
        SupervisedUserExtensionsMetricsRecorder::kExtensionsHistogramName, 1);
    EXPECT_EQ(1, user_action_tester.GetActionCount(
                     SupervisedUserExtensionsMetricsRecorder::
                         kApprovalGrantedActionName));
  // The extension should be enabled.
  CheckEnabled(id);

  // Update to a new version with increased permissions.
  UpdatePermissionsTestExtension(id, "2", DISABLED);
  const Extension* extension = CheckDisabledForPermissionsIncrease(id);
  ASSERT_TRUE(extension);

  // Simulate supervised user approving the extension without further parent
  // approval.
  registrar()->GrantPermissionsAndEnableExtension(*extension);

  // The extension should be enabled.
  CheckEnabled(id);

    // Remove extension approval.
    supervised_user_extensions_delegate()->RemoveExtensionApproval(
        *registry()->GetInstalledExtension(id));

    // Should see 1 kApprovalRemoved metric count.
    histogram_tester.ExpectBucketCount(
        SupervisedUserExtensionsMetricsRecorder::kExtensionsHistogramName,
        SupervisedUserExtensionsMetricsRecorder::UmaExtensionState::
            kApprovalRemoved,
        1);
    histogram_tester.ExpectTotalCount(
        SupervisedUserExtensionsMetricsRecorder::kExtensionsHistogramName, 2);
    EXPECT_EQ(1, user_action_tester.GetActionCount(
                     SupervisedUserExtensionsMetricsRecorder::
                         kApprovalRemovedActionName));

    // The extension should be disabled now.
    CheckDisabledForCustodianApproval(id);
}

// Tests that if an approved extension is updated to a newer version that
// doesn't require additional permissions, it is still enabled.
TEST_F(SupervisedUserExtensionTest, UpdateWithoutPermissionIncrease) {
  InitServices(/*profile_is_supervised=*/true);
  SetDefaultParentalControlSettings();

  const Extension* extension =
      InstallNoPermissionsTestExtension(GetDefaultInstalledState());
  // Save the id, as the extension object will be destroyed during updating.
  std::string id = extension->id();
  supervised_user_extensions_delegate()->AddExtensionApproval(*extension);
  // The extension should be enabled now.
  CheckEnabled(id);

  // Update to a new version.
  std::string version2("2");
  UpdateNoPermissionsTestExtension(id, version2, ENABLED);

  // The extension should still be there and enabled.
  const Extension* extension1 = CheckEnabled(id);
  ASSERT_TRUE(extension1);
  // The version should have changed.
  EXPECT_EQ(base::Version(version2), extension1->version());

  supervised_user_test_util::
      SetSupervisedUserExtensionsMayRequestPermissionsPref(profile(), false);
  std::string version3("3");
  UpdateNoPermissionsTestExtension(id, version3, ENABLED);

  // The extension should still be there and enabled.
  const Extension* extension2 = CheckEnabled(id);
  ASSERT_TRUE(extension2);
  // The version should have changed again.
  EXPECT_EQ(base::Version(version3), extension2->version());

    // Check that the approved extension id has been updated in the prefs as
    // well. Prefs are updated via sync.
    PrefService* pref_service = profile()->GetPrefs();
    ASSERT_TRUE(pref_service);
    const base::Value::Dict& approved_extensions =
        pref_service->GetDict(prefs::kSupervisedUserApprovedExtensions);
    EXPECT_TRUE(approved_extensions.FindString(id));
}

// Tests that toggling the "Permissions" switch has no longer effect on the
// extensions' installation.
TEST_F(SupervisedUserExtensionTest,
       SupervisedUserCannotInstallExtensionUnderPermissionsToggle) {
  InitServices(/*profile_is_supervised=*/true);
  supervised_user_test_util::SetSkipParentApprovalToInstallExtensionsPref(
      profile(), false);
  supervised_user_test_util::
      SetSupervisedUserExtensionsMayRequestPermissionsPref(profile(), false);

  base::FilePath path = data_dir().AppendASCII("good.crx");
  // Installation is successful. The extension is installed disabled in the
  // default mode.
  const Extension* extension = InstallCRX(path, INSTALL_WITHOUT_LOAD);
  EXPECT_TRUE(extension);
  CheckDisabledForCustodianApproval(extension->id());
}

// Tests that disabling the "Permissions" Family Link toggle
// has no effect on regular users.
TEST_F(SupervisedUserExtensionTest, RegularUserCanInstallExtension) {
  InitServices(/*profile_is_supervised=*/false);
  supervised_user_test_util::
      SetSupervisedUserExtensionsMayRequestPermissionsPref(profile(), false);
  supervised_user_test_util::SetSkipParentApprovalToInstallExtensionsPref(
      profile(), false);

  base::FilePath path = data_dir().AppendASCII("good.crx");
  // The extension should be installed and enabled.
  const Extension* extension = InstallCRX(path, INSTALL_NEW);

  ASSERT_TRUE(extension);
  CheckEnabled(extension->id());
}

// Tests that if the "Permissions" Family Link toggle becomes disabled
// previously approved extensions are still enabled.
TEST_F(SupervisedUserExtensionTest,
       PermissionsToggleOffDoesNotAffectAlreadyEnabled) {
  InitServices(/*profile_is_supervised=*/true);
  supervised_user_test_util::
      SetSupervisedUserExtensionsMayRequestPermissionsPref(profile(), false);
  supervised_user_test_util::SetSkipParentApprovalToInstallExtensionsPref(
      profile(), false);

  // The installation helper function checks that the extension is initially
  // disabled.
  const Extension* extension =
      InstallNoPermissionsTestExtension(GetDefaultInstalledState());
  std::string id = extension->id();

  // Now approve the extension.
  supervised_user_extensions_delegate()->AddExtensionApproval(*extension);

  // The extension should be enabled now.
  CheckEnabled(id);

  // Custodian toggles "Permissions for sites, apps and extensions" to false.
  supervised_user_test_util::
      SetSupervisedUserExtensionsMayRequestPermissionsPref(profile(), false);

  // Already installed and enabled extensions should remain that way.
  CheckEnabled(id);
}

// Tests that extensions installed when the "Extensions" Family Link toggle
// is "On" are installed enabled and are granted parent approval.
TEST_F(SupervisedUserExtensionTest,
       ExtensionsToggleOnGrantsParentApprovalOnInstallation) {
  InitServices(/*profile_is_supervised=*/true);
  // Set the "Extensions" toggle to true, allowing installation without parental
  // approval.
  supervised_user_test_util::SetSkipParentApprovalToInstallExtensionsPref(
      profile(), true);
  const Extension* extension = InstallNoPermissionsTestExtension(INSTALL_NEW);
  std::string id = extension->id();

    // The extension has already been granted approval on its installation.
    CheckEnabled(id);
}

// Tests that installed extensions remain parent-approved and enabled
// if the "Extensions" switch is toggled to false.
TEST_F(SupervisedUserExtensionTest,
       ExtensionsToggleOffDoesNotAffectAlreadyEnabled) {
  InitServices(/*profile_is_supervised=*/true);
  // Set the "Extensions" toggle to true, allowing installation without parental
  // approval.
  supervised_user_test_util::SetSkipParentApprovalToInstallExtensionsPref(
      profile(), true);

  // If the Extensions toggle applies, the extension is installed and enabled.
  const Extension* extension = InstallNoPermissionsTestExtension(INSTALL_NEW);
  std::string id = extension->id();

    // The extension has already been granted approval on its installation.
    CheckEnabled(id);

  // Custodian sets the "Extensions" toggle to false.
  supervised_user_test_util::SetSkipParentApprovalToInstallExtensionsPref(
      profile(), false);

  // Already installed and enabled extensions should remain that way.
    CheckEnabled(id);
}

// Tests toggling the "Extensions" Family Link switch from false to true grants
// parental approval.
TEST_F(SupervisedUserExtensionTest,
       ExtensionsToggleOnGrantsMissingParentalApproval) {
  InitServices(/*profile_is_supervised=*/true);
  SetDefaultParentalControlSettings();

  const Extension* extension =
      InstallNoPermissionsTestExtension(GetDefaultInstalledState());
  std::string id = extension->id();

    CheckDisabledForCustodianApproval(id);
  // Custodian sets the "Extensions" toggle to True.
  supervised_user_test_util::SetSkipParentApprovalToInstallExtensionsPref(
      profile(), true);

  // The extension has been
  // granted approval and becomes enabled on toggling the switch.
  CheckEnabled(id);
}

// Tests the case when the extension approval arrives through sync before the
// extension itself is installed.
TEST_F(SupervisedUserExtensionTest, ExtensionApprovalBeforeInstallation) {
  InitServices(/*profile_is_supervised=*/true);
  SetDefaultParentalControlSettings();

  scoped_refptr<const Extension> extension =
      ExtensionBuilder(good_crx).SetID(good_crx).SetVersion("0").Build();
  // Approve the extension.
  supervised_user_extensions_delegate()->AddExtensionApproval(*extension);

  // Now install an extension, it should be enabled upon installation.
  base::FilePath path = data_dir().AppendASCII("good.crx");
  InstallCRX(path, INSTALL_NEW);

  // Make sure it's enabled.
  CheckEnabled(good_crx);
}

// Tests that when the parental controls start applying on Desktop (solution
// release), existing extensions remain enabled. On ChromeOS they are disabled.
TEST_F(SupervisedUserExtensionTest,
       ExtensionsOnDesktopRemainEnabledOnSkipParentApprovalRelease) {
  ExtensionServiceInitParams params;
  params.profile_is_supervised = true;
  InitializeExtensionService(std::move(params));
  SetDefaultParentalControlSettings();
  // Install an extension. It should be enabled as we haven't created the SU
  // extension manager yet. Treated as a pre-existing extension.
  base::FilePath path = data_dir().AppendASCII("good.crx");
  const Extension* extension = InstallCRX(path, INSTALL_NEW);
  ASSERT_TRUE(extension);

  // Make sure it's enabled.
  CheckEnabled(good_crx);

  // Create the extensions manager. If the
  // `SkipParentApprovalToInstallExtensions` feature applies for the first time,
  // the existing extensions remain enabled on Desktop.
  CreateExtensionManager();

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
  // On Desktop, before the release of parental controls the extensions would be
  // installed and enabled by default.
  CheckEnabled(good_crx);
#else
  CheckDisabledForCustodianApproval(good_crx);
#endif

  // Parent approval can be granted even if the extension behaves already as
  // parent-approved on Win/Linux/Mac.
  supervised_user_extensions_delegate()->AddExtensionApproval(*extension);
  CheckEnabled(good_crx);
}

// Tests that when the parental controls start applying on Desktop (solution
// release), existing extensions that have been marked parent-approved on
// Desktop by default can be upgraded without further parental approval.
TEST_F(SupervisedUserExtensionTest,
       ExtensionsEnabledOnSkipParentApprovalReleaseCanBeUpgraded) {
  ExtensionServiceInitParams params;
  params.profile_is_supervised = true;
  InitializeExtensionService(std::move(params));
  SetDefaultParentalControlSettings();
  // Install an extension. It should be enabled as we haven't created the SU
  // extension manager yet. Treated as a pre-existing extension.
  const Extension* extension = InstallPermissionsTestExtension(INSTALL_NEW);
  ASSERT_TRUE(extension);
  std::string extension_id = extension->id();

  // Make sure it's enabled.
  CheckEnabled(extension_id);

  // Create the extensions manager. If the
  // `SkipParentApprovalToInstallExtensions` feature applies for the first time,
  // the existing extensions remain enabled on Desktop.
  CreateExtensionManager();

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
  // On Desktop, before the release of parental controls the extensions would be
  // installed and enabled by default.
  CheckEnabled(extension_id);
#else
  CheckDisabledForCustodianApproval(extension_id);
#endif

  // Update to a new version with increased permissions.
  UpdatePermissionsTestExtension(extension_id, "2", DISABLED);
  const Extension* extension2 =
      CheckDisabledForPermissionsIncrease(extension_id);
  ASSERT_TRUE(extension2);

  // Grant the upgraded permissions.
  registrar()->GrantPermissionsAndEnableExtension(*extension2);
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
  // The parental approval granted at SU Extension manager still applies.
  CheckEnabled(extension_id);
#else
  // In ChromeOS the extension is still disabled as parent approval was never
  // granted.
  CheckDisabledForCustodianApproval(extension_id);
#endif
}

// Tests that uninstalling a parent-approved extension removes the parental
// approval.
TEST_F(SupervisedUserExtensionTest, UnistallingRevokesParentApproval) {
  InitServices(/*profile_is_supervised=*/true);
  SetDefaultParentalControlSettings();

  const Extension* extension =
      InstallNoPermissionsTestExtension(GetDefaultInstalledState());
  std::string extension_id = extension->id();

    CheckDisabledForCustodianApproval(extension_id);
    // Simulate parent approval.
    supervised_user_extensions_delegate()->AddExtensionApproval(*extension);

  CheckEnabled(extension_id);
  EXPECT_TRUE(profile()
                  ->GetPrefs()
                  ->GetDict(prefs::kSupervisedUserApprovedExtensions)
                  .contains(extension_id));

  // Uninstall the extension.
  std::u16string error;
  registrar()->UninstallExtension(
      extension_id, UninstallReason::UNINSTALL_REASON_FOR_TESTING, &error);
  EXPECT_FALSE(profile()
                   ->GetPrefs()
                   ->GetDict(prefs::kSupervisedUserApprovedExtensions)
                   .contains(extension_id));
}

// Tests that the kApprovalGranted UMA metric only increments once without
// duplication for the same extension id.
TEST_F(SupervisedUserExtensionTest, DontTriggerMetricsIfAlreadyApproved) {
  InitServices(/*profile_is_supervised=*/true);
  SetDefaultParentalControlSettings();

  base::HistogramTester histogram_tester;

  base::FilePath path = data_dir().AppendASCII("good.crx");
  // The extension should be installed but disabled until we perform custodian
  // approval.
  const Extension* extension = InstallCRX(path, INSTALL_WITHOUT_LOAD);
  ASSERT_TRUE(extension);
  CheckDisabledForCustodianApproval(extension->id());

  // Simulate parent approval for the extension installation.
  supervised_user_extensions_delegate()->AddExtensionApproval(*extension);
  // The extension should be enabled now.
  CheckEnabled(extension->id());

  // Should see 1 kApprovalGranted metric count recorded.
  histogram_tester.ExpectUniqueSample(
      SupervisedUserExtensionsMetricsRecorder::kExtensionsHistogramName,
      SupervisedUserExtensionsMetricsRecorder::UmaExtensionState::
          kApprovalGranted,
      1);
  histogram_tester.ExpectTotalCount(
      SupervisedUserExtensionsMetricsRecorder::kExtensionsHistogramName, 1);

  // Simulate the supervised user disabling and re-enabling the extension
  // without changing anything else.
  supervised_user_extensions_delegate()->AddExtensionApproval(*extension);

  // Should not see another kApprovalGranted metric count recorded because it
  // was already approved. The previous step should be a no-op.
  histogram_tester.ExpectBucketCount(
      SupervisedUserExtensionsMetricsRecorder::kExtensionsHistogramName,
      SupervisedUserExtensionsMetricsRecorder::UmaExtensionState::
          kApprovalGranted,
      1);
  histogram_tester.ExpectTotalCount(
      SupervisedUserExtensionsMetricsRecorder::kExtensionsHistogramName, 1);

  // Now remove approval.
  supervised_user_extensions_delegate()->RemoveExtensionApproval(*extension);

  // There should be a kApprovalRemoved metric count.
  histogram_tester.ExpectBucketCount(
      SupervisedUserExtensionsMetricsRecorder::kExtensionsHistogramName,
      SupervisedUserExtensionsMetricsRecorder::UmaExtensionState::
          kApprovalRemoved,
      1);
  histogram_tester.ExpectTotalCount(
      SupervisedUserExtensionsMetricsRecorder::kExtensionsHistogramName, 2);
}

// Tests that parent approval is necessary but not sufficient to enable
// extensions when both disable reasons custodian_approval_required and
// permissions_increase are present.
TEST_F(SupervisedUserExtensionTest, ParentApprovalNecessaryButNotSufficient) {
  InitServices(/*profile_is_supervised=*/true);
  SetDefaultParentalControlSettings();

  std::string id =
      InstallPermissionsTestExtension(GetDefaultInstalledState())->id();
  // Update to a new version with increased permissions.
  UpdatePermissionsTestExtension(id, "2", DISABLED);
  // Expect both disable reasons.
  CheckDisabledForCustodianApproval(id);
  const Extension* extension = CheckDisabledForPermissionsIncrease(id);
  ASSERT_TRUE(extension);

  // Try to enable the extension without parent approval to prove that it's
  // necessary.
  registrar()->GrantPermissionsAndEnableExtension(*extension);
  // The extension is still disabled.
  CheckDisabledForCustodianApproval(id);
  CheckDisabledForPermissionsIncrease(id);

  // Simulate parent approval.
  supervised_user_extensions_delegate()->AddExtensionApproval(
      *registry()->GetInstalledExtension(id));
  // The extension is still disabled (not sufficient).
  CheckDisabledForPermissionsIncrease(id);

  // Now grant permissions and try to enable again.
  registrar()->GrantPermissionsAndEnableExtension(*extension);
  // The extension should be enabled.
  CheckEnabled(id);
}
}  // namespace extensions
