// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>
#include <vector>

#include "base/auto_reset.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/api/webstore_private/webstore_private_api.h"
#include "chrome/browser/extensions/extension_allowlist.h"
#include "chrome/browser/extensions/extension_install_prompt.h"
#include "chrome/browser/extensions/mixin_based_extension_apitest.h"
#include "chrome/browser/extensions/webstore_installer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ssl/https_upgrades_util.h"
#include "chrome/browser/ui/extensions/extension_install_ui.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_test_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/platform_browser_test.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/supervised_user/core/common/features.h"
#include "components/supervised_user/core/common/pref_names.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"
#include "content/public/browser/gpu_data_manager.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/allowlist_state.h"
#include "extensions/browser/api/management/management_api.h"
#include "extensions/browser/api_test_utils.h"
#include "extensions/browser/extension_dialog_auto_confirm.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/install_approval.h"
#include "extensions/browser/pref_names.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/switches.h"
#include "gpu/config/gpu_feature_type.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/gl/gl_switches.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/supervised_user/chromeos/parent_access_extension_approvals_manager.h"
#include "chrome/browser/ui/webui/ash/parent_access/fake_parent_access_dialog.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_ANDROID)
#include "extensions/browser/supervised_user_extensions_delegate.h"
#endif  // BUILDFLAG(IS_ANDROID)

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/browser_process.h"
#include "chrome/browser/supervised_user/supervised_user_extensions_delegate_impl.h"
#include "chrome/browser/supervised_user/supervised_user_test_util.h"  // nogncheck
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/supervised_user/parent_permission_dialog_view.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chrome/test/supervised_user/supervision_mixin.h"
#include "components/enterprise/browser/promotion/promotion_eligibility_checker.h"
#include "components/enterprise/browser/promotion/promotion_prefs.h"
#include "components/enterprise/promotion_types.h"
#include "components/policy/core/common/cloud/cloud_policy_manager.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#endif  // !BUILDFLAG(IS_ANDROID)

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {

namespace utils = api_test_utils;

namespace {

constexpr char kExtensionId[] = "enfkhcelefdadlmkffamgdlgplcionje";

class WebstoreInstallListener : public WebstorePrivateApi::Delegate {
 public:
  WebstoreInstallListener() = default;

  void OnExtensionInstallSuccess(const std::string& id) override {
    received_success_ = true;
    id_ = id;

    if (waiting_) {
      waiting_ = false;
      loop_.QuitWhenIdle();
    }
  }

  void OnExtensionInstallFailure(
      const std::string& id,
      const std::string& error,
      WebstoreInstaller::FailureReason reason) override {
    received_failure_ = true;
    id_ = id;
    error_ = error;
    last_failure_reason_ = reason;

    if (waiting_) {
      waiting_ = false;
      loop_.QuitWhenIdle();
    }
  }

  void Wait() {
    if (received_success_ || received_failure_) {
      return;
    }

    waiting_ = true;
    loop_.Run();
  }
  bool received_success() const { return received_success_; }
  bool received_failure() const { return received_failure_; }
  const std::string& id() const { return id_; }
  WebstoreInstaller::FailureReason last_failure_reason() {
    return last_failure_reason_;
  }

 private:
  bool received_failure_ = false;
  bool received_success_ = false;
  bool waiting_ = false;
  WebstoreInstaller::FailureReason last_failure_reason_;
  std::string id_;
  std::string error_;
  base::RunLoop loop_;
};

#if !BUILDFLAG(IS_ANDROID)
class FakePromotionEligibilityChecker
    : public enterprise_promotion::PromotionEligibilityChecker {
 public:
  explicit FakePromotionEligibilityChecker(
      enterprise_management::GetUserEligiblePromotionsResponse response)
      : enterprise_promotion::PromotionEligibilityChecker("",
                                                          nullptr,
                                                          nullptr,
                                                          "",
                                                          false),
        response_(std::move(response)) {}

  // The only logic: immediately run the callback with our stored response.
  void MaybeCheckPromotionEligibility(
      PromotionEligibilityCallback callback) override {
    std::move(callback).Run(response_);
  }

 private:
  enterprise_management::GetUserEligiblePromotionsResponse response_;
};

class FailIfCalledPromotionEligibilityChecker
    : public enterprise_promotion::PromotionEligibilityChecker {
 public:
  FailIfCalledPromotionEligibilityChecker()
      : enterprise_promotion::PromotionEligibilityChecker("",
                                                          nullptr,
                                                          nullptr,
                                                          "",
                                                          false) {}

  void MaybeCheckPromotionEligibility(
      PromotionEligibilityCallback callback) override {
    ADD_FAILURE() << "Network check should not be called when cache is valid.";
  }
};
#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace

// A base class for tests below.
class ExtensionWebstorePrivateApiTest : public MixinBasedExtensionApiTest {
 public:
  ExtensionWebstorePrivateApiTest() = default;

  ExtensionWebstorePrivateApiTest(const ExtensionWebstorePrivateApiTest&) =
      delete;
  ExtensionWebstorePrivateApiTest& operator=(
      const ExtensionWebstorePrivateApiTest&) = delete;

  ~ExtensionWebstorePrivateApiTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    MixinBasedExtensionApiTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(switches::kAppsGalleryURL,
                                    "http://www.example.com/");
    command_line->AppendSwitch(switches::kExtensionTestApiOnWebPages);
  }

  void SetUpOnMainThread() override {
    MixinBasedExtensionApiTest::SetUpOnMainThread();

    // Start up the test server and get us ready for calling the install
    // API functions.
    host_resolver()->AddRule("www.example.com", "127.0.0.1");
    ASSERT_TRUE(StartEmbeddedTestServer());
    base::AutoReset<bool> disable_ui =
        ExtensionInstallUI::disable_ui_for_tests(true);

    auto_confirm_install_ = std::make_unique<ScopedTestDialogAutoConfirm>(
        ScopedTestDialogAutoConfirm::ACCEPT);

    ASSERT_TRUE(webstore_install_dir_.CreateUniqueTempDir());
    webstore_install_dir_copy_ = webstore_install_dir_.GetPath();
    WebstoreInstaller::SetDownloadDirectoryForTests(
        &webstore_install_dir_copy_);
  }

 protected:
  virtual GURL GetTestServerURL(const std::string& path) {
    return embedded_test_server()->GetURL(
        "www.example.com",
        std::string("/extensions/api_test/webstore_private/") + path);
  }

  // Navigates to |page| and runs the Extension API test there. Any downloads
  // of extensions will return the contents of |crx_file|.
  bool RunInstallTest(const std::string& page, const std::string& crx_file) {
    const GURL crx_url = GetTestServerURL(crx_file);
    extension_test_util::SetGalleryUpdateURL(crx_url);

    GURL page_url = GetTestServerURL(page);
    return OpenTestURL(page_url);
  }

 private:
  base::ScopedTempDir webstore_install_dir_;
  // WebstoreInstaller needs a reference to a FilePath when setting the download
  // directory for testing.
  base::FilePath webstore_install_dir_copy_;

  std::unique_ptr<ScopedTestDialogAutoConfirm> auto_confirm_install_;
};

// Test cases where the user accepts the install confirmation dialog.
IN_PROC_BROWSER_TEST_F(ExtensionWebstorePrivateApiTest, InstallAccepted) {
  ASSERT_TRUE(RunInstallTest("accepted.html", "extension.crx"));
}

// Test having the default download directory missing.
IN_PROC_BROWSER_TEST_F(ExtensionWebstorePrivateApiTest, MissingDownloadDir) {
  // Set a non-existent directory as the download path.
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir temp_dir;
  EXPECT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath missing_directory = temp_dir.Take();
  EXPECT_TRUE(base::DeletePathRecursively(missing_directory));
  WebstoreInstaller::SetDownloadDirectoryForTests(&missing_directory);

  // Now run the install test, which should succeed.
  ASSERT_TRUE(RunInstallTest("accepted.html", "extension.crx"));

  // Cleanup.
  if (base::DirectoryExists(missing_directory)) {
    EXPECT_TRUE(base::DeletePathRecursively(missing_directory));
  }
}

// Tests passing a localized name.
IN_PROC_BROWSER_TEST_F(ExtensionWebstorePrivateApiTest, InstallLocalized) {
  ASSERT_TRUE(RunInstallTest("localized.html", "localized_extension.crx"));
}

// Now test the case where the user cancels the confirmation dialog.
IN_PROC_BROWSER_TEST_F(ExtensionWebstorePrivateApiTest, InstallCancelled) {
  ScopedTestDialogAutoConfirm auto_cancel(ScopedTestDialogAutoConfirm::CANCEL);
  ASSERT_TRUE(RunInstallTest("cancelled.html", "extension.crx"));
}

IN_PROC_BROWSER_TEST_F(ExtensionWebstorePrivateApiTest, IncorrectManifest1) {
  ASSERT_TRUE(RunInstallTest("incorrect_manifest1.html", "extension.crx"));
}

IN_PROC_BROWSER_TEST_F(ExtensionWebstorePrivateApiTest, IncorrectManifest2) {
  ASSERT_TRUE(RunInstallTest("incorrect_manifest2.html", "extension.crx"));
}

// Tests that we can request an app installed bubble (instead of the default
// UI when an app is installed).
IN_PROC_BROWSER_TEST_F(ExtensionWebstorePrivateApiTest, AppInstallBubble) {
  WebstoreInstallListener listener;
  auto delegate_reset = WebstorePrivateApi::SetDelegateForTesting(&listener);
  ASSERT_TRUE(RunInstallTest("app_install_bubble.html", "app.crx"));
  listener.Wait();
  ASSERT_TRUE(listener.received_success());
  ASSERT_EQ("iladmdjkfniedhfhcfoefgojhgaiaccc", listener.id());
}

IN_PROC_BROWSER_TEST_F(ExtensionWebstorePrivateApiTest, IsInIncognitoMode) {
  // TODO(crbug.com/40937027): Convert test to use HTTPS and then remove.
  ScopedAllowHttpForHostnamesForTesting allow_http({"www.example.com"},
                                                   profile()->GetPrefs());

  GURL page_url = GetTestServerURL("incognito.html");
  ASSERT_TRUE(OpenTestURL(page_url, /*open_in_incognito=*/true));
}

IN_PROC_BROWSER_TEST_F(ExtensionWebstorePrivateApiTest, IsNotInIncognitoMode) {
  GURL page_url = GetTestServerURL("not_incognito.html");
  ASSERT_TRUE(OpenTestURL(page_url));
}

// Tests using the iconUrl parameter to the install function.
IN_PROC_BROWSER_TEST_F(ExtensionWebstorePrivateApiTest, IconUrl) {
  ASSERT_TRUE(RunInstallTest("icon_url.html", "extension.crx"));
}

// Tests that the Approvals are properly created in beginInstall.
IN_PROC_BROWSER_TEST_F(ExtensionWebstorePrivateApiTest, BeginInstall) {
  std::string appId = "iladmdjkfniedhfhcfoefgojhgaiaccc";
  ASSERT_TRUE(RunInstallTest("begin_install.html", "extension.crx"));

  std::unique_ptr<InstallApproval> approval =
      WebstorePrivateApi::PopApprovalForTesting(profile(), appId);
  EXPECT_EQ(appId, approval->extension_id);
  EXPECT_TRUE(approval->use_app_installed_bubble);
  EXPECT_FALSE(approval->skip_post_install_ui);
  EXPECT_EQ("2", approval->authuser);
  EXPECT_EQ(profile(), Profile::FromBrowserContext(approval->browser_context));

  approval = WebstorePrivateApi::PopApprovalForTesting(profile(), kExtensionId);
  EXPECT_EQ(kExtensionId, approval->extension_id);
  EXPECT_FALSE(approval->use_app_installed_bubble);
  EXPECT_FALSE(approval->skip_post_install_ui);
  EXPECT_TRUE(approval->authuser.empty());
  EXPECT_EQ(profile(), Profile::FromBrowserContext(approval->browser_context));
}

// Tests that themes are installed without an install prompt.
IN_PROC_BROWSER_TEST_F(ExtensionWebstorePrivateApiTest, InstallTheme) {
  WebstoreInstallListener listener;
  auto delegate_reset = WebstorePrivateApi::SetDelegateForTesting(&listener);
  ASSERT_TRUE(RunInstallTest("theme.html", "../../theme.crx"));
  listener.Wait();
  ASSERT_TRUE(listener.received_success());
  ASSERT_EQ("idlfhncioikpdnlhnmcjogambnefbbfp", listener.id());
}

// Tests that an error is properly reported when an empty crx is returned.
IN_PROC_BROWSER_TEST_F(ExtensionWebstorePrivateApiTest, EmptyCrx) {
  ASSERT_TRUE(RunInstallTest("empty.html", "empty.crx"));
}

static constexpr char kTestAppId[] = "iladmdjkfniedhfhcfoefgojhgaiaccc";

#if !BUILDFLAG(IS_ANDROID)

static constexpr char kTestAppVersion[] = "0.1";

// Test fixture for various cases of installation for child accounts.
class SupervisedUserExtensionWebstorePrivateApiTest
    : public ExtensionWebstorePrivateApiTest,
      public TestParentPermissionDialogViewObserver {
 public:
  // The next dialog action to take.
  enum class NextDialogAction {
    kCancel,
    kAccept,
  };

  SupervisedUserExtensionWebstorePrivateApiTest()
      : TestParentPermissionDialogViewObserver(this),
        embedded_test_server_(std::make_unique<net::EmbeddedTestServer>()),
        supervision_mixin_(
            mixin_host_,
            this,
            embedded_test_server_.get(),
            {
                .consent_level = signin::ConsentLevel::kSignin,
                .sign_in_mode =
                    supervised_user::SupervisionMixin::SignInMode::kSupervised,
            }) {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ExtensionWebstorePrivateApiTest::SetUpCommandLine(command_line);
    // Shortens the merge session timeout from 20 to 1 seconds to speed up the
    // test by about 19 seconds.
    // TODO (crbug.com/41477104): figure out why this switch speeds up the test,
    // and fix the test setup so this is not required.
#if BUILDFLAG(IS_CHROMEOS)
    command_line->AppendSwitch(::switches::kShortMergeSessionTimeoutForTest);
#endif
  }

  void SetUpOnMainThread() override {
    ExtensionWebstorePrivateApiTest::SetUpOnMainThread();

    extensions_delegate_ = static_cast<SupervisedUserExtensionsDelegateImpl*>(
        BrowserContextKeyedAPIFactory<ManagementAPI>::GetIfExists(profile())
            ->GetSupervisedUserExtensionsDelegate());

#if BUILDFLAG(IS_CHROMEOS)
    auto dialog_provider =
        std::make_unique<ash::FakeParentAccessDialogProvider>();
    fake_parent_access_dialog_provider_ = dialog_provider.get();
    extensions_delegate_->SetParentAccessExtensionApprovalsManagerForTesting(
        std::make_unique<extensions::ParentAccessExtensionApprovalsManager>(
            std::move(dialog_provider)));
#endif

    supervised_user_test_util::
        SetSupervisedUserExtensionsMayRequestPermissionsPref(profile(), true);

    parent_permission_dialog_appeared_ = false;
  }

  void TearDownOnMainThread() override {
#if BUILDFLAG(IS_CHROMEOS)
    fake_parent_access_dialog_provider_ = nullptr;
#endif
    extensions_delegate_ = nullptr;
    ExtensionWebstorePrivateApiTest::TearDownOnMainThread();
  }

  void SetNextReAuthStatus(
      const GaiaAuthConsumer::ReAuthProofTokenStatus next_status) {
    supervision_mixin_.SetNextReAuthStatus(next_status);
  }

  // TestParentPermissionDialogViewObserver override:
  void OnTestParentPermissionDialogViewCreated(
      ParentPermissionDialogView* view) override {
    view->SetRepromptAfterIncorrectCredential(false);
    view->SetIdentityManagerForTesting(
        supervision_mixin_.GetIdentityTestEnvironment()->identity_manager());
    parent_permission_dialog_appeared_ = true;
    // Everything is set up, so take the next action.
    if (next_dialog_action_) {
      switch (next_dialog_action_.value()) {
        case NextDialogAction::kCancel:
          view->CancelDialog();
          break;
        case NextDialogAction::kAccept:
          // Tell the Reauth API client to return a success for the next reauth
          // request.
          SetNextReAuthStatus(
              GaiaAuthConsumer::ReAuthProofTokenStatus::kSuccess);
          view->AcceptDialog();
          break;
      }
    }
  }

  bool IsParentPermissionDialogAppeared() {
#if BUILDFLAG(IS_CHROMEOS)
    return bool(fake_parent_access_dialog_provider_->TakeLastParams());
#else
    return parent_permission_dialog_appeared_;
#endif  // BUILDFLAG(IS_CHROMEOS)
  }

  void set_next_dialog_action(NextDialogAction action) {
#if BUILDFLAG(IS_CHROMEOS)
    auto result = std::make_unique<ash::ParentAccessDialog::Result>();
    switch (action) {
      case NextDialogAction::kCancel:
        result->status = ash::ParentAccessDialog::Result::Status::kCanceled;
        break;
      case NextDialogAction::kAccept:
        result->status = ash::ParentAccessDialog::Result::Status::kApproved;
        result->parent_access_token = "test_token";
        result->parent_access_token_expire_timestamp =
            base::Time::FromSecondsSinceUnixEpoch(123456L);
        break;
    }
    fake_parent_access_dialog_provider_->SetNextAction(
        ash::FakeParentAccessDialogProvider::Action::WithResult(
            std::move(result)));
#else
    next_dialog_action_ = action;
#endif
  }

 protected:
  raw_ptr<SupervisedUserExtensionsDelegateImpl> extensions_delegate_;

 private:
  // Create another embedded test server to avoid starting the same one twice.
  std::unique_ptr<net::EmbeddedTestServer> embedded_test_server_;
  supervised_user::SupervisionMixin supervision_mixin_;
  std::optional<NextDialogAction> next_dialog_action_;

  bool parent_permission_dialog_appeared_ = false;
#if BUILDFLAG(IS_CHROMEOS)
  raw_ptr<ash::FakeParentAccessDialogProvider>
      fake_parent_access_dialog_provider_;
#endif
};

// Tests install for a child when parent permission is granted.
IN_PROC_BROWSER_TEST_F(SupervisedUserExtensionWebstorePrivateApiTest,
                       ParentPermissionGranted) {
  base::UserActionTester user_action_tester;
  WebstoreInstallListener listener;
  auto delegate_reset = WebstorePrivateApi::SetDelegateForTesting(&listener);
  set_next_dialog_action(NextDialogAction::kAccept);

  ASSERT_TRUE(RunInstallTest("install_child.html", "app.crx"));
  listener.Wait();
  ASSERT_TRUE(listener.received_success());
  ASSERT_EQ(kTestAppId, listener.id());

  scoped_refptr<const Extension> extension =
      extensions::ExtensionBuilder("test extension")
          .SetID(kTestAppId)
          .SetVersion(kTestAppVersion)
          .Build();
  EXPECT_TRUE(extensions_delegate_->IsExtensionAllowedByParent(*extension));
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   SupervisedUserExtensionsMetricsRecorder::
                       kParentPermissionDialogOpenedActionName));
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   SupervisedUserExtensionsMetricsRecorder::
                       kParentPermissionDialogParentApprovedActionName));
  EXPECT_EQ(
      1,
      user_action_tester.GetActionCount(
          SupervisedUserExtensionsMetricsRecorder::kApprovalGrantedActionName));
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
}

