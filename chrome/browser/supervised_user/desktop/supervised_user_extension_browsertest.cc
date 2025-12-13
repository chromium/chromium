// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/test/gtest_util.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/supervised_user/supervised_user_extensions_delegate_impl.h"
#include "chrome/browser/supervised_user/supervised_user_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/supervised_user/parent_permission_dialog_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chrome/test/supervised_user/supervision_mixin.h"
#include "components/supervised_user/core/browser/supervised_user_preferences.h"
#include "components/supervised_user/core/browser/supervised_user_utils.h"
#include "components/supervised_user/core/common/pref_names.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_launcher.h"
#include "content/public/test/web_contents_tester.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/supervised_user_extensions_delegate.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/supervised_user/chromeos/parent_access_extension_approvals_manager.h"
#include "chrome/browser/ui/webui/ash/parent_access/fake_parent_access_dialog.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace {
constexpr char kGoodCrxId[] = "ldnnhddmnhbkjipkidpdiheffobcpfmf";
}  // namespace

namespace extensions {

using SupervisionMixinSigninModeCallback =
    base::RepeatingCallback<supervised_user::SupervisionMixin::SignInMode()>;

// Tests interaction between supervised users and extensions.
class SupervisionExtensionTestBase
    : public InProcessBrowserTestMixinHostSupport<ExtensionBrowserTest>,
      public ::testing::WithParamInterface<SupervisionMixinSigninModeCallback> {
 public:
  SupervisionExtensionTestBase() = default;

 protected:
  bool IsDisabledForCustodianApproval(const std::string& extension_id) {
    ExtensionPrefs* extension_prefs = ExtensionPrefs::Get(profile());
    return extension_prefs->HasDisableReason(
        extension_id,
        extensions::disable_reason::DISABLE_CUSTODIAN_APPROVAL_REQUIRED);
  }

  void InstallExtensionAndCheckStatus(
      bool should_be_loaded,
      bool should_be_enabled,
      const std::string& extension_id = kGoodCrxId,
      const std::string& extension_crx = "good.crx") {
    base::FilePath path = test_data_dir_.AppendASCII(extension_crx);
    EXPECT_EQ(LoadExtension(path) != nullptr, should_be_loaded);
    const Extension* extension =
        extension_registry()->GetInstalledExtension(extension_id);
    EXPECT_TRUE(extension);

    EXPECT_EQ(
        extension_registry()->disabled_extensions().Contains(extension_id),
        !should_be_enabled);
    EXPECT_EQ(IsDisabledForCustodianApproval(extension_id), !should_be_enabled);
    EXPECT_EQ(extension_registry()->enabled_extensions().Contains(extension_id),
              should_be_enabled);
  }

  supervised_user::SupervisionMixin::SignInMode GetMixinSigninMode() {
    return GetParam().Run();
  }

 private:
  supervised_user::SupervisionMixin supervision_mixin_{
      mixin_host_,
      this,
      embedded_test_server(),
      {.sign_in_mode = GetMixinSigninMode()}};
};

// Tests interaction between supervised users and extensions after the optional
// supervision is removed from the account.
class SupervisionRemovalExtensionTest : public SupervisionExtensionTestBase {};

// Tests that removing supervision should also remove associated disable
// reasons, such as DISABLE_CUSTODIAN_APPROVAL_REQUIRED. Extensions should
// become enabled again after removing supervision. Prevents a regression to
// crbug.com/1045625.
IN_PROC_BROWSER_TEST_P(SupervisionRemovalExtensionTest,
                       PRE_RemoveCustodianApprovalRequirement) {
  ASSERT_TRUE(profile()->IsChild());

  base::FilePath path = test_data_dir_.AppendASCII("good.crx");
  bool extension_should_be_loaded = false;
  EXPECT_EQ(LoadExtension(path) != nullptr, extension_should_be_loaded);
  const Extension* extension =
      extension_registry()->GetInstalledExtension(kGoodCrxId);
  EXPECT_TRUE(extension);

  // This extension is a supervised user initiated install and should remain
  // disabled.
  EXPECT_TRUE(extension_registry()->disabled_extensions().Contains(kGoodCrxId));
  EXPECT_TRUE(IsDisabledForCustodianApproval(kGoodCrxId));
}

IN_PROC_BROWSER_TEST_P(SupervisionRemovalExtensionTest,
                       RemoveCustodianApprovalRequirement) {
  ASSERT_FALSE(profile()->IsChild());

  // The extension should still be installed since we are sharing the same data
  // directory as the PRE test.
  const Extension* extension =
      extension_registry()->GetInstalledExtension(kGoodCrxId);
  EXPECT_TRUE(extension);

  // The extension should be enabled now after removing supervision.
  EXPECT_TRUE(extension_registry()->enabled_extensions().Contains(kGoodCrxId));
  EXPECT_FALSE(
      extension_registry()->disabled_extensions().Contains(kGoodCrxId));

  EXPECT_FALSE(IsDisabledForCustodianApproval(kGoodCrxId));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    SupervisionRemovalExtensionTest,
    testing::Values(base::BindRepeating([]() {
      // The test covers the removal of supervision. Pre-test should start with
      // a supervised profile, main test with a regular profile.
      return content::IsPreTest()
                 ? supervised_user::SupervisionMixin::SignInMode::kSupervised
                 : supervised_user::SupervisionMixin::SignInMode::kRegular;
    })));

// Tests interaction between supervised users and extensions after the optional
// supervision is added to an the account.
class UserGellerizationExtensionTest : public SupervisionExtensionTestBase {
 public:
  UserGellerizationExtensionTest() = default;
};

IN_PROC_BROWSER_TEST_P(UserGellerizationExtensionTest,
                       PRE_UserGellerizationDisablesExistingExtensions) {
  ASSERT_FALSE(profile()->IsChild());
  base::FilePath path = test_data_dir_.AppendASCII("good.crx");
  EXPECT_TRUE(LoadExtension(path) != nullptr);
  const Extension* extension =
      extension_registry()->GetInstalledExtension(kGoodCrxId);
  EXPECT_TRUE(extension);

  // The extension is installed and enabled.
  EXPECT_TRUE(extension_registry()->enabled_extensions().Contains(kGoodCrxId));
}

IN_PROC_BROWSER_TEST_P(UserGellerizationExtensionTest,
                       UserGellerizationDisablesExistingExtensions) {
  ASSERT_TRUE(profile()->IsChild());

  // The extension should still be installed since we are sharing the same data
  // directory as the PRE test.
  const Extension* extension =
      extension_registry()->GetInstalledExtension(kGoodCrxId);
  EXPECT_TRUE(extension);

  // The extension should be disabled, pending parent approval.
  EXPECT_TRUE(extension_registry()->disabled_extensions().Contains(kGoodCrxId));
  EXPECT_TRUE(IsDisabledForCustodianApproval(kGoodCrxId));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    UserGellerizationExtensionTest,
    testing::Values(base::BindRepeating([]() {
      //  Pre-test should start with a regular
      //  profile, main test with a supervised profile.
      return content::IsPreTest()
                 ? supervised_user::SupervisionMixin::SignInMode::kRegular
                 : supervised_user::SupervisionMixin::SignInMode::kSupervised;
    })));

// Tests the parental controls applied on extensions for supervised users
// under different values of the Family Link Extensions Switch
// ("Allow to add extensions without asking for permission").
class ParentApprovalHandlingByExtensionSwitchTest
    : public SupervisionExtensionTestBase {
 public:
  ParentApprovalHandlingByExtensionSwitchTest() = default;
};

IN_PROC_BROWSER_TEST_P(ParentApprovalHandlingByExtensionSwitchTest,
                       GrantParentApprovalWhenExtensionSwitchBecomesEnabled) {
  ASSERT_TRUE(profile()->IsChild());

  // Set the Extensions preference to OFF, requiring parent approval for
  // installations.
  supervised_user_test_util::SetSkipParentApprovalToInstallExtensionsPref(
      profile(), false);
  bool should_be_loaded = false;
  bool should_be_enabled = false;
  InstallExtensionAndCheckStatus(should_be_loaded, should_be_enabled);

  // Flip the Extensions preference to ON.
  supervised_user_test_util::SetSkipParentApprovalToInstallExtensionsPref(
      profile(), true);

  // The extension should become enabled and parent-approved.
  EXPECT_TRUE(extension_registry()->enabled_extensions().Contains(kGoodCrxId));

  // Flip the Extensions preference to OFF.
  // The previously approved and enabled extensions should remain enabled.
  supervised_user_test_util::SetSkipParentApprovalToInstallExtensionsPref(
      profile(), false);
  EXPECT_TRUE(extension_registry()->enabled_extensions().Contains(kGoodCrxId));
}

IN_PROC_BROWSER_TEST_P(
    ParentApprovalHandlingByExtensionSwitchTest,
    GrantParentParentApprovalOnInstallationIfExtensionSwitchEnabled) {
  // Set the Extensions preference to ON.
  supervised_user_test_util::SetSkipParentApprovalToInstallExtensionsPref(
      profile(), true);
  bool should_be_loaded = true;
  bool should_be_enabled = should_be_loaded;
  InstallExtensionAndCheckStatus(should_be_loaded, should_be_enabled);

  // Flip the Extensions preference to OFF.
  // The previously approved and enabled extensions should remain enabled.
  supervised_user_test_util::SetSkipParentApprovalToInstallExtensionsPref(
      profile(), false);
  EXPECT_EQ(should_be_enabled,
            extension_registry()->enabled_extensions().Contains(kGoodCrxId));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    ParentApprovalHandlingByExtensionSwitchTest,
    testing::Values(base::BindRepeating([]() {
      return supervised_user::SupervisionMixin::SignInMode::kSupervised;
    })));

class ParentApprovalRequestTest
    : public SupervisionExtensionTestBase,
      public TestParentPermissionDialogViewObserver {
 public:
  ParentApprovalRequestTest() : TestParentPermissionDialogViewObserver(this) {}

  // TestParentPermissionDialogViewObserver implementation:
  void OnTestParentPermissionDialogViewCreated(
      ParentPermissionDialogView* view) override {
    parent_permission_dialog_appeared_ = true;
  }

  void ClearCustodianPrefs() {
    // Clears the preferences relating to both custodians.
    profile()->GetPrefs()->ClearPref(prefs::kSupervisedUserCustodianProfileURL);
    profile()->GetPrefs()->ClearPref(prefs::kSupervisedUserCustodianName);
    profile()->GetPrefs()->ClearPref(prefs::kSupervisedUserCustodianEmail);
    profile()->GetPrefs()->ClearPref(
        prefs::kSupervisedUserCustodianObfuscatedGaiaId);
    profile()->GetPrefs()->ClearPref(
        prefs::kSupervisedUserCustodianProfileImageURL);
    profile()->GetPrefs()->ClearPref(
        prefs::kSupervisedUserSecondCustodianProfileURL);
    profile()->GetPrefs()->ClearPref(prefs::kSupervisedUserSecondCustodianName);
    profile()->GetPrefs()->ClearPref(
        prefs::kSupervisedUserSecondCustodianEmail);
    profile()->GetPrefs()->ClearPref(
        prefs::kSupervisedUserSecondCustodianObfuscatedGaiaId);
    profile()->GetPrefs()->ClearPref(
        prefs::kSupervisedUserSecondCustodianProfileImageURL);
  }

  bool parent_permission_dialog_appeared_ = false;
};

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
// Tests that installation fails when the custodian info is missing and the
// right histograms are recorded. Regression test for crbug.com/35071637.
IN_PROC_BROWSER_TEST_P(ParentApprovalRequestTest,
                       RequestToInstallExtensionMissingCustodianInfo) {
  // Set the preferences to the default values from Family Link.
  supervised_user_test_util::SetSkipParentApprovalToInstallExtensionsPref(
      profile(), false);

  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder("An extension").Build();
  CHECK(extension);

  ClearCustodianPrefs();

  // Request Approval to add a new extension.
  base::HistogramTester histogram_tester;
  SkBitmap icon;
  auto supervised_user_extensions_delegate =
      std::make_unique<SupervisedUserExtensionsDelegateImpl>(profile());

  supervised_user_extensions_delegate->RequestToAddExtensionOrShowError(
      *extension.get(), browser()->tab_strip_model()->GetActiveWebContents(),
      gfx::ImageSkia::CreateFrom1xBitmap(icon), base::DoNothing());

  // The dialog should not have appeared.
  EXPECT_FALSE(parent_permission_dialog_appeared_);
  histogram_tester.ExpectBucketCount(SupervisedUserExtensionsMetricsRecorder::
                                         kParentPermissionDialogHistogramName,
                                     SupervisedUserExtensionsMetricsRecorder::
                                         ParentPermissionDialogState::kFailed,
                                     1);
  histogram_tester.ExpectBucketCount(
      SupervisedUserExtensionsMetricsRecorder::
          kParentPermissionDialogHistogramName,
      SupervisedUserExtensionsMetricsRecorder::ParentPermissionDialogState::
          kNoParentError,
      1);
}
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

// Tests that the method to request extension approval can be triggered
// without errors for new (uninstalled) extensions that already have been
// granted parent approval.
// Prevents regressions to b/321016032.
IN_PROC_BROWSER_TEST_P(ParentApprovalRequestTest,
                       RequestToInstallApprovedExtension) {
  // Create and extension and give it parent approval.
  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder("An extension").Build();
  CHECK(extension);

  base::Value::Dict approved_extensions;
  approved_extensions.Set(extension->id(), true);
  profile()->GetPrefs()->SetDict(prefs::kSupervisedUserApprovedExtensions,
                                 std::move(approved_extensions));
  ASSERT_FALSE(extension_registry()->GetInstalledExtension(extension->id()));

  // Request Approval to add a new extension,
  auto supervised_user_extensions_delegate =
      std::make_unique<SupervisedUserExtensionsDelegateImpl>(profile());

#if BUILDFLAG(IS_CHROMEOS)
  auto fake_parent_access_dialog_provider =
      std::make_unique<ash::FakeParentAccessDialogProvider>();
  auto fake_parent_access_dialog_provider_ptr =
      fake_parent_access_dialog_provider.get();
  supervised_user_extensions_delegate
      ->SetParentAccessExtensionApprovalsManagerForTesting(
          std::make_unique<ParentAccessExtensionApprovalsManager>(
              std::move(fake_parent_access_dialog_provider)));
  fake_parent_access_dialog_provider_ptr->SetNextAction(
      ash::FakeParentAccessDialogProvider::Action::CaptureCallback(
          base::DoNothing()));
#endif

  SkBitmap icon;
  supervised_user_extensions_delegate->RequestToAddExtensionOrShowError(
      *extension.get(), browser()->tab_strip_model()->GetActiveWebContents(),
      gfx::ImageSkia::CreateFrom1xBitmap(icon), base::DoNothing());

  // Confirm that the parent approval dialog for extensions for each OS is
  // created.
#if BUILDFLAG(IS_CHROMEOS)
  EXPECT_TRUE(fake_parent_access_dialog_provider_ptr->TakeLastParams());
#else
  EXPECT_TRUE(parent_permission_dialog_appeared_);
#endif
}

INSTANTIATE_TEST_SUITE_P(
    All,
    ParentApprovalRequestTest,
    testing::Values(base::BindRepeating([]() {
      return supervised_user::SupervisionMixin::SignInMode::kSupervised;
    })));

}  // namespace extensions
