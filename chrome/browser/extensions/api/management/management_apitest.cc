// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>

#include "base/auto_reset.h"
#include "base/strings/stringprintf.h"
#include "base/test/gtest_tags.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/browsertest_util.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/launch_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/web_app_dialogs.h"
#include "chrome/browser/web_applications/extension_status_utils.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/proto/web_app_install_state.pb.h"
#include "chrome/browser/web_applications/test/fake_web_app_ui_manager.h"
#include "chrome/browser/web_applications/test/os_integration_test_override_impl.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_constants.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
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
#include "testing/gtest/include/gtest/gtest.h"

using extensions::Extension;
using extensions::Manifest;
using extensions::mojom::ManifestLocation;

namespace {

#if !BUILDFLAG(IS_CHROMEOS_LACROS)
// Find a browser other than |browser|.
Browser* FindOtherBrowser(Browser* browser) {
  Browser* found = nullptr;
  for (Browser* b : *BrowserList::GetInstance()) {
    if (b == browser)
      continue;
    found = b;
  }
  return found;
}

bool ExpectChromeAppsDefaultEnabled() {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  return false;
#else
  return true;
#endif
}
#endif  // !BUILDFLAG(IS_CHROMEOS_LACROS)

}  // namespace

using ContextType = extensions::ExtensionBrowserTest::ContextType;

class ExtensionManagementApiTest
    : public extensions::ExtensionApiTest,
      public testing::WithParamInterface<ContextType> {
 public:
  ExtensionManagementApiTest()
      : ExtensionApiTest(GetParam()),
        enable_chrome_apps_(
            &extensions::testing::g_enable_chrome_apps_for_testing,
            true) {}
  ~ExtensionManagementApiTest() override = default;
  ExtensionManagementApiTest& operator=(const ExtensionManagementApiTest&) =
      delete;
  ExtensionManagementApiTest(const ExtensionManagementApiTest&) = delete;

 protected:
  void LoadExtensions() {
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
    ExtensionTestMessageListener launched_app("launched app");
    ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII(app_path),
                              {.context_type = ContextType::kFromManifest}));

    if (out_app_id)
      *out_app_id = last_loaded_extension_id();

    ASSERT_TRUE(launched_app.WaitUntilSatisfied());
  }

  void LoadNamedExtension(const base::FilePath& path,
                          const std::string& name) {
    const Extension* extension = LoadExtension(
        path.AppendASCII(name), {.context_type = ContextType::kFromManifest});
    ASSERT_TRUE(extension);
    extension_ids_[name] = extension->id();
  }

  void InstallNamedExtension(const base::FilePath& path,
                             const std::string& name,
                             ManifestLocation install_source) {
    const Extension* extension = InstallExtension(path.AppendASCII(name), 1,
                                                  install_source);
    ASSERT_TRUE(extension);
    extension_ids_[name] = extension->id();
  }

  // Maps installed extension names to their IDs.
  std::map<std::string, std::string> extension_ids_;

 protected:
  base::AutoReset<bool> enable_chrome_apps_;
  web_app::OsIntegrationTestOverrideBlockingRegistration faked_os_integration_;
};

INSTANTIATE_TEST_SUITE_P(PersistentBackground,
                         ExtensionManagementApiTest,
                         ::testing::Values(ContextType::kPersistentBackground));
INSTANTIATE_TEST_SUITE_P(ServiceWorker,
                         ExtensionManagementApiTest,
                         ::testing::Values(ContextType::kServiceWorker));

IN_PROC_BROWSER_TEST_P(ExtensionManagementApiTest, Basics) {
  base::AddFeatureIdTagToTestResult(
      "screenplay-7a245632-83b2-4dc8-a1db-283ef595e2df");

  LoadExtensions();

  base::FilePath basedir = test_data_dir_.AppendASCII("management");
  InstallNamedExtension(basedir, "internal_extension",
                        ManifestLocation::kInternal);
  InstallNamedExtension(basedir, "external_extension",
                        ManifestLocation::kExternalPref);
  InstallNamedExtension(basedir, "admin_extension",
                        ManifestLocation::kExternalPolicyDownload);
  InstallNamedExtension(basedir, "version_name", ManifestLocation::kInternal);

  ASSERT_TRUE(RunExtensionTest("management/basics"));
}

IN_PROC_BROWSER_TEST_P(ExtensionManagementApiTest, NoPermission) {
  LoadExtensions();
  ASSERT_TRUE(RunExtensionTest("management/no_permission"));
}

IN_PROC_BROWSER_TEST_P(ExtensionManagementApiTest, Uninstall) {
  LoadExtensions();
  // Confirmation dialog will be shown for uninstallations except for self.
  extensions::ScopedTestDialogAutoConfirm auto_confirm(
      extensions::ScopedTestDialogAutoConfirm::ACCEPT);
  ASSERT_TRUE(RunExtensionTest("management/uninstall"));
}

