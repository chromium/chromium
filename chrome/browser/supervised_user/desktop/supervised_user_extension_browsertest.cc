// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/test/gtest_util.h"
#include "base/test/scoped_feature_list.h"
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
#include "components/supervised_user/core/common/features.h"
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
#include "chromeos/crosapi/mojom/parent_access.mojom.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace {
constexpr char kGoodCrxId[] = "ldnnhddmnhbkjipkidpdiheffobcpfmf";

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
constexpr char kSimpleWithIconCrxId[] = "dehdlahnlebladnfleagmjdapdjdcnlp";
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
}  // namespace

namespace extensions {

enum class ExtensionsParentalControlState : int { kEnabled = 0, kDisabled };

enum class ExtensionManagementSwitch : int {
  kManagedByExtensions = 0,
  kManagedByPermissions = 1
};

std::string CreateTestSuffixFromParam(const auto& info) {
  return std::string(std::get<0>(info.param) ==
                             ExtensionsParentalControlState::kEnabled
                         ? "WithParentalControlsOnExtensions"
                         : "WithoutParentalControlsOnExtensions") +
         std::string(std::get<1>(info.param) ==
                             ExtensionManagementSwitch::kManagedByExtensions
                         ? "ManagedByExtensions"
                         : "ManagedByPermissions");
}

using SupervisionMixinSigninModeCallback =
    base::RepeatingCallback<supervised_user::SupervisionMixin::SignInMode()>;

// Tests interaction between supervised users and extensions.
class SupervisionExtensionTestBase
    : public ExtensionBrowserTest,
      public ::testing::WithParamInterface<
          std::tuple<ExtensionsParentalControlState,
                     ExtensionManagementSwitch,
                     SupervisionMixinSigninModeCallback>> {
 public:
  SupervisionExtensionTestBase() {
    std::vector<base::test::FeatureRef> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;

    if (ApplyParentalControlsOnExtensions()) {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
      enabled_features.push_back(
          supervised_user::
              kEnableExtensionsPermissionsForSupervisedUsersOnDesktop);
#endif // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
      if (GetExtensionManagementSwitch() ==
          ExtensionManagementSwitch::kManagedByExtensions) {
        // Managed by preference `SkipParentApprovalToInstallExtensions` (new
        // flow).
        enabled_features.push_back(
            supervised_user::
                kEnableSupervisedUserSkipParentApprovalToInstallExtensions);
      } else {
        disabled_features.push_back(
            supervised_user::
                kEnableSupervisedUserSkipParentApprovalToInstallExtensions);
      }
    } else {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
      disabled_features.push_back(
          supervised_user::
              kEnableExtensionsPermissionsForSupervisedUsersOnDesktop);
#else
    // For ChromeOS, the parental controls should always apply to extensions
    // and this case should not be reached. See the instantiation of the test suite.
    NOTREACHED();
#endif // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
    }
    scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }

  ~SupervisionExtensionTestBase() override { scoped_feature_list_.Reset(); }

  // We have to essentially replicate what MixinBasedInProcessBrowserTest does
  // here because ExtensionBrowserTest doesn't inherit from that class.
  void SetUp() override {
    mixin_host_.SetUp();
    ExtensionBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    mixin_host_.SetUpCommandLine(command_line);
    ExtensionBrowserTest::SetUpCommandLine(command_line);
  }

  void SetUpDefaultCommandLine(base::CommandLine* command_line) override {
    mixin_host_.SetUpDefaultCommandLine(command_line);
    ExtensionBrowserTest::SetUpDefaultCommandLine(command_line);
  }

  bool SetUpUserDataDirectory() override {
    return mixin_host_.SetUpUserDataDirectory() &&
           ExtensionBrowserTest::SetUpUserDataDirectory();
  }

  void SetUpInProcessBrowserTestFixture() override {
    mixin_host_.SetUpInProcessBrowserTestFixture();
    ExtensionBrowserTest::SetUpInProcessBrowserTestFixture();
  }

  void CreatedBrowserMainParts(
      content::BrowserMainParts* browser_main_parts) override {
    mixin_host_.CreatedBrowserMainParts(browser_main_parts);
    ExtensionBrowserTest::CreatedBrowserMainParts(browser_main_parts);
  }

  void SetUpOnMainThread() override {
    mixin_host_.SetUpOnMainThread();
    ExtensionBrowserTest::SetUpOnMainThread();
  }

  void TearDownOnMainThread() override {
    mixin_host_.TearDownOnMainThread();
    ExtensionBrowserTest::TearDownOnMainThread();
  }

  void TearDownInProcessBrowserTestFixture() override {
    mixin_host_.TearDownInProcessBrowserTestFixture();
    ExtensionBrowserTest::TearDownInProcessBrowserTestFixture();
  }

  void TearDown() override {
    mixin_host_.TearDown();
    ExtensionBrowserTest::TearDown();
  }

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

  bool ApplyParentalControlsOnExtensions() {
    return std::get<0>(GetParam()) == ExtensionsParentalControlState::kEnabled;
  }

  ExtensionManagementSwitch GetExtensionManagementSwitch() {
    return std::get<1>(GetParam());
  }

  supervised_user::SupervisionMixin::SignInMode GetMixinSigninMode() {
    return std::get<2>(GetParam()).Run();
  }

  InProcessBrowserTestMixinHost mixin_host_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  supervised_user::SupervisionMixin supervision_mixin_{
      mixin_host_,
      this,
      embedded_test_server(),
      {.sign_in_mode = GetMixinSigninMode()}};
};

// Tests interaction between supervised users and extensions after the optional
// supervision is removed from the account.
class SupervisionRemovalExtensionTest : public SupervisionExtensionTestBase {
 public:
  SupervisionRemovalExtensionTest() = default;
};

// If extension restrictions apply to supervised users, removing supervision
// should also remove associated disable reasons, such as
// DISABLE_CUSTODIAN_APPROVAL_REQUIRED. Extensions should become enabled again
// after removing supervision. Prevents a regression to crbug/1045625.
// If extension restrictions are disabled, removing supervision leaves the
// extension unchanged and enabled.
IN_PROC_BROWSER_TEST_P(SupervisionRemovalExtensionTest,
                       PRE_RemoveCustodianApprovalRequirement) {
  ASSERT_TRUE(profile()->IsChild());
  // Set the preference that manages the extensions to the value,
  // that allows installations (pending approval), if extensions are subject
  // to parental controls.
  if (ApplyParentalControlsOnExtensions()) {
    if (GetExtensionManagementSwitch() ==
        ExtensionManagementSwitch::kManagedByExtensions) {
      // Note: Setting to true would have the same effect as the extension
      // will be installed disabled (pending approval) by `LoadExtension`,
      // as `LoadExtension` does not reach the point where we grant the parent
      // approval on installation success in this mode (in WebstorePrivateApi).
      supervised_user_test_util::SetSkipParentApprovalToInstallExtensionsPref(
          profile(), false);
    } else {
      supervised_user_test_util::
          SetSupervisedUserExtensionsMayRequestPermissionsPref(profile(), true);
    }
  }

  base::FilePath path = test_data_dir_.AppendASCII("good.crx");
  bool extension_should_be_loaded = !ApplyParentalControlsOnExtensions();
  EXPECT_EQ(LoadExtension(path) != nullptr, extension_should_be_loaded);
  const Extension* extension =
      extension_registry()->GetInstalledExtension(kGoodCrxId);
  EXPECT_TRUE(extension);

  if (ApplyParentalControlsOnExtensions()) {
    // This extension is a supervised user initiated install and should remain
    // disabled.
    EXPECT_TRUE(
        extension_registry()->disabled_extensions().Contains(kGoodCrxId));
    EXPECT_TRUE(IsDisabledForCustodianApproval(kGoodCrxId));
  } else {
    // When extension permissions are disabled, the extension is installed and
    // enabled.
    EXPECT_TRUE(
        extension_registry()->enabled_extensions().Contains(kGoodCrxId));
  }
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
    testing::Combine(
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
        testing::Values(ExtensionsParentalControlState::kDisabled,
                        ExtensionsParentalControlState::kEnabled),
#else
        // Extensions parental controls always enabled on ChromeOS.
        testing::Values(ExtensionsParentalControlState::kEnabled),
#endif // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
        testing::Values(ExtensionManagementSwitch::kManagedByExtensions,
                        ExtensionManagementSwitch::kManagedByPermissions),
        testing::Values(base::BindRepeating([]() {
          // The test covers the removal of supervision. Pre-test should start with a
          // supervised profile, main test with a regular profile.
          return content::IsPreTest()
                     ? supervised_user::SupervisionMixin::SignInMode::
                           kSupervised
                     : supervised_user::SupervisionMixin::SignInMode::kRegular;
        }))),
    [](const auto& info) { return CreateTestSuffixFromParam(info); });

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
  EXPECT_EQ(ApplyParentalControlsOnExtensions(),
            extension_registry()->disabled_extensions().Contains(kGoodCrxId));
  EXPECT_EQ(ApplyParentalControlsOnExtensions(),
            IsDisabledForCustodianApproval(kGoodCrxId));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    UserGellerizationExtensionTest,
    testing::Combine(
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
        testing::Values(ExtensionsParentalControlState::kDisabled,
                        ExtensionsParentalControlState::kEnabled),
#else
        // Extensions parental controls always enabled on ChromeOS.
        testing::Values(ExtensionsParentalControlState::kEnabled),
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
        testing::Values(ExtensionManagementSwitch::kManagedByExtensions,
                        ExtensionManagementSwitch::kManagedByPermissions),
        testing::Values(base::BindRepeating([]() {
          //  Pre-test should start with a regular
          //  profile, main test with a supervised profile.
          return content::IsPreTest()
                     ? supervised_user::SupervisionMixin::SignInMode::kRegular
                     : supervised_user::SupervisionMixin::SignInMode::
                           kSupervised;
        }))),
    [](const auto& info) { return CreateTestSuffixFromParam(info); });

