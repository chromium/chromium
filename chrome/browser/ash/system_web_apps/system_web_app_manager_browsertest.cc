// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/shelf_item_delegate.h"
#include "ash/public/cpp/shelf_model.h"
#include "ash/webui/system_apps/public/system_web_app_type.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/gtest_tags.h"
#include "base/test/scoped_feature_list.h"
#include "build/chromeos_buildflags.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/apps/app_service/app_icon/app_icon_factory.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/browser_app_launcher.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "chrome/browser/ash/accessibility/speech_monitor.h"
#include "chrome/browser/ash/app_list/app_list_client_impl.h"
#include "chrome/browser/ash/app_list/app_list_model_updater.h"
#include "chrome/browser/ash/app_list/test/chrome_app_list_test_support.h"
#include "chrome/browser/ash/extensions/default_app_order.h"
#include "chrome/browser/ash/file_manager/file_manager_test_util.h"
#include "chrome/browser/ash/file_manager/volume.h"
#include "chrome/browser/ash/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/ash/system_web_apps/test_support/system_web_app_browsertest_base.h"
#include "chrome/browser/ash/system_web_apps/test_support/test_system_web_app_installation.h"
#include "chrome/browser/file_system_access/file_system_access_permission_request_manager.h"
#include "chrome/browser/policy/system_features_disable_list_policy_handler.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_test_util.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller_util.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/web_applications/proto/web_app.pb.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_tab_helper.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/permissions/permission_util.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/app_update.h"
#include "components/services/app_service/public/cpp/intent_util.h"
#include "components/services/app_service/public/cpp/types_util.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/test_launcher.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/browsertest_util.h"
#include "extensions/common/constants.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/context_menu/context_menu.mojom.h"
#include "ui/base/idle/idle.h"
#include "ui/base/idle/scoped_set_idle_state.h"
#include "ui/display/display.h"
#include "ui/display/types/display_constants.h"
#include "ui/events/test/event_generator.h"