IN_PROC_BROWSER_TEST_P(ExtensionManagementApiTest, CreateAppShortcut) {
  LoadExtensions();
  base::FilePath basedir = test_data_dir_.AppendASCII("management");
  LoadNamedExtension(basedir, "packaged_app");

  extensions::ManagementCreateAppShortcutFunction::SetAutoConfirmForTest(true);
  ASSERT_TRUE(RunExtensionTest("management/create_app_shortcut"));
}

IN_PROC_BROWSER_TEST_P(ExtensionManagementApiTest, GenerateAppForLink) {
  web_app::test::WaitUntilReady(web_app::WebAppProvider::GetForTest(profile()));
  ASSERT_TRUE(RunExtensionTest("management/generate_app_for_link"));
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

    web_app::test::WaitUntilReady(
        web_app::WebAppProvider::GetForTest(profile()));
  }

  void RunTest(const char* manifest,
               const char* web_app_path,
               const char* background_script,
               bool from_webstore) {
    extensions::TestExtensionDir extension_dir;
    extension_dir.WriteManifest(base::StringPrintfNonConstexpr(
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

    web_app::SetAutoAcceptPWAInstallConfirmationForTesting(true);
    const GURL start_url = https_test_server_.GetURL(web_app_start_url);
    webapps::AppId web_app_id =
        web_app::GenerateAppId(/*manifest_id_path=*/std::nullopt, start_url);
    auto* provider = web_app::WebAppProvider::GetForTest(browser()->profile());
    EXPECT_TRUE(provider->registrar_unsafe().IsNotInRegistrar(web_app_id));
    EXPECT_EQ(0, static_cast<int>(
                     provider->ui_manager().GetNumWindowsForApp(web_app_id)));

    RunTest(manifest, web_app_url, kInstallReplacementWebApp,
            true /* from_webstore */);
    EXPECT_EQ(provider->registrar_unsafe().GetInstallState(web_app_id),
              web_app::proto::INSTALLED_WITH_OS_INTEGRATION);
    EXPECT_EQ(1, static_cast<int>(
                     provider->ui_manager().GetNumWindowsForApp(web_app_id)));

    // Call API again. It should launch the app.
    RunTest(manifest, web_app_url, kInstallReplacementWebApp,
            true /* from_webstore */);
    EXPECT_EQ(provider->registrar_unsafe().GetInstallState(web_app_id),
              web_app::proto::INSTALLED_WITH_OS_INTEGRATION);
    EXPECT_EQ(2, static_cast<int>(
                     provider->ui_manager().GetNumWindowsForApp(web_app_id)));

    web_app::SetAutoAcceptPWAInstallConfirmationForTesting(false);
  }

  net::EmbeddedTestServer https_test_server_;
};

INSTANTIATE_TEST_SUITE_P(PersistentBackground,
                         InstallReplacementWebAppApiTest,
                         ::testing::Values(ContextType::kPersistentBackground));
INSTANTIATE_TEST_SUITE_P(ServiceWorker,
                         InstallReplacementWebAppApiTest,
                         ::testing::Values(ContextType::kServiceWorker));

const char InstallReplacementWebAppApiTest::kManifest[] =
    R"({
          "name": "Management API Test",
          "version": "0.1",
          "manifest_version": 2,
          "background": {
            "scripts": ["background.js"],
            "persistent": true
          },
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

IN_PROC_BROWSER_TEST_P(InstallReplacementWebAppApiTest, NotWebstore) {
  static constexpr char kBackground[] = R"(
  chrome.management.installReplacementWebApp(function() {
    chrome.test.assertLastError(
        'Only extensions from the web store can install replacement web apps.');
    chrome.test.notifyPass();
  });)";

  RunTest(
      kManifest,
      "/management/install_replacement_web_app/acceptable_web_app/index.html",
      kBackground, false /* from_webstore */);
}

IN_PROC_BROWSER_TEST_P(InstallReplacementWebAppApiTest, NoGesture) {
  static constexpr char kBackground[] = R"(
  chrome.management.installReplacementWebApp(function() {
    chrome.test.assertLastError(
        'chrome.management.installReplacementWebApp requires a user gesture.');
    chrome.test.notifyPass();
  });)";

  RunTest(
      kManifest,
      "/management/install_replacement_web_app/acceptable_web_app/index.html",
      kBackground, true /* from_webstore */);
}

