// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/platform_apps/app_browsertest_util.h"

#include <memory>
#include <string>

#include "base/command_line.h"
#include "base/strings/stringprintf.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/browser_app_launcher.h"
#include "chrome/browser/extensions/api/tabs/tabs_api.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/apps/chrome_app_delegate.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/extensions/app_launch_params.h"
#include "chrome/browser/web_applications/extension_status_utils.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/api_test_utils.h"
#include "extensions/browser/app_window/app_window_contents.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/app_window/native_app_window.h"
#include "extensions/browser/process_manager.h"
#include "extensions/common/constants.h"
#include "extensions/common/switches.h"
#include "extensions/test/extension_test_message_listener.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ui/ash/cast_config/cast_config_controller_media_router.h"
#include "components/media_router/browser/media_routes_observer.h"
#include "testing/gmock/include/gmock/gmock.h"
#endif

using content::WebContents;

namespace {

const char kAppWindowTestApp[] = "app_window/generic";

}  // namespace

namespace extensions {

namespace utils = api_test_utils;

PlatformAppBrowserTest::PlatformAppBrowserTest()
    : enable_chrome_apps_(
          &extensions::testing::g_enable_chrome_apps_for_testing,
          true) {
  ChromeAppDelegate::DisableExternalOpenForTesting();
}

PlatformAppBrowserTest::~PlatformAppBrowserTest() {}

void PlatformAppBrowserTest::SetUpCommandLine(base::CommandLine* command_line) {
  // Skips ExtensionApiTest::SetUpCommandLine.
  ExtensionBrowserTest::SetUpCommandLine(command_line);

  // Make event pages get suspended quicker.
  ProcessManager::SetEventPageIdleTimeForTesting(1000);
  ProcessManager::SetEventPageSuspendingTimeForTesting(1000);
}

void PlatformAppBrowserTest::SetUpOnMainThread() {
  ExtensionApiTest::SetUpOnMainThread();
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Mock the Media Router in extension api tests. Several of the
  // PlatformAppBrowserTest suites call RunAllPendingInMessageLoop() when there
  // are mojo messages that will call back into Profile creation through the
  // media router.
  media_router_ = std::make_unique<media_router::MockMediaRouter>();
  ON_CALL(*media_router_, RegisterMediaSinksObserver(::testing::_))
      .WillByDefault(::testing::Return(true));

  CastConfigControllerMediaRouter::SetMediaRouterForTest(media_router_.get());
#endif
}

void PlatformAppBrowserTest::TearDownOnMainThread() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  CastConfigControllerMediaRouter::SetMediaRouterForTest(nullptr);
#endif
  ExtensionApiTest::TearDownOnMainThread();
}

// static
AppWindow* PlatformAppBrowserTest::GetFirstAppWindowForBrowser(
    Browser* browser) {
  AppWindowRegistry* app_registry = AppWindowRegistry::Get(browser->profile());
  const AppWindowRegistry::AppWindowList& app_windows =
      app_registry->app_windows();

  auto iter = app_windows.begin();
  if (iter != app_windows.end())
    return *iter;

  return nullptr;
}

const Extension* PlatformAppBrowserTest::LoadAndLaunchPlatformApp(
    const char* name,
    ExtensionTestMessageListener* listener) {
  DCHECK(listener);
  const Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("platform_apps").AppendASCII(name));
  EXPECT_TRUE(extension);

  LaunchPlatformApp(extension);

  EXPECT_TRUE(listener->WaitUntilSatisfied())
      << "'" << listener->message() << "' message was not receieved";

  return extension;
}

const Extension* PlatformAppBrowserTest::LoadAndLaunchPlatformApp(
    const char* name,
    const std::string& message) {
  ExtensionTestMessageListener launched_listener(message);
  const Extension* extension =
      LoadAndLaunchPlatformApp(name, &launched_listener);

  return extension;
}

const Extension* PlatformAppBrowserTest::InstallPlatformApp(const char* name) {
  const Extension* extension = InstallExtension(
      test_data_dir_.AppendASCII("platform_apps").AppendASCII(name), 1);
  EXPECT_TRUE(extension);

  return extension;
}

const Extension* PlatformAppBrowserTest::InstallHostedApp() {
  const Extension* extension =
      InstallExtension(test_data_dir_.AppendASCII("hosted_app"), 1);
  EXPECT_TRUE(extension);

  return extension;
}

const Extension* PlatformAppBrowserTest::InstallAndLaunchPlatformApp(
    const char* name) {
  content::CreateAndLoadWebContentsObserver app_loaded_observer;

  const Extension* extension = InstallPlatformApp(name);

  LaunchPlatformApp(extension);

  app_loaded_observer.Wait();

  return extension;
}

void PlatformAppBrowserTest::LaunchPlatformApp(const Extension* extension) {
  apps::AppServiceProxyFactory::GetForProfile(profile())
      ->BrowserAppLauncher()
      ->LaunchAppWithParamsForTesting(apps::AppLaunchParams(
          extension->id(), apps::LaunchContainer::kLaunchContainerNone,
          WindowOpenDisposition::NEW_WINDOW, apps::LaunchSource::kFromTest));
}