namespace ash {

namespace {

// Helper to call AppServiceProxyFactory::GetForProfile().
apps::AppServiceProxyBase* GetAppServiceProxy(Profile* profile) {
  // Crash if there is no AppService support for |profile|. GetForProfile() will
  // DumpWithoutCrashing, which will not fail a test. No codepath should trigger
  // that in normal operation.
  DCHECK(
      apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(profile));
  return apps::AppServiceProxyFactory::GetForProfile(profile);
}

}  // namespace

using SystemWebAppManagerBrowserTestBasicInstall =
    SystemWebAppManagerBrowserTest;

// Test that System Apps install correctly with a manifest.
IN_PROC_BROWSER_TEST_P(SystemWebAppManagerBrowserTestBasicInstall, Install) {
  WaitForTestSystemAppInstall();

  // Don't wait for page load because we want to verify AppController identifies
  // the System Web App before when the app loads.
  Browser* app_browser;
  LaunchAppWithoutWaiting(GetAppType(), &app_browser);

  webapps::AppId app_id = app_browser->app_controller()->app_id();
  EXPECT_EQ(GetManager().GetAppIdForSystemApp(GetAppType()), app_id);
  EXPECT_TRUE(GetManager().IsSystemWebApp(app_id));

  Profile* profile = app_browser->profile();
  web_app::WebAppRegistrar& registrar =
      web_app::WebAppProvider::GetForTest(profile)->registrar_unsafe();

  EXPECT_EQ("Test System App", registrar.GetAppShortName(app_id));
  EXPECT_EQ(SkColorSetRGB(0, 0xFF, 0), registrar.GetAppThemeColor(app_id));
  EXPECT_TRUE(registrar.HasExternalAppWithInstallSource(
      app_id, web_app::ExternalInstallSource::kSystemInstalled));
  EXPECT_EQ(
      registrar.FindAppWithUrlInScope(content::GetWebUIURL("test-system-app/")),
      app_id);

  GetAppServiceProxy(browser()->profile())
      ->AppRegistryCache()
      .ForOneApp(app_id, [](const apps::AppUpdate& update) {
        EXPECT_TRUE(update.ShowInLauncher().value_or(false));
        EXPECT_TRUE(update.ShowInSearch().value_or(false));
        EXPECT_FALSE(update.ShowInManagement().value_or(true));
        EXPECT_EQ(apps::Readiness::kReady, update.Readiness());
      });
}

// Check the toolbar is not shown for system web apps for pages on the chrome://
// scheme but is shown off the chrome:// scheme.
IN_PROC_BROWSER_TEST_P(SystemWebAppManagerBrowserTest,
                       ToolbarVisibilityForSystemWebApp) {
  WaitForTestSystemAppInstall();

  // Don't wait for page load because we want to verify the toolbar is hidden
  // when the window first opens.
  Browser* app_browser;
  LaunchAppWithoutWaiting(GetAppType(), &app_browser);

  // In scope, the toolbar should not be visible.
  EXPECT_FALSE(app_browser->app_controller()->ShouldShowCustomTabBar());

  // Out of scope chrome:// URL.
  GURL out_of_scope_chrome_page("chrome://foo");
  content::NavigateToURLBlockUntilNavigationsComplete(
      app_browser->tab_strip_model()->GetActiveWebContents(),
      out_of_scope_chrome_page, 1);
  EXPECT_TRUE(app_browser->app_controller()->ShouldShowCustomTabBar());

  // Even though the url is secure it is not being served over chrome:// so a
  // toolbar should be shown.
  GURL off_scheme_page("https://example.com");
  content::NavigateToURLBlockUntilNavigationsComplete(
      app_browser->tab_strip_model()->GetActiveWebContents(), off_scheme_page,
      1);
  EXPECT_TRUE(app_browser->app_controller()->ShouldShowCustomTabBar());

  // URL has been added to be within scope for the SWA.
  GURL in_scope_for_swa_page("http://example.com/in-scope");
  content::NavigateToURLBlockUntilNavigationsComplete(
      app_browser->tab_strip_model()->GetActiveWebContents(),
      in_scope_for_swa_page, 1);
  EXPECT_FALSE(app_browser->app_controller()->ShouldShowCustomTabBar());
}

IN_PROC_BROWSER_TEST_P(SystemWebAppManagerBrowserTest, LaunchMetricsWork) {
  WaitForTestSystemAppInstall();

  base::HistogramTester histograms;

  content::TestNavigationObserver navigation_observer(GetStartUrl());
  navigation_observer.StartWatchingNewWebContents();

  ash::SystemAppLaunchParams params;
  params.launch_source = apps::LaunchSource::kFromAppListGrid;
  LaunchSystemWebAppAsync(browser()->profile(), GetAppType(), params);

  navigation_observer.Wait();
  histograms.ExpectTotalCount("Apps.DefaultAppLaunch.FromAppListGrid", 1);
  histograms.ExpectUniqueSample("Apps.DefaultAppLaunch.FromAppListGrid", 39, 1);
}

IN_PROC_BROWSER_TEST_P(SystemWebAppManagerBrowserTest,
                       LaunchMetricsWorkFromAppProxy) {
  WaitForTestSystemAppInstall();

  base::HistogramTester histograms;
  content::TestNavigationObserver navigation_observer(GetStartUrl());
  navigation_observer.StartWatchingNewWebContents();

  auto* proxy = GetAppServiceProxy(browser()->profile());

  proxy->Launch(GetManager().GetAppIdForSystemApp(GetAppType()).value(),
                ui::EF_NONE, apps::LaunchSource::kFromAppListGrid,
                std::make_unique<apps::WindowInfo>(display::kDefaultDisplayId));
  navigation_observer.Wait();

  histograms.ExpectTotalCount("Apps.DefaultAppLaunch.FromAppListGrid", 1);
  histograms.ExpectUniqueSample("Apps.DefaultAppLaunch.FromAppListGrid", 39, 1);
}

IN_PROC_BROWSER_TEST_P(SystemWebAppManagerBrowserTest,
                       LaunchMetricsWorkWithIntent) {
  WaitForTestSystemAppInstall();

  base::HistogramTester histograms;
  content::TestNavigationObserver navigation_observer(GetStartUrl());
  navigation_observer.StartWatchingNewWebContents();

  auto* proxy = GetAppServiceProxy(browser()->profile());
  auto intent = std::make_unique<apps::Intent>(apps_util::kIntentActionView);
  intent->mime_type = "text/plain";

  proxy->LaunchAppWithIntent(
      GetManager().GetAppIdForSystemApp(GetAppType()).value(), ui::EF_NONE,
      std::move(intent), apps::LaunchSource::kFromAppListGrid,
      std::make_unique<apps::WindowInfo>(display::kDefaultDisplayId),
      base::DoNothing());
  navigation_observer.Wait();

  histograms.ExpectTotalCount("Apps.DefaultAppLaunch.FromAppListGrid", 1);
  histograms.ExpectUniqueSample("Apps.DefaultAppLaunch.FromAppListGrid", 39, 1);
}

IN_PROC_BROWSER_TEST_P(SystemWebAppManagerBrowserTest, UpdatesLaunchStats) {
  WaitForTestSystemAppInstall();
  auto app_id = GetManager().GetAppIdForSystemApp(GetAppType()).value();

  content::TestNavigationObserver navigation_observer(GetStartUrl());
  navigation_observer.StartWatchingNewWebContents();

  base::Time launch_start_time = base::Time::Now();

  ash::SystemAppLaunchParams params;
  params.launch_source = apps::LaunchSource::kFromAppListGrid;
  LaunchSystemWebAppAsync(browser()->profile(), GetAppType(), params);

  navigation_observer.Wait();

  auto* proxy = GetAppServiceProxy(browser()->profile());
  EXPECT_TRUE(proxy->AppRegistryCache().ForOneApp(
      app_id,
      [&](const apps::AppUpdate& update) {
        EXPECT_GE(update.LastLaunchTime(), launch_start_time);
      }))
      << "Expect app to exist";
}

class SystemWebAppManagerLaunchWithUrlBrowserTest
    : public TestProfileTypeMixin<SystemWebAppBrowserTestBase> {
 public:
  SystemWebAppManagerLaunchWithUrlBrowserTest() {
    SetSystemWebAppInstallation(
        TestSystemWebAppInstallation::SetUpAppLaunchWithUrl());
  }
};

IN_PROC_BROWSER_TEST_P(SystemWebAppManagerLaunchWithUrlBrowserTest,
                       LaunchWithCallback) {
  WaitForTestSystemAppInstall();
  content::TestNavigationObserver navigation_observer(GetStartUrl());
  navigation_observer.StartWatchingNewWebContents();
  ash::SystemAppLaunchParams params;
  params.launch_source = apps::LaunchSource::kFromOtherApp;
  params.url = GetStartUrl();
  bool is_called = false;
  LaunchSystemWebAppAsync(
      browser()->profile(), GetAppType(), params, nullptr,
      base::BindLambdaForTesting(
          [&is_called](apps::LaunchResult&& callback_result) {
            is_called = true;
          }));
  navigation_observer.Wait();
  EXPECT_TRUE(is_called);
}

class SystemWebAppManagerFileHandlingBrowserTestBase
    : public TestProfileTypeMixin<SystemWebAppBrowserTestBase> {
 public:
  using IncludeLaunchDirectory =
      TestSystemWebAppInstallation::IncludeLaunchDirectory;

  explicit SystemWebAppManagerFileHandlingBrowserTestBase(
      IncludeLaunchDirectory include_launch_directory) {
    SetSystemWebAppInstallation(
        TestSystemWebAppInstallation::SetUpAppThatReceivesLaunchFiles(
            include_launch_directory));
  }

  content::WebContents* LaunchApp(std::vector<base::FilePath> launch_files,
                                  bool wait_for_load = true) {
    apps::AppLaunchParams params = LaunchParamsForApp(GetAppType());
    params.launch_source = apps::LaunchSource::kFromChromeInternal;
    params.override_url = GetStartUrl();
    params.launch_files = std::move(launch_files);

    return SystemWebAppBrowserTestBase::LaunchApp(std::move(params));
  }

  content::WebContents* LaunchAppWithoutWaiting(
      std::vector<base::FilePath> launch_files) {
    apps::AppLaunchParams params = LaunchParamsForApp(GetAppType());
    params.launch_source = apps::LaunchSource::kFromChromeInternal;
    params.override_url = GetStartUrl();
    params.launch_files = std::move(launch_files);

    return SystemWebAppBrowserTestBase::LaunchAppWithoutWaiting(
        std::move(params));
  }

  // Must be called before WaitAndExposeLaunchParamsToWindow. This sets up the
  // promise used to wait for launchParam callback.
  [[nodiscard]] ::testing::AssertionResult PrepareToReceiveLaunchParams(
      content::WebContents* web_contents) {
    return content::ExecJs(
        web_contents,
        "window.launchParamsPromise = new Promise(resolve => {"
        "  window.resolveLaunchParamsPromise = resolve;"
        "});"
        "launchQueue.setConsumer(launchParams => {"
        "  window.resolveLaunchParamsPromise(launchParams);"
        "  window.resolveLaunchParamsPromise = null;"
        "});");
  }

  // Must be called after PrepareToReceiveLaunchParams. This method waits for
  // launchParams being received, the stores it to a |js_property_name| on JS
  // window object.
  [[nodiscard]] ::testing::AssertionResult WaitAndExposeLaunchParamsToWindow(
      content::WebContents* web_contents,
      const std::string js_property_name = "launchParams") {
    return content::ExecJs(
        web_contents,
        content::JsReplace("window.launchParamsPromise.then(launchParams => {"
                           "  window[$1] = launchParams"
                           "})",
                           js_property_name));
  }

 private:
  base::test::ScopedFeatureList scoped_feature_web_app_provider_type_;
};

class SystemWebAppManagerLaunchFilesBrowserTest
    : public SystemWebAppManagerFileHandlingBrowserTestBase {
 public:
  SystemWebAppManagerLaunchFilesBrowserTest()
      : SystemWebAppManagerFileHandlingBrowserTestBase(
            IncludeLaunchDirectory::kNo) {}
};

// Check launch files are passed to application.
IN_PROC_BROWSER_TEST_P(SystemWebAppManagerLaunchFilesBrowserTest,
                       LaunchFilesForSystemWebApp) {
  WaitForTestSystemAppInstall();

  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir temp_directory;
  ASSERT_TRUE(temp_directory.CreateUniqueTempDir());
  base::FilePath temp_file_path;
  ASSERT_TRUE(base::CreateTemporaryFileInDir(temp_directory.GetPath(),
                                             &temp_file_path));

  // First launch.
  content::WebContents* web_contents = LaunchApp({temp_file_path});

  // Check the App is launched with the correct launch file.
  EXPECT_TRUE(PrepareToReceiveLaunchParams(web_contents));
  EXPECT_TRUE(WaitAndExposeLaunchParamsToWindow(web_contents, "launchParams1"));
  EXPECT_EQ(
      temp_file_path.BaseName().AsUTF8Unsafe(),
      content::EvalJs(web_contents, "window.launchParams1.files[0].name"));

  // Second launch.
  base::FilePath temp_file_path2;
  ASSERT_TRUE(base::CreateTemporaryFileInDir(temp_directory.GetPath(),
                                             &temp_file_path2));

  // The second launch reuses the opened application. It should pass the
  // launchParams to the opened page, and return the same content::WebContents*.
  EXPECT_TRUE(PrepareToReceiveLaunchParams(web_contents));
  EXPECT_EQ(web_contents, LaunchAppWithoutWaiting({temp_file_path2}));
  EXPECT_TRUE(WaitAndExposeLaunchParamsToWindow(web_contents, "launchParams2"));

  // Second launch_files are correct.
  EXPECT_EQ(
      temp_file_path2.BaseName().AsUTF8Unsafe(),
      content::EvalJs(web_contents, "window.launchParams2.files[0].name"));
}

IN_PROC_BROWSER_TEST_P(SystemWebAppManagerLaunchFilesBrowserTest,
                       LaunchMetricsWorks) {
  WaitForTestSystemAppInstall();

  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir temp_directory;
  ASSERT_TRUE(temp_directory.CreateUniqueTempDir());
  base::FilePath temp_file_path;
  ASSERT_TRUE(base::CreateTemporaryFileInDir(temp_directory.GetPath(),
                                             &temp_file_path));

  base::HistogramTester histograms;

  content::TestNavigationObserver navigation_observer(GetStartUrl());
  navigation_observer.StartWatchingNewWebContents();

  ash::SystemAppLaunchParams params;
  params.launch_paths = {temp_file_path};
  params.launch_source = apps::LaunchSource::kFromOtherApp;
  LaunchSystemWebAppAsync(browser()->profile(), GetAppType(), params);

  navigation_observer.Wait();
  histograms.ExpectTotalCount("Apps.DefaultAppLaunch.FromOtherApp", 1);
}

class SystemWebAppManagerLaunchDirectoryBrowserTest
    : public SystemWebAppManagerFileHandlingBrowserTestBase {
 public:
  SystemWebAppManagerLaunchDirectoryBrowserTest()
      : SystemWebAppManagerFileHandlingBrowserTestBase(
            IncludeLaunchDirectory::kYes) {}

  // Returns the content of |file_handle_or_promise| file handle.
  [[nodiscard]] content::EvalJsResult ReadContentFromJsFileHandle(
      content::WebContents* web_contents,
      const std::string& file_handle_or_promise) {
    return content::EvalJs(web_contents,
                           "Promise.resolve(" + file_handle_or_promise + ")" +
                               ".then(async fileHandle => {"
                               "  const file = await fileHandle.getFile();"
                               "  return file.text();"
                               "})");
  }

  // Writes |content_to_write| to |file_handle_or_promise| file handle.
  [[nodiscard]] ::testing::AssertionResult WriteContentToJsFileHandle(
      content::WebContents* web_contents,
      const std::string& file_handle_or_promise,
      const std::string& content_to_write) {
    return content::ExecJs(
        web_contents,
        content::JsReplace(
            "Promise.resolve(" + file_handle_or_promise + ")" +
                ".then(async (fileHandle) => {"
                "  const writable = await fileHandle.createWritable();"
                "  await writable.write($1);"
                "  await writable.close();"
                "})",
            content_to_write));
  }

  // Remove file by |file_name| from |dir_handle_or_promise| directory handle.
  [[nodiscard]] ::testing::AssertionResult RemoveFileFromJsDirectoryHandle(
      content::WebContents* web_contents,
      const std::string& dir_handle_or_promise,
      const std::string& file_name) {
    return content::ExecJs(
        web_contents, content::JsReplace(
                          "Promise.resolve(" + dir_handle_or_promise + ")" +
                              ".then(dir_handle => dir_handle.removeEntry($1))",
                          file_name));
  }

  std::string ReadFileContent(const base::FilePath& path) {
    std::string content;
    EXPECT_TRUE(base::ReadFileToString(path, &content));
    return content;
  }

  // Launch the App with |base_dir| and a file inside this directory, then test
  // SWA can 1) read and write to the launch file; 2) read and write to other
  // files inside the launch directory; 3) read and write to the launch
  // directory (i.e. list and delete files).
  void TestPermissionsForLaunchDirectory(const base::FilePath& base_dir) {
    base::ScopedAllowBlockingForTesting allow_blocking;

    // Create the launch file, which stores 4 characters "test".
    base::FilePath launch_file_path;
    ASSERT_TRUE(base::CreateTemporaryFileInDir(base_dir, &launch_file_path));
    ASSERT_TRUE(base::WriteFile(launch_file_path, "test"));

    // Launch the App.
    content::WebContents* web_contents = LaunchApp({launch_file_path});

    // Launch directories and files passed to system web apps should
    // automatically be granted write permission. Users should not get
    // permission prompts. So we auto deny them (if they show up).
    FileSystemAccessPermissionRequestManager::FromWebContents(web_contents)
        ->set_auto_response_for_test(permissions::PermissionAction::DENIED);

    // Wait for launchParams.
    EXPECT_TRUE(PrepareToReceiveLaunchParams(web_contents));
    EXPECT_TRUE(WaitAndExposeLaunchParamsToWindow(web_contents));

    // Check we can read and write to the launch file.
    std::string launch_file_js_handle = "window.launchParams.files[1]";
    EXPECT_EQ("test",
              ReadContentFromJsFileHandle(web_contents, launch_file_js_handle));
    EXPECT_TRUE(WriteContentToJsFileHandle(web_contents, launch_file_js_handle,
                                           "js_written"));
    EXPECT_EQ("js_written", ReadFileContent(launch_file_path));

    // Check we can read and write to a different file inside the directory.
    // Note, this also checks we can read the launch directory, using
    // directory_handle.getFileHandle().
    base::FilePath non_launch_file_path;
    ASSERT_TRUE(
        base::CreateTemporaryFileInDir(base_dir, &non_launch_file_path));
    ASSERT_TRUE(base::WriteFile(non_launch_file_path, "test2"));

    std::string non_launch_file_js_handle =
        content::JsReplace("window.launchParams.files[0].getFileHandle($1)",
                           non_launch_file_path.BaseName().AsUTF8Unsafe());
    EXPECT_EQ("test2", ReadContentFromJsFileHandle(web_contents,
                                                   non_launch_file_js_handle));
    EXPECT_TRUE(WriteContentToJsFileHandle(
        web_contents, non_launch_file_js_handle, "js_written2"));
    EXPECT_EQ("js_written2", ReadFileContent(non_launch_file_path));

    // Check the launch file can be deleted.
    std::string launch_dir_js_handle = "window.launchParams.files[0]";
    EXPECT_TRUE(RemoveFileFromJsDirectoryHandle(
        web_contents, launch_dir_js_handle,
        launch_file_path.BaseName().AsUTF8Unsafe()));
    EXPECT_FALSE(base::PathExists(launch_file_path));

    // Check the non-launch file can be deleted.
    EXPECT_TRUE(RemoveFileFromJsDirectoryHandle(
        web_contents, launch_dir_js_handle,
        non_launch_file_path.BaseName().AsUTF8Unsafe()));
    EXPECT_FALSE(base::PathExists(non_launch_file_path));

    // Check a file can be created.
    std::string new_file_js_handle = content::JsReplace(
        "window.launchParams.files[0].getFileHandle($1, {create:true})",
        "new_file");
    EXPECT_TRUE(WriteContentToJsFileHandle(web_contents, new_file_js_handle,
                                           "js_new_file"));
    EXPECT_EQ("js_new_file", ReadFileContent(base_dir.AppendASCII("new_file")));
  }
};

IN_PROC_BROWSER_TEST_P(SystemWebAppManagerLaunchDirectoryBrowserTest,
                       LaunchDirectoryForSystemWebApp) {
  WaitForTestSystemAppInstall();

  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir temp_directory;
  ASSERT_TRUE(temp_directory.CreateUniqueTempDir());
  base::FilePath temp_file_path;
  ASSERT_TRUE(base::CreateTemporaryFileInDir(temp_directory.GetPath(),
                                             &temp_file_path));

  // First launch.
  content::WebContents* web_contents = LaunchApp({temp_file_path});
  EXPECT_TRUE(PrepareToReceiveLaunchParams(web_contents));
  EXPECT_TRUE(WaitAndExposeLaunchParamsToWindow(web_contents, "launchParams1"));

  // Check launch directory and launch files are correct.
  EXPECT_EQ("directory", content::EvalJs(web_contents,
                                         "window.launchParams1.files[0].kind"));
  EXPECT_EQ(
      temp_directory.GetPath().BaseName().AsUTF8Unsafe(),
      content::EvalJs(web_contents, "window.launchParams1.files[0].name"));
  EXPECT_EQ("file", content::EvalJs(web_contents,
                                    "window.launchParams1.files[1].kind"));
  EXPECT_EQ(
      temp_file_path.BaseName().AsUTF8Unsafe(),
      content::EvalJs(web_contents, "window.launchParams1.files[1].name"));

  // Second launch.
  base::ScopedTempDir temp_directory2;
  ASSERT_TRUE(temp_directory2.CreateUniqueTempDir());
  base::FilePath temp_file_path2;
  ASSERT_TRUE(base::CreateTemporaryFileInDir(temp_directory2.GetPath(),
                                             &temp_file_path2));

  // The second launch reuses the opened application. It should pass the
  // launchParams to the opened page, and return the same content::WebContents*.
  EXPECT_TRUE(PrepareToReceiveLaunchParams(web_contents));
  EXPECT_EQ(web_contents, LaunchAppWithoutWaiting({temp_file_path2}));
  EXPECT_TRUE(WaitAndExposeLaunchParamsToWindow(web_contents, "launchParams2"));

  // Check the second launch directory and launch files are correct.
  EXPECT_EQ("directory", content::EvalJs(web_contents,
                                         "window.launchParams2.files[0].kind"));
  EXPECT_EQ(
      temp_directory2.GetPath().BaseName().AsUTF8Unsafe(),
      content::EvalJs(web_contents, "window.launchParams2.files[0].name"));
  EXPECT_EQ("file", content::EvalJs(web_contents,
                                    "window.launchParams2.files[1].kind"));
  EXPECT_EQ(
      temp_file_path2.BaseName().AsUTF8Unsafe(),
      content::EvalJs(web_contents, "window.launchParams2.files[1].name"));
}

IN_PROC_BROWSER_TEST_P(SystemWebAppManagerLaunchDirectoryBrowserTest,
                       ReadWritePermissions_OrdinaryDirectory) {
  WaitForTestSystemAppInstall();

  // Test for ordinary directory.
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir temp_directory;
  ASSERT_TRUE(temp_directory.CreateUniqueTempDir());
  TestPermissionsForLaunchDirectory(temp_directory.GetPath());
}

IN_PROC_BROWSER_TEST_P(SystemWebAppManagerLaunchDirectoryBrowserTest,
                       ReadWritePermissions_SensitiveDirectory) {
  WaitForTestSystemAppInstall();

  // Test for sensitive directory (which are otherwise blocked by
  // FileSystemAccess API). It is safe to use |chrome::DIR_DEFAULT_DOWNLOADS|,
  // because InProcBrowserTest fixture sets up different download directory for
  // each test cases.
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::FilePath sensitive_dir;
  ASSERT_TRUE(
      base::PathService::Get(chrome::DIR_DEFAULT_DOWNLOADS, &sensitive_dir));
  ASSERT_TRUE(base::DirectoryExists(sensitive_dir));
  TestPermissionsForLaunchDirectory(sensitive_dir);
}

// Base class for testing File Handling and File System Access with Chrome OS
// File System Provider features.
class SystemWebAppManagerLaunchDirectoryFileSystemProviderBrowserTest
    : public SystemWebAppManagerLaunchDirectoryBrowserTest {
 public:
  [[nodiscard]] content::EvalJsResult FileHandleIsGif(
      content::WebContents* web_contents,
      const std::string& file_handle_or_promise) {
    return content::EvalJs(
        web_contents,
        "Promise.resolve(" + file_handle_or_promise + ")" +
            ".then(async fileHandle => {"
            "  const file = await fileHandle.getFile();"
            "  const arrayBuf = await file.arrayBuffer();"
            "  const bytes = new Uint8Array(arrayBuf.slice(0, 3));"
            "  return bytes[0] === 0x47     /* G */"
            "      && bytes[1] === 0x49     /* I */"
            "      && bytes[2] === 0x46;    /* F */"
            "});");
  }

  [[nodiscard]] content::EvalJsResult FileHandleIsPng(
      content::WebContents* web_contents,
      const std::string& file_handle_or_promise) {
    return content::EvalJs(
        web_contents,
        "Promise.resolve(" + file_handle_or_promise + ")" +
            ".then(async fileHandle => {"
            "  const file = await fileHandle.getFile();"
            "  const arrayBuf = await file.arrayBuffer();"
            "  const bytes = new Uint8Array(arrayBuf.slice(0, 4));"
            "  return bytes[0] === 0x89     /* 0x89 */"
            "      && bytes[1] === 0x50     /* P */"
            "      && bytes[2] === 0x4E     /* N */"
            "      && bytes[3] === 0x47;    /* G */"
            "});");
  }

  // Returns whether the file is written.
  [[nodiscard]] ::testing::AssertionResult CanWriteFile(
      content::WebContents* web_contents,
      const std::string& file_handle_or_promise) {
    return content::ExecJs(
        web_contents,
        "Promise.resolve(" + file_handle_or_promise + ")" +
            ".then(async fileHandle => {"
            "  const writable = await fileHandle.createWritable();"
            "  await writable.write('test');"
            "  await writable.close();"
            "});");
  }

  void InstallTestFileSystemProvider(Profile* profile) {
    volume_ = file_manager::test::InstallFileSystemProviderChromeApp(profile);
  }

  base::FilePath GetFileSystemProviderFilePath(const std::string& file_name) {
    return volume_->mount_path().AppendASCII(file_name);
  }

 private:
  base::WeakPtr<file_manager::Volume> volume_;
};

IN_PROC_BROWSER_TEST_P(
    SystemWebAppManagerLaunchDirectoryFileSystemProviderBrowserTest,
    LaunchFromFileSystemProvider_ReadFiles) {
  Profile* profile = browser()->profile();

  WaitForTestSystemAppInstall();
  InstallTestFileSystemProvider(profile);

  // Launch from FileSystemProvider path.
  const char kTestGifFile[] = "readwrite.gif";
  const char kTestPngFile[] = "readonly.png";
  const base::FilePath launch_file =
      GetFileSystemProviderFilePath(kTestGifFile);

  content::WebContents* web_contents = LaunchApp({launch_file});
  EXPECT_TRUE(PrepareToReceiveLaunchParams(web_contents));
  EXPECT_TRUE(WaitAndExposeLaunchParamsToWindow(web_contents, "launchParams"));

  // Check the launch file is the one we expect, and we can read the file.
  EXPECT_EQ(kTestGifFile,
            content::EvalJs(web_contents, "window.launchParams.files[1].name"));
  EXPECT_EQ(true,
            FileHandleIsGif(web_contents, "window.launchParams.files[1]"));

  // Check we can list the directory.
  EXPECT_EQ(base::StrCat({kTestPngFile, ";", kTestGifFile}),
            content::EvalJs(
                web_contents,
                "(async function() {"
                "  let fileNames = [];"
                "  const files = await window.launchParams.files[0].keys();"
                "  for await (const name of files)"
                "    fileNames.push(name);"
                "  return fileNames.sort().join(';');"
                "})();"));

  // Verify we can read a file (other than launch file) inside the directory.
  EXPECT_EQ(true, FileHandleIsPng(
                      web_contents,
                      content::JsReplace(
                          "window.launchParams.files[0].getFileHandle($1)",
                          kTestPngFile)));
}

// Test that the File System Access implementation doesn't cause a crash when
// writing to readonly files.
IN_PROC_BROWSER_TEST_P(
    SystemWebAppManagerLaunchDirectoryFileSystemProviderBrowserTest,
    LaunchFromFileSystemProvider_WriteFileFails) {
  Profile* profile = browser()->profile();

  WaitForTestSystemAppInstall();
  InstallTestFileSystemProvider(profile);

  content::WebContents* web_contents =
      LaunchApp({GetFileSystemProviderFilePath("readonly.png")});

  EXPECT_TRUE(PrepareToReceiveLaunchParams(web_contents));
  EXPECT_TRUE(WaitAndExposeLaunchParamsToWindow(web_contents, "launchParams"));

  // Try to write the file.
  EXPECT_FALSE(CanWriteFile(web_contents, "window.launchParams.files[1]"));

  // Do a no-op JavaScript to check the page is still operational. If the page
  // crashed, the following call will fail.
  EXPECT_TRUE(content::ExecJs(web_contents, "(function(){})();"));
}

// Test that the File System Access implementation doesn't cause a crash when
// deleting readonly files.
IN_PROC_BROWSER_TEST_P(
    SystemWebAppManagerLaunchDirectoryFileSystemProviderBrowserTest,
    LaunchFromFileSystemProvider_DeleteFileFails) {
  Profile* profile = browser()->profile();

  WaitForTestSystemAppInstall();
  InstallTestFileSystemProvider(profile);

  content::WebContents* web_contents =
      LaunchApp({GetFileSystemProviderFilePath("readonly.png")});

  EXPECT_TRUE(PrepareToReceiveLaunchParams(web_contents));
  EXPECT_TRUE(WaitAndExposeLaunchParamsToWindow(web_contents, "launchParams"));

  // Deleting the file should fail.
  EXPECT_FALSE(content::ExecJs(
      web_contents,
      content::JsReplace("window.launchParams.files[0].removeEntry($1)",
                         "readonly.png")));

  // Do a no-op JavaScript to check the page is still operational. If the page
  // crashed, the following call will fail.
  EXPECT_TRUE(content::ExecJs(web_contents, "(function() {})();"));
}

class SystemWebAppManagerNotShownInLauncherTest
    : public TestProfileTypeMixin<SystemWebAppBrowserTestBase> {
 public:
  SystemWebAppManagerNotShownInLauncherTest() {
    SetSystemWebAppInstallation(
        TestSystemWebAppInstallation::SetUpAppNotShownInLauncher());
  }
};

IN_PROC_BROWSER_TEST_P(SystemWebAppManagerNotShownInLauncherTest,
                       NotShownInLauncher) {
  WaitForTestSystemAppInstall();

  webapps::AppId app_id =
      GetManager().GetAppIdForSystemApp(GetAppType()).value();

  GetAppServiceProxy(browser()->profile())
      ->AppRegistryCache()
      .ForOneApp(app_id, [](const apps::AppUpdate& update) {
        EXPECT_FALSE(update.ShowInLauncher().value_or(true));
      });
  // The |AppList| should have all apps visible in the launcher, apps get
  // removed from the |AppList| when they are hidden.
  AppListClientImpl* client = AppListClientImpl::GetInstance();
  ASSERT_TRUE(client);
  AppListModelUpdater* model_updater = ::test::GetModelUpdater(client);
  const ChromeAppListItem* mock_app = model_updater->FindItem(app_id);
  // |mock_app| shouldn't be found in |AppList| because it should be hidden in
  // launcher.
  EXPECT_FALSE(mock_app);
}

class SystemWebAppManagerNotShownInSearchTest
    : public TestProfileTypeMixin<SystemWebAppBrowserTestBase> {
 public:
  SystemWebAppManagerNotShownInSearchTest() {
    SetSystemWebAppInstallation(
        TestSystemWebAppInstallation::SetUpAppNotShownInSearch());
  }
};

IN_PROC_BROWSER_TEST_P(SystemWebAppManagerNotShownInSearchTest,
                       NotShownInSearch) {
  WaitForTestSystemAppInstall();
  webapps::AppId app_id =
      GetManager().GetAppIdForSystemApp(GetAppType()).value();

  GetAppServiceProxy(browser()->profile())
      ->AppRegistryCache()
      .ForOneApp(app_id, [](const apps::AppUpdate& update) {
        EXPECT_FALSE(update.ShowInSearch().value_or(true));
      });
}

class SystemWebAppManagerHandlesFileOpenIntentsTest
    : public TestProfileTypeMixin<SystemWebAppBrowserTestBase> {
 public:
  SystemWebAppManagerHandlesFileOpenIntentsTest() {
    SetSystemWebAppInstallation(
        TestSystemWebAppInstallation::SetUpAppThatHandlesFileOpenIntents());
  }
};

IN_PROC_BROWSER_TEST_P(SystemWebAppManagerHandlesFileOpenIntentsTest,
                       HandlesFileOpenIntents) {
  WaitForTestSystemAppInstall();
  webapps::AppId app_id =
      GetManager().GetAppIdForSystemApp(GetAppType()).value();

  GetAppServiceProxy(browser()->profile())
      ->AppRegistryCache()
      .ForOneApp(app_id, [](const apps::AppUpdate& update) {
        EXPECT_TRUE(update.HandlesIntents().value_or(false));
      });
}

class SystemWebAppManagerAdditionalSearchTermsTest
    : public TestProfileTypeMixin<SystemWebAppBrowserTestBase> {
 public:
  SystemWebAppManagerAdditionalSearchTermsTest() {
    SetSystemWebAppInstallation(
        TestSystemWebAppInstallation::SetUpAppWithAdditionalSearchTerms());
  }
};

IN_PROC_BROWSER_TEST_P(SystemWebAppManagerAdditionalSearchTermsTest,
                       AdditionalSearchTerms) {
  WaitForTestSystemAppInstall();
  webapps::AppId app_id =
      GetManager().GetAppIdForSystemApp(GetAppType()).value();

  // AdditionalSearchTerms is flaky on Windows as it's a Chrome OS feature.
  GetAppServiceProxy(browser()->profile())
      ->AppRegistryCache()
      .ForOneApp(app_id, [](const apps::AppUpdate& update) {
        EXPECT_EQ(std::vector<std::string>({"Security"}),
                  update.AdditionalSearchTerms());
      });
}

class SystemWebAppManagerHasTabStripWithNewTabButtonTest
    : public TestProfileTypeMixin<SystemWebAppBrowserTestBase> {
 public:
  SystemWebAppManagerHasTabStripWithNewTabButtonTest() {
    SetSystemWebAppInstallation(
        TestSystemWebAppInstallation::SetUpAppWithTabStrip(
            /*has_tab_strip=*/true, /*hide_new_tab_button=*/false));
  }
};

IN_PROC_BROWSER_TEST_P(SystemWebAppManagerHasTabStripWithNewTabButtonTest,
                       ShouldHaveTabStripWithNewTabButton) {
  WaitForTestSystemAppInstall();

  Browser* browser;
  EXPECT_TRUE(LaunchApp(GetAppType(), &browser));
  EXPECT_TRUE(browser->app_controller()->has_tab_strip());
  EXPECT_FALSE(browser->app_controller()->ShouldHideNewTabButton());
}

class SystemWebAppManagerHasTabStripWithHiddenNewTabButtonTest
    : public TestProfileTypeMixin<SystemWebAppBrowserTestBase> {
 public:
  SystemWebAppManagerHasTabStripWithHiddenNewTabButtonTest() {
    SetSystemWebAppInstallation(
        TestSystemWebAppInstallation::SetUpAppWithTabStrip(
            /*has_tab_strip=*/true, /*hide_new_tab_button=*/true));
  }
};

IN_PROC_BROWSER_TEST_P(SystemWebAppManagerHasTabStripWithHiddenNewTabButtonTest,
                       HasTabStripWithNoNewTabButton) {
  WaitForTestSystemAppInstall();

  Browser* browser;
  EXPECT_TRUE(LaunchApp(GetAppType(), &browser));
  EXPECT_TRUE(browser->app_controller()->has_tab_strip());
  EXPECT_TRUE(browser->app_controller()->ShouldHideNewTabButton());
}

class SystemWebAppManagerHasNoTabStripWithNewTabButtonTest
    : public TestProfileTypeMixin<SystemWebAppBrowserTestBase> {
 public:
  SystemWebAppManagerHasNoTabStripWithNewTabButtonTest() {
    SetSystemWebAppInstallation(
        TestSystemWebAppInstallation::SetUpAppWithTabStrip(
            /*has_tab_strip=*/false, /*hide_new_tab_button=*/false));
  }
};

IN_PROC_BROWSER_TEST_P(SystemWebAppManagerHasNoTabStripWithNewTabButtonTest,
                       HasNoTabStripWithNoNewTabButton) {
  WaitForTestSystemAppInstall();

  Browser* browser;
  EXPECT_TRUE(LaunchApp(GetAppType(), &browser));
  EXPECT_FALSE(browser->app_controller()->has_tab_strip());
  EXPECT_TRUE(browser->app_controller()->ShouldHideNewTabButton());
}

class SystemWebAppManagerHasNoTabStripWithHiddenNewTabButtonTest
    : public TestProfileTypeMixin<SystemWebAppBrowserTestBase> {
 public:
  SystemWebAppManagerHasNoTabStripWithHiddenNewTabButtonTest() {
    SetSystemWebAppInstallation(
        TestSystemWebAppInstallation::SetUpAppWithTabStrip(
            /*has_tab_strip=*/false, /*hide_new_tab_button=*/true));
  }
};

IN_PROC_BROWSER_TEST_P(
    SystemWebAppManagerHasNoTabStripWithHiddenNewTabButtonTest,
    HasNoTabStripWithNoNewTabButton) {
  WaitForTestSystemAppInstall();

  Browser* browser;
  EXPECT_TRUE(LaunchApp(GetAppType(), &browser));
  EXPECT_FALSE(browser->app_controller()->has_tab_strip());
  EXPECT_TRUE(browser->app_controller()->ShouldHideNewTabButton());
}

// We only support custom bounds on Chrome OS.
class SystemWebAppManagerDefaultBoundsTest
    : public TestProfileTypeMixin<SystemWebAppBrowserTestBase> {
 public:
  SystemWebAppManagerDefaultBoundsTest() {
    SetSystemWebAppInstallation(
        TestSystemWebAppInstallation::SetUpAppWithDefaultBounds(
            kDefaultBounds));
  }

 protected:
  const gfx::Rect kDefaultBounds = {0, 0, 333, 444};
};

IN_PROC_BROWSER_TEST_P(SystemWebAppManagerDefaultBoundsTest, HasDefaultBounds) {
  WaitForTestSystemAppInstall();

  Browser* browser;
  EXPECT_TRUE(LaunchApp(GetAppType(), &browser));
  EXPECT_EQ(kDefaultBounds, browser->app_controller()->GetDefaultBounds());
  EXPECT_EQ(kDefaultBounds, browser->window()->GetBounds());
}

// Tests that SWA are correctly uninstalled across restarts.
class SystemWebAppManagerUninstallBrowserTest
    : public TestProfileTypeMixin<SystemWebAppBrowserTestBase> {
 public:
  SystemWebAppManagerUninstallBrowserTest() {
    if (content::IsPreTest()) {
      // Use an app with FileHandling enabled since it will perform extra setup
      // steps.
      SetSystemWebAppInstallation(
          TestSystemWebAppInstallation::SetUpAppThatReceivesLaunchFiles(
              TestSystemWebAppInstallation::IncludeLaunchDirectory::kNo));
    } else {
      SetSystemWebAppInstallation(
          TestSystemWebAppInstallation::SetUpWithoutApps());
    }
  }
  ~SystemWebAppManagerUninstallBrowserTest() override = default;
};

IN_PROC_BROWSER_TEST_P(SystemWebAppManagerUninstallBrowserTest, PRE_Uninstall) {
  WaitForTestSystemAppInstall();
  EXPECT_TRUE(GetManager().GetAppIdForSystemApp(GetAppType()).has_value());
}

IN_PROC_BROWSER_TEST_P(SystemWebAppManagerUninstallBrowserTest, Uninstall) {
  WaitForTestSystemAppInstall();
  EXPECT_TRUE(GetManager().GetAppIds().empty());

  auto* app_service_proxy =
      apps::AppServiceProxyFactory::GetForProfile(browser()->profile());

  bool swa_found = false;
  app_service_proxy->AppRegistryCache().ForEachApp(
      [&](const apps::AppUpdate& app) {
        if ((app.AppType() == apps::AppType::kSystemWeb ||
             app.AppType() == apps::AppType::kWeb) &&
            apps_util::IsInstalled(app.Readiness())) {
          swa_found = true;
        }
      });
  EXPECT_FALSE(swa_found);
}

// Test that all registered System Apps can be re-installed.
class SystemWebAppManagerInstallAllAppsBrowserTest
    : public TestProfileTypeMixin<SystemWebAppBrowserTestBase> {
 public:
  SystemWebAppManagerInstallAllAppsBrowserTest() {
    features_.InitAndEnableFeature(features::kEnableAllSystemWebApps);
  }
  ~SystemWebAppManagerInstallAllAppsBrowserTest() override = default;

 private:
  base::test::ScopedFeatureList features_;
};

// TODO(crbug.com/40162953): At the moment, PRE_Test failures aren't
// reported in test summary, thus won't fail the CI build job. So we need a
// ordinary test to fail the job and block CQ.
//
// Technically speaking, this test can merge into PRE_Upgrade if the
// aforementioned crbug is fixed.
IN_PROC_BROWSER_TEST_P(SystemWebAppManagerInstallAllAppsBrowserTest,
                       BasicConsistencyCheck) {
  // Wait for apps to install before performing assertions, otherwise the test
  // might flake. See https://crbug.com/1286600#c6.
  GetManager().InstallSystemAppsForTesting();

  const auto& app_map = GetManager().system_app_delegates();
  ASSERT_GT(app_map.size(), 0U);

  for (const auto& type_and_info : app_map) {
    // Check all system app types has a corresponding SystemWebAppDataProto
    // entry defined.
    EXPECT_TRUE(SystemWebAppDataProto_SystemWebAppType_IsValid(
        static_cast<SystemWebAppDataProto_SystemWebAppType>(
            type_and_info.first)))
        << "Please make sure you have added a corresponding entry to "
           "SystemWebAppDataProto when adding a new System Web App.";

    // Check app's install_url and start_url are from the same origin.
    //
    // TODO(crbug.com/40709016): Include OS Settings in this check.
    //
    // OS Settings uses a different install_url origin (by mistake) which are
    // persisted to disk. We can't fix it until the above crbug is fixed.
    // Without fixing the above bug, non-fresh profiles will run into
    // https://crbug.com/1220354.
    if (type_and_info.first != SystemWebAppType::SETTINGS) {
      EXPECT_TRUE(url::IsSameOriginWith(
          type_and_info.second->GetInstallUrl(),
          type_and_info.second->GetWebAppInfo()->start_url()));
    }

    // Check app's web app shortcuts fields is self-consistent.
    auto install_info = type_and_info.second->GetWebAppInfo();
    EXPECT_EQ(install_info->shortcuts_menu_icon_bitmaps.size(),
              install_info->shortcuts_menu_item_infos.size());
  }

  // Check each SWA app has their own unique origin (i.e. doesn't share origin
  // with a different app).
  std::set<url::Origin> install_url_origins;
  std::set<url::Origin> start_url_origins;
  for (const auto& type_and_info : app_map) {
    auto install_url_origin =
        url::Origin::Create(type_and_info.second->GetInstallUrl());
    EXPECT_EQ(0u, install_url_origins.count(install_url_origin))
        << "System web app's install_url origin should be unique.";
    install_url_origins.insert(install_url_origin);

    auto start_url_origin =
        url::Origin::Create(type_and_info.second->GetWebAppInfo()->start_url());
    EXPECT_EQ(0u, start_url_origins.count(start_url_origin))
        << "System web app's start_url origin should be unique.";
    start_url_origins.insert(start_url_origin);
  }

  // Check apps (other than Terminal, which is published by its own App
  // publisher) are exposed in AppService.
  for (const auto& type_and_info : app_map) {
    if (type_and_info.first == SystemWebAppType::TERMINAL)
      continue;

    std::optional<std::string> app_id =
        GetManager().GetAppIdForSystemApp(type_and_info.first);
    EXPECT_TRUE(app_id);

    bool app_found = false;
    apps::AppServiceProxyFactory::GetForProfile(browser()->profile())
        ->AppRegistryCache()
        .ForOneApp(*app_id, [&](const apps::AppUpdate& app) {
          app_found = true;
          EXPECT_EQ(
              app.Name(),
              base::UTF16ToUTF8(type_and_info.second->GetWebAppInfo()->title));
        });
    EXPECT_TRUE(app_found) << "System Web App "
                           << type_and_info.second->GetInternalName()
                           << " can't be found in AppService after install.";
  }

  // Verify that all system web apps which are enabled by default and appear in
  // the launcher have an explicit launcher position set.
  std::vector<std::string> app_order;
  chromeos::default_app_order::Get(&app_order);

  // Demo/testing apps don't need a launcher position.
  const base::flat_set<SystemWebAppType> kLauncherPositionExemptTypes = {
      SystemWebAppType::SAMPLE};

  for (const auto& [app_type, app_delegate] : app_map) {
    if (app_delegate->IsAppEnabled() && app_delegate->ShouldShowInLauncher() &&
        !base::Contains(kLauncherPositionExemptTypes, app_type)) {
      EXPECT_TRUE(base::Contains(app_order,
                                 GetManager().GetAppIdForSystemApp(app_type)))
          << "System app '" << app_delegate->GetInternalName()
          << "' appears in the launcher but does not have an app order "
             "definition. Its app ID should be added to GetDefault() in "
             "//chrome/browser/ash/extensions/default_app_order.cc, which "
             "should match the order in go/default-apps";
    }
  }

  // Verify that all system web apps have an icon.
  for (const auto& [_, delegate] : app_map) {
    const auto info = delegate->GetWebAppInfo();
    EXPECT_FALSE(info->manifest_icons.empty())
        << delegate->GetInternalName() << " needs a manifest icon";
    EXPECT_FALSE(delegate->GetWebAppInfo()->icon_bitmaps.empty())
        << delegate->GetInternalName() << " needs an icon bitmap";
  }
}

IN_PROC_BROWSER_TEST_P(SystemWebAppManagerInstallAllAppsBrowserTest, Upgrade) {
  GetManager().InstallSystemAppsForTesting();
  const auto& app_ids = GetManager().GetAppIds();

  EXPECT_EQ(GetManager().system_app_delegates().size(), app_ids.size());

  // Some system web apps keep their resources (e.g. html pages) in real
  // Chrome OS images. Here we test a few apps whose resources are bundled in
  // chrome and always available. These apps are able to cover the code path we
  // execute when launching the app.
  const SystemWebAppType apps_to_launch[] = {
      SystemWebAppType::SETTINGS,
      SystemWebAppType::MEDIA,  // Uses File Handling with launch directory
  };

  for (const auto& type : apps_to_launch) {
    EXPECT_TRUE(LaunchApp(type));
  }
}

class SystemWebAppManagerChromeUntrustedTest
    : public TestProfileTypeMixin<SystemWebAppBrowserTestBase> {
 public:
  SystemWebAppManagerChromeUntrustedTest() {
    SetSystemWebAppInstallation(
        TestSystemWebAppInstallation::SetUpChromeUntrustedApp());
  }
};

IN_PROC_BROWSER_TEST_P(SystemWebAppManagerChromeUntrustedTest, Install) {
  WaitForTestSystemAppInstall();

  // Don't wait for page load because we want to verify AppController identifies
  // the System Web App before the app loads.
  Browser* app_browser;
  LaunchAppWithoutWaiting(GetAppType(), &app_browser);

  webapps::AppId app_id =
      GetManager().GetAppIdForSystemApp(GetAppType()).value();
  EXPECT_EQ(app_id, app_browser->app_controller()->app_id());
  EXPECT_TRUE(GetManager().IsSystemWebApp(app_id));

  Profile* profile = app_browser->profile();
  web_app::WebAppRegistrar& registrar =
      web_app::WebAppProvider::GetForTest(profile)->registrar_unsafe();

  EXPECT_EQ("Test System App Untrusted", registrar.GetAppShortName(app_id));
  EXPECT_EQ(SkColorSetRGB(0xFF, 0, 0), registrar.GetAppThemeColor(app_id));
  EXPECT_TRUE(registrar.HasExternalAppWithInstallSource(
      app_id, web_app::ExternalInstallSource::kSystemInstalled));
  EXPECT_EQ(registrar.FindAppWithUrlInScope(
                GURL("chrome-untrusted://test-system-app/")),
            app_id);
}

class SystemWebAppManagerOriginTrialsBrowserTest
    : public TestProfileTypeMixin<SystemWebAppBrowserTestBase> {
 public:
  SystemWebAppManagerOriginTrialsBrowserTest() {
    SetSystemWebAppInstallation(
        TestSystemWebAppInstallation::SetUpAppWithEnabledOriginTrials(
            OriginTrialsMap({{GetOrigin(main_url_), main_url_trials_},
                             {GetOrigin(trial_url_), trial_url_trials_}})));
  }

  ~SystemWebAppManagerOriginTrialsBrowserTest() override = default;

 protected:
  std::unique_ptr<content::WebContents> CreateTestWebContents() {
    content::WebContents::CreateParams create_params(browser()->profile());
    return content::WebContents::Create(create_params);
  }

  const std::vector<std::string> main_url_trials_ = {"Frobulate"};
  const std::vector<std::string> trial_url_trials_ = {"FrobulateNavigation"};

  const GURL main_url_ = GURL("chrome://test-system-app/pwa.html");
  const GURL trial_url_ = GURL("chrome://test-subframe/title2.html");
  const GURL notrial_url_ = GURL("chrome://notrial-subframe/title3.html");

 private:
  url::Origin GetOrigin(const GURL& url) { return url::Origin::Create(url); }
};

IN_PROC_BROWSER_TEST_P(SystemWebAppManagerOriginTrialsBrowserTest,
                       ForceEnabledOriginTrials_FirstNavigationIntoPage) {
  WaitForTestSystemAppInstall();
  auto app_id = GetManager().GetAppIdForSystemApp(GetAppType()).value();

  std::unique_ptr<content::WebContents> web_contents = CreateTestWebContents();
  web_app::WebAppTabHelper::CreateForWebContents(web_contents.get());
  auto& tab_helper =
      *web_app::WebAppTabHelper::FromWebContents(web_contents.get());

  // Simulate when first navigating into app's launch url.
  {
    content::MockNavigationHandle mock_nav_handle(main_url_, nullptr);
    mock_nav_handle.set_is_in_primary_main_frame(true);
    mock_nav_handle.set_is_same_document(false);
    EXPECT_CALL(mock_nav_handle, ForceEnableOriginTrials(main_url_trials_));
    tab_helper.ReadyToCommitNavigation(&mock_nav_handle);
    ASSERT_EQ(app_id, *web_app::WebAppTabHelper::GetAppId(web_contents.get()));
  }

  // Simulate loading app's embedded child-frame that has origin trials.
  {
    content::MockNavigationHandle mock_nav_handle(trial_url_, nullptr);
    mock_nav_handle.set_is_in_primary_main_frame(false);
    mock_nav_handle.set_is_same_document(false);
    EXPECT_CALL(mock_nav_handle, ForceEnableOriginTrials(trial_url_trials_));
    tab_helper.ReadyToCommitNavigation(&mock_nav_handle);
  }

  // Simulate loading app's embedded child-frame that has no origin trial.
  {
    content::MockNavigationHandle mock_nav_handle(notrial_url_, nullptr);
    mock_nav_handle.set_is_in_primary_main_frame(false);
    mock_nav_handle.set_is_same_document(false);
    EXPECT_CALL(mock_nav_handle, ForceEnableOriginTrials).Times(0);
    tab_helper.ReadyToCommitNavigation(&mock_nav_handle);
  }
}

IN_PROC_BROWSER_TEST_P(SystemWebAppManagerOriginTrialsBrowserTest,
                       ForceEnabledOriginTrials_IntraDocumentNavigation) {
  WaitForTestSystemAppInstall();
  auto app_id = GetManager().GetAppIdForSystemApp(GetAppType()).value();

  std::unique_ptr<content::WebContents> web_contents = CreateTestWebContents();
  web_app::WebAppTabHelper::CreateForWebContents(web_contents.get());
  auto& tab_helper =
      *web_app::WebAppTabHelper::FromWebContents(web_contents.get());

  // Simulate when first navigating into app's launch url.
  {
    content::MockNavigationHandle mock_nav_handle(main_url_, nullptr);
    mock_nav_handle.set_is_in_primary_main_frame(true);
    mock_nav_handle.set_is_same_document(false);
    EXPECT_CALL(mock_nav_handle, ForceEnableOriginTrials(main_url_trials_));
    tab_helper.ReadyToCommitNavigation(&mock_nav_handle);
    ASSERT_EQ(app_id, *web_app::WebAppTabHelper::GetAppId(web_contents.get()));
  }

  // Simulate same-document navigation.
  {
    content::MockNavigationHandle mock_nav_handle(main_url_, nullptr);
    mock_nav_handle.set_is_in_primary_main_frame(true);
    mock_nav_handle.set_is_same_document(true);
    EXPECT_CALL(mock_nav_handle, ForceEnableOriginTrials).Times(0);
    tab_helper.ReadyToCommitNavigation(&mock_nav_handle);
  }
}

// This test checks origin trials are correctly enabled for navigations on the
// main frame, this test checks:
// - The app's main page |main_url_| has OT.
// - The iframe page |trial_url_| has OT, only if it is embedded by the app.
// - When navigating from a cross-origin page to the app's main page, the main
// page has OT.
IN_PROC_BROWSER_TEST_P(SystemWebAppManagerOriginTrialsBrowserTest,
                       ForceEnabledOriginTrials_Navigation) {
  WaitForTestSystemAppInstall();
  auto app_id = GetManager().GetAppIdForSystemApp(GetAppType()).value();

  std::unique_ptr<content::WebContents> web_contents = CreateTestWebContents();
  web_app::WebAppTabHelper::CreateForWebContents(web_contents.get());
  auto& tab_helper =
      *web_app::WebAppTabHelper::FromWebContents(web_contents.get());

  // Simulate when first navigating into app's launch url.
  {
    content::MockNavigationHandle mock_nav_handle(main_url_, nullptr);
    mock_nav_handle.set_is_in_primary_main_frame(true);
    mock_nav_handle.set_is_same_document(false);
    EXPECT_CALL(mock_nav_handle, ForceEnableOriginTrials(main_url_trials_));
    tab_helper.ReadyToCommitNavigation(&mock_nav_handle);
    ASSERT_EQ(app_id, *web_app::WebAppTabHelper::GetAppId(web_contents.get()));
  }

  // Simulate navigating to a different site without origin trials.
  {
    content::MockNavigationHandle mock_nav_handle(notrial_url_, nullptr);
    mock_nav_handle.set_is_in_primary_main_frame(true);
    mock_nav_handle.set_is_same_document(false);
    EXPECT_CALL(mock_nav_handle, ForceEnableOriginTrials).Times(0);
    tab_helper.ReadyToCommitNavigation(&mock_nav_handle);
    ASSERT_EQ(nullptr, web_app::WebAppTabHelper::GetAppId(web_contents.get()));
  }

  // Simulate navigating back to a SWA with origin trials.
  {
    content::MockNavigationHandle mock_nav_handle(main_url_, nullptr);
    mock_nav_handle.set_is_in_primary_main_frame(true);
    mock_nav_handle.set_is_same_document(false);
    EXPECT_CALL(mock_nav_handle, ForceEnableOriginTrials(main_url_trials_));
    tab_helper.ReadyToCommitNavigation(&mock_nav_handle);
    ASSERT_EQ(app_id, *web_app::WebAppTabHelper::GetAppId(web_contents.get()));
  }

  // Simulate navigating the main frame to a url embedded by SWA. This url has
  // origin trials when embedded by SWA. However, when this url is loaded in the
  // main frame, it should not get origin trials.
  {
    content::MockNavigationHandle mock_nav_handle(trial_url_, nullptr);
    mock_nav_handle.set_is_in_primary_main_frame(true);
    mock_nav_handle.set_is_same_document(false);
    EXPECT_CALL(mock_nav_handle, ForceEnableOriginTrials).Times(0);
    tab_helper.ReadyToCommitNavigation(&mock_nav_handle);
    ASSERT_EQ(nullptr, web_app::WebAppTabHelper::GetAppId(web_contents.get()));
  }
}

class SystemWebAppManagerAppSuspensionBrowserTest
    : public TestProfileTypeMixin<SystemWebAppBrowserTestBase> {
 public:
  SystemWebAppManagerAppSuspensionBrowserTest() = default;

  apps::Readiness GetAppReadiness(const webapps::AppId& app_id) {
    apps::Readiness readiness;
    bool app_found =
        GetAppServiceProxy(browser()->profile())
            ->AppRegistryCache()
            .ForOneApp(app_id, [&readiness](const apps::AppUpdate& update) {
              readiness = update.Readiness();
            });
    CHECK(app_found);
    return readiness;
  }

  std::optional<apps::IconKey> GetAppIconKey(const webapps::AppId& app_id) {
    std::optional<apps::IconKey> icon_key;
    bool app_found =
        GetAppServiceProxy(browser()->profile())
            ->AppRegistryCache()
            .ForOneApp(app_id, [&icon_key](const apps::AppUpdate& update) {
              icon_key = update.IconKey();
            });
    CHECK(app_found);
    return icon_key;
  }
};

// Tests that System Apps can be suspended when the policy is set before the app
// is installed.
IN_PROC_BROWSER_TEST_P(SystemWebAppManagerAppSuspensionBrowserTest,
                       AppSuspendedBeforeInstall) {
  ASSERT_FALSE(GetManager()
                   .GetAppIdForSystemApp(SystemWebAppType::SETTINGS)
                   .has_value());
  {
    ScopedListPrefUpdate update(
        TestingBrowserProcess::GetGlobal()->local_state(),
        policy::policy_prefs::kSystemFeaturesDisableList);
    update->Append(static_cast<int>(policy::SystemFeature::kOsSettings));
  }
  WaitForTestSystemAppInstall();
  std::optional<webapps::AppId> settings_id =
      GetManager().GetAppIdForSystemApp(SystemWebAppType::SETTINGS);
  DCHECK(settings_id.has_value());

  EXPECT_EQ(apps::Readiness::kDisabledByPolicy, GetAppReadiness(*settings_id));
  EXPECT_TRUE(apps::IconEffects::kBlocked &
              GetAppIconKey(*settings_id)->icon_effects);

  {
    ScopedListPrefUpdate update(
        TestingBrowserProcess::GetGlobal()->local_state(),
        policy::policy_prefs::kSystemFeaturesDisableList);
    update->clear();
  }
  SystemWebAppManager::GetWebAppProvider(browser()->profile())
      ->command_manager()
      .AwaitAllCommandsCompleteForTesting();
  EXPECT_EQ(apps::Readiness::kReady, GetAppReadiness(*settings_id));
  EXPECT_FALSE(apps::IconEffects::kBlocked &
               GetAppIconKey(*settings_id)->icon_effects);
}

// Tests that System Apps can be suspended when the policy is set after the app
// is installed.
IN_PROC_BROWSER_TEST_P(SystemWebAppManagerAppSuspensionBrowserTest,
                       AppSuspendedAfterInstall) {
  base::AddFeatureIdTagToTestResult(
      "screenplay-44570758-2d0f-4ed9-8172-102244523249");

  WaitForTestSystemAppInstall();
  std::optional<webapps::AppId> settings_id =
      GetManager().GetAppIdForSystemApp(SystemWebAppType::SETTINGS);
  DCHECK(settings_id.has_value());
  EXPECT_EQ(apps::Readiness::kReady, GetAppReadiness(*settings_id));

  {
    ScopedListPrefUpdate update(
        TestingBrowserProcess::GetGlobal()->local_state(),
        policy::policy_prefs::kSystemFeaturesDisableList);
    update->Append(static_cast<int>(policy::SystemFeature::kOsSettings));
  }
  SystemWebAppManager::GetWebAppProvider(browser()->profile())
      ->command_manager()
      .AwaitAllCommandsCompleteForTesting();

  EXPECT_EQ(apps::Readiness::kDisabledByPolicy, GetAppReadiness(*settings_id));
  EXPECT_TRUE(apps::IconEffects::kBlocked &
              GetAppIconKey(*settings_id)->icon_effects);

  {
    ScopedListPrefUpdate update(
        TestingBrowserProcess::GetGlobal()->local_state(),
        policy::policy_prefs::kSystemFeaturesDisableList);
    update->clear();
  }
  SystemWebAppManager::GetWebAppProvider(browser()->profile())
      ->command_manager()
      .AwaitAllCommandsCompleteForTesting();
  EXPECT_EQ(apps::Readiness::kReady, GetAppReadiness(*settings_id));
  EXPECT_FALSE(apps::IconEffects::kBlocked &
               GetAppIconKey(*settings_id)->icon_effects);
}
INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_REGULAR_PROFILE_P(
    SystemWebAppManagerAppSuspensionBrowserTest);

class SystemWebAppManagerShortcutTest
    : public TestProfileTypeMixin<SystemWebAppBrowserTestBase> {
 public:
  SystemWebAppManagerShortcutTest() {
    SetSystemWebAppInstallation(
        TestSystemWebAppInstallation::SetUpAppWithShortcuts());
  }
};

IN_PROC_BROWSER_TEST_P(SystemWebAppManagerShortcutTest, ShortcutUrl) {
  WaitForTestSystemAppInstall();
  webapps::AppId app_id =
      GetManager()
          .GetAppIdForSystemApp(SystemWebAppType::SHORTCUT_CUSTOMIZATION)
          .value();
  Browser* browser;
  content::WebContents* web_contents =
      LaunchApp(SystemWebAppType::SHORTCUT_CUSTOMIZATION, &browser);
  EXPECT_TRUE(web_contents);

  std::unique_ptr<ui::SimpleMenuModel> menu_model;
  {
    ShelfModel* const shelf_model = ShelfModel::Get();
    PinAppWithIDToShelf(app_id);
    ShelfItemDelegate* const delegate =
        shelf_model->GetShelfItemDelegate(ShelfID(app_id));
    base::RunLoop run_loop;
    delegate->GetContextMenu(
        display::Display::GetDefaultDisplay().id(),
        base::BindLambdaForTesting(
            [&run_loop,
             &menu_model](std::unique_ptr<ui::SimpleMenuModel> model) {
              menu_model = std::move(model);
              run_loop.Quit();
            }));
    run_loop.Run();
  }

  auto check_shortcut = [&menu_model](size_t index, int shortcut_index,
                                      const std::u16string& label) {
    EXPECT_EQ(menu_model->GetTypeAt(index), ui::MenuModel::TYPE_COMMAND);
    EXPECT_EQ(menu_model->GetCommandIdAt(index),
              LAUNCH_APP_SHORTCUT_FIRST + shortcut_index);
    EXPECT_EQ(menu_model->GetLabelAt(index), label);
  };

  // Shortcuts appear last in the context menu.
  check_shortcut(menu_model->GetItemCount() - 3, 0, u"One");
  // menu_model->GetItemCount() - 2 is used by a separator
  check_shortcut(menu_model->GetItemCount() - 1, 1, u"Two");

  const int command_id = LAUNCH_APP_SHORTCUT_FIRST + 1;
  content::LoadStopObserver url_observer(web_contents);
  menu_model->ActivatedAt(menu_model->GetIndexOfCommandId(command_id).value(),
                          ui::EF_LEFT_MOUSE_BUTTON);
  url_observer.Wait();
}

class SystemWebAppManagerBackgroundTaskTest
    : public TestProfileTypeMixin<SystemWebAppBrowserTestBase> {
 public:
  SystemWebAppManagerBackgroundTaskTest() {
    SetSystemWebAppInstallation(
        TestSystemWebAppInstallation::SetUpAppWithBackgroundTask());
  }

  void WaitForSystemAppsBackgroundTasksStart() {
    base::RunLoop run_loop;
    SystemWebAppManager::Get(browser()->profile())
        ->on_tasks_started()
        .Post(FROM_HERE, run_loop.QuitClosure());

    run_loop.Run();
  }
};

IN_PROC_BROWSER_TEST_P(SystemWebAppManagerBackgroundTaskTest, TimerFires) {
  // The SystemWebAppManager gets created in the Setup(), in the test
  // constructor, and the background tasks get created during synchronize.
  // Ideally, we'd make a TestNavigationObserver in the constructor, but they
  // have to be single threaded, and throw a check fail. There's a race
  // condition here because the background tasks are fired as callbacks in
  // response to the install finishing. So, we wait for the apps to be
  // installed, then wait on the navigation. A cleaner solution would be to have
  // a hook in the background pages to detect the navigation as an event. That's
  // a little too much work for one test though, and since this is mostly tested
  // in unittests, this is probably enough.
  content::TestNavigationObserver navigation_observer(
      GURL("chrome://test-system-app/page2.html"));
  navigation_observer.StartWatchingNewWebContents();
  ui::ScopedSetIdleState idle(ui::IDLE_STATE_IDLE);

  WaitForSystemAppsBackgroundTasksStart();

  auto& tasks = GetManager().GetBackgroundTasksForTesting();
  auto* timer = tasks[0]->get_timer_for_testing();
  EXPECT_EQ(base::Seconds(120), timer->GetCurrentDelay());
  EXPECT_EQ(SystemWebAppBackgroundTask::INITIAL_WAIT,
            tasks[0]->get_state_for_testing());
  // The "Immediate" timer waits for 2 minutes, and it's really hard to mock
  // time properly in a browser test, so just fire the thing now. We're not
  // testing that base::Timer works.
  timer->FireNow();

  navigation_observer.Wait();
  EXPECT_TRUE(timer->IsRunning());
  EXPECT_EQ(1u, tasks.size());
  EXPECT_TRUE(tasks[0]->open_immediately_for_testing());
  EXPECT_EQ(base::Days(1), tasks[0]->period_for_testing());
  EXPECT_EQ(1u, tasks[0]->timer_activated_count_for_testing());
  EXPECT_EQ(SystemWebAppBackgroundTask::WAIT_PERIOD,
            tasks[0]->get_state_for_testing());
  EXPECT_EQ(base::Days(1), timer->GetCurrentDelay());
}

class SystemWebAppManagerContextMenuBrowserTest
    : public TestProfileTypeMixin<SystemWebAppBrowserTestBase> {
 public:
  SystemWebAppManagerContextMenuBrowserTest() {
    SetSystemWebAppInstallation(
        TestSystemWebAppInstallation::SetUpAppsForContestMenuTest());
  }
  ~SystemWebAppManagerContextMenuBrowserTest() override = default;

 protected:
  std::unique_ptr<TestRenderViewContextMenu> CreateContextMenu(
      content::WebContents* web_contents,
      const GURL& link_href) {
    content::ContextMenuParams params;
    params.unfiltered_link_url = link_href;
    params.link_url = link_href;
    params.src_url = link_href;
    params.link_text = std::u16string();
    params.media_type = blink::mojom::ContextMenuDataMediaType::kNone;
    params.page_url = web_contents->GetVisibleURL();
    params.source_type = ui::MENU_SOURCE_NONE;
    auto menu = std::make_unique<TestRenderViewContextMenu>(
        *web_contents->GetPrimaryMainFrame(), params);
    menu->Init();
    return menu;
  }

  // See TestSystemWebAppInstallation::SetUpAppsForContestMenuTest.
  const SystemWebAppType kAppTypeSingleWindow = SystemWebAppType::SETTINGS;
  const SystemWebAppType kAppTypeMultiWindow = SystemWebAppType::FILE_MANAGER;
  const SystemWebAppType kAppTypeSingleWindowTabStrip = SystemWebAppType::MEDIA;
  const SystemWebAppType kAppTypeMultiWindowTabStrip = SystemWebAppType::HELP;
};

IN_PROC_BROWSER_TEST_P(SystemWebAppManagerContextMenuBrowserTest,
                       LinkToAppItself) {
  WaitForTestSystemAppInstall();

  {
    // Single window, no tab strip.
    auto* web_contents = LaunchApp(kAppTypeSingleWindow);
    auto menu =
        CreateContextMenu(web_contents, web_contents->GetLastCommittedURL());
    EXPECT_FALSE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKNEWTAB));
    EXPECT_FALSE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKNEWWINDOW));
    EXPECT_FALSE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKOFFTHERECORD));
    EXPECT_FALSE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKBOOKMARKAPP));
  }

  {
    // Single window, has tab strip.
    auto* web_contents = LaunchApp(kAppTypeSingleWindowTabStrip);
    auto menu =
        CreateContextMenu(web_contents, web_contents->GetLastCommittedURL());
    EXPECT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKNEWTAB));
    EXPECT_FALSE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKNEWWINDOW));
    EXPECT_FALSE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKOFFTHERECORD));
    EXPECT_FALSE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKBOOKMARKAPP));
  }

  {
    // Multi window, no tab strip.
    auto* web_contents = LaunchApp(kAppTypeMultiWindow);
    auto menu =
        CreateContextMenu(web_contents, web_contents->GetLastCommittedURL());
    EXPECT_FALSE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKNEWTAB));
    EXPECT_FALSE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKNEWWINDOW));
    EXPECT_FALSE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKOFFTHERECORD));
    EXPECT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKBOOKMARKAPP));
  }

  {
    // Multi window, has tab strip.
    auto* web_contents = LaunchApp(kAppTypeMultiWindowTabStrip);
    auto menu =
        CreateContextMenu(web_contents, web_contents->GetLastCommittedURL());
    EXPECT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKNEWTAB));
    EXPECT_FALSE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKNEWWINDOW));
    EXPECT_FALSE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKOFFTHERECORD));
    EXPECT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKBOOKMARKAPP));
  }
}

