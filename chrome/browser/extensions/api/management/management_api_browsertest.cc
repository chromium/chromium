// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/management/management_api.h"

#include <optional>
#include <string>
#include <vector>

#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/pattern.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/to_string.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/chrome_app_deprecation.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/api/management/management_api_constants.h"
#include "extensions/browser/api_test_utils.h"
#include "extensions/browser/blocklist_state.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/extension_dialog_auto_confirm.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_host_test_helper.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registrar.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/install_verifier.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/extension_set.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/test_extension_dir.h"
#include "net/base/host_port_pair.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/public/cpp/network_switches.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/apps/app_service/chrome_app_deprecation/chrome_app_deprecation.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#endif

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace keys = extension_management_api_constants;

namespace extensions {
namespace {

#if !BUILDFLAG(IS_ANDROID)
// This function is unused on Android.
bool ExpectChromeAppsDefaultEnabled() {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  return false;
#else
  return true;
#endif
}
#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace

namespace test_utils = api_test_utils;

class ExtensionManagementApiBrowserTest : public ExtensionBrowserTest {
 public:
  ExtensionManagementApiBrowserTest() = default;
  ~ExtensionManagementApiBrowserTest() override = default;
  ExtensionManagementApiBrowserTest(const ExtensionManagementApiBrowserTest&) =
      delete;
  ExtensionManagementApiBrowserTest& operator=(
      const ExtensionManagementApiBrowserTest&) = delete;

 protected:
  bool CrashEnabledExtension(const ExtensionId& extension_id) {
    ExtensionHost* background_host =
        ProcessManager::Get(profile())->GetBackgroundHostForExtension(
            extension_id);
    if (!background_host) {
      return false;
    }
    content::CrashTab(background_host->host_contents());
    return true;
  }

 private:
  ScopedInstallVerifierBypassForTest install_verifier_bypass_;
};

class ExtensionManagementApiTestWithBackgroundType
    : public ExtensionManagementApiBrowserTest {
 public:
  ExtensionManagementApiTestWithBackgroundType() {
#if !BUILDFLAG(IS_ANDROID)
    // Android does not support Chrome apps and does not have access to the
    // variable g_enable_chrome_apps_for_testing.
    enable_chrome_apps_ = std::make_unique<base::AutoReset<bool>>(
        &extensions::testing::g_enable_chrome_apps_for_testing, true);
#endif
#if BUILDFLAG(IS_CHROMEOS)
    scoped_feature_list_.InitAndEnableFeature(
        apps::chrome_app_deprecation::kAllowUserInstalledChromeApps);
#endif
  }
  ~ExtensionManagementApiTestWithBackgroundType() override = default;
  ExtensionManagementApiTestWithBackgroundType(
      const ExtensionManagementApiTestWithBackgroundType&) = delete;
  ExtensionManagementApiTestWithBackgroundType& operator=(
      const ExtensionManagementApiTestWithBackgroundType&) = delete;

 private:
  std::unique_ptr<base::AutoReset<bool>> enable_chrome_apps_;
#if BUILDFLAG(IS_CHROMEOS)
  base::test::ScopedFeatureList scoped_feature_list_;
#endif
};

// We test this here instead of in an ExtensionApiTest because normal extensions
// are not allowed to call the install function.
IN_PROC_BROWSER_TEST_F(ExtensionManagementApiTestWithBackgroundType,
                       InstallEvent) {
  ExtensionTestMessageListener listener1("ready");
  ASSERT_TRUE(
      LoadExtension(test_data_dir_.AppendASCII("management/install_event")));
  ASSERT_TRUE(listener1.WaitUntilSatisfied());

  ExtensionTestMessageListener listener2("got_event");
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("api_test/management/enabled_extension"),
      {.context_type = ContextType::kFromManifest}));
  ASSERT_TRUE(listener2.WaitUntilSatisfied());
}