IN_PROC_BROWSER_TEST_P(InstallReplacementWebAppApiTest, NotInstallableWebApp) {
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

#if !BUILDFLAG(IS_CHROMEOS_LACROS)
// TODO(crbug.com/40211465): Run these tests on Chrome OS with both Ash and
// Lacros processes active.
IN_PROC_BROWSER_TEST_P(InstallReplacementWebAppApiTest, InstallableWebApp) {
  static constexpr char kGoodWebAppURL[] =
      "/management/install_replacement_web_app/acceptable_web_app/index.html";

  RunInstallableWebAppTest(kManifest, kGoodWebAppURL, kGoodWebAppURL);
}
#endif

#if !BUILDFLAG(IS_CHROMEOS_LACROS)
// TODO(crbug.com/40211465): Run these tests on Chrome OS with both Ash and
// Lacros processes active.

// Check that web app still installs and launches correctly when start_url does
// not match replacement_web_app_url.
IN_PROC_BROWSER_TEST_P(InstallReplacementWebAppApiTest,
                       InstallableWebAppWithStartUrl) {
  static constexpr char kGoodWebAppUrl[] =
      "/management/install_replacement_web_app/"
      "acceptable_web_app_with_start_url/"
      "index.html";
  static constexpr char kGoodWebAppStartUrl[] =
      "/management/install_replacement_web_app/"
      "acceptable_web_app_with_start_url/"
      "pwa_start_url.html";

  RunInstallableWebAppTest(kManifest, kGoodWebAppUrl, kGoodWebAppStartUrl);
}

IN_PROC_BROWSER_TEST_P(InstallReplacementWebAppApiTest,
                       InstallableWebAppInPlatformApp) {
  static constexpr char kGoodWebAppURL[] =
      "/management/install_replacement_web_app/acceptable_web_app/index.html";

  RunInstallableWebAppTest(kAppManifest, kGoodWebAppURL, kGoodWebAppURL);
}
#endif

// Tests actions on extensions when no management policy is in place.
IN_PROC_BROWSER_TEST_P(ExtensionManagementApiTest, ManagementPolicyAllowed) {
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

  ASSERT_TRUE(RunExtensionTest("management/management_policy",
                               {.custom_arg = "runAllowedTests"}));
  // The last thing the test does is uninstall the "enabled_extension".
  EXPECT_FALSE(
      registry->GetExtensionById(extension_ids_["enabled_extension"],
                                 extensions::ExtensionRegistry::EVERYTHING));
}

// Tests actions on extensions when management policy prohibits those actions.
IN_PROC_BROWSER_TEST_P(ExtensionManagementApiTest, ManagementPolicyProhibited) {
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
  ASSERT_TRUE(RunExtensionTest("management/management_policy",
                               {.custom_arg = "runProhibitedTests"}));
}

#if !BUILDFLAG(IS_CHROMEOS_LACROS)
// TODO(crbug.com/40211465): Run these tests on Chrome OS with both Ash and
// Lacros processes active.

IN_PROC_BROWSER_TEST_P(ExtensionManagementApiTest, LaunchPanelApp) {
  // Load an extension that calls launchApp() on any app that gets
  // installed.
  ExtensionTestMessageListener launcher_loaded("launcher loaded");
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("management/launch_on_install")));
  ASSERT_TRUE(launcher_loaded.WaitUntilSatisfied());

  // Load an app with app.launch.container = "panel".
  std::string app_id;
  LoadAndWaitForLaunch("management/launch_app_panel", &app_id);
  ASSERT_FALSE(HasFatalFailure());  // Stop the test if any ASSERT failed.

  // Find the app's browser.  Check that it is a popup.
  ASSERT_EQ(2u, extensions::browsertest_util::GetWindowControllerCountInProfile(
                    browser()->profile()));
  Browser* app_browser = FindOtherBrowser(browser());
  ASSERT_TRUE(app_browser->is_type_app());

  // Close the app panel.
  CloseBrowserSynchronously(app_browser);

  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(browser()->profile());
  // Unload the extension.
  UninstallExtension(app_id);
  ASSERT_EQ(1u, extensions::browsertest_util::GetWindowControllerCountInProfile(
                    browser()->profile()));
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
  ASSERT_EQ(2u, extensions::browsertest_util::GetWindowControllerCountInProfile(
                    browser()->profile()));
  app_browser = FindOtherBrowser(browser());
  ASSERT_TRUE(app_browser->is_type_app());
}

