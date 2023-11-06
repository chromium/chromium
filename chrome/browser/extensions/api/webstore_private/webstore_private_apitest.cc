// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <vector>

#include "base/auto_reset.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/api/webstore_private/webstore_private_api.h"
#include "chrome/browser/extensions/extension_install_prompt.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/mixin_based_extension_apitest.h"
#include "chrome/browser/extensions/webstore_installer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_test_util.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/supervised_user/core/common/buildflags.h"
#include "components/supervised_user/core/common/features.h"
#include "content/public/browser/gpu_data_manager.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/allowlist_state.h"
#include "extensions/browser/api/management/management_api.h"
#include "extensions/browser/api_test_utils.h"
#include "extensions/browser/extension_dialog_auto_confirm.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/install/extension_install_ui.h"
#include "extensions/common/extension_features.h"
#include "gpu/config/gpu_feature_type.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gl/gl_switches.h"

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "chrome/browser/supervised_user/supervised_user_extensions_delegate_impl.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chrome/browser/supervised_user/supervised_user_test_util.h"  // nogncheck
#include "chrome/browser/ui/supervised_user/parent_permission_dialog.h"
#include "chrome/browser/ui/views/supervised_user/parent_permission_dialog_view.h"
#include "chrome/test/supervised_user/supervision_mixin.h"
#include "components/account_id/account_id.h"
#include "components/supervised_user/core/common/pref_names.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"
#include "extensions/common/extension_builder.h"
#endif  // BUILDFLAG(ENABLE_SUPERVISED_USERS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/supervised_user/chromeos/parent_access_extension_approvals_manager.h"
#include "chrome/browser/ui/webui/ash/parent_access/parent_access_dialog.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace extensions {

namespace utils = api_test_utils;

namespace {

constexpr char kExtensionId[] = "enfkhcelefdadlmkffamgdlgplcionje";

class WebstoreInstallListener : public WebstorePrivateApi::Delegate {
 public:
  WebstoreInstallListener()
      : received_failure_(false), received_success_(false), waiting_(false) {}

  void OnExtensionInstallSuccess(const std::string& id) override {
    received_success_ = true;
    id_ = id;

    if (waiting_) {
      waiting_ = false;
      base::RunLoop::QuitCurrentWhenIdleDeprecated();
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
      base::RunLoop::QuitCurrentWhenIdleDeprecated();
    }
  }

  void Wait() {
    if (received_success_ || received_failure_)
      return;

    waiting_ = true;
    content::RunMessageLoop();
  }
  bool received_success() const { return received_success_; }
  bool received_failure() const { return received_failure_; }
  const std::string& id() const { return id_; }
  WebstoreInstaller::FailureReason last_failure_reason() {
    return last_failure_reason_;
  }

 private:
  bool received_failure_;
  bool received_success_;
  bool waiting_;
  WebstoreInstaller::FailureReason last_failure_reason_;
  std::string id_;
  std::string error_;
};

}  // namespace

// A base class for tests below.
class ExtensionWebstorePrivateApiTest : public MixinBasedExtensionApiTest {
 public:
  ExtensionWebstorePrivateApiTest() {}

  ExtensionWebstorePrivateApiTest(const ExtensionWebstorePrivateApiTest&) =
      delete;
  ExtensionWebstorePrivateApiTest& operator=(
      const ExtensionWebstorePrivateApiTest&) = delete;

  ~ExtensionWebstorePrivateApiTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    MixinBasedExtensionApiTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(
        switches::kAppsGalleryURL,
        "http://www.example.com/extensions/api_test");
  }

  void SetUpOnMainThread() override {
    MixinBasedExtensionApiTest::SetUpOnMainThread();

    // Start up the test server and get us ready for calling the install
    // API functions.
    host_resolver()->AddRule("www.example.com", "127.0.0.1");
    ASSERT_TRUE(StartEmbeddedTestServer());
    extensions::ExtensionInstallUI::set_disable_ui_for_tests();

    auto_confirm_install_ = std::make_unique<ScopedTestDialogAutoConfirm>(
        ScopedTestDialogAutoConfirm::ACCEPT);

    ASSERT_TRUE(webstore_install_dir_.CreateUniqueTempDir());
    webstore_install_dir_copy_ = webstore_install_dir_.GetPath();
    WebstoreInstaller::SetDownloadDirectoryForTests(
        &webstore_install_dir_copy_);
  }

 protected:
  // Returns a test server URL, but with host 'www.example.com' so it matches
  // the web store app's extent that we set up via command line flags.
  GURL DoGetTestServerURL(const std::string& path) {
    GURL url = embedded_test_server()->GetURL(path);

    // Replace the host with 'www.example.com' so it matches the web store
    // app's extent.
    GURL::Replacements replace_host;
    replace_host.SetHostStr("www.example.com");

    return url.ReplaceComponents(replace_host);
  }

  virtual GURL GetTestServerURL(const std::string& path) {
    return DoGetTestServerURL(
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

  ExtensionService* service() {
    return ExtensionSystem::Get(browser()->profile())->extension_service();
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
  if (base::DirectoryExists(missing_directory))
    EXPECT_TRUE(base::DeletePathRecursively(missing_directory));
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

  std::unique_ptr<WebstoreInstaller::Approval> approval =
      WebstorePrivateApi::PopApprovalForTesting(browser()->profile(), appId);
  EXPECT_EQ(appId, approval->extension_id);
  EXPECT_TRUE(approval->use_app_installed_bubble);
  EXPECT_FALSE(approval->skip_post_install_ui);
  EXPECT_EQ("2", approval->authuser);
  EXPECT_EQ(browser()->profile(), approval->profile);

  approval = WebstorePrivateApi::PopApprovalForTesting(browser()->profile(),
                                                       kExtensionId);
  EXPECT_EQ(kExtensionId, approval->extension_id);
  EXPECT_FALSE(approval->use_app_installed_bubble);
  EXPECT_FALSE(approval->skip_post_install_ui);
  EXPECT_TRUE(approval->authuser.empty());
  EXPECT_EQ(browser()->profile(), approval->profile);
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

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
static constexpr char kTestAppId[] = "iladmdjkfniedhfhcfoefgojhgaiaccc";
static constexpr char kTestAppVersion[] = "0.1";

// Test fixture for various cases of installation for child accounts.
class SupervisedUserExtensionWebstorePrivateApiTest
    : public ExtensionWebstorePrivateApiTest,
#if BUILDFLAG(IS_CHROMEOS_ASH)
      public TestExtensionApprovalsManagerObserver,
#endif
      public TestParentPermissionDialogViewObserver {
 public:
  // The next dialog action to take.
  enum class NextDialogAction {
    kCancel,
    kAccept,
  };

  SupervisedUserExtensionWebstorePrivateApiTest()
      :
#if BUILDFLAG(IS_CHROMEOS_ASH)
        TestExtensionApprovalsManagerObserver(this),
#endif
        TestParentPermissionDialogViewObserver(this),
        embedded_test_server_(std::make_unique<net::EmbeddedTestServer>()),
        supervision_mixin_(
            mixin_host_,
            this,
            embedded_test_server_.get(),
            {
                .consent_level = signin::ConsentLevel::kSignin,
                .sign_in_mode =
                    supervised_user::SupervisionMixin::SignInMode::kSupervised,
            }) {
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN)
    feature_list_.InitWithFeatures(
        /*enabled_features=*/
        {supervised_user::
             kEnableExtensionsPermissionsForSupervisedUsersOnDesktop,
         // Used to prevent user sign-out which crashes the test.
         supervised_user::kClearingCookiesKeepsSupervisedUsersSignedIn},
        /*disabled_features=*/{});
#endif
  }

  ~SupervisedUserExtensionWebstorePrivateApiTest() override {
    // Reset the feature list explicitly here, as other test members that may
    // contain it will try to destruct it (e.g. objects contained in
    // supervision_mixin_).
    feature_list_.Reset();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ExtensionWebstorePrivateApiTest::SetUpCommandLine(command_line);
    // Shortens the merge session timeout from 20 to 1 seconds to speed up the
    // test by about 19 seconds.
    // TODO (crbug.com/995575): figure out why this switch speeds up the test,
    // and fix the test setup so this is not required.
#if BUILDFLAG(IS_CHROMEOS_ASH)
    command_line->AppendSwitch(switches::kShortMergeSessionTimeoutForTest);
#endif
  }

  void SetUpOnMainThread() override {
    ExtensionWebstorePrivateApiTest::SetUpOnMainThread();

    extensions_delegate_ =
        std::make_unique<SupervisedUserExtensionsDelegateImpl>(profile());

    supervised_user_test_util::
        SetSupervisedUserExtensionsMayRequestPermissionsPref(profile(), true);
  }

  void TearDownOnMainThread() override {
    extensions_delegate_.reset();
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

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // TestExtensionApprovalsManagerObserver override:
  void OnTestParentAccessDialogCreated() override {
    if (next_dialog_action_) {
      switch (next_dialog_action_.value()) {
        case NextDialogAction::kCancel:
          ash::ParentAccessDialog::GetInstance()->SetCanceled();
          break;
        case NextDialogAction::kAccept:
          bool can_request_permission =
              browser()->profile()->GetPrefs()->GetBoolean(
                  prefs::kSupervisedUserExtensionsMayRequestPermissions);
          if (!can_request_permission) {
            ash::ParentAccessDialog::GetInstance()->SetDisabled();
            break;
          }
          ash::ParentAccessDialog::GetInstance()->SetApproved(
              "test_token", base::Time::FromSecondsSinceUnixEpoch(123456L));
          break;
      }
    }
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  void set_next_dialog_action(NextDialogAction action) {
    next_dialog_action_ = action;
  }

 protected:
  std::unique_ptr<SupervisedUserExtensionsDelegateImpl> extensions_delegate_;

 private:
  // Create another embedded test server to avoid starting the same one twice.
  std::unique_ptr<net::EmbeddedTestServer> embedded_test_server_;
  supervised_user::SupervisionMixin supervision_mixin_;
  absl::optional<NextDialogAction> next_dialog_action_;
  base::test::ScopedFeatureList feature_list_;
};

// Tests install for a child when parent permission is granted.
IN_PROC_BROWSER_TEST_F(SupervisedUserExtensionWebstorePrivateApiTest,
                       ParentPermissionGranted) {
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
  ASSERT_TRUE(extensions_delegate_->IsExtensionAllowedByParent(*extension));
}

// Tests no install occurs for a child when the parent permission
// dialog is canceled.
IN_PROC_BROWSER_TEST_F(SupervisedUserExtensionWebstorePrivateApiTest,
                       ParentPermissionCanceled) {
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
  ASSERT_FALSE(extensions_delegate_->IsExtensionAllowedByParent(*extension));
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

// Tests that even if the kSupervisedUserInitiatedExtensionInstall feature flag
// is enabled, supervised user extension installs are blocked if the
// "Permissions for sites, apps and extensions" toggle is off.
IN_PROC_BROWSER_TEST_F(SupervisedUserExtensionWebstorePrivateApiTest,
                       InstallBlockedWhenPermissionsToggleOff) {
  base::HistogramTester histogram_tester;
  base::UserActionTester user_action_tester;

  supervised_user_test_util::
      SetSupervisedUserExtensionsMayRequestPermissionsPref(profile(), false);

  set_next_dialog_action(NextDialogAction::kAccept);
  // Tell the Reauth API client to return a success for the next reauth
  // request.
  SetNextReAuthStatus(GaiaAuthConsumer::ReAuthProofTokenStatus::kSuccess);
  ASSERT_TRUE(RunInstallTest("install_blocked_child.html", "app.crx"));
  histogram_tester.ExpectUniqueSample(
      SupervisedUserExtensionsMetricsRecorder::kEnablementHistogramName,
      SupervisedUserExtensionsMetricsRecorder::EnablementState::kFailedToEnable,
      1);
  histogram_tester.ExpectTotalCount(
      SupervisedUserExtensionsMetricsRecorder::kEnablementHistogramName, 1);
  EXPECT_EQ(
      1,
      user_action_tester.GetActionCount(
          SupervisedUserExtensionsMetricsRecorder::kFailedToEnableActionName));
}

#endif  // BUILDFLAG(ENABLE_SUPERVISED_USERS)

class ExtensionWebstoreGetWebGLStatusTest : public InProcessBrowserTest {
 protected:
  void RunTest(bool webgl_allowed) {
    // If Gpu access is disallowed then WebGL will not be available.
    if (!content::GpuDataManager::GetInstance()->GpuAccessAllowed(nullptr))
      webgl_allowed = false;

    static const char kEmptyArgs[] = "[]";
    static const char kWebGLStatusAllowed[] = "webgl_allowed";
    static const char kWebGLStatusBlocked[] = "webgl_blocked";
    scoped_refptr<WebstorePrivateGetWebGLStatusFunction> function =
        new WebstorePrivateGetWebGLStatusFunction();
    absl::optional<base::Value> result =
        utils::RunFunctionAndReturnSingleResult(function.get(), kEmptyArgs,
                                                browser()->profile());
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
    // TODO(crbug.com/1394910): Use HTTPS URLs in tests to avoid having to
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
  PrefService* pref_service = browser()->profile()->GetPrefs();
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
    feature_list_.InitWithFeatures(
        {extensions_features::kSafeBrowsingCrxAllowlistShowWarnings,
         extensions_features::kSafeBrowsingCrxAllowlistAutoDisable},
        {});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(ExtensionWebstorePrivateApiAllowlistEnforcementTest,
                       EnhancedSafeBrowsingNotAllowlisted) {
  safe_browsing::SetSafeBrowsingState(
      browser()->profile()->GetPrefs(),
      safe_browsing::SafeBrowsingState::ENHANCED_PROTECTION);
  ASSERT_TRUE(
      RunInstallTest("safebrowsing_not_allowlisted.html", "extension.crx"));

  EXPECT_EQ(ALLOWLIST_NOT_ALLOWLISTED,
            extension_service()->allowlist()->GetExtensionAllowlistState(
                kExtensionId));
  EXPECT_EQ(
      ALLOWLIST_ACKNOWLEDGE_ENABLED_BY_USER,
      extension_service()->allowlist()->GetExtensionAllowlistAcknowledgeState(
          kExtensionId));
}

IN_PROC_BROWSER_TEST_F(ExtensionWebstorePrivateApiAllowlistEnforcementTest,
                       EnhancedSafeBrowsingAllowlisted) {
  safe_browsing::SetSafeBrowsingState(
      browser()->profile()->GetPrefs(),
      safe_browsing::SafeBrowsingState::ENHANCED_PROTECTION);
  ASSERT_TRUE(RunInstallTest("safebrowsing_allowlisted.html", "extension.crx"));

  EXPECT_EQ(ALLOWLIST_UNDEFINED,
            extension_service()->allowlist()->GetExtensionAllowlistState(
                kExtensionId));
}

IN_PROC_BROWSER_TEST_F(ExtensionWebstorePrivateApiAllowlistEnforcementTest,
                       StandardSafeBrowsingNotAllowlisted) {
  safe_browsing::SetSafeBrowsingState(
      browser()->profile()->GetPrefs(),
      safe_browsing::SafeBrowsingState::STANDARD_PROTECTION);
  ASSERT_TRUE(
      RunInstallTest("safebrowsing_not_allowlisted.html", "extension.crx"));

  EXPECT_EQ(ALLOWLIST_UNDEFINED,
            extension_service()->allowlist()->GetExtensionAllowlistState(
                kExtensionId));
}

}  // namespace extensions