#if !BUILDFLAG(IS_ANDROID)
// Android does not support Chrome apps.
IN_PROC_BROWSER_TEST_F(ExtensionManagementApiTestWithBackgroundType,
                       LaunchApp) {
  ExtensionTestMessageListener listener1("app_launched");
  ExtensionTestMessageListener listener2("got_expected_error");
  ASSERT_TRUE(
      LoadExtension(test_data_dir_.AppendASCII("management/simple_extension"),
                    {.context_type = ContextType::kFromManifest}));
  ASSERT_TRUE(
      LoadExtension(test_data_dir_.AppendASCII("management/packaged_app"),
                    {.context_type = ContextType::kFromManifest}));
  ASSERT_TRUE(
      LoadExtension(test_data_dir_.AppendASCII("management/launch_app")));
  ASSERT_TRUE(listener1.WaitUntilSatisfied());
  ASSERT_TRUE(listener2.WaitUntilSatisfied());
}

// Android does not support Chrome apps.
IN_PROC_BROWSER_TEST_F(ExtensionManagementApiTestWithBackgroundType,
                       NoLaunchAppDeprecated) {
  extensions::testing::g_enable_chrome_apps_for_testing = false;
  const Extension* packaged_app =
      LoadExtension(test_data_dir_.AppendASCII("management/packaged_app"),
                    {.context_type = ContextType::kFromManifest});
  ASSERT_TRUE(packaged_app);
  EXPECT_TRUE(packaged_app->is_app());

  ExtensionTestMessageListener error("got_chrome_apps_error");
  ExtensionTestMessageListener launched("app_launched");
  ASSERT_TRUE(
      LoadExtension(test_data_dir_.AppendASCII("management/launch_app")));
  if (ExpectChromeAppsDefaultEnabled()) {
    EXPECT_TRUE(launched.WaitUntilSatisfied());
    EXPECT_FALSE(error.was_satisfied());
  } else {
    EXPECT_TRUE(error.WaitUntilSatisfied());
    EXPECT_FALSE(launched.was_satisfied());
  }
}

// Android does not support Chrome apps.
IN_PROC_BROWSER_TEST_F(ExtensionManagementApiTestWithBackgroundType,
                       LaunchAppFromBackground) {
  ExtensionTestMessageListener listener1("success");
  ASSERT_TRUE(
      LoadExtension(test_data_dir_.AppendASCII("management/packaged_app"),
                    {.context_type = ContextType::kFromManifest}));
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("management/launch_app_from_background")));
  ASSERT_TRUE(listener1.WaitUntilSatisfied());
}

// Android does not support Chrome apps.
IN_PROC_BROWSER_TEST_F(ExtensionManagementApiTestWithBackgroundType,
                       NoLaunchAppFromBackgroundDeprecated) {
  extensions::testing::g_enable_chrome_apps_for_testing = false;
  const Extension* packaged_app =
      LoadExtension(test_data_dir_.AppendASCII("management/packaged_app"),
                    {.context_type = ContextType::kFromManifest});
  ASSERT_TRUE(packaged_app);
  EXPECT_TRUE(packaged_app->is_app());

  // Also verify launching from background does not work. This helper is not an
  // app.
  ExtensionTestMessageListener error("got_chrome_apps_error");
  ExtensionTestMessageListener launched_failure("not_launched");
  ExtensionTestMessageListener success("success");
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("management/launch_app_from_background")));
  if (ExpectChromeAppsDefaultEnabled()) {
    EXPECT_TRUE(success.WaitUntilSatisfied());
    EXPECT_FALSE(error.was_satisfied());
    EXPECT_FALSE(launched_failure.was_satisfied());
  } else {
    EXPECT_TRUE(error.WaitUntilSatisfied());
    EXPECT_TRUE(launched_failure.WaitUntilSatisfied());
    EXPECT_FALSE(success.was_satisfied());
  }
}
#endif  // !BUILDFLAG(IS_ANDROID)