// Tests no install occurs for a child when the parent permission
// dialog is canceled.
IN_PROC_BROWSER_TEST_F(SupervisedUserExtensionWebstorePrivateApiTest,
                       ParentPermissionCanceled) {
  base::UserActionTester user_action_tester;
  WebstoreInstallListener listener;
  set_next_dialog_action(NextDialogAction::kCancel);
  auto delegate_reset = WebstorePrivateApi::SetDelegateForTesting(&listener);
  ASSERT_TRUE(RunInstallTest("install_cancel_child.html", "app.crx"));
  listener.Wait();
  ASSERT_TRUE(listener.received_failure());
  ASSERT_EQ(kTestAppId, listener.id());
  ASSERT_EQ(listener.last_failure_reason(),
            WebstoreInstaller::FailureReason::FAILURE_REASON_CANCELLED);

  scoped_refptr<const Extension> extension =
      extensions::ExtensionBuilder("test extension")
          .SetID(kTestAppId)
          .SetVersion(kTestAppVersion)
          .Build();
  EXPECT_FALSE(extensions_delegate_->IsExtensionAllowedByParent(*extension));
// On the default configuration only the Parent approval dialog is used for
// extension installations.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   SupervisedUserExtensionsMetricsRecorder::
                       kParentPermissionDialogOpenedActionName));
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   SupervisedUserExtensionsMetricsRecorder::
                       kParentPermissionDialogParentCanceledActionName));
  EXPECT_EQ(0, user_action_tester.GetActionCount(
                   SupervisedUserExtensionsMetricsRecorder::
                       kExtensionInstallDialogChildCanceledActionName));
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
}

