// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "chrome/browser/extensions/api/webstore_private/webstore_private_api.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_with_install.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/supervised_user/supervised_user_extensions_metrics_recorder.h"
#include "chrome/browser/supervised_user/supervised_user_service.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "extensions/browser/api_test_utils.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "testing/gtest/include/gtest/gtest.h"

using extensions::api_test_utils::RunFunctionAndReturnSingleResult;

namespace {
const char good_crx[] = "ldnnhddmnhbkjipkidpdiheffobcpfmf";
const char autoupdate[] = "ogjcoiohnmldgjemafoockdghcjciccf";
const char permissions_increase[] = "pgdpcfcocojkjfbgpiianjngphoopgmo";
}  // namespace

namespace extensions {

class SupervisedUserExtensionTest : public ExtensionServiceTestWithInstall {
 public:
  SupervisedUserExtensionTest() = default;

 protected:
  void SetSupervisedUserExtensionsMayRequestPermissionsPref(bool enabled) {
    supervised_user_service()
        ->SetSupervisedUserExtensionsMayRequestPermissionsPrefForTesting(
            enabled);
  }

  void InitServices(bool profile_is_supervised) {
    ExtensionServiceInitParams params = CreateDefaultInitParams();
    params.profile_is_supervised = profile_is_supervised;
    // If profile is supervised, don't pass a pref file such that the testing
    // profile creates a pref service that uses SupervisedUserPrefStore.
    if (profile_is_supervised) {
      params.pref_file = base::FilePath();
    }
    InitializeExtensionService(params);

    supervised_user_service()->Init();
  }

  std::string InstallPermissionsTestExtension() {
    return InstallTestExtension(permissions_increase, dir_path("1"),
                                pem_path());
  }

  void UpdatePermissionsTestExtension(const std::string& id,
                                      const std::string& version,
                                      UpdateState expected_state) {
    UpdateTestExtension(dir_path(version), pem_path(), id, version,
                        expected_state);
  }

  std::string InstallNoPermissionsTestExtension() {
    base::FilePath base_path = data_dir().AppendASCII("autoupdate");
    base::FilePath pem_path = base_path.AppendASCII("key.pem");
    base::FilePath dir_path = base_path.AppendASCII("v1");

    return InstallTestExtension(autoupdate, dir_path, pem_path);
  }

  void UpdateNoPermissionsTestExtension(const std::string& id,
                                        const std::string& version,
                                        UpdateState expected_state) {
    base::FilePath base_path = data_dir().AppendASCII("autoupdate");
    base::FilePath pem_path = base_path.AppendASCII("key.pem");
    base::FilePath dir_path = base_path.AppendASCII("v" + version);

    UpdateTestExtension(dir_path, pem_path, id, version, expected_state);
  }