IN_PROC_BROWSER_TEST_P(SystemWebAppManagerContextMenuBrowserTest,
                       LinkToOtherSystemWebApp) {
  WaitForTestSystemAppInstall();

  {
    // Typical SWA, single window, no tab strip.
    auto* web_contents = LaunchApp(kAppTypeSingleWindow);
    auto menu = CreateContextMenu(web_contents,
                                  GetStartUrl(kAppTypeSingleWindowTabStrip));
    EXPECT_FALSE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKNEWTAB));
    EXPECT_FALSE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKNEWWINDOW));
    EXPECT_FALSE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKOFFTHERECORD));
    EXPECT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKBOOKMARKAPP));
  }

  {
    // Deliberately test on a multi-window, tab-strip app to cover edge cases.
    auto* web_contents = LaunchApp(kAppTypeMultiWindowTabStrip);
    auto menu =
        CreateContextMenu(web_contents, GetStartUrl(kAppTypeMultiWindow));
    EXPECT_FALSE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKNEWTAB));
    EXPECT_FALSE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKNEWWINDOW));
    EXPECT_FALSE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKOFFTHERECORD));
    EXPECT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKBOOKMARKAPP));
  }
}

IN_PROC_BROWSER_TEST_P(SystemWebAppManagerContextMenuBrowserTest, WebLink) {
  WaitForTestSystemAppInstall();

  GURL kWebUrl = GURL("https://example.com/");

  {
    // Typical SWA, single window, no tab strip.
    auto* web_contents = LaunchApp(kAppTypeSingleWindow);
    auto menu = CreateContextMenu(web_contents, kWebUrl);
    EXPECT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKNEWTAB));
    EXPECT_FALSE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKNEWWINDOW));
    EXPECT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKOFFTHERECORD));
    EXPECT_FALSE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKBOOKMARKAPP));
  }

  {
    // Deliberately test on a multi-window, tab-strip app to cover edge cases.
    auto* web_contents = LaunchApp(kAppTypeMultiWindowTabStrip);
    auto menu = CreateContextMenu(web_contents, kWebUrl);
    EXPECT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKNEWTAB));
    EXPECT_FALSE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKNEWWINDOW));
    EXPECT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKOFFTHERECORD));
    EXPECT_FALSE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKBOOKMARKAPP));
  }
}