// Tests that no parent permission is required for a child to install a theme.
IN_PROC_BROWSER_TEST_F(SupervisedUserExtensionWebstorePrivateApiTest,
                       NoParentPermissionRequiredForTheme) {
  WebstoreInstallListener listener;
  auto delegate_reset = WebstorePrivateApi::SetDelegateForTesting(&listener);
  ASSERT_TRUE(RunInstallTest("theme.html", "../../theme.crx"));
  listener.Wait();
  ASSERT_TRUE(listener.received_success());
  ASSERT_EQ("idlfhncioikpdnlhnmcjogambnefbbfp", listener.id());
}

// Tests that supervised user extension installs are never blocked.
IN_PROC_BROWSER_TEST_F(SupervisedUserExtensionWebstorePrivateApiTest,
                       InstallAlwaysAllowed) {
  base::HistogramTester histogram_tester;
  base::UserActionTester user_action_tester;

  supervised_user_test_util::
      SetSupervisedUserExtensionsMayRequestPermissionsPref(profile(), false);

  WebstoreInstallListener listener;
  auto delegate_reset = WebstorePrivateApi::SetDelegateForTesting(&listener);
  set_next_dialog_action(NextDialogAction::kAccept);
  // Tell the Reauth API client to return a success for the next reauth
  // request.
  SetNextReAuthStatus(GaiaAuthConsumer::ReAuthProofTokenStatus::kSuccess);

  // Expect the extension to be blocked or installed normally based on the
  // toggle that manages supervised user extensions.
  std::string page = "install_child.html";
  ASSERT_TRUE(RunInstallTest(page, "app.crx"));

  listener.Wait();
  ASSERT_TRUE(listener.received_success());
  ASSERT_EQ(kTestAppId, listener.id());

  scoped_refptr<const Extension> extension =
      extensions::ExtensionBuilder("test extension")
          .SetID(kTestAppId)
          .SetVersion(kTestAppVersion)
          .Build();
  ASSERT_TRUE(extensions_delegate_->IsExtensionAllowedByParent(*extension));

  int expected_count_failed = 0;
  histogram_tester.ExpectUniqueSample(
      SupervisedUserExtensionsMetricsRecorder::kEnablementHistogramName,
      SupervisedUserExtensionsMetricsRecorder::EnablementState::kFailedToEnable,
      expected_count_failed);
  histogram_tester.ExpectTotalCount(
      SupervisedUserExtensionsMetricsRecorder::kEnablementHistogramName,
      expected_count_failed);
  EXPECT_EQ(
      expected_count_failed,
      user_action_tester.GetActionCount(
          SupervisedUserExtensionsMetricsRecorder::kFailedToEnableActionName));
}