// Tests the parental controls applied on extensions for supervised users
// under different values of the Family Link Extensions Switch
// ("Allow to add extensions without asking for permission").
class ParentApprovalHandlingByExtensionSwitchTest
    : public SupervisionExtensionTestBase {
 public:
  ParentApprovalHandlingByExtensionSwitchTest() = default;
};

IN_PROC_BROWSER_TEST_P(
    ParentApprovalHandlingByExtensionSwitchTest,
    PRE_GrantParentApprovalWhenExtensionSwitchBecomesEnabled) {
  ASSERT_TRUE(profile()->IsChild());

  // Set the Extensions preference to OFF, requiring parent approval for
  // installations.
  supervised_user_test_util::SetSkipParentApprovalToInstallExtensionsPref(
      profile(), false);
  // If parental controls apply the extensions should be disabled, pending
  // approval.
  bool should_be_loaded = !ApplyParentalControlsOnExtensions();
  bool should_be_enabled = !ApplyParentalControlsOnExtensions();
  InstallExtensionAndCheckStatus(should_be_loaded, should_be_enabled);
}

IN_PROC_BROWSER_TEST_P(ParentApprovalHandlingByExtensionSwitchTest,
                       GrantParentApprovalWhenExtensionSwitchBecomesEnabled) {
  ASSERT_TRUE(profile()->IsChild());

  // Flip the Extensions preference to ON.
  supervised_user_test_util::SetSkipParentApprovalToInstallExtensionsPref(
      profile(), true);

  // If extensions are managed by the Extensions
  // Family Link switch, the extension should become enabled and
  // parent-approved.
  bool should_be_enabled = !ApplyParentalControlsOnExtensions() ||
                           GetExtensionManagementSwitch() ==
                               ExtensionManagementSwitch::kManagedByExtensions;
  EXPECT_EQ(should_be_enabled,
            extension_registry()->enabled_extensions().Contains(kGoodCrxId));

  // Flip the Extensions preference to OFF.
  // The previously approved and enabled extensions should remain enabled.
  supervised_user_test_util::SetSkipParentApprovalToInstallExtensionsPref(
      profile(), false);
  EXPECT_EQ(should_be_enabled,
            extension_registry()->enabled_extensions().Contains(kGoodCrxId));
}