class SystemWebAppSingleWindowTest
    : public TestProfileTypeMixin<SystemWebAppBrowserTestBase> {
 public:
  SystemWebAppSingleWindowTest() {
    SetSystemWebAppInstallation(
        TestSystemWebAppInstallation::SetUpStandaloneSingleWindowApp());
  }
  ~SystemWebAppSingleWindowTest() override = default;
};

IN_PROC_BROWSER_TEST_P(SystemWebAppSingleWindowTest, WindowReuse) {
  WaitForTestSystemAppInstall();

  content::WebContents* web_contents = LaunchApp(GetAppType());

  // Second launch reuses the window.
  EXPECT_EQ(web_contents, LaunchAppWithoutWaiting(GetAppType()));

  // Third launch reuses the window despite different URL.
  apps::AppLaunchParams params = LaunchParamsForApp(GetAppType());
  params.override_url = GURL("http://example.com/in-scope");
  EXPECT_EQ(web_contents, LaunchAppWithoutWaiting(std::move(params)));
}

class SystemWebAppAccessibilityTest : public SystemWebAppSingleWindowTest {
 protected:
  void EnableChromeVox();
  test::SpeechMonitor speech_monitor_;
};

void SystemWebAppAccessibilityTest::EnableChromeVox() {
  AccessibilityManager::Get()->EnableSpokenFeedback(true);
  speech_monitor_.ExpectSpeechPattern("*");
  speech_monitor_.Call([this]() {
    extensions::browsertest_util::ExecuteScriptInBackgroundPageDeprecated(
        browser()->profile(), extension_misc::kChromeVoxExtensionId, R"JS(
        import('/chromevox/background/chromevox_state.js').then(
            module => module.ChromeVoxState.ready().then(() =>
                window.domAutomationController.send('done')));
        )JS");
  });
}

