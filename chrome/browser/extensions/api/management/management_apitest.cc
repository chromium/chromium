// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>

#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/launch_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/web_applications/components/app_registrar.h"
#include "chrome/browser/web_applications/components/web_app_provider_base.h"
#include "chrome/browser/web_applications/test/test_web_app_ui_manager.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_constants.h"
#include "extensions/browser/api/management/management_api.h"
#include "extensions/browser/extension_dialog_auto_confirm.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/test_management_policy.h"
#include "extensions/common/manifest.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "extensions/test/test_extension_dir.h"
#include "net/dns/mock_host_resolver.h"

using extensions::Extension;
using extensions::Manifest;

namespace {

// Find a browser other than |browser|.
Browser* FindOtherBrowser(Browser* browser) {
  Browser* found = NULL;
  for (auto* b : *BrowserList::GetInstance()) {
    if (b == browser)
      continue;
    found = b;
  }
  return found;
}

}  // namespace

class ExtensionManagementApiTest : public extensions::ExtensionApiTest {
 public:
  virtual void LoadExtensions() {
    base::FilePath basedir = test_data_dir_.AppendASCII("management");

    // Load 5 enabled items.
    LoadNamedExtension(basedir, "enabled_extension");
    LoadNamedExtension(basedir, "enabled_app");
    LoadNamedExtension(basedir, "description");
    LoadNamedExtension(basedir, "permissions");
    LoadNamedExtension(basedir, "short_name");

    // Load 2 disabled items.
    LoadNamedExtension(basedir, "disabled_extension");
    DisableExtension(extension_ids_["disabled_extension"]);
    LoadNamedExtension(basedir, "disabled_app");
    DisableExtension(extension_ids_["disabled_app"]);
  }

  // Load an app, and wait for a message that it has been launched. This should
  // be sent by the launched app, to ensure the page is fully loaded.
  void LoadAndWaitForLaunch(const std::string& app_path,
                            std::string* out_app_id) {
    ExtensionTestMessageListener launched_app("launched app", false);
    ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII(app_path)));

    if (out_app_id)
      *out_app_id = last_loaded_extension_id();

    ASSERT_TRUE(launched_app.WaitUntilSatisfied());
  }

 protected:
  void LoadNamedExtension(const base::FilePath& path,
                          const std::string& name) {
    const Extension* extension = LoadExtension(path.AppendASCII(name));
    ASSERT_TRUE(extension);
    extension_ids_[name] = extension->id();
  }

  void InstallNamedExtension(const base::FilePath& path,
                             const std::string& name,
                             Manifest::Location install_source) {
    const Extension* extension = InstallExtension(path.AppendASCII(name), 1,
                                                  install_source);
    ASSERT_TRUE(extension);
    extension_ids_[name] = extension->id();
  }

  // Maps installed extension names to their IDs.
  std::map<std::string, std::string> extension_ids_;
};

IN_PROC_BROWSER_TEST_F(ExtensionManagementApiTest, Basics) {
  LoadExtensions();

  base::FilePath basedir = test_data_dir_.AppendASCII("management");
  InstallNamedExtension(basedir, "internal_extension", Manifest::INTERNAL);
  InstallNamedExtension(basedir, "external_extension",
                        Manifest::EXTERNAL_PREF);
  InstallNamedExtension(basedir, "admin_extension",
                        Manifest::EXTERNAL_POLICY_DOWNLOAD);
  InstallNamedExtension(basedir, "version_name", Manifest::INTERNAL);

  ASSERT_TRUE(RunExtensionSubtest("management/test", "basics.html"));
}

IN_PROC_BROWSER_TEST_F(ExtensionManagementApiTest, NoPermission) {
  LoadExtensions();
  ASSERT_TRUE(RunExtensionSubtest("management/no_permission", "test.html"));
}

// Disabled: http://crbug.com/174411
#if defined(OS_WIN)
#define MAYBE_Uninstall DISABLED_Uninstall
#else
#define MAYBE_Uninstall Uninstall
#endif