void PlatformAppBrowserTest::LaunchHostedApp(const Extension* extension) {
  apps::AppServiceProxyFactory::GetForProfile(profile())
      ->BrowserAppLauncher()
      ->LaunchAppWithParamsForTesting(CreateAppLaunchParamsUserContainer(
          browser()->profile(), extension,
          WindowOpenDisposition::NEW_FOREGROUND_TAB,
          apps::LaunchSource::kFromCommandLine));
}

WebContents* PlatformAppBrowserTest::GetFirstAppWindowWebContents() {
  AppWindow* window = GetFirstAppWindow();
  if (window)
    return window->web_contents();

  return nullptr;
}

AppWindow* PlatformAppBrowserTest::GetFirstAppWindow() {
  return GetFirstAppWindowForBrowser(browser());
}

AppWindow* PlatformAppBrowserTest::GetFirstAppWindowForApp(
    const std::string& app_id) {
  AppWindowRegistry* app_registry =
      AppWindowRegistry::Get(browser()->profile());
  const AppWindowRegistry::AppWindowList& app_windows =
      app_registry->GetAppWindowsForApp(app_id);

  auto iter = app_windows.begin();
  if (iter != app_windows.end())
    return *iter;

  return nullptr;
}

size_t PlatformAppBrowserTest::RunGetWindowsFunctionForExtension(
    const Extension* extension) {
  scoped_refptr<WindowsGetAllFunction> function = new WindowsGetAllFunction();
  function->set_extension(extension);
  base::Value::List result(
      utils::ToList(utils::RunFunctionAndReturnSingleResult(
          function.get(), "[]", browser()->profile())));
  return result.size();
}

bool PlatformAppBrowserTest::RunGetWindowFunctionForExtension(
    int window_id,
    const Extension* extension) {
  scoped_refptr<WindowsGetFunction> function = new WindowsGetFunction();
  function->set_extension(extension);
  utils::RunFunction(function.get(), base::StringPrintf("[%u]", window_id),
                     browser()->profile(), api_test_utils::FunctionMode::kNone);
  return *function->response_type() == ExtensionFunction::SUCCEEDED;
}

size_t PlatformAppBrowserTest::GetAppWindowCount() {
  return AppWindowRegistry::Get(browser()->profile())->app_windows().size();
}

size_t PlatformAppBrowserTest::GetAppWindowCountForApp(
    const std::string& app_id) {
  return AppWindowRegistry::Get(browser()->profile())
      ->GetAppWindowsForApp(app_id)
      .size();
}

AppWindow* PlatformAppBrowserTest::CreateAppWindow(
    content::BrowserContext* context,
    const Extension* extension) {
  return CreateAppWindowFromParams(context, extension,
                                   AppWindow::CreateParams());
}

AppWindow* PlatformAppBrowserTest::CreateAppWindowFromParams(
    content::BrowserContext* context,
    const Extension* extension,
    const AppWindow::CreateParams& params) {
  AppWindow* window = new AppWindow(
      browser()->profile(),
      std::make_unique<ChromeAppDelegate>(browser()->profile(), true),
      extension);
  ProcessManager* process_manager = ProcessManager::Get(context);
  ExtensionHost* background_host =
      process_manager->GetBackgroundHostForExtension(extension->id());
  window->Init(GURL(std::string()),
               std::make_unique<AppWindowContentsImpl>(window),
               background_host->host_contents()->GetPrimaryMainFrame(), params);
  return window;
}

void PlatformAppBrowserTest::CloseAppWindow(AppWindow* window) {
  content::WebContentsDestroyedWatcher destroyed_watcher(
      window->web_contents());
  window->GetBaseWindow()->Close();
  destroyed_watcher.Wait();
}

void PlatformAppBrowserTest::CallAdjustBoundsToBeVisibleOnScreenForAppWindow(
    AppWindow* window,
    const gfx::Rect& cached_bounds,
    const gfx::Rect& cached_screen_bounds,
    const gfx::Rect& current_screen_bounds,
    const gfx::Size& minimum_size,
    gfx::Rect* bounds) {
  window->AdjustBoundsToBeVisibleOnScreen(cached_bounds, cached_screen_bounds,
                                          current_screen_bounds, minimum_size,
                                          bounds);
}

AppWindow* PlatformAppBrowserTest::CreateTestAppWindow(
    const std::string& window_create_options) {
  ExtensionTestMessageListener launched_listener("launched",
                                                 ReplyBehavior::kWillReply);
  ExtensionTestMessageListener loaded_listener("window_loaded");

  // Load and launch the test app.
  const Extension* extension =
      LoadAndLaunchPlatformApp(kAppWindowTestApp, &launched_listener);
  EXPECT_TRUE(extension);
  EXPECT_TRUE(launched_listener.WaitUntilSatisfied());

  // Send the options for window creation.
  launched_listener.Reply(window_create_options);

  // Wait for the window to be opened and loaded.
  EXPECT_TRUE(loaded_listener.WaitUntilSatisfied());

  EXPECT_EQ(1U, GetAppWindowCount());
  AppWindow* app_window = GetFirstAppWindow();
  return app_window;
}

NativeAppWindow* PlatformAppBrowserTest::GetNativeAppWindowForAppWindow(
    AppWindow* window) {
  return window->native_app_window_.get();
}

void ExperimentalPlatformAppBrowserTest::SetUpCommandLine(
    base::CommandLine* command_line) {
  PlatformAppBrowserTest::SetUpCommandLine(command_line);
  command_line->AppendSwitch(switches::kEnableExperimentalExtensionApis);
}

}  // namespace extensions