IN_PROC_BROWSER_TEST_P(SystemWebAppAccessibilityTest,
                       CanCycleToWindowControlButtons) {
  EnableChromeVox();
  WaitForTestSystemAppInstall();

  // Launch the app so it shows up in shelf.
  Browser* app_browser;
  gfx::NativeWindow app_window;

  speech_monitor_.Call([&]() {
    LaunchApp(GetAppType(), &app_browser);
    app_window = app_browser->window()->GetNativeWindow();
    // F6 to switch pane.
    ui::test::EventGenerator generator(app_window->GetRootWindow(), app_window);
    generator.PressAndReleaseKey(ui::VKEY_F6, ui::EF_FINAL);
  });
  speech_monitor_.ExpectSpeech("Test System App");
  speech_monitor_.ExpectSpeech("Application");

  // Launcher-B to find minimize button.
  speech_monitor_.Call([&]() {
    // Search+B to switch pane.
    ui::test::EventGenerator generator(app_window->GetRootWindow());
    generator.PressAndReleaseKeyAndModifierKeys(
        ui::VKEY_B, ui::EF_COMMAND_DOWN | ui::EF_FINAL);
  });
  speech_monitor_.ExpectSpeech("Minimize");
  speech_monitor_.ExpectSpeech("Button");

  // Start the actions.
  speech_monitor_.Replay();
}