IN_PROC_BROWSER_TEST_F(ExtensionManagementApiTest, MAYBE_Uninstall) {
  LoadExtensions();
  // Confirmation dialog will be shown for uninstallations except for self.
  extensions::ScopedTestDialogAutoConfirm auto_confirm(
      extensions::ScopedTestDialogAutoConfirm::ACCEPT);
  ASSERT_TRUE(RunExtensionSubtest("management/test", "uninstall.html"));
}

IN_PROC_BROWSER_TEST_F(ExtensionManagementApiTest, CreateAppShortcut) {
  LoadExtensions();
  base::FilePath basedir = test_data_dir_.AppendASCII("management");
  LoadNamedExtension(basedir, "packaged_app");

  extensions::ManagementCreateAppShortcutFunction::SetAutoConfirmForTest(true);
  ASSERT_TRUE(RunExtensionSubtest("management/test",
                                  "createAppShortcut.html"));
}

IN_PROC_BROWSER_TEST_F(ExtensionManagementApiTest, GenerateAppForLink) {
  ASSERT_TRUE(RunExtensionSubtest("management/test",
                                  "generateAppForLink.html"));
}

class InstallReplacementWebAppApiTest : public ExtensionManagementApiTest {
 public:
  InstallReplacementWebAppApiTest()
      : https_test_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}
  ~InstallReplacementWebAppApiTest() override = default;

 protected:
  static const char kManifest[];
  static const char kAppManifest[];

  void SetUpOnMainThread() override {
    ExtensionManagementApiTest::SetUpOnMainThread();
    https_test_server_.ServeFilesFromDirectory(test_data_dir_);
    ASSERT_TRUE(https_test_server_.Start());
  }

  void RunTest(const char* manifest,
               const char* web_app_path,
               const char* background_script,
               bool from_webstore) {
    extensions::TestExtensionDir extension_dir;
    extension_dir.WriteManifest(base::StringPrintf(
        manifest, https_test_server_.GetURL(web_app_path).spec().c_str()));
    extension_dir.WriteFile(FILE_PATH_LITERAL("background.js"),
                            background_script);
    extensions::ResultCatcher catcher;
    if (from_webstore) {
      // |expected_change| is the expected change in the number of installed
      // extensions.
      ASSERT_TRUE(InstallExtensionFromWebstore(extension_dir.UnpackedPath(),
                                               1 /* expected_change */));
    } else {
      ASSERT_TRUE(LoadExtension(extension_dir.UnpackedPath()));
    }

    ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
  }

  void RunInstallableWebAppTest(const char* manifest,
                                const char* web_app_url,
                                const char* web_app_start_url) {
    static constexpr char kInstallReplacementWebApp[] =
        R"(chrome.test.runWithUserGesture(function() {
             chrome.management.installReplacementWebApp(function() {
               chrome.test.assertNoLastError();
               chrome.test.notifyPass();
             });
           });)";

    chrome::SetAutoAcceptPWAInstallConfirmationForTesting(true);
    const GURL start_url = https_test_server_.GetURL(web_app_start_url);
    web_app::AppId web_app_id = web_app::GenerateAppIdFromURL(start_url);

    auto* provider =
        web_app::WebAppProviderBase::GetProviderBase(browser()->profile());
    EXPECT_FALSE(provider->registrar().IsLocallyInstalled(start_url));
    EXPECT_EQ(0, static_cast<int>(
                     provider->ui_manager().GetNumWindowsForApp(web_app_id)));

    RunTest(manifest, web_app_url, kInstallReplacementWebApp,
            true /* from_webstore */);
    EXPECT_TRUE(provider->registrar().IsLocallyInstalled(start_url));
    EXPECT_EQ(1, static_cast<int>(
                     provider->ui_manager().GetNumWindowsForApp(web_app_id)));

    // Call API again. It should launch the app.
    RunTest(manifest, web_app_url, kInstallReplacementWebApp,
            true /* from_webstore */);
    EXPECT_TRUE(provider->registrar().IsLocallyInstalled(start_url));
    EXPECT_EQ(2, static_cast<int>(
                     provider->ui_manager().GetNumWindowsForApp(web_app_id)));

    chrome::SetAutoAcceptPWAInstallConfirmationForTesting(false);
  }

  net::EmbeddedTestServer https_test_server_;
};