// Tests a successful install for a child when parent permission can be skipped
// on installation (i.e. the "Extensions" toggle in ON).
IN_PROC_BROWSER_TEST_F(SupervisedUserExtensionWebstorePrivateApiTest,
                       InstallSuccessfulWhenExtensionsToggleOn) {
  base::UserActionTester user_action_tester;
  WebstoreInstallListener listener;
  auto delegate_reset = WebstorePrivateApi::SetDelegateForTesting(&listener);

  // Turn on preference that skips parent approval on extension installations.
  supervised_user_test_util::SetSkipParentApprovalToInstallExtensionsPref(
      profile(), true);
  // Turn off the "Permissions for sites, apps and extensions" toggle. It does
  // not affect the successful installation of extensions.
  supervised_user_test_util::
      SetSupervisedUserExtensionsMayRequestPermissionsPref(profile(), false);

  ASSERT_TRUE(RunInstallTest("install_child.html", "app.crx"));
  listener.Wait();
  ASSERT_TRUE(listener.received_success());
  ASSERT_EQ(kTestAppId, listener.id());

  EXPECT_FALSE(IsParentPermissionDialogAppeared());

  scoped_refptr<const Extension> extension =
      extensions::ExtensionBuilder("test extension")
          .SetID(kTestAppId)
          .SetVersion(kTestAppVersion)
          .Build();
  EXPECT_TRUE(extensions_delegate_->IsExtensionAllowedByParent(*extension));

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
  int parent_approval_dialog_count = 0;
  // Parent Approval dialog metrics (when managed by Permissions toggle):
  EXPECT_EQ(parent_approval_dialog_count,
            user_action_tester.GetActionCount(
                SupervisedUserExtensionsMetricsRecorder::
                    kParentPermissionDialogOpenedActionName));
  EXPECT_EQ(
      parent_approval_dialog_count,
      user_action_tester.GetActionCount(
          SupervisedUserExtensionsMetricsRecorder::kApprovalGrantedActionName));
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)

  // Extension Installation dialog metrics.
  int extension_install_dialog_count = 1;
  EXPECT_EQ(extension_install_dialog_count,
            user_action_tester.GetActionCount(
                SupervisedUserExtensionsMetricsRecorder::
                    kExtensionInstallDialogOpenedActionName));
  EXPECT_EQ(extension_install_dialog_count,
            user_action_tester.GetActionCount(
                SupervisedUserExtensionsMetricsRecorder::
                    kApprovalGrantedByDefaultName));
}
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_ANDROID)