  std::string InstallTestExtension(const std::string& id,
                                   const base::FilePath& dir_path,
                                   const base::FilePath& pem_path) {
    const Extension* extension =
        PackAndInstallCRX(dir_path, pem_path, INSTALL_WITHOUT_LOAD);
    // The extension must now be installed.
    EXPECT_TRUE(extension);
    EXPECT_EQ(extension->id(), id);
    CheckDisabledForCustodianApproval(id);
    EXPECT_EQ(base::Version("1"), extension->version());
    return id;
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
    EXPECT_EQ(disable_reason::DISABLE_NONE,
              extension_prefs->GetDisableReasons(extension_id));
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

  SupervisedUserService* supervised_user_service() {
    return SupervisedUserServiceFactory::GetForProfile(profile());
  }

 private:
  // Returns true if the extension has disable reason permissions_increase or
  // custodian_approval_required. Tests the Webstore Private Api.
  bool IsPendingCustodianApproval(const std::string& extension_id) {
    auto function = base::MakeRefCounted<
        WebstorePrivateIsPendingCustodianApprovalFunction>();

    absl::optional<base::Value> result(RunFunctionAndReturnSingleResult(
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
};

// Tests that regular users are not affecting supervised user UMA metrics.
TEST_F(SupervisedUserExtensionTest,
       RegularUsersNotAffectingSupervisedUserMetrics) {
  InitServices(/*profile_is_supervised=*/false);

  base::HistogramTester histogram_tester;

  base::FilePath path = data_dir().AppendASCII("good.crx");
  const Extension* extension = InstallCRX(path, INSTALL_NEW);
  ASSERT_TRUE(extension);

  supervised_user_service()->AddExtensionApproval(*extension);

  histogram_tester.ExpectTotalCount(
      SupervisedUserExtensionsMetricsRecorder::kExtensionsHistogramName, 0);

  supervised_user_service()->RemoveExtensionApproval(*extension);

  histogram_tester.ExpectTotalCount(
      SupervisedUserExtensionsMetricsRecorder::kExtensionsHistogramName, 0);
}

// Tests that simulating custodian approval for regular users doesn't cause any
// unexpected behavior.
TEST_F(SupervisedUserExtensionTest,
       CustodianApprovalDoesNotAffectRegularUsers) {
  InitServices(/*profile_is_supervised=*/false);
  SetSupervisedUserExtensionsMayRequestPermissionsPref(false);

  // Install an extension.
  base::FilePath path = data_dir().AppendASCII("good.crx");
  const Extension* extension = InstallCRX(path, INSTALL_NEW);
  std::string id = extension->id();

  // The extension should be enabled because this is a regular user.
  CheckEnabled(id);

  // Simulate custodian approval and removal.
  supervised_user_service()->AddExtensionApproval(*extension);
  supervised_user_service()->RemoveExtensionApproval(*extension);
  // The extension should still be enabled.
  CheckEnabled(id);
}

// Tests that adding supervision to a regular account (Gellerization) disables
// previously installed extensions.
TEST_F(SupervisedUserExtensionTest, ExtensionsDisabledAfterGellerization) {
  InitServices(/*profile_is_supervised=*/false);
  SetSupervisedUserExtensionsMayRequestPermissionsPref(true);

  // Install an extension.
  base::FilePath path = data_dir().AppendASCII("good.crx");
  const Extension* extension = InstallCRX(path, INSTALL_NEW);
  ASSERT_TRUE(extension);
  std::string id = extension->id();
  // The extension should be enabled because this is a regular user.
  CheckEnabled(id);

  // Now make the profile supervised.
  profile()->AsTestingProfile()->SetIsSupervisedProfile();

  // The extension should be disabled now pending custodian approval.
  CheckDisabledForCustodianApproval(id);

  // Grant parent approval.
  supervised_user_service()->AddExtensionApproval(*extension);

  // The extension should be enabled now.
  CheckEnabled(id);

  // Remove parent approval.
  supervised_user_service()->RemoveExtensionApproval(*extension);

  // The extension should be disabled again now.
  CheckDisabledForCustodianApproval(id);
}

// Tests that a child user is allowed to install extensions when pref
// kSupervisedUserExtensionsMayRequestPermissions is set to true, but that
// newly-installed extensions are disabled until approved by the parent.
TEST_F(SupervisedUserExtensionTest,
       InstallAllowedButDisabledForSupervisedUser) {
  InitServices(/*profile_is_supervised=*/true);
  SetSupervisedUserExtensionsMayRequestPermissionsPref(true);

  base::FilePath path = data_dir().AppendASCII("good.crx");
  const Extension* extension = InstallCRX(path, INSTALL_WITHOUT_LOAD);
  ASSERT_TRUE(extension);
  std::string id = extension->id();

  // This extension is a supervised user initiated install and should remain
  // disabled.
  CheckDisabledForCustodianApproval(id);

  // Grant parent approval.
  supervised_user_service()->AddExtensionApproval(*extension);
  // The extension is now enabled.
  CheckEnabled(id);

  // Remove parent approval.
  supervised_user_service()->RemoveExtensionApproval(*extension);

  // The extension should be disabled again now.
  CheckDisabledForCustodianApproval(id);
}

// Tests that supervised users may approve permission updates without parent
// approval if kSupervisedUserExtensionsMayRequestPermissions is true.
TEST_F(SupervisedUserExtensionTest, UpdateWithPermissionsIncrease) {
  InitServices(/*profile_is_supervised=*/true);
  SetSupervisedUserExtensionsMayRequestPermissionsPref(true);

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

  std::string id = InstallPermissionsTestExtension();
  // Simulate parent approval.
  supervised_user_service()->UpdateApprovedExtensionForTesting(
      id, SupervisedUserService::ApprovedExtensionChange::kAdd);
  // The extension should be enabled.
  CheckEnabled(id);

  // Should see 1 kApprovalGranted metric count.
  histogram_tester.ExpectUniqueSample(
      SupervisedUserExtensionsMetricsRecorder::kExtensionsHistogramName,
      SupervisedUserExtensionsMetricsRecorder::UmaExtensionState::
          kApprovalGranted,
      1);
  histogram_tester.ExpectTotalCount(
      SupervisedUserExtensionsMetricsRecorder::kExtensionsHistogramName, 1);
  EXPECT_EQ(
      1,
      user_action_tester.GetActionCount(
          SupervisedUserExtensionsMetricsRecorder::kApprovalGrantedActionName));

  // Update to a new version with increased permissions.
  UpdatePermissionsTestExtension(id, "2", DISABLED);
  const Extension* extension = CheckDisabledForPermissionsIncrease(id);
  ASSERT_TRUE(extension);

  // Simulate supervised user approving the extension without further parent
  // approval.
  service()->GrantPermissionsAndEnableExtension(extension);

  // The extension should be enabled.
  CheckEnabled(id);

  // Remove extension approval.
  supervised_user_service()->UpdateApprovedExtensionForTesting(
      id, SupervisedUserService::ApprovedExtensionChange::kRemove);

  // Should see 1 kApprovalRemoved metric count.
  histogram_tester.ExpectBucketCount(
      SupervisedUserExtensionsMetricsRecorder::kExtensionsHistogramName,
      SupervisedUserExtensionsMetricsRecorder::UmaExtensionState::
          kApprovalRemoved,
      1);
  histogram_tester.ExpectTotalCount(
      SupervisedUserExtensionsMetricsRecorder::kExtensionsHistogramName, 2);
  EXPECT_EQ(
      1,
      user_action_tester.GetActionCount(
          SupervisedUserExtensionsMetricsRecorder::kApprovalRemovedActionName));

  // The extension should be disabled now.
  CheckDisabledForCustodianApproval(id);
}

// Tests that if "Permissions for sites, apps and extensions" toggle is
// disabled, resulting in the pref
// kSupervisedUserExtensionsMayRequestPermissions set to false, then child users
// cannot approve permission updates.
TEST_F(SupervisedUserExtensionTest,
       ChildUserCannotApproveAdditionalPermissions) {
  InitServices(/*profile_is_supervised=*/true);
  // Keep the toggle on initially just to install the extension.
  SetSupervisedUserExtensionsMayRequestPermissionsPref(true);

  base::HistogramTester histogram_tester;

  std::string id = InstallPermissionsTestExtension();
  const Extension* extension1 = CheckDisabledForCustodianApproval(id);
  ASSERT_TRUE(extension1);
  // Simulate parent granting approval for the initial version.
  supervised_user_service()->AddExtensionApproval(*extension1);
  // The extension should be enabled now.
  CheckEnabled(id);

  // Should see 1 kApprovalGranted metric count.
  histogram_tester.ExpectUniqueSample(
      SupervisedUserExtensionsMetricsRecorder::kExtensionsHistogramName,
      SupervisedUserExtensionsMetricsRecorder::UmaExtensionState::
          kApprovalGranted,
      1);
  histogram_tester.ExpectTotalCount(
      SupervisedUserExtensionsMetricsRecorder::kExtensionsHistogramName, 1);

  // Update to a new version with increased permissions.
  UpdatePermissionsTestExtension(id, "2", DISABLED);
  const Extension* extension2 = CheckDisabledForPermissionsIncrease(id);
  ASSERT_TRUE(extension2);

  // Flip toggle to off. Now the extension is blocked since it requires
  // additional permissions and the child can't approve additional permissions.
  SetSupervisedUserExtensionsMayRequestPermissionsPref(false);

  // Simulate child granting approval for the new permissions.
  service()->GrantPermissionsAndEnableExtension(extension2);

  // The extension is still disabled. What worked in the
  // UpdateWithPermissionsIncrease test no longer works here because the toggle
  // is off.
  CheckDisabledForPermissionsIncrease(id);

  // Now uninstall the extension, just for UMA histogram tests.
  UninstallExtension(id);

  // Should see 1 kApprovalRemoved metric count.
  histogram_tester.ExpectBucketCount(
      SupervisedUserExtensionsMetricsRecorder::kExtensionsHistogramName,
      SupervisedUserExtensionsMetricsRecorder::UmaExtensionState::
          kApprovalRemoved,
      1);
  histogram_tester.ExpectTotalCount(
      SupervisedUserExtensionsMetricsRecorder::kExtensionsHistogramName, 2);
}

// Tests that if an approved extension is updated to a newer version that
// doesn't require additional permissions, it is still enabled.
TEST_F(SupervisedUserExtensionTest, UpdateWithoutPermissionIncrease) {
  InitServices(/*profile_is_supervised=*/true);
  SetSupervisedUserExtensionsMayRequestPermissionsPref(true);

  // Save the id, as the extension object will be destroyed during updating.
  std::string id = InstallNoPermissionsTestExtension();
  supervised_user_service()->UpdateApprovedExtensionForTesting(
      id, SupervisedUserService::ApprovedExtensionChange::kAdd);
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

  // Even though supervised users can't approve additional approvals when the
  // "Permissions for sites, apps and extensions" toggle is off, updates with no
  // additional permissions should be okay.
  SetSupervisedUserExtensionsMayRequestPermissionsPref(false);
  std::string version3("3");
  UpdateNoPermissionsTestExtension(id, version3, ENABLED);

  // The extension should still be there and enabled.
  const Extension* extension2 = CheckEnabled(id);
  ASSERT_TRUE(extension2);
  // The version should have changed again.
  EXPECT_EQ(base::Version(version3), extension2->version());

  // Check that the approved extension id has been updated in the prefs as well.
  // Prefs are updated via sync.
  PrefService* pref_service = profile()->GetPrefs();
  ASSERT_TRUE(pref_service);
  const base::Value::Dict& approved_extensions =
      pref_service->GetDict(prefs::kSupervisedUserApprovedExtensions);
  EXPECT_TRUE(approved_extensions.FindString(id));
}

// Tests that the kApprovalGranted UMA metric only increments once without
// duplication for the same extension id.
TEST_F(SupervisedUserExtensionTest, DontTriggerMetricsIfAlreadyApproved) {
  InitServices(/*profile_is_supervised=*/true);
  SetSupervisedUserExtensionsMayRequestPermissionsPref(true);

  base::HistogramTester histogram_tester;

  base::FilePath path = data_dir().AppendASCII("good.crx");
  const Extension* extension = InstallCRX(path, INSTALL_WITHOUT_LOAD);
  ASSERT_TRUE(extension);
  // The extension should be installed but disabled pending custodian approval.
  CheckDisabledForCustodianApproval(extension->id());

  // Simulate parent approval for the extension installation.
  supervised_user_service()->AddExtensionApproval(*extension);
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
  supervised_user_service()->AddExtensionApproval(*extension);

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
  supervised_user_service()->RemoveExtensionApproval(*extension);

  // There should be a kApprovalRemoved metric count.
  histogram_tester.ExpectBucketCount(
      SupervisedUserExtensionsMetricsRecorder::kExtensionsHistogramName,
      SupervisedUserExtensionsMetricsRecorder::UmaExtensionState::
          kApprovalRemoved,
      1);
  histogram_tester.ExpectTotalCount(
      SupervisedUserExtensionsMetricsRecorder::kExtensionsHistogramName, 2);
}

// Tests that if "Permissions for sites, apps and extensions" toggle is
// disabled, resulting in the pref
// kSupervisedUserExtensionsMayRequestPermissions set to false, then child users
// cannot install new extensions.
TEST_F(SupervisedUserExtensionTest, SupervisedUserCannotInstallExtension) {
  InitServices(/*profile_is_supervised=*/true);
  SetSupervisedUserExtensionsMayRequestPermissionsPref(false);

  base::FilePath path = data_dir().AppendASCII("good.crx");
  const Extension* extension = InstallCRX(path, INSTALL_FAILED);
  // The extension should not have been installed.
  EXPECT_FALSE(extension);
}

// Tests that disabling the "Permissions for sites, apps and extensions" toggle
// has no effect on regular users.
TEST_F(SupervisedUserExtensionTest, RegularUserCanInstallExtension) {
  InitServices(/*profile_is_supervised=*/false);
  SetSupervisedUserExtensionsMayRequestPermissionsPref(false);

  base::FilePath path = data_dir().AppendASCII("good.crx");
  const Extension* extension = InstallCRX(path, INSTALL_NEW);
  // The extension should be installed and enabled.
  ASSERT_TRUE(extension);
  CheckEnabled(extension->id());
}

// Tests that if "Permissions for sites, apps and extensions" toggle is
// disabled, resulting in the pref
// kSupervisedUserExtensionsMayRequestPermissions set to false, previously
// approved extensions are still enabled.
TEST_F(SupervisedUserExtensionTest, ToggleOffDoesNotAffectAlreadyEnabled) {
  InitServices(/*profile_is_supervised=*/true);
  SetSupervisedUserExtensionsMayRequestPermissionsPref(true);

  // The installation helper function checks that the extension is initially
  // disabled.
  std::string id = InstallNoPermissionsTestExtension();

  // Now approve the extension.
  supervised_user_service()->UpdateApprovedExtensionForTesting(
      id, SupervisedUserService::ApprovedExtensionChange::kAdd);

  // The extension should be enabled now.
  CheckEnabled(id);

  // Custodian toggles "Permissions for sites, apps and extensions" to false.
  SetSupervisedUserExtensionsMayRequestPermissionsPref(false);

  // Already installed and enabled extensions should remain that way.
  CheckEnabled(id);
}

// Tests the case when the extension approval arrives through sync before the
// extension itself is installed.
TEST_F(SupervisedUserExtensionTest, ExtensionApprovalBeforeInstallation) {
  InitServices(/*profile_is_supervised=*/true);
  SetSupervisedUserExtensionsMayRequestPermissionsPref(true);

  supervised_user_service()->UpdateApprovedExtensionForTesting(
      good_crx, SupervisedUserService::ApprovedExtensionChange::kAdd);

  // Now install an extension.
  base::FilePath path = data_dir().AppendASCII("good.crx");
  InstallCRX(path, INSTALL_NEW);

  // Make sure it's enabled.
  CheckEnabled(good_crx);
}

// Tests that parent approval is necessary but not sufficient to enable
// extensions when both disable reasons custodian_approval_required and
// permissions_increase are present.
TEST_F(SupervisedUserExtensionTest, ParentApprovalNecessaryButNotSufficient) {
  InitServices(/*profile_is_supervised=*/true);
  SetSupervisedUserExtensionsMayRequestPermissionsPref(true);

  std::string id = InstallPermissionsTestExtension();
  // Update to a new version with increased permissions.
  UpdatePermissionsTestExtension(id, "2", DISABLED);
  // Expect both disable reasons.
  CheckDisabledForCustodianApproval(id);
  const Extension* extension = CheckDisabledForPermissionsIncrease(id);
  ASSERT_TRUE(extension);

  // Try to enable the extension without parent approval to prove that it's
  // necessary.
  service()->GrantPermissionsAndEnableExtension(extension);
  // The extension is still disabled.
  CheckDisabledForCustodianApproval(id);
  CheckDisabledForPermissionsIncrease(id);

  // Simulate parent approval.
  supervised_user_service()->UpdateApprovedExtensionForTesting(
      id, SupervisedUserService::ApprovedExtensionChange::kAdd);
  // The extension is still disabled (not sufficient).
  CheckDisabledForPermissionsIncrease(id);

  // Now grant permissions and try to enable again.
  service()->GrantPermissionsAndEnableExtension(extension);
  // The extension should be enabled.
  CheckEnabled(id);
}

}  // namespace extensions