IN_PROC_BROWSER_TEST_P(
    ParentApprovalHandlingByExtensionSwitchTest,
    GrantParentParentApprovalOnInstallationIfExtensionSwitchEnabled) {
  // Set the Extensions preference to ON.
  supervised_user_test_util::SetSkipParentApprovalToInstallExtensionsPref(
      profile(), true);

  // If parental controls apply and the extensions are managed by the Permission
  // switch, they should be disabled, pending approval. IF not parental controls
  // apply, or the extensions are managed by the Extensions switch, it will be
  // installed enabled with parent approval.
  bool should_be_loaded = !ApplyParentalControlsOnExtensions() ||
                          GetExtensionManagementSwitch() ==
                              ExtensionManagementSwitch::kManagedByExtensions;
  bool should_be_enabled = should_be_loaded;
  InstallExtensionAndCheckStatus(should_be_loaded, should_be_enabled);

  // Flip the Extensions preference to OFF.
  // The previously approved and enabled extensions should remain enabled.
  supervised_user_test_util::SetSkipParentApprovalToInstallExtensionsPref(
      profile(), false);
  EXPECT_EQ(should_be_enabled,
            extension_registry()->enabled_extensions().Contains(kGoodCrxId));
}

// TODO(b/321240025): Add test case on permission increase.
// TODO(b/321240025): Add test case on an extension trying to change settings of
// a website.