IN_PROC_BROWSER_TEST_F(ExtensionManagementApiTestWithBackgroundType,
                       SelfUninstall) {
  // Wait for the helper script to finish before loading the primary
  // extension. This ensures that the onUninstall event listener is
  // added before we proceed to the uninstall step.
  ExtensionTestMessageListener listener1("ready");
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("management/self_uninstall_helper")));
  ASSERT_TRUE(listener1.WaitUntilSatisfied());
  ExtensionTestMessageListener listener2("success");
  ASSERT_TRUE(
      LoadExtension(test_data_dir_.AppendASCII("management/self_uninstall")));
  ASSERT_TRUE(listener2.WaitUntilSatisfied());
}

IN_PROC_BROWSER_TEST_F(ExtensionManagementApiTestWithBackgroundType,
                       SelfUninstallNoPermissions) {
  // Wait for the helper script to finish before loading the primary
  // extension. This ensures that the onUninstall event listener is
  // added before we proceed to the uninstall step.
  ExtensionTestMessageListener listener1("ready");
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("management/self_uninstall_helper")));
  ASSERT_TRUE(listener1.WaitUntilSatisfied());
  ExtensionTestMessageListener listener2("success");
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("management/self_uninstall_noperm")));
  ASSERT_TRUE(listener2.WaitUntilSatisfied());
}

IN_PROC_BROWSER_TEST_F(ExtensionManagementApiTestWithBackgroundType, Get) {
  ExtensionTestMessageListener listener("success");
  ASSERT_TRUE(
      LoadExtension(test_data_dir_.AppendASCII("management/simple_extension"),
                    {.context_type = ContextType::kFromManifest}));
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("management/get")));
  ASSERT_TRUE(listener.WaitUntilSatisfied());
}

IN_PROC_BROWSER_TEST_F(ExtensionManagementApiTestWithBackgroundType,
                       GetSelfNoPermissions) {
  ExtensionTestMessageListener listener1("success");
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("management/get_self")));
  ASSERT_TRUE(listener1.WaitUntilSatisfied());
}

#if !BUILDFLAG(IS_ANDROID)
// Android does not support Chrome apps.
IN_PROC_BROWSER_TEST_F(ExtensionManagementApiBrowserTest,
                       CreateAppShortcutConfirmDialog) {
  const Extension* app = InstallExtension(
      test_data_dir_.AppendASCII("api_test/management/packaged_app"), 1);
  ASSERT_TRUE(app);

  const extensions::ExtensionId app_id = app->id();

  scoped_refptr<ManagementCreateAppShortcutFunction> create_shortcut_function(
      new ManagementCreateAppShortcutFunction());
  create_shortcut_function->set_user_gesture(true);
  ManagementCreateAppShortcutFunction::SetAutoConfirmForTest(true);
  test_utils::RunFunctionAndReturnSingleResult(
      create_shortcut_function.get(),
      base::StringPrintf("[\"%s\"]", app_id.c_str()), profile());

  create_shortcut_function = new ManagementCreateAppShortcutFunction();
  create_shortcut_function->set_user_gesture(true);
  ManagementCreateAppShortcutFunction::SetAutoConfirmForTest(false);
  EXPECT_TRUE(base::MatchPattern(
      test_utils::RunFunctionAndReturnError(
          create_shortcut_function.get(),
          base::StringPrintf("[\"%s\"]", app_id.c_str()), profile()),
      keys::kCreateShortcutCanceledError));
}
#endif  // !BUILDFLAG(IS_ANDROID)