// Test delegate that overrides the parent approval request for Android.
class TestSupervisedUserExtensionsDelegateAndroid
    : public SupervisedUserExtensionsDelegate {
 public:
  TestSupervisedUserExtensionsDelegateAndroid() = default;
  ~TestSupervisedUserExtensionsDelegateAndroid() override = default;

  // SupervisedUserExtensionsDelegate:
  bool IsChild() const override { return true; }
  bool IsExtensionAllowedByParent(const Extension& extension) const override {
    return false;
  }

  // This method is called to show the parent authentication dialog.
  void RequestToAddExtensionOrShowError(
      const Extension& extension,
      content::WebContents* web_contents,
      const gfx::ImageSkia& icon,
      ExtensionApprovalDoneCallback extension_approval_callback) override {
    // Simulate the parent approval install dialog result.
    switch (dialog_actions_.value()) {
      case DialogActions::kDismissParentAuthenticationDialog:
        // For this case, we will approve the Ask Parent dialog (done by the
        // scoped_auto_confirm_ set previously). Then cancel the parent
        // authentication dialog (done by the canceled callback).
        std::move(extension_approval_callback)
            .Run(SupervisedExtensionApprovalResult::kCanceled);
        break;
      case DialogActions::kDismissExtensionInstallDialog:
        // For this case, we will approve the Ask Parent dialog (done by the
        // scoped_auto_confirm_ set previously). Then approve the parent
        // authentication dialog (done by the approved callback). This will show
        // the extension install dialog, which is then cancelled by the updated
        // scoped_auto_confirm_ value.
        ExtensionInstallPrompt::g_last_prompt_type_for_tests =
            ExtensionInstallPrompt::EXTENSION_PARENT_APPROVAL_PROMPT;

        // Set the auto confirm value to cancel the install dialog.
        scoped_auto_confirm_.reset();
        scoped_auto_confirm_ = std::make_unique<ScopedTestDialogAutoConfirm>(
            ScopedTestDialogAutoConfirm::CANCEL);

        // We need to approve the parent authentication dialog in
        // order to progress to the last extension install dialog.
        std::move(extension_approval_callback)
            .Run(SupervisedExtensionApprovalResult::kApproved);
        break;
      case DialogActions::kFullInstall:
        // For this case, all dialogs will be approved.
        ExtensionInstallPrompt::g_last_prompt_type_for_tests =
            ExtensionInstallPrompt::EXTENSION_PARENT_APPROVAL_PROMPT;

        std::move(extension_approval_callback)
            .Run(SupervisedExtensionApprovalResult::kApproved);
        break;
      case DialogActions::kDismissAskParentDialog:
        NOTREACHED();
    }
  }

  // SupervisedUserExtensionsDelegate:
  void RequestToEnableExtensionOrShowError(
      const Extension& extension,
      content::WebContents* web_contents,
      ExtensionApprovalDoneCallback extension_approval_callback) override {}
  void UpdateManagementPolicyRegistration() override {}
  bool CanInstallExtensions() const override { return true; }
  void AddExtensionApproval(const extensions::Extension& extension) override {}
  void MaybeRecordPermissionsIncreaseMetrics(
      const extensions::Extension& extension) override {}
  void RemoveExtensionApproval(
      const extensions::Extension& extension) override {}
  void RecordExtensionEnablementUmaMetrics(bool enabled) const override {}

  // The sequence of dialog action to take. A total of 3 dialogs can be shown in
  // the supervised user extension installation flow:
  // 1. The Ask Parent dialog
  // 2. The parent authentication dialog
  // 3. The extension install dialog
  enum class DialogActions {
    // The initial Ask Parent dialog is dismissed.
    kDismissAskParentDialog,
    // The parent authentication dialog is dismissed.
    kDismissParentAuthenticationDialog,
    // The last extension install dialog is dismissed.
    kDismissExtensionInstallDialog,
    // The extension is installed.
    kFullInstall,
  };

  void set_dialog_actions(DialogActions action) {
    scoped_auto_confirm_.reset();
    scoped_auto_confirm_ = std::make_unique<ScopedTestDialogAutoConfirm>(
        action == DialogActions::kDismissAskParentDialog
            ? ScopedTestDialogAutoConfirm::CANCEL
            : ScopedTestDialogAutoConfirm::ACCEPT);

    dialog_actions_ = action;
  }

 private:
  std::optional<DialogActions> dialog_actions_;
  std::unique_ptr<ScopedTestDialogAutoConfirm> scoped_auto_confirm_;
};