class SystemWebAppAbortsLaunchTest
    : public TestProfileTypeMixin<SystemWebAppBrowserTestBase> {
 public:
  SystemWebAppAbortsLaunchTest() {
    SetSystemWebAppInstallation(
        TestSystemWebAppInstallation::SetUpAppThatAbortsLaunch());
  }
  ~SystemWebAppAbortsLaunchTest() override = default;
};

IN_PROC_BROWSER_TEST_P(SystemWebAppAbortsLaunchTest, LaunchAborted) {
  WaitForTestSystemAppInstall();

  LaunchSystemWebAppAsync(browser()->profile(), GetAppType());

  EXPECT_EQ(0U, GetSystemWebAppBrowserCount(GetAppType()));
}

class SystemWebAppIconHealthMetricsTest
    : public TestProfileTypeMixin<SystemWebAppBrowserTestBase> {
 public:
  SystemWebAppIconHealthMetricsTest() {
    // Only reinstall on version change, so we don't force reinstall
    // and overwrite the broken icon.
    auto installation = TestSystemWebAppInstallation::SetUpAppWithValidIcons();
    installation->set_update_policy(
        SystemWebAppManager::UpdatePolicy::kOnVersionChange);

    SetSystemWebAppInstallation(std::move(installation));
  }
  ~SystemWebAppIconHealthMetricsTest() override = default;

 protected:
  static constexpr char kIconsAreHealthyHistogramName[] =
      "Webapp.SystemApps.IconsAreHealthyInSession";
  base::HistogramTester tester_;

  void WaitForInstallAndIconCheck() {
    WaitForTestSystemAppInstall();

    base::RunLoop run_loop;
    SystemWebAppManager::Get(browser()->profile())
        ->on_icon_check_completed()
        .Post(FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
  }
};

IN_PROC_BROWSER_TEST_P(SystemWebAppIconHealthMetricsTest, ReportsMetrics) {
  WaitForInstallAndIconCheck();

  tester_.ExpectBucketCount(kIconsAreHealthyHistogramName, true, 1);
  // Given SWA install with no broken icon, pref should report no broken icons.
  EXPECT_FALSE(browser()->profile()->GetPrefs()->GetBoolean(
      SystemWebAppManager::kSystemWebAppSessionHasBrokenIconsPrefName));
}

IN_PROC_BROWSER_TEST_P(SystemWebAppIconHealthMetricsTest,
                       PRE_PRE_ReinstallFixesBrokenIcon) {
  WaitForInstallAndIconCheck();

  // Given SWA install with no broken icon, pref should report no broken icons.
  CHECK_EQ(
      false,
      browser()->profile()->GetPrefs()->GetBoolean(
          SystemWebAppManager::kSystemWebAppSessionHasBrokenIconsPrefName));

  // Intentionally break icons by corrupting the on-disk icon file.
  auto app_id = GetManager().GetAppIdForSystemApp(GetAppType()).value();
  base::FilePath icon_path =
      SystemWebAppManager::GetWebAppProvider(browser()->profile())
          ->icon_manager()
          .GetIconFilePathForTesting(app_id, web_app::IconPurpose::ANY, 32);

  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    base::WriteFile(icon_path, "Not a PNG file");
  }

  // Restart to let SystemWebAppManager perform the check.
}