IN_PROC_BROWSER_TEST_F(ExtensionManagementApiBrowserTest,
                       GetAllIncludesTerminated) {
  // Load an extension with a background page, so that we know it has a process
  // running.
  ExtensionTestMessageListener listener("ready");
  const Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("management/install_event"));
  ASSERT_TRUE(extension);
  ASSERT_TRUE(listener.WaitUntilSatisfied());

  // The management API should list this extension.
  scoped_refptr<ManagementGetAllFunction> function =
      base::MakeRefCounted<ManagementGetAllFunction>();
  std::optional<base::Value> result =
      test_utils::RunFunctionAndReturnSingleResult(function.get(), "[]",
                                                   profile());
  ASSERT_TRUE(result->is_list());
  EXPECT_EQ(1U, result->GetList().size());

  // And it should continue to do so even after it crashes.
  ASSERT_TRUE(CrashEnabledExtension(extension->id()));

  function = base::MakeRefCounted<ManagementGetAllFunction>();
  result = test_utils::RunFunctionAndReturnSingleResult(function.get(), "[]",
                                                        profile());
  ASSERT_TRUE(result->is_list());
  EXPECT_EQ(1U, result->GetList().size());
}

class ExtensionManagementApiEscalationTest :
    public ExtensionManagementApiBrowserTest {
 protected:
  // The id of the permissions escalation test extension we use.
  static const char kId[];

  void SetUpOnMainThread() override {
    ExtensionManagementApiBrowserTest::SetUpOnMainThread();
    EXPECT_TRUE(scoped_temp_dir_.CreateUniqueTempDir());
    base::FilePath pem_path = test_data_dir_.
        AppendASCII("permissions_increase").AppendASCII("permissions.pem");
    base::FilePath path_v1 = PackExtensionWithOptions(
        test_data_dir_.AppendASCII("permissions_increase").AppendASCII("v1"),
        scoped_temp_dir_.GetPath().AppendASCII("permissions1.crx"), pem_path,
        base::FilePath());
    base::FilePath path_v2 = PackExtensionWithOptions(
        test_data_dir_.AppendASCII("permissions_increase").AppendASCII("v2"),
        scoped_temp_dir_.GetPath().AppendASCII("permissions2.crx"), pem_path,
        base::FilePath());

    // Install low-permission version of the extension.
    ASSERT_TRUE(InstallExtension(path_v1, 1));
    EXPECT_TRUE(extension_registry()->enabled_extensions().GetByID(kId));

    // Update to a high-permission version - it should get disabled.
    EXPECT_FALSE(UpdateExtension(kId, path_v2, -1));
    EXPECT_FALSE(extension_registry()->enabled_extensions().GetByID(kId));
    EXPECT_TRUE(extension_registry()->disabled_extensions().GetByID(kId));
    EXPECT_TRUE(
        ExtensionPrefs::Get(profile())->DidExtensionEscalatePermissions(kId));
  }

  void SetEnabled(bool enabled,
                  bool user_gesture,
                  const std::string& expected_error,
                  scoped_refptr<const Extension> extension) {
    scoped_refptr<ManagementSetEnabledFunction> function(
        new ManagementSetEnabledFunction);
    function->set_extension(extension);
    if (user_gesture) {
      function->set_user_gesture(true);
    }
    function->SetRenderFrameHost(GetActiveWebContents()->GetPrimaryMainFrame());
    bool response = test_utils::RunFunction(
        function.get(),
        base::StringPrintf("[\"%s\", %s]", kId, base::ToString(enabled)),
        profile(), api_test_utils::FunctionMode::kNone);
    if (expected_error.empty()) {
      EXPECT_EQ(true, response);
    } else {
      EXPECT_TRUE(response == false);
      EXPECT_EQ(expected_error, function->GetError());
    }
  }


 private:
  base::ScopedTempDir scoped_temp_dir_;
};

const char ExtensionManagementApiEscalationTest::kId[] =
    "pgdpcfcocojkjfbgpiianjngphoopgmo";