// Test fixture for installation flows for child accounts on Android.
class SupervisedUserExtensionWebstorePrivateApiTestAndroid
    : public ExtensionWebstorePrivateApiTest {
 public:
  SupervisedUserExtensionWebstorePrivateApiTestAndroid() = default;

  void SetUpOnMainThread() override {
    ManagementAPI::GetFactoryInstance()->SetTestingFactory(
        profile(), base::BindRepeating(
                       &SupervisedUserExtensionWebstorePrivateApiTestAndroid::
                           CreateManagementAPIWithTestDelegate,
                       base::Unretained(this)));

    ExtensionWebstorePrivateApiTest::SetUpOnMainThread();

    // Set the profile as a child account.
    profile()->GetPrefs()->SetString(prefs::kSupervisedUserId,
                                     supervised_user::kChildAccountSUID);
    // Enable the "Permissions for sites, apps and extensions" toggle.
    profile()->GetPrefs()->SetBoolean(
        prefs::kSupervisedUserExtensionsMayRequestPermissions, true);

    // Force creation of the ManagementAPI service to ensure our test delegate
    // is initialized.
    BrowserContextKeyedAPIFactory<ManagementAPI>::Get(profile());
  }

  std::unique_ptr<KeyedService> CreateManagementAPIWithTestDelegate(
      content::BrowserContext* context) {
    std::unique_ptr<ManagementAPI> api =
        std::make_unique<ManagementAPI>(context);
    auto delegate =
        std::make_unique<TestSupervisedUserExtensionsDelegateAndroid>();
    test_delegate_ = delegate.get();
    api->set_supervised_user_extensions_delegate_for_test(std::move(delegate));
    return api;
  }

  void set_dialog_actions(
      TestSupervisedUserExtensionsDelegateAndroid::DialogActions action) {
    ASSERT_TRUE(test_delegate_) << "Test delegate not initialized";
    test_delegate_->set_dialog_actions(action);
  }

 private:
  raw_ptr<TestSupervisedUserExtensionsDelegateAndroid> test_delegate_ = nullptr;
};

// Tests that the initial InstallAskParent dialog is shown, then dismissed.
IN_PROC_BROWSER_TEST_F(SupervisedUserExtensionWebstorePrivateApiTestAndroid,
                       ParentApprovalInstallAskParentDialogShown) {
  WebstoreInstallListener listener;
  auto delegate_reset = WebstorePrivateApi::SetDelegateForTesting(&listener);

  set_dialog_actions(TestSupervisedUserExtensionsDelegateAndroid::
                         DialogActions::kDismissAskParentDialog);
  ASSERT_TRUE(RunInstallTest("install_cancel_child.html", "app.crx"));

  listener.Wait();
  ASSERT_TRUE(listener.received_failure());
  ASSERT_EQ(kTestAppId, listener.id());
  ASSERT_EQ(listener.last_failure_reason(),
            WebstoreInstaller::FailureReason::FAILURE_REASON_CANCELLED);
}

// Tests that the parent approval install dialog is NOT shown when the parent
// authentication is canceled or fails.
IN_PROC_BROWSER_TEST_F(
    SupervisedUserExtensionWebstorePrivateApiTestAndroid,
    ParentApprovalInstallDialogNotShownOnParentAuthenticationCancel) {
  // Set the prompt type to ensure we are testing the parent approval install
  // dialog.
  ExtensionInstallPrompt::g_last_prompt_type_for_tests =
      ExtensionInstallPrompt::UNSET_PROMPT_TYPE;

  WebstoreInstallListener listener;
  auto delegate_reset = WebstorePrivateApi::SetDelegateForTesting(&listener);

  set_dialog_actions(TestSupervisedUserExtensionsDelegateAndroid::
                         DialogActions::kDismissParentAuthenticationDialog);
  ASSERT_TRUE(RunInstallTest("install_cancel_child.html", "app.crx"));

  listener.Wait();
  ASSERT_TRUE(listener.received_failure());
  ASSERT_EQ(kTestAppId, listener.id());
  ASSERT_EQ(listener.last_failure_reason(),
            WebstoreInstaller::FailureReason::FAILURE_REASON_CANCELLED);

  // Verify that the parent approval install dialog was NOT shown.
  EXPECT_NE(ExtensionInstallPrompt::g_last_prompt_type_for_tests,
            ExtensionInstallPrompt::EXTENSION_PARENT_APPROVAL_PROMPT);
}

// Tests that the parent approval install dialog is shown when the parent
// authentication is successful, but the installation is cancelled by the user
// on the dialog.
IN_PROC_BROWSER_TEST_F(SupervisedUserExtensionWebstorePrivateApiTestAndroid,
                       ParentApprovalDialogShownAndCancelled) {
  // Set the prompt type to ensure we are testing the parent approval install
  // dialog.
  ExtensionInstallPrompt::g_last_prompt_type_for_tests =
      ExtensionInstallPrompt::UNSET_PROMPT_TYPE;

  WebstoreInstallListener listener;
  auto delegate_reset = WebstorePrivateApi::SetDelegateForTesting(&listener);

  // The parent approval install dialog
  // (ExtensionInstallPrompt::EXTENSION_PARENT_APPROVAL_PROMPT) will be shown
  // after the parent authentication dialog. Auto-cancel it.
  set_dialog_actions(TestSupervisedUserExtensionsDelegateAndroid::
                         DialogActions::kDismissExtensionInstallDialog);
  ASSERT_TRUE(RunInstallTest("install_blocked_child.html", "app.crx"));

  listener.Wait();
  ASSERT_TRUE(listener.received_failure());
  ASSERT_EQ(kTestAppId, listener.id());
  ASSERT_EQ(listener.last_failure_reason(),
            WebstoreInstaller::FailureReason::FAILURE_REASON_CANCELLED);

  // Verify that the parent approval install dialog was shown.
  EXPECT_EQ(ExtensionInstallPrompt::g_last_prompt_type_for_tests,
            ExtensionInstallPrompt::EXTENSION_PARENT_APPROVAL_PROMPT);
}

// Tests that the parent approval install dialog is shown when the parent
// authentication is successful. Then accept the dialog.
IN_PROC_BROWSER_TEST_F(SupervisedUserExtensionWebstorePrivateApiTestAndroid,
                       ParentApprovalDialogShownAndAccepted) {
  // Set the prompt type to ensure we are testing the parent approval install
  // dialog.
  ExtensionInstallPrompt::g_last_prompt_type_for_tests =
      ExtensionInstallPrompt::UNSET_PROMPT_TYPE;

  WebstoreInstallListener listener;
  auto delegate_reset = WebstorePrivateApi::SetDelegateForTesting(&listener);

  // The parent approval install dialog
  // (ExtensionInstallPrompt::EXTENSION_PARENT_APPROVAL_PROMPT) will be shown
  // after the parent authentication dialog. Auto-accept it.
  set_dialog_actions(
      TestSupervisedUserExtensionsDelegateAndroid::DialogActions::kFullInstall);
  ASSERT_TRUE(RunInstallTest("install_child.html", "app.crx"));

  listener.Wait();
  ASSERT_TRUE(listener.received_success());
  ASSERT_EQ(kTestAppId, listener.id());

  // Verify that the parent approval install dialog was shown.
  EXPECT_EQ(ExtensionInstallPrompt::g_last_prompt_type_for_tests,
            ExtensionInstallPrompt::EXTENSION_PARENT_APPROVAL_PROMPT);
}
#endif  // BUILDFLAG(IS_ANDROID)