IN_PROC_BROWSER_TEST_P(ExtensionManagementApiTest, LaunchTabApp) {
  // Load an extension that calls launchApp() on any app that gets
  // installed.
  ExtensionTestMessageListener launcher_loaded("launcher loaded");
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("management/launch_on_install")));
  ASSERT_TRUE(launcher_loaded.WaitUntilSatisfied());

  // Code below assumes that the test starts with a single browser window
  // hosting one tab.
  ASSERT_EQ(1u, extensions::browsertest_util::GetWindowControllerCountInProfile(
                    browser()->profile()));
  ASSERT_EQ(1, browser()->tab_strip_model()->count());

  // Load an app with app.launch.container = "tab".
  std::string app_id;
  LoadAndWaitForLaunch("management/launch_app_tab", &app_id);
  ASSERT_FALSE(HasFatalFailure());

  // Check that the app opened in a new tab of the existing browser.
  ASSERT_EQ(1u, extensions::browsertest_util::GetWindowControllerCountInProfile(
                    browser()->profile()));
  ASSERT_EQ(2, browser()->tab_strip_model()->count());

  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(browser()->profile());
  // Unload the extension.
  UninstallExtension(app_id);
  ASSERT_EQ(1u, extensions::browsertest_util::GetWindowControllerCountInProfile(
                    browser()->profile()));
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
  ASSERT_EQ(2u, extensions::browsertest_util::GetWindowControllerCountInProfile(
                    browser()->profile()));
  Browser* app_browser = FindOtherBrowser(browser());
  ASSERT_TRUE(app_browser->is_type_app());
}

IN_PROC_BROWSER_TEST_P(ExtensionManagementApiTest,
                       NoLaunchPanelAppsDeprecated) {
  extensions::testing::g_enable_chrome_apps_for_testing = false;
  // Load an extension that calls launchApp() on any app that gets
  // installed.
  ExtensionTestMessageListener launcher_loaded("launcher loaded");
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("management/launch_on_install")));
  ASSERT_TRUE(launcher_loaded.WaitUntilSatisfied());

  // Load an app with app.launch.container = "panel". This is a chrome app, so
  // it shouldn't be launched where that functionality has been deprecated.
  ExtensionTestMessageListener launched_app("launched app");
  ExtensionTestMessageListener chrome_apps_error("got_chrome_apps_error");
  ASSERT_TRUE(
      LoadExtension(test_data_dir_.AppendASCII("management/launch_app_panel"),
                    {.context_type = ContextType::kFromManifest}));
  if (ExpectChromeAppsDefaultEnabled()) {
    EXPECT_TRUE(launched_app.WaitUntilSatisfied());
    EXPECT_FALSE(chrome_apps_error.was_satisfied());
  } else {
    EXPECT_TRUE(chrome_apps_error.WaitUntilSatisfied());
    EXPECT_FALSE(launched_app.was_satisfied());
  }
}

IN_PROC_BROWSER_TEST_P(ExtensionManagementApiTest, NoLaunchTabAppDeprecated) {
  extensions::testing::g_enable_chrome_apps_for_testing = false;
  // Load an extension that calls launchApp() on any app that gets
  // installed.
  ExtensionTestMessageListener launcher_loaded("launcher loaded");
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("management/launch_on_install")));
  ASSERT_TRUE(launcher_loaded.WaitUntilSatisfied());

  // Code below assumes that the test starts with a single browser window
  // hosting one tab.
  ASSERT_EQ(1u, extensions::browsertest_util::GetWindowControllerCountInProfile(
                    browser()->profile()));
  ASSERT_EQ(1, browser()->tab_strip_model()->count());

  // Load an app with app.launch.container = "tab". This is a chrome app, so
  // it shouldn't be launched where that functionality has been deprecated.
  ExtensionTestMessageListener launched_app("launched app");
  ExtensionTestMessageListener chrome_apps_error("got_chrome_apps_error");
  ASSERT_TRUE(
      LoadExtension(test_data_dir_.AppendASCII("management/launch_app_tab"),
                    {.context_type = ContextType::kFromManifest}));
  if (ExpectChromeAppsDefaultEnabled()) {
    EXPECT_TRUE(launched_app.WaitUntilSatisfied());
    EXPECT_FALSE(chrome_apps_error.was_satisfied());
  } else {
    EXPECT_TRUE(chrome_apps_error.WaitUntilSatisfied());
    EXPECT_FALSE(launched_app.was_satisfied());
  }
}
#endif  // !BUILDFLAG(IS_CHROMEOS_LACROS)

// Flaky on MacOS: crbug.com/915339
#if BUILDFLAG(IS_MAC)
#define MAYBE_LaunchType DISABLED_LaunchType
#else
#define MAYBE_LaunchType LaunchType
#endif
IN_PROC_BROWSER_TEST_P(ExtensionManagementApiTest, MAYBE_LaunchType) {
  LoadExtensions();
  base::FilePath basedir = test_data_dir_.AppendASCII("management");
  LoadNamedExtension(basedir, "packaged_app");

  ASSERT_TRUE(RunExtensionTest("management/launch_type"));
}