IN_PROC_BROWSER_TEST_F(ExtensionManagementApiEscalationTest,
                       DisabledReason) {
  scoped_refptr<ManagementGetFunction> function =
      new ManagementGetFunction();
  base::DictValue dict =
      test_utils::ToDict(test_utils::RunFunctionAndReturnSingleResult(
          function.get(), base::StringPrintf("[\"%s\"]", kId), profile()));
  std::string reason =
      api_test_utils::GetString(dict, keys::kDisabledReasonKey);
  EXPECT_TRUE(base::IsStringASCII(reason));
  EXPECT_EQ(reason, std::string(keys::kDisabledReasonPermissionsIncrease));
}

IN_PROC_BROWSER_TEST_F(ExtensionManagementApiEscalationTest,
                       SetEnabled) {
  scoped_refptr<const Extension> source_extension =
      ExtensionBuilder("test").Build();

  // Expect an error about no gesture.
  SetEnabled(true, false, keys::kGestureNeededForEscalationError,
             source_extension);

  {
    // Expect an error that user cancelled the dialog.
    ScopedTestDialogAutoConfirm auto_confirm(
        ScopedTestDialogAutoConfirm::CANCEL);
    SetEnabled(true, true, keys::kUserDidNotReEnableError, source_extension);
  }

  {
    // The extension should load when the user accepts the dialog, triggering
    // a new ExtensionHost creation.
    ExtensionHostTestHelper host_helper(profile(), kId);
    ScopedTestDialogAutoConfirm auto_confirm(
        ScopedTestDialogAutoConfirm::ACCEPT);
    SetEnabled(true, true, std::string(), source_extension);
    host_helper.WaitForRenderProcessReady();
  }

  {
    // Crash the extension. Mock a reload by disabling and then enabling. The
    // extension should be reloaded and enabled.
    ScopedTestDialogAutoConfirm auto_confirm(
        ScopedTestDialogAutoConfirm::ACCEPT);
    ASSERT_TRUE(CrashEnabledExtension(kId));
    // Register the target extension with extension service.
    scoped_refptr<const Extension> target_extension =
        ExtensionBuilder("TargetExtension").SetID(kId).Build();
    extension_registrar()->AddExtension(target_extension);
    SetEnabled(false, true, std::string(), source_extension);
    SetEnabled(true, true, std::string(), source_extension);
    EXPECT_TRUE(extension_registry()->enabled_extensions().GetByID(kId));
  }
}