IN_PROC_BROWSER_TEST_P(SystemWebAppIconHealthMetricsTest,
                       PRE_ReinstallFixesBrokenIcon) {
  WaitForInstallAndIconCheck();

  tester_.ExpectBucketCount(kIconsAreHealthyHistogramName, false, 1);

  // TODO(crbug.com/40162953): Change CHECK_EQ to EXPECT_TRUE when
  // assertions report correctly as test failure in PRE_TESTs.

  // Icon check should update pref to report broken icons.
  CHECK_EQ(
      true,
      browser()->profile()->GetPrefs()->GetBoolean(
          SystemWebAppManager::kSystemWebAppSessionHasBrokenIconsPrefName));
}

IN_PROC_BROWSER_TEST_P(SystemWebAppIconHealthMetricsTest,
                       ReinstallFixesBrokenIcon) {
  WaitForInstallAndIconCheck();

  tester_.ExpectBucketCount(kIconsAreHealthyHistogramName, true, 1);
  tester_.ExpectBucketCount(
      SystemWebAppManager::kIconsFixedOnReinstallHistogramName, true, 1);
}

INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_REGULAR_PROFILE_P(
    SystemWebAppManagerBrowserTestBasicInstall);

INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_REGULAR_PROFILE_P(
    SystemWebAppManagerBrowserTest);

INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_REGULAR_PROFILE_P(
    SystemWebAppManagerLaunchFilesBrowserTest);

INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_REGULAR_PROFILE_P(
    SystemWebAppManagerLaunchDirectoryBrowserTest);

INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_REGULAR_PROFILE_P(
    SystemWebAppManagerLaunchDirectoryFileSystemProviderBrowserTest);

INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_REGULAR_PROFILE_P(
    SystemWebAppManagerNotShownInLauncherTest);

INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_REGULAR_PROFILE_P(
    SystemWebAppManagerNotShownInSearchTest);

INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_REGULAR_PROFILE_P(
    SystemWebAppManagerHandlesFileOpenIntentsTest);

INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_REGULAR_PROFILE_P(
    SystemWebAppManagerAdditionalSearchTermsTest);

INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_REGULAR_PROFILE_P(
    SystemWebAppManagerChromeUntrustedTest);

INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_REGULAR_PROFILE_P(
    SystemWebAppManagerOriginTrialsBrowserTest);

INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_REGULAR_PROFILE_P(
    SystemWebAppManagerUninstallBrowserTest);

INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_REGULAR_PROFILE_P(
    SystemWebAppManagerInstallAllAppsBrowserTest);

INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_REGULAR_PROFILE_P(
    SystemWebAppManagerShortcutTest);

INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_REGULAR_PROFILE_P(
    SystemWebAppManagerBackgroundTaskTest);

INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_REGULAR_PROFILE_P(
    SystemWebAppManagerHasTabStripWithNewTabButtonTest);

INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_REGULAR_PROFILE_P(
    SystemWebAppManagerHasTabStripWithHiddenNewTabButtonTest);

INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_REGULAR_PROFILE_P(
    SystemWebAppManagerHasNoTabStripWithNewTabButtonTest);

INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_REGULAR_PROFILE_P(
    SystemWebAppManagerHasNoTabStripWithHiddenNewTabButtonTest);

INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_REGULAR_PROFILE_P(
    SystemWebAppManagerDefaultBoundsTest);

INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_REGULAR_PROFILE_P(
    SystemWebAppSingleWindowTest);

INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_REGULAR_PROFILE_P(
    SystemWebAppAccessibilityTest);

INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_REGULAR_PROFILE_P(
    SystemWebAppAbortsLaunchTest);

INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_REGULAR_PROFILE_P(
    SystemWebAppIconHealthMetricsTest);

INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_REGULAR_PROFILE_P(
    SystemWebAppManagerContextMenuBrowserTest);

INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_REGULAR_PROFILE_P(
    SystemWebAppManagerLaunchWithUrlBrowserTest);

}  // namespace ash