INSTANTIATE_TEST_SUITE_P(
    All,
    ParentApprovalHandlingByExtensionSwitchTest,
    testing::Combine(
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
        testing::Values(ExtensionsParentalControlState::kDisabled,
                        ExtensionsParentalControlState::kEnabled),
#else
        // Extensions parental controls always enabled on ChromeOS.
        testing::Values(ExtensionsParentalControlState::kEnabled),
#endif // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

        testing::Values(ExtensionManagementSwitch::kManagedByExtensions,
                        ExtensionManagementSwitch::kManagedByPermissions),
        testing::Values(base::BindRepeating([]() {
          return supervised_user::SupervisionMixin::SignInMode::kSupervised;
        }))),
    [](const auto& info) { return CreateTestSuffixFromParam(info); });

class ParentApprovalRequestTest
    : public SupervisionExtensionTestBase,
#if BUILDFLAG(IS_CHROMEOS)
      public TestExtensionApprovalsManagerObserver,
#endif
      public TestParentPermissionDialogViewObserver {
 public:
  ParentApprovalRequestTest()
      :
#if BUILDFLAG(IS_CHROMEOS)
        TestExtensionApprovalsManagerObserver(this),
#endif
        TestParentPermissionDialogViewObserver(this) {
  }

#if BUILDFLAG(IS_CHROMEOS)
  // TestExtensionApprovalsManagerObserver implementation:
  void OnTestParentAccessDialogCreated() override {
    parent_permission_dialog_appeared_ = true;
    SetParentAccessDialogResult(crosapi::mojom::ParentAccessResult::NewCanceled(
        crosapi::mojom::ParentAccessCanceledResult::New()));
  }
#endif

  // TestParentPermissionDialogViewObserver implementation:
  void OnTestParentPermissionDialogViewCreated(
      ParentPermissionDialogView* view) override {
    parent_permission_dialog_appeared_ = true;
  }

  bool parent_permission_dialog_appeared_ = false;
};