class ExtensionManagementWebstoreBrowserTest
    : public ExtensionManagementApiBrowserTest {
 public:
  ExtensionManagementWebstoreBrowserTest() = default;
  ~ExtensionManagementWebstoreBrowserTest() override = default;

 protected:
  // ExtensionManagementApiBrowserTest:
  void SetUp() override {
    UseHttpsTestServer();
    net::EmbeddedTestServer::ServerCertificateConfig cert_config;
    cert_config.dns_names = {"chromewebstore.google.com"};
    embedded_test_server()->SetSSLConfig(cert_config);
    embedded_test_server()->ServeFilesFromSourceDirectory(
        "chrome/test/data/extensions");
    ASSERT_TRUE(embedded_test_server()->Start());
    ExtensionManagementApiBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(
        network::switches::kHostResolverRules,
        "MAP chromewebstore.google.com " +
            embedded_test_server()->host_port_pair().ToString());
    ExtensionManagementApiBrowserTest::SetUpCommandLine(command_line);
  }

  void SetUpOnMainThread() override {
    ExtensionManagementApiBrowserTest::SetUpOnMainThread();

    // Greylist an unpacked target extension so each test starts with
    // `target_id_` disabled with `disable_reason::DISABLE_GREYLIST`.
    target_dir_.emplace();
    target_dir_->WriteManifest(R"({
      "name": "Target Extension",
      "version": "1.0",
      "manifest_version": 3
    })");
    const Extension* target_extension =
        LoadExtension(target_dir_->UnpackedPath());
    ASSERT_TRUE(target_extension);
    target_id_ = target_extension->id();
    expected_greylisted_error_ = ErrorUtils::FormatErrorMessage(
        keys::kCannotReEnableGreylistedError, target_id_);

    extension_registrar()->GreylistExtensionForTest(
        target_id_, BitMapBlocklistState::BLOCKLISTED_POTENTIALLY_UNWANTED);
    ASSERT_TRUE(
        extension_registry()->disabled_extensions().Contains(target_id_));
    ASSERT_TRUE(ExtensionPrefs::Get(profile())->HasDisableReason(
        target_id_, disable_reason::DISABLE_GREYLIST));
  }

  // Navigates the active `content::WebContents` to a newly loaded caller
  // extension page with the `"management"` permission:
  // - Returns the navigated `content::WebContents` on success.
  // - Returns `nullptr` on failure.
  content::WebContents* LoadAndNavigateToCallerExtension() {
    caller_dir_.emplace();
    caller_dir_->WriteManifest(R"({
      "name": "Caller Extension",
      "version": "1.0",
      "manifest_version": 3,
      "permissions": ["management"]
    })");
    caller_dir_->WriteFile(/*filename=*/FILE_PATH_LITERAL("page.html"),
                           /*contents=*/"<html></html>");
    const Extension* caller_extension =
        LoadExtension(caller_dir_->UnpackedPath());
    if (!caller_extension) {
      return nullptr;
    }

    content::WebContents* web_contents = GetActiveWebContents();
    if (!web_contents ||
        !NavigateToURL(web_contents,
                       caller_extension->GetResourceURL("page.html"))) {
      return nullptr;
    }
    return web_contents;
  }

  // Evaluates `chrome.management.setEnabled(extension_id, enabled)` in
  // `web_contents` with a transient user gesture:
  // - Returns `"succeeded"` when the promise resolves.
  // - Returns the error message when the promise rejects.
  content::EvalJsResult EvalSetEnabled(content::WebContents* web_contents,
                                       const ExtensionId& extension_id,
                                       bool enabled) {
    constexpr char kSetEnabledScript[] = R"(
      (async () => {
        try {
          await chrome.management.setEnabled($1, $2);
          return 'succeeded';
        } catch (e) {
          return e.message;
        }
      })();
    )";
    return content::EvalJs(
        web_contents,
        content::JsReplace(kSetEnabledScript, extension_id, enabled));
  }

  // Returns the `ExtensionId` of the greylisted target extension loaded in
  // `SetUpOnMainThread()`.
  const ExtensionId& target_id() const { return target_id_; }

  // Returns the formatted `keys::kCannotReEnableGreylistedError` message for
  // `target_id()`.
  const std::string& expected_greylisted_error() const {
    return expected_greylisted_error_;
  }

 private:
  std::optional<TestExtensionDir> target_dir_;
  std::optional<TestExtensionDir> caller_dir_;
  ExtensionId target_id_;
  std::string expected_greylisted_error_;
};

// Verifies `chrome.management.setEnabled()` when called by an extension with
// the `"management"` permission with a transient user gesture on a greylisted
// target. Regression test for crbug.com/514533069.
IN_PROC_BROWSER_TEST_F(ExtensionManagementWebstoreBrowserTest,
                       SetEnabledGreylistedExtension_ExtensionCaller) {
  content::WebContents* web_contents = LoadAndNavigateToCallerExtension();
  ASSERT_TRUE(web_contents);

  // `EvalSetEnabled()` runs the script with a transient user gesture.
  EXPECT_EQ(expected_greylisted_error(),
            EvalSetEnabled(web_contents, target_id(), /*enabled=*/true));
  EXPECT_TRUE(
      extension_registry()->disabled_extensions().Contains(target_id()));
  EXPECT_TRUE(ExtensionPrefs::Get(profile())->HasDisableReason(
      target_id(), disable_reason::DISABLE_GREYLIST));
}