class ExtensionWebstoreGetWebGLStatusTest : public PlatformBrowserTest {
 protected:
  void RunTest(bool webgl_allowed) {
    // If Gpu access is disallowed then WebGL will not be available.
    if (!content::GpuDataManager::GetInstance()->GpuAccessAllowed(nullptr)) {
      webgl_allowed = false;
    }

    static const char kEmptyArgs[] = "[]";
    static const char kWebGLStatusAllowed[] = "webgl_allowed";
    static const char kWebGLStatusBlocked[] = "webgl_blocked";
    scoped_refptr<WebstorePrivateGetWebGLStatusFunction> function =
        new WebstorePrivateGetWebGLStatusFunction();
    Profile* profile = chrome_test_utils::GetProfile(this);
    std::optional<base::Value> result = utils::RunFunctionAndReturnSingleResult(
        function.get(), kEmptyArgs, profile);
    ASSERT_TRUE(result);
    EXPECT_EQ(base::Value::Type::STRING, result->type());
    EXPECT_TRUE(result->is_string());
    std::string webgl_status = result->GetString();
    EXPECT_STREQ(webgl_allowed ? kWebGLStatusAllowed : kWebGLStatusBlocked,
                 webgl_status.c_str());
  }
};

// Tests getWebGLStatus function when WebGL is allowed.
// Flaky on Mac. https://crbug.com/1346413.
#if BUILDFLAG(IS_MAC)
#define MAYBE_Allowed DISABLED_Allowed
#else
#define MAYBE_Allowed Allowed
#endif
IN_PROC_BROWSER_TEST_F(ExtensionWebstoreGetWebGLStatusTest, MAYBE_Allowed) {
  bool webgl_allowed = true;
  RunTest(webgl_allowed);
}

// Tests getWebGLStatus function when WebGL is blocklisted.
IN_PROC_BROWSER_TEST_F(ExtensionWebstoreGetWebGLStatusTest, Blocked) {
  content::GpuDataManager::GetInstance()->BlocklistWebGLForTesting();

  bool webgl_allowed = false;
  RunTest(webgl_allowed);
}

