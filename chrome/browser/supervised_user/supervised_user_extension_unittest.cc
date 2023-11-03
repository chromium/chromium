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

// Base class for the extension permission tests for supervised users.

class SupervisedUserExtensionTestBase : public ExtensionServiceTestWithInstall {
 public:
  void InitServices(bool profile_is_supervised) {
    ExtensionServiceInitParams params;
    params.profile_is_supervised = profile_is_supervised;
    InitializeExtensionService(params);
    supervised_user_extensions_delegate_ =
        std::make_unique<SupervisedUserExtensionsDelegateImpl>(profile());
  }

  const Extension* InstallPermissionsTestExtension() {
    const Extension* extension =
        InstallTestExtension(permissions_increase, dir_path("1"), pem_path());
    return extension;
  }

  void UpdatePermissionsTestExtension(const std::string& id,
                                      const std::string& version,
                                      UpdateState expected_state) {
    UpdateTestExtension(dir_path(version), pem_path(), id, version,
                        expected_state);
  }

  const Extension* InstallNoPermissionsTestExtension() {
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
    if (ShouldExtensionPermissionsApply()) {
      EXPECT_TRUE(IsPendingCustodianApproval(extension_id));
    }
    ExtensionPrefs* extension_prefs = ExtensionPrefs::Get(profile());
    EXPECT_TRUE(extension_prefs->HasDisableReason(
        extension_id, disable_reason::DISABLE_PERMISSIONS_INCREASE));
    return registry()->disabled_extensions().GetByID(extension_id);
  }

  SupervisedUserExtensionsDelegate* supervised_user_extensions_delegate() {
    return supervised_user_extensions_delegate_.get();
  }

  const Extension* InstallTestExtension(const std::string& id,
                                        const base::FilePath& dir_path,
                                        const base::FilePath& pem_path) {
    // When extension permissions are enabled the extensions will be installed
    // but disabled until custodian approvals are performed.
    // When extension permissions are disabled the extensions will be installed
    // and enabled.
    auto install_state =
        ShouldExtensionPermissionsApply() ? INSTALL_WITHOUT_LOAD : INSTALL_NEW;
    const Extension* extension =
        PackAndInstallCRX(dir_path, pem_path, install_state);
    // The extension must now be installed.
    EXPECT_TRUE(extension);
    EXPECT_EQ(extension->id(), id);
    if (ShouldExtensionPermissionsApply()) {
      CheckDisabledForCustodianApproval(id);
    } else {
      CheckEnabled(id);
    }
    EXPECT_EQ(base::Version("1"), extension->version());
    return extension;
  }

  virtual bool ShouldExtensionPermissionsApply() = 0;

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

  std::unique_ptr<SupervisedUserExtensionsDelegateImpl>
      supervised_user_extensions_delegate_;
};

class SupervisedUserExtensionTest
    : public SupervisedUserExtensionTestBase,
      public testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  SupervisedUserExtensionTest() {
    // Parental restrictions on the extensions permissions for supervised users
    // on Desktop apply when the feature
    // kEnableExtensionsPermissionsForSupervisedUsersOnDesktop is enabled.
    // Extension permissions for supervised users are already enabled on
    // ChromeOS by default.
    are_extension_permissions_enabled = std::get<0>(GetParam());
    is_url_filtering_enabled = std::get<1>(GetParam());
    std::vector<base::test::FeatureRef> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
    if (are_extension_permissions_enabled) {
      enabled_features.push_back(
          supervised_user::
              kEnableExtensionsPermissionsForSupervisedUsersOnDesktop);
    } else {
      disabled_features.push_back(
          supervised_user::
              kEnableExtensionsPermissionsForSupervisedUsersOnDesktop);
    }
    if (is_url_filtering_enabled) {
      // We want to test that this feature has no impact on the test's
      // behavior.
      enabled_features.push_back(
          supervised_user::kFilterWebsitesForSupervisedUsersOnDesktopAndIOS);
    } else {
      disabled_features.push_back(
          supervised_user::kFilterWebsitesForSupervisedUsersOnDesktopAndIOS);
    }
    feature_list_.InitWithFeatures(enabled_features, disabled_features);
#endif
  }

  // SupervisedUserExtensionTestBase implementation:
  bool ShouldExtensionPermissionsApply() override {
    return are_extension_permissions_enabled;
  }

 private:
  bool are_extension_permissions_enabled;
  bool is_url_filtering_enabled;
  base::test::ScopedFeatureList feature_list_;
};