// Tests that the method to request extension approval can be triggered
// without errors for new (uninstalled) extensions that already have been
// granted parent approval.
// Prevents regressions to b/321016032.
IN_PROC_BROWSER_TEST_P(ParentApprovalRequestTest,
                       RequestToInstallApprovedExtension) {
  base::HistogramTester histogram_tester;

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
  SkBitmap icon;
  auto supervised_user_extensions_delegate =
      std::make_unique<SupervisedUserExtensionsDelegateImpl>(profile());
  supervised_user_extensions_delegate->RequestToAddExtensionOrShowError(
      *extension.get(), browser()->tab_strip_model()->GetActiveWebContents(),
      gfx::ImageSkia::CreateFrom1xBitmap(icon),
      SupervisedUserExtensionParentApprovalEntryPoint::kOnWebstoreInstallation,
      base::DoNothing());
  // Confirm that the parent approval dialog for extensions for each OS is
  // created.
  EXPECT_TRUE(parent_permission_dialog_appeared_);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    ParentApprovalRequestTest,
    testing::Combine(
        testing::Values(ExtensionsParentalControlState::kEnabled),
        testing::Values(ExtensionManagementSwitch::kManagedByExtensions,
                        ExtensionManagementSwitch::kManagedByPermissions),
        testing::Values(base::BindRepeating([]() {
          return supervised_user::SupervisionMixin::SignInMode::kSupervised;
        }))),
    [](const auto& info) { return CreateTestSuffixFromParam(info); });

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
// Tests the behavior of existing and new extensions for a supervised user
// on the release of the `SkipParentalApproval` feature.
class SupervisedUserSkipParentalApprovalModeReleaseTest
    : public SupervisionExtensionTestBase {
 public:
  SupervisedUserSkipParentalApprovalModeReleaseTest() {
    // Over-writes any feature enabling of the parent class.
    std::vector<base::test::FeatureRef> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;

    if (content::IsPreTest()) {
      // Start with inactive features on Pre-test.
      disabled_features.push_back(
          supervised_user::
              kEnableExtensionsPermissionsForSupervisedUsersOnDesktop);
      disabled_features.push_back(
          supervised_user::
              kEnableSupervisedUserSkipParentApprovalToInstallExtensions);
    } else {
      // Simulate feature release on Main test.
      enabled_features.push_back(
          supervised_user::
              kEnableExtensionsPermissionsForSupervisedUsersOnDesktop);
      enabled_features.push_back(
          supervised_user::
              kEnableSupervisedUserSkipParentApprovalToInstallExtensions);
    }
    scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }

  ~SupervisedUserSkipParentalApprovalModeReleaseTest() override {
    scoped_feature_list_.Reset();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(SupervisedUserSkipParentalApprovalModeReleaseTest,
                       PRE_OnFeatureReleaseForSupervisedUserWithExtensions) {
  ASSERT_TRUE(profile()->IsChild());
  // Before the release of features `SkipParentalApprovalToInstallExtensions`
  // and `EnableExtensionsPermissionsForSupervisedUsersOnDesktop` no parental
  // controls apply.
  InstallExtensionAndCheckStatus(/*should_be_loaded=*/true,
                                 /*should_be_enabled=*/true);
}

IN_PROC_BROWSER_TEST_P(SupervisedUserSkipParentalApprovalModeReleaseTest,
                       OnFeatureReleaseForSupervisedUserWithExtensions) {
  ASSERT_TRUE(profile()->IsChild());
  // On feature release the extensions are enabled (due to the local parent
  // approval migration).
  EXPECT_TRUE(extension_registry()->enabled_extensions().Contains(kGoodCrxId));

  // Extensions installed after the feature release (i.e. local installation,
  // synced extensions) are disabled and pending approval.
  InstallExtensionAndCheckStatus(/*should_be_loaded=*/false,
                                 /*should_be_enabled=*/false,
                                 kSimpleWithIconCrxId, "simple_with_icon.crx");
}

INSTANTIATE_TEST_SUITE_P(
    All,
    SupervisedUserSkipParentalApprovalModeReleaseTest,
    testing::Combine(
        testing::Values(ExtensionsParentalControlState::kEnabled),
        testing::Values(ExtensionManagementSwitch::kManagedByExtensions),
        testing::Values(base::BindRepeating([]() {
          return supervised_user::SupervisionMixin::SignInMode::kSupervised;
        }))),
    [](const auto& info) { return CreateTestSuffixFromParam(info); });

#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

}  // namespace extensions