class ExtensionWebstorePrivateGetReferrerChainApiTest
    : public ExtensionWebstorePrivateApiTest {
 public:
  ExtensionWebstorePrivateGetReferrerChainApiTest() {
    // TODO(crbug.com/40248833): Use HTTPS URLs in tests to avoid having to
    // disable this feature.
    feature_list_.InitAndDisableFeature(features::kHttpsUpgrades);
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("redirect1.com", "127.0.0.1");
    host_resolver()->AddRule("redirect2.com", "127.0.0.1");

    ExtensionWebstorePrivateApiTest::SetUpOnMainThread();
  }

  GURL GetTestServerURLWithReferrers(const std::string& path) {
    // Hand craft a url that will cause the test server to issue redirects.
    const std::vector<std::string> redirects = {"redirect1.com",
                                                "redirect2.com"};
    net::HostPortPair host_port = embedded_test_server()->host_port_pair();
    std::string redirect_chain;
    for (const auto& redirect : redirects) {
      std::string redirect_url = base::StringPrintf(
          "http://%s:%d/server-redirect?", redirect.c_str(), host_port.port());
      redirect_chain += redirect_url;
    }

    return GURL(redirect_chain + GetTestServerURL(path).spec());
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Tests that the GetReferrerChain API returns the redirect information.
IN_PROC_BROWSER_TEST_F(ExtensionWebstorePrivateGetReferrerChainApiTest,
                       GetReferrerChain) {
  GURL page_url = GetTestServerURLWithReferrers("referrer_chain.html");
  ASSERT_TRUE(OpenTestURL(page_url));
}

// Tests that the GetReferrerChain API returns an empty string for profiles
// opted out of SafeBrowsing.
IN_PROC_BROWSER_TEST_F(ExtensionWebstorePrivateGetReferrerChainApiTest,
                       GetReferrerChainForNonSafeBrowsingUser) {
  PrefService* pref_service = profile()->GetPrefs();
  EXPECT_TRUE(pref_service->GetBoolean(prefs::kSafeBrowsingEnabled));
  // Disable SafeBrowsing.
  pref_service->SetBoolean(prefs::kSafeBrowsingEnabled, false);

  GURL page_url = GetTestServerURLWithReferrers("empty_referrer_chain.html");
  ASSERT_TRUE(OpenTestURL(page_url));
}

class ExtensionWebstorePrivateApiAllowlistEnforcementTest
    : public ExtensionWebstorePrivateApiTest {
 public:
  ExtensionWebstorePrivateApiAllowlistEnforcementTest() {
    feature_list_.InitAndEnableFeature(
        extensions_features::kSafeBrowsingCrxAllowlistAutoDisable);
  }

  ExtensionAllowlist* GetAllowlist() {
    return ExtensionAllowlist::Get(profile());
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(ExtensionWebstorePrivateApiAllowlistEnforcementTest,
                       EnhancedSafeBrowsingNotAllowlisted) {
  safe_browsing::SetSafeBrowsingState(
      profile()->GetPrefs(),
      safe_browsing::SafeBrowsingState::ENHANCED_PROTECTION);
  ASSERT_TRUE(
      RunInstallTest("safebrowsing_not_allowlisted.html", "extension.crx"));

  EXPECT_EQ(ALLOWLIST_NOT_ALLOWLISTED,
            GetAllowlist()->GetExtensionAllowlistState(kExtensionId));
  EXPECT_EQ(
      ALLOWLIST_ACKNOWLEDGE_ENABLED_BY_USER,
      GetAllowlist()->GetExtensionAllowlistAcknowledgeState(kExtensionId));
}

IN_PROC_BROWSER_TEST_F(ExtensionWebstorePrivateApiAllowlistEnforcementTest,
                       EnhancedSafeBrowsingAllowlisted) {
  safe_browsing::SetSafeBrowsingState(
      profile()->GetPrefs(),
      safe_browsing::SafeBrowsingState::ENHANCED_PROTECTION);
  ASSERT_TRUE(RunInstallTest("safebrowsing_allowlisted.html", "extension.crx"));

  EXPECT_EQ(ALLOWLIST_UNDEFINED,
            GetAllowlist()->GetExtensionAllowlistState(kExtensionId));
}

IN_PROC_BROWSER_TEST_F(ExtensionWebstorePrivateApiAllowlistEnforcementTest,
                       StandardSafeBrowsingNotAllowlisted) {
  safe_browsing::SetSafeBrowsingState(
      profile()->GetPrefs(),
      safe_browsing::SafeBrowsingState::STANDARD_PROTECTION);
  ASSERT_TRUE(
      RunInstallTest("safebrowsing_not_allowlisted.html", "extension.crx"));

  EXPECT_EQ(ALLOWLIST_UNDEFINED,
            GetAllowlist()->GetExtensionAllowlistState(kExtensionId));
}

#if !BUILDFLAG(IS_ANDROID)
class WebstorePrivateEnterprisePromotionApiTest
    : public MixinBasedInProcessBrowserTest {
 public:
  WebstorePrivateEnterprisePromotionApiTest() {
    feature_list_.InitAndEnableFeature(
        extensions_features::kEnableShouldShowPromotion);
  }
  ~WebstorePrivateEnterprisePromotionApiTest() override = default;

 protected:
#if BUILDFLAG(IS_CHROMEOS)
  ash::DeviceStateMixin device_state_{
      &mixin_host_,
      ash::DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED};
#endif  // BUILDFLAG(IS_CHROMEOS)

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(WebstorePrivateEnterprisePromotionApiTest,
                       DeterminesAndSavesPromotionEligibility) {
  enterprise_management::GetUserEligiblePromotionsResponse mock_response;
  mock_response.mutable_promotions()->set_cws_privacy_details_promotion(
      enterprise_management::CHROME_ENTERPRISE_CORE);
  auto function = base::MakeRefCounted<
      WebstorePrivateShouldShowEnterprisePromotionBannerFunction>();
  function->SetFakePromotionEligibilityCheckerForTesting(
      std::make_unique<FakePromotionEligibilityChecker>(
          std::move(mock_response)));

  std::optional<base::Value> result = utils::RunFunctionAndReturnSingleResult(
      function.get(), "[]", browser()->profile());

  ASSERT_TRUE(result);
  EXPECT_EQ("CHROME_ENTERPRISE_CORE", result->GetString());
  EXPECT_EQ(static_cast<int>(enterprise::PromotionType::kChromeEnterpriseCore),
            browser()->profile()->GetPrefs()->GetInteger(
                enterprise_promotion::kEnterprisePromotionEligibility));
}

IN_PROC_BROWSER_TEST_F(WebstorePrivateEnterprisePromotionApiTest,
                       ReturnsCachedPromotionEligibility) {
  PrefService* prefs = browser()->profile()->GetPrefs();
  prefs->SetInteger(
      enterprise_promotion::kEnterprisePromotionEligibility,
      static_cast<int>(enterprise::PromotionType::kChromeEnterprisePremium));
  base::Time future_expiration = base::Time::Now() + base::Hours(1);
  prefs->SetTime(pref_names::kEnterprisePromotionExpirationTime,
                 future_expiration);
  auto function = base::MakeRefCounted<
      WebstorePrivateShouldShowEnterprisePromotionBannerFunction>();
  // It should return saved prefs and NEVER call this checker.
  function->SetFakePromotionEligibilityCheckerForTesting(
      std::make_unique<FailIfCalledPromotionEligibilityChecker>());

  std::optional<base::Value> result = utils::RunFunctionAndReturnSingleResult(
      function.get(), "[]", browser()->profile());

  ASSERT_TRUE(result);
  EXPECT_TRUE(result->is_string());
  EXPECT_EQ("CHROME_ENTERPRISE_PREMIUM", result->GetString());
  EXPECT_EQ(
      static_cast<int>(enterprise::PromotionType::kChromeEnterprisePremium),
      prefs->GetInteger(enterprise_promotion::kEnterprisePromotionEligibility));
  EXPECT_EQ(future_expiration,
            prefs->GetTime(pref_names::kEnterprisePromotionExpirationTime));
}

IN_PROC_BROWSER_TEST_F(WebstorePrivateEnterprisePromotionApiTest,
                       ReturnsUnspecifiedResponseWhenBannerWasDismissed) {
#if !BUILDFLAG(IS_CHROMEOS)
  policy::CloudPolicyManager* manager =
      browser()->profile()->GetCloudPolicyManager();
  auto client = std::make_unique<policy::MockCloudPolicyClient>();
  client->SetDMToken("fake-dm-token");
  manager->Connect(g_browser_process->local_state(), std::move(client));
#endif  // !BUILDFLAG(IS_CHROMEOS)
  PrefService* prefs = browser()->profile()->GetPrefs();
  prefs->SetBoolean(pref_names::kHasDismissedEnterprisePromotion, true);
  scoped_refptr<WebstorePrivateShouldShowEnterprisePromotionBannerFunction>
      function = base::MakeRefCounted<
          WebstorePrivateShouldShowEnterprisePromotionBannerFunction>();

  std::optional<base::Value> result = utils::RunFunctionAndReturnSingleResult(
      function.get(), "[]", browser()->profile());

  ASSERT_TRUE(result);
  EXPECT_EQ(
      api::webstore_private::ToString(
          api::webstore_private::PromotionType::kPromotionTypeUnspecified),
      result->GetString());
  EXPECT_EQ(static_cast<int>(enterprise::PromotionType::kUnspecified),
            browser()->profile()->GetPrefs()->GetInteger(
                enterprise_promotion::kEnterprisePromotionEligibility));
  EXPECT_EQ(
      static_cast<int>(enterprise::PromotionType::kUnspecified),
      prefs->GetInteger(enterprise_promotion::kEnterprisePromotionEligibility));
}
#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace extensions