const char InstallReplacementWebAppApiTest::kManifest[] =
    R"({
          "name": "Management API Test",
          "version": "0.1",
          "manifest_version": 2,
          "background": { "scripts": ["background.js"] },
          "replacement_web_app": "%s"
        })";

const char InstallReplacementWebAppApiTest::kAppManifest[] =
    R"({
          "name": "Management API Test",
          "version": "0.1",
          "manifest_version": 2,
          "app": {
            "background": { "scripts": ["background.js"] }
          },
          "replacement_web_app": "%s"
        })";

IN_PROC_BROWSER_TEST_F(InstallReplacementWebAppApiTest, NotWebstore) {
  static constexpr char kBackground[] = R"(
  chrome.management.installReplacementWebApp(function() {
    chrome.test.assertLastError(
        'Only extensions from the web store can install replacement web apps.');
    chrome.test.notifyPass();
  });)";

  RunTest(kManifest,
          "/management/install_replacement_web_app/good_web_app/index.html",
          kBackground, false /* from_webstore */);
}

IN_PROC_BROWSER_TEST_F(InstallReplacementWebAppApiTest, NoGesture) {
  static constexpr char kBackground[] = R"(
  chrome.management.installReplacementWebApp(function() {
    chrome.test.assertLastError(
        'chrome.management.installReplacementWebApp requires a user gesture.');
    chrome.test.notifyPass();
  });)";

  RunTest(kManifest,
          "/management/install_replacement_web_app/good_web_app/index.html",
          kBackground, true /* from_webstore */);
}

IN_PROC_BROWSER_TEST_F(InstallReplacementWebAppApiTest, NotInstallableWebApp) {
  static constexpr char kBackground[] =
      R"(chrome.test.runWithUserGesture(function() {
           chrome.management.installReplacementWebApp(function() {
             chrome.test.assertLastError(
                 'Web app is not a valid installable web app.');
             chrome.test.notifyPass();
           });
         });)";

  RunTest(kManifest,
          "/management/install_replacement_web_app/bad_web_app/index.html",
          kBackground, true /* from_webstore */);
}

IN_PROC_BROWSER_TEST_F(InstallReplacementWebAppApiTest, InstallableWebApp) {
  static constexpr char kGoodWebAppURL[] =
      "/management/install_replacement_web_app/good_web_app/index.html";

  RunInstallableWebAppTest(kManifest, kGoodWebAppURL, kGoodWebAppURL);
}

// Check that web app still installs and launches correctly when start_url does
// not match replacement_web_app_url.
IN_PROC_BROWSER_TEST_F(InstallReplacementWebAppApiTest,
                       InstallableWebAppWithStartUrl) {
  static constexpr char kGoodWebAppUrl[] =
      "/management/install_replacement_web_app/good_web_app_with_start_url/"
      "index.html";
  static constexpr char kGoodWebAppStartUrl[] =
      "/management/install_replacement_web_app/good_web_app_with_start_url/"
      "pwa_start_url.html";

  RunInstallableWebAppTest(kManifest, kGoodWebAppUrl, kGoodWebAppStartUrl);
}

IN_PROC_BROWSER_TEST_F(InstallReplacementWebAppApiTest,
                       InstallableWebAppInPlatformApp) {
  static constexpr char kGoodWebAppURL[] =
      "/management/install_replacement_web_app/good_web_app/index.html";

  RunInstallableWebAppTest(kAppManifest, kGoodWebAppURL, kGoodWebAppURL);
}