// Tests that regular users are not affecting supervised user UMA metrics.
TEST_P(SupervisedUserExtensionTest,
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
TEST_P(SupervisedUserExtensionTest,
       CustodianApprovalDoesNotAffectRegularUsers) {
  InitServices(/*profile_is_supervised=*/false);
  supervised_user_test_util::
      SetSupervisedUserExtensionsMayRequestPermissionsPref(profile(), false);

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

// Tests that if the extension permissions are enabled, adding supervision
// to a regular account (Gellerization) disables previously installed
// extensions. Otherise the extension remains enabled.
TEST_P(SupervisedUserExtensionTest, ExtensionsStateAfterGellerization) {
  InitServices(/*profile_is_supervised=*/false);
  supervised_user_test_util::
      SetSupervisedUserExtensionsMayRequestPermissionsPref(profile(), true);

  // Install an extension, it should be enabled because this is a regular user.
  base::FilePath path = data_dir().AppendASCII("good.crx");
  const Extension* extension = InstallCRX(path, INSTALL_NEW);
  ASSERT_TRUE(extension);
  std::string id = extension->id();
  CheckEnabled(id);

  // Now make the profile supervised.
  profile()->AsTestingProfile()->SetIsSupervisedProfile();

  if (ShouldExtensionPermissionsApply()) {
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
  } else {
    // The extension should still be enabled.
    CheckEnabled(id);
  }
}

// Tests that a child user is allowed to install extensions when pref
// kSupervisedUserExtensionsMayRequestPermissions is set to true.
// If the extension permissions are enabled the newly-installed extensions
// are disabled until approved by the parent.
// Otherwise the newly-installed extensions are enabled.
TEST_P(SupervisedUserExtensionTest, InstallAllowedForSupervisedUser) {
  InitServices(/*profile_is_supervised=*/true);
  supervised_user_test_util::
      SetSupervisedUserExtensionsMayRequestPermissionsPref(profile(), true);

  // When extension permissions are enabled the extensions will be installed
  // but disabled until custodian approvals are performed.
  // When extension permissions are disabled the extensions will be installed
  // and enabled.
  auto install_state =
      ShouldExtensionPermissionsApply() ? INSTALL_WITHOUT_LOAD : INSTALL_NEW;
  base::FilePath path = data_dir().AppendASCII("good.crx");
  const Extension* extension = InstallCRX(path, install_state);
  ASSERT_TRUE(extension);
  std::string id = extension->id();

  if (ShouldExtensionPermissionsApply()) {
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
  } else {
    // The new installed extension should be enabled.
    CheckEnabled(id);
  }
}

// Tests that supervised users may approve permission updates without parent
// approval if kSupervisedUserExtensionsMayRequestPermissions is true and
// the extension permission are enable.
// If the extension permission are disabled, the child can approve permission
// updates by default.
TEST_P(SupervisedUserExtensionTest, UpdateWithPermissionsIncrease) {
  InitServices(true);
  supervised_user_test_util::
      SetSupervisedUserExtensionsMayRequestPermissionsPref(profile(), true);

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

  std::string id = InstallPermissionsTestExtension()->id();
  if (ShouldExtensionPermissionsApply()) {
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
  }
  // The extension should be enabled.
  CheckEnabled(id);

  // Update to a new version with increased permissions.
  UpdatePermissionsTestExtension(id, "2", DISABLED);
  const Extension* extension = CheckDisabledForPermissionsIncrease(id);
  ASSERT_TRUE(extension);

  // Simulate supervised user approving the extension without further parent
  // approval.
  service()->GrantPermissionsAndEnableExtension(extension);

  // The extension should be enabled.
  CheckEnabled(id);

  if (ShouldExtensionPermissionsApply()) {
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
}

// Tests that if "Permissions for sites, apps and extensions" toggle is
// disabled, resulting in the pref
// kSupervisedUserExtensionsMayRequestPermissions set to false, then:
// If the extension permissions are enabled child users cannot approve
// permission updates, otherwise they can approve permission updates.
TEST_P(SupervisedUserExtensionTest,
       ChildUserCannotApproveAdditionalPermissions) {
  InitServices(/*profile_is_supervised=*/true);
  // Keep the toggle on initially just to install the extension.
  supervised_user_test_util::
      SetSupervisedUserExtensionsMayRequestPermissionsPref(profile(), true);

  base::HistogramTester histogram_tester;

  std::string id = InstallPermissionsTestExtension()->id();
  const Extension* extension1 = nullptr;
  if (ShouldExtensionPermissionsApply()) {
    extension1 = CheckDisabledForCustodianApproval(id);
    ASSERT_TRUE(extension1);

    // Simulate parent granting approval for the initial version.
    supervised_user_extensions_delegate()->AddExtensionApproval(*extension1);
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
  } else {
    // The extension is installed as enabled.
    extension1 = CheckEnabled(id);
    ASSERT_TRUE(extension1);
  }

  // Update to a new version with increased permissions.
  std::string version2("2");
  UpdatePermissionsTestExtension(id, version2, DISABLED);
  const Extension* extension2 = CheckDisabledForPermissionsIncrease(id);
  ASSERT_TRUE(extension2);

  // Flip toggle to off.
  supervised_user_test_util::
      SetSupervisedUserExtensionsMayRequestPermissionsPref(profile(), false);
  // Now the extension is blocked since it requires additional permissions.
  // Simulate child granting approval for the new permissions.
  service()->GrantPermissionsAndEnableExtension(extension2);

  if (ShouldExtensionPermissionsApply()) {
    // The extension is still disabled because the child cannot grant additonal
    // permissions.
    CheckDisabledForPermissionsIncrease(id);
  } else {
    // The extension should now be enabled and the version number increased.
    extension2 = CheckEnabled(id);
    EXPECT_TRUE(extension2);
    EXPECT_EQ(base::Version(version2), extension2->version());
  }
}

// Tests that if an approved extension is updated to a newer version that
// doesn't require additional permissions, it is still enabled.
TEST_P(SupervisedUserExtensionTest, UpdateWithoutPermissionIncrease) {
  InitServices(/*profile_is_supervised=*/true);
  supervised_user_test_util::
      SetSupervisedUserExtensionsMayRequestPermissionsPref(profile(), true);

  const Extension* extension = InstallNoPermissionsTestExtension();
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

  // Even though supervised users can't approve additional approvals when the
  // 1) "Permissions for sites, apps and extensions" toggle is off, and 2)
  // 2) extension permissions are enabled, additional permissions should be
  // okay. If extension permissions are disabled, additional permissions should
  // also be okay.
  supervised_user_test_util::
      SetSupervisedUserExtensionsMayRequestPermissionsPref(profile(), false);
  std::string version3("3");
  UpdateNoPermissionsTestExtension(id, version3, ENABLED);

  // The extension should still be there and enabled.
  const Extension* extension2 = CheckEnabled(id);
  ASSERT_TRUE(extension2);
  // The version should have changed again.
  EXPECT_EQ(base::Version(version3), extension2->version());

  if (ShouldExtensionPermissionsApply()) {
    // Check that the approved extension id has been updated in the prefs as
    // well. Prefs are updated via sync.
    PrefService* pref_service = profile()->GetPrefs();
    ASSERT_TRUE(pref_service);
    const base::Value::Dict& approved_extensions =
        pref_service->GetDict(prefs::kSupervisedUserApprovedExtensions);
    EXPECT_TRUE(approved_extensions.FindString(id));
  }
}

// Tests that if "Permissions for sites, apps and extensions" toggle is
// disabled, resulting in the pref
// kSupervisedUserExtensionsMayRequestPermissions set to false, then:
// - If extension permissions are enabled child users cannot install new
// extensions.
// - If extension permissions are disabled child users can install new
// extensions.
TEST_P(SupervisedUserExtensionTest, SupervisedUserCannotInstallExtension) {
  InitServices(/*profile_is_supervised=*/true);
  supervised_user_test_util::
      SetSupervisedUserExtensionsMayRequestPermissionsPref(profile(), false);

  base::FilePath path = data_dir().AppendASCII("good.crx");
  // When extension permissions are enabled the extensions will fail
  // installation. When extension permissions are disabled the extensions will
  // be installed and enabled.
  if (ShouldExtensionPermissionsApply()) {
    const Extension* extension = InstallCRX(path, INSTALL_FAILED);
    EXPECT_FALSE(extension);
  } else {
    const Extension* extension = InstallCRX(path, INSTALL_NEW);
    EXPECT_TRUE(extension);
    CheckEnabled(extension->id());
  }
}

// Tests that disabling the "Permissions for sites, apps and extensions" toggle
// has no effect on regular users.
TEST_P(SupervisedUserExtensionTest, RegularUserCanInstallExtension) {
  InitServices(/*profile_is_supervised=*/false);
  supervised_user_test_util::
      SetSupervisedUserExtensionsMayRequestPermissionsPref(profile(), false);

  base::FilePath path = data_dir().AppendASCII("good.crx");
  // The extension should be installed and enabled.
  const Extension* extension = InstallCRX(path, INSTALL_NEW);

  ASSERT_TRUE(extension);
  CheckEnabled(extension->id());
}

// Tests that if "Permissions for sites, apps and extensions" toggle is
// disabled, resulting in the pref
// kSupervisedUserExtensionsMayRequestPermissions set to false, previously
// approved extensions are still enabled.
TEST_P(SupervisedUserExtensionTest, ToggleOffDoesNotAffectAlreadyEnabled) {
  InitServices(/*profile_is_supervised=*/true);
  supervised_user_test_util::
      SetSupervisedUserExtensionsMayRequestPermissionsPref(profile(), true);

  // The installation helper function checks that the extension is initially
  // disabled.
  const Extension* extension = InstallNoPermissionsTestExtension();
  std::string id = extension->id();

  if (ShouldExtensionPermissionsApply()) {
    // Now approve the extension.
    supervised_user_extensions_delegate()->AddExtensionApproval(*extension);
  }

  // The extension should be enabled now.
  CheckEnabled(id);

  // Custodian toggles "Permissions for sites, apps and extensions" to false.
  supervised_user_test_util::
      SetSupervisedUserExtensionsMayRequestPermissionsPref(profile(), false);

  // Already installed and enabled extensions should remain that way.
  CheckEnabled(id);
}

// Tests the case when the extension approval arrives through sync before the
// extension itself is installed.
TEST_P(SupervisedUserExtensionTest, ExtensionApprovalBeforeInstallation) {
  InitServices(/*profile_is_supervised=*/true);
  supervised_user_test_util::
      SetSupervisedUserExtensionsMayRequestPermissionsPref(profile(), true);

  scoped_refptr<const Extension> extension =
      ExtensionBuilder(good_crx).SetID(good_crx).SetVersion("0").Build();
  if (ShouldExtensionPermissionsApply()) {
    // Now approve the extension.
    supervised_user_extensions_delegate()->AddExtensionApproval(*extension);
  }

  // Now install an extension, it should be enabled upon installation.
  base::FilePath path = data_dir().AppendASCII("good.crx");
  InstallCRX(path, INSTALL_NEW);

  // Make sure it's enabled.
  CheckEnabled(good_crx);
}

INSTANTIATE_TEST_SUITE_P(
    ExtensionsPermissionsForSupervisedUsersOnDesktopFeature,
    SupervisedUserExtensionTest,
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
    testing::Values(std::make_tuple(/*are_extension_permissions_enabled=*/false,
                                    /*is_url_filtering_enabled*/ false),
                    std::make_tuple(/*are_extension_permissions_enabled=*/false,
                                    /*is_url_filtering_enabled*/ true),
                    std::make_tuple(/*are_extension_permissions_enabled=*/true,
                                    /*is_url_filtering_enabled*/ false),
                    std::make_tuple(/*are_extension_permissions_enabled=*/true,
                                    /*is_url_filtering_enabled*/ true))
#else
    // On ChromeOS the extension permissions and the url filtering are on by
    // default.
    testing::Values(std::make_tuple(/*are_extension_permissions_enabled=*/true,
                                    /*is_url_filtering_enabled*/ true))
#endif
);

// Test class for cases that apply only when Extension Permissions are enabled.
class SupervisedUserWithEnabledExtensionPermissionsTest
    : public SupervisedUserExtensionTestBase,
      public testing::WithParamInterface<bool> {
 public:
  SupervisedUserWithEnabledExtensionPermissionsTest() {
    // Parental restrictions on the extensions permissions for supervised users
    // on Desktop apply when the feature
    // kEnableExtensionsPermissionsForSupervisedUsersOnDesktop is enabled.
    // Extension permissions for supervised users are already enabled on
    // ChromeOS by default.
    is_url_filtering_enabled = GetParam();
    std::vector<base::test::FeatureRef> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
    // We want to test that this feature has no impact on the test's behavior.
    if (is_url_filtering_enabled) {
      enabled_features.push_back(
          supervised_user::kFilterWebsitesForSupervisedUsersOnDesktopAndIOS);
    } else {
      disabled_features.push_back(
          supervised_user::kFilterWebsitesForSupervisedUsersOnDesktopAndIOS);
    }

    enabled_features.push_back(
        supervised_user::
            kEnableExtensionsPermissionsForSupervisedUsersOnDesktop);
    feature_list_.InitWithFeatures(enabled_features, disabled_features);
#endif
  }

  // SupervisedUserExtensionTestBase implementation:
  bool ShouldExtensionPermissionsApply() override { return true; }

 private:
  bool is_url_filtering_enabled;
  base::test::ScopedFeatureList feature_list_;
};

// Tests that the kApprovalGranted UMA metric only increments once without
// duplication for the same extension id.
TEST_P(SupervisedUserWithEnabledExtensionPermissionsTest,
       DontTriggerMetricsIfAlreadyApproved) {
  InitServices(/*profile_is_supervised=*/true);
  supervised_user_test_util::
      SetSupervisedUserExtensionsMayRequestPermissionsPref(profile(), true);

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
TEST_P(SupervisedUserWithEnabledExtensionPermissionsTest,
       ParentApprovalNecessaryButNotSufficient) {
  InitServices(/*profile_is_supervised=*/true);
  supervised_user_test_util::
      SetSupervisedUserExtensionsMayRequestPermissionsPref(profile(), true);

  std::string id = InstallPermissionsTestExtension()->id();
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
  supervised_user_extensions_delegate()->AddExtensionApproval(
      *registry()->GetInstalledExtension(id));
  // The extension is still disabled (not sufficient).
  CheckDisabledForPermissionsIncrease(id);

  // Now grant permissions and try to enable again.
  service()->GrantPermissionsAndEnableExtension(extension);
  // The extension should be enabled.
  CheckEnabled(id);
}

INSTANTIATE_TEST_SUITE_P(
    ExtensionsPermissionsEnabledForSupervisedUsersOnDesktop,
    SupervisedUserWithEnabledExtensionPermissionsTest,
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
    testing::Values(
        /*is_url_filtering_enabled=*/false,
        /*is_url_filtering_enabled=*/true)
#else
    // On Chrome OS Url filtering is always enabled.
    testing::Values(/*is_url_filtering_enabled=*/true)
#endif
);

}  // namespace extensions