// Verifies `chrome.management.setEnabled()` when called from the Chrome Web
// Store web page (`mojom::ContextType::kWebPage`) with a transient user
// gesture on a greylisted target. Regression test for crbug.com/514533069.
IN_PROC_BROWSER_TEST_F(ExtensionManagementWebstoreBrowserTest,
                       SetEnabledGreylistedExtension_Webstore) {
  content::WebContents* web_contents = GetActiveWebContents();
  ASSERT_TRUE(NavigateToURL(
      web_contents,
      GURL("https://chromewebstore.google.com/webstore/mock_store.html")));

  // `EvalSetEnabled()` runs the script with a transient user gesture.
  EXPECT_EQ(expected_greylisted_error(),
            EvalSetEnabled(web_contents, target_id(), /*enabled=*/true));
  EXPECT_TRUE(
      extension_registry()->disabled_extensions().Contains(target_id()));
  EXPECT_TRUE(ExtensionPrefs::Get(profile())->HasDisableReason(
      target_id(), disable_reason::DISABLE_GREYLIST));
}

// Verifies `chrome.management.setEnabled()` when called from
// chrome://extensions WebUI (`mojom::ContextType::kWebUi`) on a greylisted
// target. Regression test for crbug.com/514533069.
IN_PROC_BROWSER_TEST_F(ExtensionManagementWebstoreBrowserTest,
                       SetEnabledGreylistedExtension_ExtensionsWebUiAllowed) {
  content::WebContents* web_contents = GetActiveWebContents();
  ASSERT_TRUE(NavigateToURL(web_contents, GURL("chrome://extensions")));

  // Manual re-enablement from chrome://extensions is permitted.
  EXPECT_EQ("succeeded",
            EvalSetEnabled(web_contents, target_id(), /*enabled=*/true));
  EXPECT_TRUE(extension_registry()->enabled_extensions().Contains(target_id()));
}

// Verifies `chrome.management.setEnabled()` from a `"management"`-permission
// extension when only the Safe Browsing greylist state in `ExtensionPrefs`
// marks the target as greylisted. Manual re-enablement clears
// `disable_reason::DISABLE_GREYLIST` but keeps that state, and disabling via
// `chrome.management.setEnabled()` does not restore the disable reason. The
// re-enable call fails with the greylisted error and the target stays disabled,
// because `blocklist_prefs::IsExtensionGreylisted()` still reports the target
// as greylisted. Regression test for crbug.com/514533069.
IN_PROC_BROWSER_TEST_F(
    ExtensionManagementWebstoreBrowserTest,
    SetEnabledGreylistedExtension_ReEnableAfterDisableWhileGreylisted) {
  // Simulate prior manual re-enablement, which clears
  // `disable_reason::DISABLE_GREYLIST` while keeping the Safe Browsing greylist
  // state in `ExtensionPrefs`.
  EnableExtension(target_id());
  ASSERT_TRUE(extension_registry()->enabled_extensions().Contains(target_id()));

  content::WebContents* web_contents = LoadAndNavigateToCallerExtension();
  ASSERT_TRUE(web_contents);

  // Disabling the target from the caller extension succeeds without restoring
  // `disable_reason::DISABLE_GREYLIST`.
  ASSERT_EQ("succeeded",
            EvalSetEnabled(web_contents, target_id(), /*enabled=*/false));
  ASSERT_TRUE(
      extension_registry()->disabled_extensions().Contains(target_id()));
  ASSERT_FALSE(ExtensionPrefs::Get(profile())->HasDisableReason(
      target_id(), disable_reason::DISABLE_GREYLIST));

  // Re-enabling from the caller extension still fails while the target remains
  // on the greylist.
  EXPECT_EQ(expected_greylisted_error(),
            EvalSetEnabled(web_contents, target_id(), /*enabled=*/true));
  EXPECT_TRUE(
      extension_registry()->disabled_extensions().Contains(target_id()));
}

}  // namespace extensions