// Fails often on Windows dbg bots. http://crbug.com/177163
#if defined(OS_WIN)
#define MAYBE_ManagementPolicyAllowed DISABLED_ManagementPolicyAllowed
#else
#define MAYBE_ManagementPolicyAllowed ManagementPolicyAllowed
#endif  // defined(OS_WIN)
// Tests actions on extensions when no management policy is in place.
IN_PROC_BROWSER_TEST_F(ExtensionManagementApiTest,
                       MAYBE_ManagementPolicyAllowed) {
  LoadExtensions();
  extensions::ScopedTestDialogAutoConfirm auto_confirm(
      extensions::ScopedTestDialogAutoConfirm::ACCEPT);
  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(browser()->profile());
  EXPECT_TRUE(registry->enabled_extensions().GetByID(
      extension_ids_["enabled_extension"]));

  // Ensure that all actions are allowed.
  extensions::ExtensionSystem::Get(
      browser()->profile())->management_policy()->UnregisterAllProviders();

  ASSERT_TRUE(RunExtensionSubtest("management/management_policy",
                                  "allowed.html"));
  // The last thing the test does is uninstall the "enabled_extension".
  EXPECT_FALSE(
      registry->GetExtensionById(extension_ids_["enabled_extension"],
                                 extensions::ExtensionRegistry::EVERYTHING));
}

// Fails often on Windows dbg bots. http://crbug.com/177163
#if defined(OS_WIN)
#define MAYBE_ManagementPolicyProhibited DISABLED_ManagementPolicyProhibited
#else
#define MAYBE_ManagementPolicyProhibited ManagementPolicyProhibited
#endif  // defined(OS_WIN)
// Tests actions on extensions when management policy prohibits those actions.
IN_PROC_BROWSER_TEST_F(ExtensionManagementApiTest,
                       MAYBE_ManagementPolicyProhibited) {
  LoadExtensions();
  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(browser()->profile());
  EXPECT_TRUE(registry->enabled_extensions().GetByID(
      extension_ids_["enabled_extension"]));

  // Prohibit status changes.
  extensions::ManagementPolicy* policy = extensions::ExtensionSystem::Get(
      browser()->profile())->management_policy();
  policy->UnregisterAllProviders();
  extensions::TestManagementPolicyProvider provider(
      extensions::TestManagementPolicyProvider::PROHIBIT_MODIFY_STATUS |
      extensions::TestManagementPolicyProvider::MUST_REMAIN_ENABLED |
      extensions::TestManagementPolicyProvider::MUST_REMAIN_INSTALLED);
  policy->RegisterProvider(&provider);
  ASSERT_TRUE(RunExtensionSubtest("management/management_policy",
                                  "prohibited.html"));
}

IN_PROC_BROWSER_TEST_F(ExtensionManagementApiTest, LaunchPanelApp) {
  // Load an extension that calls launchApp() on any app that gets
  // installed.
  ExtensionTestMessageListener launcher_loaded("launcher loaded", false);
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("management/launch_on_install")));
  ASSERT_TRUE(launcher_loaded.WaitUntilSatisfied());

  // Load an app with app.launch.container = "panel".
  std::string app_id;
  LoadAndWaitForLaunch("management/launch_app_panel", &app_id);
  ASSERT_FALSE(HasFatalFailure());  // Stop the test if any ASSERT failed.

  // Find the app's browser.  Check that it is a popup.
  ASSERT_EQ(2u, chrome::GetBrowserCount(browser()->profile()));
  Browser* app_browser = FindOtherBrowser(browser());
  ASSERT_TRUE(app_browser->is_type_app());

  // Close the app panel.
  CloseBrowserSynchronously(app_browser);

  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(browser()->profile());
  // Unload the extension.
  UninstallExtension(app_id);
  ASSERT_EQ(1u, chrome::GetBrowserCount(browser()->profile()));
  ASSERT_FALSE(registry->GetExtensionById(
      app_id, extensions::ExtensionRegistry::EVERYTHING));

  // Set a pref indicating that the user wants to launch in a regular tab.
  // This should be ignored, because panel apps always load in a popup.
  extensions::SetLaunchType(browser()->profile(), app_id,
                            extensions::LAUNCH_TYPE_REGULAR);

  // Load the extension again.
  std::string app_id_new;
  LoadAndWaitForLaunch("management/launch_app_panel", &app_id_new);
  ASSERT_FALSE(HasFatalFailure());

  // If the ID changed, then the pref will not apply to the app.
  ASSERT_EQ(app_id, app_id_new);

  // Find the app's browser.  Apps that should load in a panel ignore
  // prefs, so we should still see the launch in a popup.
  ASSERT_EQ(2u, chrome::GetBrowserCount(browser()->profile()));
  app_browser = FindOtherBrowser(browser());
  ASSERT_TRUE(app_browser->is_type_app());
}

// Disabled: crbug.com/230165, crbug.com/915339, crbug.com/979399
#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_LINUX) || \
    defined(OS_CHROMEOS)
#define MAYBE_LaunchTabApp DISABLED_LaunchTabApp
#else
#define MAYBE_LaunchTabApp LaunchTabApp
#endif

IN_PROC_BROWSER_TEST_F(ExtensionManagementApiTest, MAYBE_LaunchTabApp) {
  // Load an extension that calls launchApp() on any app that gets
  // installed.
  ExtensionTestMessageListener launcher_loaded("launcher loaded", false);
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("management/launch_on_install")));
  ASSERT_TRUE(launcher_loaded.WaitUntilSatisfied());

  // Code below assumes that the test starts with a single browser window
  // hosting one tab.
  ASSERT_EQ(1u, chrome::GetBrowserCount(browser()->profile()));
  ASSERT_EQ(1, browser()->tab_strip_model()->count());

  // Load an app with app.launch.container = "tab".
  std::string app_id;
  LoadAndWaitForLaunch("management/launch_app_tab", &app_id);
  ASSERT_FALSE(HasFatalFailure());

  // Check that the app opened in a new tab of the existing browser.
  ASSERT_EQ(1u, chrome::GetBrowserCount(browser()->profile()));
  ASSERT_EQ(2, browser()->tab_strip_model()->count());

  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(browser()->profile());
  // Unload the extension.
  UninstallExtension(app_id);
  ASSERT_EQ(1u, chrome::GetBrowserCount(browser()->profile()));
  ASSERT_FALSE(registry->GetExtensionById(
      app_id, extensions::ExtensionRegistry::EVERYTHING));

  // Set a pref indicating that the user wants to launch in a window.
  extensions::SetLaunchType(browser()->profile(), app_id,
                            extensions::LAUNCH_TYPE_WINDOW);

  std::string app_id_new;
  LoadAndWaitForLaunch("management/launch_app_tab", &app_id_new);
  ASSERT_FALSE(HasFatalFailure());

  // If the ID changed, then the pref will not apply to the app.
  ASSERT_EQ(app_id, app_id_new);

  // Find the app's browser.  Opening in a new window will create
  // a new browser.
  ASSERT_EQ(2u, chrome::GetBrowserCount(browser()->profile()));
  Browser* app_browser = FindOtherBrowser(browser());
  ASSERT_TRUE(app_browser->is_type_app());
}

// Flaky on MacOS: crbug.com/915339
#if defined(OS_MACOSX)
#define MAYBE_LaunchType DISABLED_LaunchType
#else
#define MAYBE_LaunchType LaunchType
#endif
IN_PROC_BROWSER_TEST_F(ExtensionManagementApiTest, MAYBE_LaunchType) {
  LoadExtensions();
  base::FilePath basedir = test_data_dir_.AppendASCII("management");
  LoadNamedExtension(basedir, "packaged_app");

  ASSERT_TRUE(RunExtensionSubtest("management/test", "launchType.html"));
}
