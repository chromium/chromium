// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/full_restore/app_launch_handler.h"

#include <cstdint>
#include <map>
#include <memory>

#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/autotest_desks_api.h"
#include "ash/shell.h"
#include "ash/wm/window_state.h"
#include "ash/wm/wm_event.h"
#include "base/test/scoped_feature_list.h"
#include "base/timer/timer.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/apps/platform_apps/app_browsertest_util.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/ash/web_applications/system_web_app_integration_test.h"
#include "chrome/browser/chromeos/full_restore/full_restore_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/launcher/app_service/exo_app_type_resolver.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/components/web_app_id.h"
#include "chrome/browser/web_applications/components/web_application_info.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/arc/arc_service_manager.h"
#include "components/arc/arc_util.h"
#include "components/arc/mojom/app.mojom.h"
#include "components/arc/session/arc_bridge_service.h"
#include "components/arc/test/fake_app_instance.h"
#include "components/exo/shell_surface_util.h"
#include "components/full_restore/app_launch_info.h"
#include "components/full_restore/full_restore_info.h"
#include "components/full_restore/full_restore_read_handler.h"
#include "components/full_restore/full_restore_save_handler.h"
#include "components/full_restore/full_restore_utils.h"
#include "components/full_restore/window_info.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/base/window_open_disposition.h"
#include "ui/display/types/display_constants.h"

namespace mojo {

template <>
struct TypeConverter<arc::mojom::ArcPackageInfoPtr,
                     arc::mojom::ArcPackageInfo> {
  static arc::mojom::ArcPackageInfoPtr Convert(
      const arc::mojom::ArcPackageInfo& package_info) {
    return package_info.Clone();
  }
};

}  // namespace mojo

namespace chromeos {
namespace full_restore {

namespace {

constexpr char kAppId[] = "mldnpnnoiloahfhddhobgjeophloidmo";
constexpr int32_t kId = 100;

constexpr char kTestAppName[] = "Test ARC App";
constexpr char kTestAppName2[] = "Test ARC App 2";
constexpr char kTestAppPackage[] = "test.arc.app.package";
constexpr char kTestAppActivity[] = "test.arc.app.package.activity";
constexpr char kTestAppActivity2[] = "test.arc.app.package.activity2";

// Test values for a test WindowInfo object.
constexpr int kActivationIndex = 2;
constexpr int kDeskId = 2;
constexpr gfx::Rect kRestoreBounds(100, 100);
constexpr gfx::Rect kCurrentBounds(200, 200);
constexpr chromeos::WindowStateType kWindowStateType =
    chromeos::WindowStateType::kLeftSnapped;

class TestFullRestoreInfoObserver
    : public ::full_restore::FullRestoreInfo::Observer {
 public:
  // ::full_restore::FullRestoreInfo::Observer:
  void OnAppLaunched(aura::Window* window) override {
    ++launched_windows_[window];
  }

  void OnWindowInitialized(aura::Window* window) override {
    ++initialized_windows_[window];
  }

  void Reset() {
    launched_windows_.clear();
    initialized_windows_.clear();
  }

  std::map<aura::Window*, int>& launched_windows() { return launched_windows_; }
  std::map<aura::Window*, int>& initialized_windows() {
    return initialized_windows_;
  }

 private:
  std::map<aura::Window*, int> launched_windows_;
  std::map<aura::Window*, int> initialized_windows_;
};

// Creates a WindowInfo object and then saves it.
void CreateAndSaveWindowInfo(int desk_id,
                             const gfx::Rect& restore_bounds,
                             const gfx::Rect& current_bounds,
                             chromeos::WindowStateType window_state_type) {
  // A window is needed for SaveWindowInfo, but all it needs is a layer and
  // kWindowIdKey to be set. |window| needs to be alive when save is called for
  // SaveWindowInfo to work.
  auto window = std::make_unique<aura::Window>(nullptr);
  window->Init(ui::LAYER_NOT_DRAWN);
  window->SetProperty(::full_restore::kWindowIdKey, kId);

  ::full_restore::WindowInfo window_info;
  window_info.window = window.get();
  window_info.desk_id = desk_id;
  window_info.restore_bounds = restore_bounds;
  window_info.current_bounds = current_bounds;
  window_info.window_state_type = window_state_type;
  ::full_restore::SaveWindowInfo(window_info);
}

void SaveWindowInfo(aura::Window* window) {
  ::full_restore::WindowInfo window_info;
  window_info.window = window;
  window_info.activation_index = kActivationIndex;
  window_info.desk_id = kDeskId;
  window_info.restore_bounds = kRestoreBounds;
  window_info.current_bounds = kCurrentBounds;
  window_info.window_state_type = kWindowStateType;
  ::full_restore::SaveWindowInfo(window_info);
}

void SaveWindowInfo(aura::Window* window, uint32_t activation_index) {
  ::full_restore::WindowInfo window_info;
  window_info.window = window;
  window_info.activation_index = activation_index;
  ::full_restore::SaveWindowInfo(window_info);
}

void WaitForAppLaunchInfoSaved() {
  ::full_restore::FullRestoreSaveHandler* save_handler =
      ::full_restore::FullRestoreSaveHandler::GetInstance();
  base::OneShotTimer* timer = save_handler->GetTimerForTesting();
  if (timer->IsRunning()) {
    // Simulate timeout, and the launch info is saved.
    timer->FireNow();
  }
  content::RunAllTasksUntilIdle();
}

std::string GetTestApp1Id(const std::string& package_name) {
  return ArcAppListPrefs::GetAppId(package_name, kTestAppActivity);
}

std::string GetTestApp2Id(const std::string& package_name) {
  return ArcAppListPrefs::GetAppId(package_name, kTestAppActivity2);
}

std::vector<arc::mojom::AppInfoPtr> GetTestAppsList(
    const std::string& package_name,
    bool multi_app) {
  std::vector<arc::mojom::AppInfoPtr> apps;

  arc::mojom::AppInfoPtr app(arc::mojom::AppInfo::New());
  app->name = kTestAppName;
  app->package_name = package_name;
  app->activity = kTestAppActivity;
  app->sticky = false;
  apps.push_back(std::move(app));

  if (multi_app) {
    app = arc::mojom::AppInfo::New();
    app->name = kTestAppName2;
    app->package_name = package_name;
    app->activity = kTestAppActivity2;
    app->sticky = false;
    apps.push_back(std::move(app));
  }

  return apps;
}

// Creates an exo app window and sets its shell application id.
views::Widget* CreateExoWindow(const std::string& window_app_id) {
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_WINDOW);
  params.bounds = gfx::Rect(5, 5, 20, 20);
  params.context = ash::Shell::GetPrimaryRootWindow();

  exo::WMHelper::AppPropertyResolver::Params resolver_params;
  resolver_params.app_id = window_app_id;
  resolver_params.for_creation = true;
  ExoAppTypeResolver().PopulateProperties(resolver_params,
                                          params.init_properties_container);

  views::Widget* widget = new views::Widget();
  widget->Init(std::move(params));
  exo::SetShellApplicationId(widget->GetNativeWindow(), window_app_id);
  widget->Show();
  widget->Activate();
  return widget;
}

}  // namespace

class AppLaunchHandlerBrowserTest : public extensions::PlatformAppBrowserTest {
 public:
  AppLaunchHandlerBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(ash::features::kFullRestore);
  }
  ~AppLaunchHandlerBrowserTest() override = default;

  void CreateWebApp() {
    auto web_application_info = std::make_unique<WebApplicationInfo>();
    web_application_info->start_url = GURL("https://example.org");
    web_app::AppId app_id =
        web_app::InstallWebApp(profile(), std::move(web_application_info));

    // Wait for app service to see the newly installed app.
    auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile());
    proxy->FlushMojoCallsForTesting();
  }

  bool FindWebAppWindow() {
    for (auto* browser : *BrowserList::GetInstance()) {
      aura::Window* window = browser->window()->GetNativeWindow();
      if (window->GetProperty(::full_restore::kRestoreWindowIdKey) == kId)
        return true;
    }
    return false;
  }

  void SaveChromeAppLaunchInfo(const std::string& app_id) {
    ::full_restore::SaveAppLaunchInfo(
        profile()->GetPath(),
        std::make_unique<::full_restore::AppLaunchInfo>(
            app_id, apps::mojom::LaunchContainer::kLaunchContainerWindow,
            WindowOpenDisposition::NEW_WINDOW, display::kDefaultDisplayId,
            std::vector<base::FilePath>{}, nullptr));
  }

  std::unique_ptr<::full_restore::WindowInfo> GetWindowInfo(
      int32_t restore_window_id) {
    return ::full_restore::FullRestoreReadHandler::GetInstance()->GetWindowInfo(
        restore_window_id);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(AppLaunchHandlerBrowserTest, NotLaunchBrowser) {
  // Add app launch info.
  ::full_restore::SaveAppLaunchInfo(
      profile()->GetPath(), std::make_unique<::full_restore::AppLaunchInfo>(
                                extension_misc::kChromeAppId, kId));

  WaitForAppLaunchInfoSaved();

  size_t count = BrowserList::GetInstance()->size();

  // Create AppLaunchHandler, and set should restore.
  auto app_launch_handler = std::make_unique<AppLaunchHandler>(profile());
  app_launch_handler->SetShouldRestore();

  content::RunAllTasksUntilIdle();

  // Verify there is no new browser launched.
  EXPECT_EQ(count, BrowserList::GetInstance()->size());
}

IN_PROC_BROWSER_TEST_F(AppLaunchHandlerBrowserTest, RestoreAndAddApp) {
  // Add app launch info.
  ::full_restore::SaveAppLaunchInfo(
      profile()->GetPath(),
      std::make_unique<::full_restore::AppLaunchInfo>(
          kAppId, kId, apps::mojom::LaunchContainer::kLaunchContainerWindow,
          WindowOpenDisposition::NEW_WINDOW, display::kDefaultDisplayId,
          std::vector<base::FilePath>{}, nullptr));

  WaitForAppLaunchInfoSaved();

  // Create AppLaunchHandler, and set should restore.
  auto app_launch_handler = std::make_unique<AppLaunchHandler>(profile());
  app_launch_handler->SetShouldRestore();

  CreateWebApp();

  content::RunAllTasksUntilIdle();

  EXPECT_TRUE(FindWebAppWindow());
}

IN_PROC_BROWSER_TEST_F(AppLaunchHandlerBrowserTest, AddAppAndRestore) {
  // Add app launch info.
  ::full_restore::SaveAppLaunchInfo(
      profile()->GetPath(),
      std::make_unique<::full_restore::AppLaunchInfo>(
          kAppId, kId, apps::mojom::LaunchContainer::kLaunchContainerWindow,
          WindowOpenDisposition::NEW_WINDOW, display::kDefaultDisplayId,
          std::vector<base::FilePath>{}, nullptr));

  WaitForAppLaunchInfoSaved();

  // Create AppLaunchHandler.
  auto app_launch_handler = std::make_unique<AppLaunchHandler>(profile());

  CreateWebApp();

  // Set should restore.
  app_launch_handler->SetShouldRestore();

  content::RunAllTasksUntilIdle();

  EXPECT_TRUE(FindWebAppWindow());
}

IN_PROC_BROWSER_TEST_F(AppLaunchHandlerBrowserTest, NotRestore) {
  // Add app launch infos.
  ::full_restore::SaveAppLaunchInfo(
      profile()->GetPath(), std::make_unique<::full_restore::AppLaunchInfo>(
                                extension_misc::kChromeAppId, kId));
  ::full_restore::SaveAppLaunchInfo(
      profile()->GetPath(),
      std::make_unique<::full_restore::AppLaunchInfo>(
          kAppId, kId, apps::mojom::LaunchContainer::kLaunchContainerWindow,
          WindowOpenDisposition::NEW_WINDOW, display::kDefaultDisplayId,
          std::vector<base::FilePath>{}, nullptr));

  WaitForAppLaunchInfoSaved();

  size_t count = BrowserList::GetInstance()->size();

  // Create AppLaunchHandler.
  auto app_launch_handler = std::make_unique<AppLaunchHandler>(profile());
  app_launch_handler->LaunchBrowserWhenReady();

  CreateWebApp();

  content::RunAllTasksUntilIdle();

  // Verify there is no new browser launched.
  EXPECT_EQ(count, BrowserList::GetInstance()->size());
  EXPECT_FALSE(FindWebAppWindow());
}

IN_PROC_BROWSER_TEST_F(AppLaunchHandlerBrowserTest, RestoreAndLaunchBrowser) {
  size_t count = BrowserList::GetInstance()->size();

  // Add the chrome browser launch info.
  ::full_restore::SaveAppLaunchInfo(
      profile()->GetPath(), std::make_unique<::full_restore::AppLaunchInfo>(
                                extension_misc::kChromeAppId, kId));

  WaitForAppLaunchInfoSaved();

  // Create AppLaunchHandler.
  auto app_launch_handler = std::make_unique<AppLaunchHandler>(profile());

  // Set should restore.
  app_launch_handler->SetShouldRestore();
  content::RunAllTasksUntilIdle();

  app_launch_handler->LaunchBrowserWhenReady();
  content::RunAllTasksUntilIdle();

  // Verify there is new browser launched.
  EXPECT_EQ(count + 1, BrowserList::GetInstance()->size());
}

IN_PROC_BROWSER_TEST_F(AppLaunchHandlerBrowserTest,
                       RestoreAndNoBrowserLaunchInfo) {
  size_t count = BrowserList::GetInstance()->size();

  // Add app launch info, but no browser launch info.
  ::full_restore::SaveAppLaunchInfo(
      profile()->GetPath(),
      std::make_unique<::full_restore::AppLaunchInfo>(
          kAppId, kId, apps::mojom::LaunchContainer::kLaunchContainerWindow,
          WindowOpenDisposition::NEW_WINDOW, display::kDefaultDisplayId,
          std::vector<base::FilePath>{}, nullptr));

  // Remove the browser app to mock no browser launch info.
  ::full_restore::FullRestoreSaveHandler::GetInstance()->RemoveApp(
      profile()->GetPath(), extension_misc::kChromeAppId);

  WaitForAppLaunchInfoSaved();

  // Create AppLaunchHandler.
  auto app_launch_handler = std::make_unique<AppLaunchHandler>(profile());

  // Set should restore.
  app_launch_handler->SetShouldRestore();
  content::RunAllTasksUntilIdle();

  app_launch_handler->LaunchBrowserWhenReady();
  content::RunAllTasksUntilIdle();

  // Verify there is new browser launched.
  EXPECT_EQ(count, BrowserList::GetInstance()->size());
}

IN_PROC_BROWSER_TEST_F(AppLaunchHandlerBrowserTest, LaunchBrowserAndRestore) {
  size_t count = BrowserList::GetInstance()->size();

  // Add the chrome browser launch info.
  ::full_restore::SaveAppLaunchInfo(
      profile()->GetPath(), std::make_unique<::full_restore::AppLaunchInfo>(
                                extension_misc::kChromeAppId, kId));

  WaitForAppLaunchInfoSaved();

  // Create AppLaunchHandler.
  auto app_launch_handler = std::make_unique<AppLaunchHandler>(profile());

  app_launch_handler->LaunchBrowserWhenReady();
  content::RunAllTasksUntilIdle();

  // Verify there is no new browser launched.
  EXPECT_EQ(count, BrowserList::GetInstance()->size());

  // Set should restore.
  app_launch_handler->SetShouldRestore();
  content::RunAllTasksUntilIdle();

  // Verify there is new browser launched.
  EXPECT_EQ(count + 1, BrowserList::GetInstance()->size());
}

IN_PROC_BROWSER_TEST_F(AppLaunchHandlerBrowserTest,
                       RestoreAndLaunchBrowserAndAddApp) {
  size_t count = BrowserList::GetInstance()->size();

  // Add app launch infos.
  ::full_restore::SaveAppLaunchInfo(
      profile()->GetPath(), std::make_unique<::full_restore::AppLaunchInfo>(
                                extension_misc::kChromeAppId, kId));
  ::full_restore::SaveAppLaunchInfo(
      profile()->GetPath(),
      std::make_unique<::full_restore::AppLaunchInfo>(
          kAppId, kId, apps::mojom::LaunchContainer::kLaunchContainerWindow,
          WindowOpenDisposition::NEW_WINDOW, display::kDefaultDisplayId,
          std::vector<base::FilePath>{}, nullptr));

  WaitForAppLaunchInfoSaved();

  // Create AppLaunchHandler, and set should restore.
  auto app_launch_handler = std::make_unique<AppLaunchHandler>(profile());
  app_launch_handler->SetShouldRestore();
  content::RunAllTasksUntilIdle();

  app_launch_handler->LaunchBrowserWhenReady();
  content::RunAllTasksUntilIdle();

  CreateWebApp();
  content::RunAllTasksUntilIdle();

  // Verify there is new browser launched.
  EXPECT_EQ(count + 2, BrowserList::GetInstance()->size());
  EXPECT_TRUE(FindWebAppWindow());
}

IN_PROC_BROWSER_TEST_F(AppLaunchHandlerBrowserTest,
                       LaunchBrowserAndAddAppAndRestore) {
  size_t count = BrowserList::GetInstance()->size();

  // Add app launch infos.
  ::full_restore::SaveAppLaunchInfo(
      profile()->GetPath(), std::make_unique<::full_restore::AppLaunchInfo>(
                                extension_misc::kChromeAppId, kId));
  ::full_restore::SaveAppLaunchInfo(
      profile()->GetPath(),
      std::make_unique<::full_restore::AppLaunchInfo>(
          kAppId, kId, apps::mojom::LaunchContainer::kLaunchContainerWindow,
          WindowOpenDisposition::NEW_WINDOW, display::kDefaultDisplayId,
          std::vector<base::FilePath>{}, nullptr));

  WaitForAppLaunchInfoSaved();

  // Create AppLaunchHandler.
  auto app_launch_handler = std::make_unique<AppLaunchHandler>(profile());

  app_launch_handler->LaunchBrowserWhenReady();
  content::RunAllTasksUntilIdle();

  CreateWebApp();
  content::RunAllTasksUntilIdle();

  // Set should restore.
  app_launch_handler->SetShouldRestore();
  content::RunAllTasksUntilIdle();

  // Verify there is new browser launched.
  EXPECT_EQ(count + 2, BrowserList::GetInstance()->size());
  EXPECT_TRUE(FindWebAppWindow());
}

// Tests that the window properties on the browser window match the ones we set
// in the window info.
IN_PROC_BROWSER_TEST_F(AppLaunchHandlerBrowserTest, WindowProperties) {
  size_t count = BrowserList::GetInstance()->size();

  ::full_restore::SaveAppLaunchInfo(
      profile()->GetPath(), std::make_unique<::full_restore::AppLaunchInfo>(
                                extension_misc::kChromeAppId, kId));

  CreateAndSaveWindowInfo(kDeskId, kRestoreBounds, kCurrentBounds,
                          kWindowStateType);
  WaitForAppLaunchInfoSaved();

  // Launch the browser.
  auto app_launch_handler = std::make_unique<AppLaunchHandler>(profile());
  app_launch_handler->LaunchBrowserWhenReady();
  app_launch_handler->SetShouldRestore();
  content::RunAllTasksUntilIdle();

  ASSERT_EQ(count + 1u, BrowserList::GetInstance()->size());

  // TODO(sammiequon): Check the values from the actual browser window.
  auto window = std::make_unique<aura::Window>(nullptr);
  window->Init(ui::LAYER_NOT_DRAWN);
  window->SetProperty(::full_restore::kRestoreWindowIdKey, kId);
  auto stored_window_info = ::full_restore::GetWindowInfo(window.get());
  EXPECT_EQ(kDeskId, *stored_window_info->desk_id);
  EXPECT_EQ(kRestoreBounds, *stored_window_info->restore_bounds);
  EXPECT_EQ(kCurrentBounds, *stored_window_info->current_bounds);
  EXPECT_EQ(kWindowStateType, *stored_window_info->window_state_type);
}

IN_PROC_BROWSER_TEST_F(AppLaunchHandlerBrowserTest, RestoreChromeApp) {
  // Have 4 desks total.
  ash::AutotestDesksApi().CreateNewDesk();
  ash::AutotestDesksApi().CreateNewDesk();
  ash::AutotestDesksApi().CreateNewDesk();

  ::full_restore::SetActiveProfilePath(profile()->GetPath());

  // Create the restore data.
  const extensions::Extension* extension =
      LoadAndLaunchPlatformApp("launch", "Launched");
  ASSERT_TRUE(extension);
  SaveChromeAppLaunchInfo(extension->id());

  extensions::AppWindow* app_window = CreateAppWindow(profile(), extension);
  ASSERT_TRUE(app_window);

  auto* window = app_window->GetNativeWindow();
  SaveWindowInfo(window);

  WaitForAppLaunchInfoSaved();

  // Read from the restore data.
  auto app_launch_handler = std::make_unique<AppLaunchHandler>(profile());
  app_launch_handler->SetShouldRestore();
  content::RunAllTasksUntilIdle();

  // Verify the restore window id.
  app_window = CreateAppWindow(browser()->profile(), extension);
  ASSERT_TRUE(app_window);

  window = app_window->GetNativeWindow();
  ASSERT_TRUE(window);
  int restore_window_id =
      window->GetProperty(::full_restore::kRestoreWindowIdKey);
  EXPECT_NE(0, restore_window_id);

  auto window_info = ::full_restore::GetWindowInfo(window);
  ASSERT_TRUE(window_info);
  EXPECT_TRUE(window_info->activation_index.has_value());
  int32_t* index = window->GetProperty(::full_restore::kActivationIndexKey);
  ASSERT_TRUE(index);
  EXPECT_EQ(kActivationIndex, *index);
  EXPECT_EQ(kDeskId, window->GetProperty(aura::client::kWindowWorkspaceKey));

  // Windows created from full restore are not activated. They will become
  // activatable after a post task is run.
  views::Widget* widget = views::Widget::GetWidgetForNativeView(window);
  EXPECT_FALSE(widget->IsActive());
  EXPECT_FALSE(widget->CanActivate());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(widget->CanActivate());

  EXPECT_EQ(0, ::full_restore::FetchRestoreWindowId(extension->id()));

  // Close the window.
  CloseAppWindow(app_window);
  ASSERT_FALSE(GetWindowInfo(restore_window_id));

  // Remove the added desks.
  while (true) {
    base::RunLoop run_loop;
    if (!ash::AutotestDesksApi().RemoveActiveDesk(run_loop.QuitClosure()))
      break;
    run_loop.Run();
  }
}

IN_PROC_BROWSER_TEST_F(AppLaunchHandlerBrowserTest,
                       RestoreMultipleChromeAppWindows) {
  ::full_restore::SetActiveProfilePath(profile()->GetPath());

  // Create the restore data, 2 windows for 1 chrome app.
  const extensions::Extension* extension =
      LoadAndLaunchPlatformApp("launch", "Launched");
  ASSERT_TRUE(extension);
  const std::string& app_id = extension->id();
  SaveChromeAppLaunchInfo(app_id);

  extensions::AppWindow* app_window1 = CreateAppWindow(profile(), extension);
  ASSERT_TRUE(app_window1);
  auto* window1 = app_window1->GetNativeWindow();
  SaveWindowInfo(window1);

  SaveChromeAppLaunchInfo(app_id);

  extensions::AppWindow* app_window2 = CreateAppWindow(profile(), extension);
  ASSERT_TRUE(app_window2);
  auto* window2 = app_window2->GetNativeWindow();
  SaveWindowInfo(window2);

  WaitForAppLaunchInfoSaved();

  // Read from the restore data.
  auto app_launch_handler = std::make_unique<AppLaunchHandler>(profile());
  app_launch_handler->SetShouldRestore();
  content::RunAllTasksUntilIdle();

  // Verify the restore window id;
  app_window1 = CreateAppWindow(browser()->profile(), extension);
  ASSERT_TRUE(app_window1);
  window1 = app_window1->GetNativeWindow();
  ASSERT_TRUE(window1);
  EXPECT_NE(0, window1->GetProperty(::full_restore::kRestoreWindowIdKey));

  auto window_info = ::full_restore::GetWindowInfo(window1);
  ASSERT_TRUE(window_info);
  EXPECT_TRUE(window_info->activation_index.has_value());
  EXPECT_EQ(INT32_MIN, window_info->activation_index.value());

  app_window2 = CreateAppWindow(browser()->profile(), extension);
  ASSERT_TRUE(app_window2);
  window2 = app_window2->GetNativeWindow();
  ASSERT_TRUE(window2);
  EXPECT_NE(0, window2->GetProperty(::full_restore::kRestoreWindowIdKey));

  window_info = ::full_restore::GetWindowInfo(window2);
  ASSERT_TRUE(window_info);
  EXPECT_TRUE(window_info->activation_index.has_value());
  EXPECT_EQ(INT32_MIN, window_info->activation_index.value());

  // Create a new window, verity the restore window id is 0.
  auto* app_window = CreateAppWindow(browser()->profile(), extension);
  ASSERT_TRUE(app_window);
  auto* window = app_window->GetNativeWindow();
  ASSERT_TRUE(window);
  EXPECT_EQ(0, window->GetProperty(::full_restore::kRestoreWindowIdKey));

  // Close the window.
  CloseAppWindow(app_window1);
  CloseAppWindow(app_window2);
}

class AppLaunchHandlerArcAppBrowserTest : public AppLaunchHandlerBrowserTest {
 protected:
  // AppLaunchHandlerBrowserTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    AppLaunchHandlerBrowserTest::SetUpCommandLine(command_line);
    arc::SetArcAvailableCommandLineForTesting(command_line);
  }

  void SetUpInProcessBrowserTestFixture() override {
    AppLaunchHandlerBrowserTest::SetUpInProcessBrowserTestFixture();
    arc::ArcSessionManager::SetUiEnabledForTesting(false);
  }

  void SetUpOnMainThread() override {
    AppLaunchHandlerBrowserTest::SetUpOnMainThread();
    arc::SetArcPlayStoreEnabledForProfile(profile(), true);

    // This ensures app_prefs()->GetApp() below never returns nullptr.
    base::RunLoop run_loop;
    app_prefs()->SetDefaultAppsReadyCallback(run_loop.QuitClosure());
    run_loop.Run();
  }

  void StartInstance() {
    app_instance_ = std::make_unique<arc::FakeAppInstance>(app_host());
    arc_brige_service()->app()->SetInstance(app_instance_.get());
  }

  void StopInstance() {
    if (app_instance_)
      arc_brige_service()->app()->CloseInstance(app_instance_.get());
    arc_session_manager()->Shutdown();
  }

  void SendPackageAdded(const std::string& package_name) {
    arc::mojom::ArcPackageInfo package_info;
    package_info.package_name = package_name;
    package_info.package_version = 1;
    package_info.last_backup_android_id = 1;
    package_info.last_backup_time = 1;
    package_info.sync = false;
    package_info.system = false;
    app_host()->OnPackageAdded(arc::mojom::ArcPackageInfo::From(package_info));

    base::RunLoop().RunUntilIdle();
  }

  void InstallTestApps(const std::string& package_name, bool multi_app) {
    StartInstance();

    app_host()->OnAppListRefreshed(GetTestAppsList(package_name, multi_app));

    std::unique_ptr<ArcAppListPrefs::AppInfo> app_info =
        app_prefs()->GetApp(GetTestApp1Id(package_name));
    ASSERT_TRUE(app_info);
    EXPECT_TRUE(app_info->ready);
    if (multi_app) {
      std::unique_ptr<ArcAppListPrefs::AppInfo> app_info2 =
          app_prefs()->GetApp(GetTestApp2Id(package_name));
      ASSERT_TRUE(app_info2);
      EXPECT_TRUE(app_info2->ready);
    }

    SendPackageAdded(package_name);
  }

  void CreateTask(const std::string& app_id,
                  int32_t task_id,
                  int32_t session_id) {
    auto info = app_prefs()->GetApp(app_id);
    app_host()->OnTaskCreated(task_id, info->package_name, info->activity,
                              info->name, info->intent_uri, session_id);
  }

  void SetProfile() {
    ::full_restore::FullRestoreSaveHandler::GetInstance()
        ->SetPrimaryProfilePath(profile()->GetPath());
    ::full_restore::SetActiveProfilePath(profile()->GetPath());
    test_full_restore_info_observer_.Reset();
  }

  void SaveAppLaunchInfo(const std::string& app_id, int32_t session_id) {
    ::full_restore::SaveAppLaunchInfo(
        profile()->GetPath(), std::make_unique<::full_restore::AppLaunchInfo>(
                                  app_id, ui::EventFlags::EF_NONE, session_id,
                                  display::kDefaultDisplayId));
  }

  void Restore() {
    test_full_restore_info_observer_.Reset();

    auto app_launch_handler = std::make_unique<AppLaunchHandler>(profile());
    app_launch_handler->SetShouldRestore();
    content::RunAllTasksUntilIdle();
  }

  void VerifyWindowProperty(aura::Window* window,
                            int32_t window_id,
                            int32_t restore_window_id,
                            bool hidden) {
    ASSERT_TRUE(window);
    EXPECT_EQ(window_id, window->GetProperty(::full_restore::kWindowIdKey));
    EXPECT_EQ(restore_window_id,
              window->GetProperty(::full_restore::kRestoreWindowIdKey));
    EXPECT_EQ(hidden,
              window->GetProperty(::full_restore::kParentToHiddenContainerKey));
  }

  void VerifyWindowInfo(aura::Window* window, int32_t activation_index) {
    auto window_info = ::full_restore::GetWindowInfo(window);
    ASSERT_TRUE(window_info);
    EXPECT_TRUE(window_info->activation_index.has_value());
    EXPECT_EQ(activation_index, window_info->activation_index.value());
  }

  void VerifyObserver(aura::Window* window, int launch_count, int init_count) {
    auto& launched_windows =
        test_full_restore_info_observer_.launched_windows();
    if (launch_count == 0) {
      EXPECT_TRUE(launched_windows.find(window) == launched_windows.end());
    } else {
      EXPECT_EQ(launch_count, launched_windows[window]);
    }

    auto& initialized_windows =
        test_full_restore_info_observer_.initialized_windows();
    if (init_count == 0) {
      EXPECT_TRUE(initialized_windows.find(window) ==
                  initialized_windows.end());
    } else {
      EXPECT_EQ(init_count, initialized_windows[window]);
    }
  }

  ArcAppListPrefs* app_prefs() { return ArcAppListPrefs::Get(profile()); }

  arc::mojom::AppHost* app_host() { return app_prefs(); }

  TestFullRestoreInfoObserver* test_full_restore_info_observer() {
    return &test_full_restore_info_observer_;
  }

 private:
  arc::ArcSessionManager* arc_session_manager() {
    return arc::ArcSessionManager::Get();
  }

  arc::ArcBridgeService* arc_brige_service() {
    return arc::ArcServiceManager::Get()->arc_bridge_service();
  }

  std::unique_ptr<arc::FakeAppInstance> app_instance_;
  TestFullRestoreInfoObserver test_full_restore_info_observer_;
};

// Test restoration when the ARC window is created before OnTaskCreated is
// called.
IN_PROC_BROWSER_TEST_F(AppLaunchHandlerArcAppBrowserTest, RestoreArcApp) {
  SetProfile();
  InstallTestApps(kTestAppPackage, false);

  const std::string app_id = GetTestApp1Id(kTestAppPackage);
  int32_t session_id1 =
      ::full_restore::FullRestoreSaveHandler::GetInstance()->GetArcSessionId();
  ::full_restore::FullRestoreInfo::GetInstance()->AddObserver(
      test_full_restore_info_observer());

  SaveAppLaunchInfo(app_id, session_id1);

  // Create the window for app1. The task id needs to match the |window_app_id|
  // arg of CreateExoWindow.
  int32_t kTaskId1 = 100;
  views::Widget* widget = CreateExoWindow("org.chromium.arc.100");
  aura::Window* window = widget->GetNativeWindow();

  VerifyObserver(window, /*launch_count=*/0, /*init_count=*/0);
  VerifyWindowProperty(window, kTaskId1, /*restore_window_id=*/0,
                       /*hidden=*/false);

  // Simulate creating the task.
  CreateTask(app_id, kTaskId1, session_id1);

  VerifyObserver(window, /*launch_count=*/1, /*init_count=*/0);

  SaveWindowInfo(window);

  WaitForAppLaunchInfoSaved();

  Restore();
  widget->CloseNow();

  app_host()->OnTaskDestroyed(kTaskId1);

  int32_t session_id2 =
      ::full_restore::kArcSessionIdOffsetForRestoredLaunching + 1;

  // Create the window to simulate the restoration for the app. The task id
  // needs to match the |window_app_id| arg of CreateExoWindow.
  int32_t kTaskId2 = 200;
  widget = CreateExoWindow("org.chromium.arc.200");
  window = widget->GetNativeWindow();

  VerifyObserver(window, /*launch_count=*/0, /*init_count=*/1);
  VerifyWindowProperty(window, kTaskId2,
                       ::full_restore::kParentToHiddenContainer,
                       /*hidden=*/true);

  // Simulate creating the task for the restored window.
  CreateTask(app_id, kTaskId2, session_id2);

  VerifyObserver(window, /*launch_count=*/1, /*init_count=*/2);
  VerifyWindowProperty(window, kTaskId2, kTaskId1, /*hidden=*/false);
  VerifyWindowInfo(window, kActivationIndex);

  // Destroy the task and close the window.
  app_host()->OnTaskDestroyed(kTaskId2);
  widget->CloseNow();

  ASSERT_FALSE(GetWindowInfo(kTaskId1));

  ::full_restore::FullRestoreInfo::GetInstance()->RemoveObserver(
      test_full_restore_info_observer());
  StopInstance();
}

// Test restoration with multiple ARC apps, when the ARC windows are created
// before and after OnTaskCreated is called.
IN_PROC_BROWSER_TEST_F(AppLaunchHandlerArcAppBrowserTest,
                       RestoreMultipleArcApps) {
  SetProfile();
  InstallTestApps(kTestAppPackage, true);

  const std::string app_id1 = GetTestApp1Id(kTestAppPackage);
  const std::string app_id2 = GetTestApp2Id(kTestAppPackage);
  int32_t session_id1 =
      ::full_restore::FullRestoreSaveHandler::GetInstance()->GetArcSessionId();
  int32_t session_id2 =
      ::full_restore::FullRestoreSaveHandler::GetInstance()->GetArcSessionId();
  ::full_restore::FullRestoreInfo::GetInstance()->AddObserver(
      test_full_restore_info_observer());

  SaveAppLaunchInfo(app_id1, session_id1);
  SaveAppLaunchInfo(app_id2, session_id2);

  // Simulate creating kTaskId1. The task id needs to match the |window_app_id|
  // arg of CreateExoWindow.
  int32_t kTaskId1 = 100;
  CreateTask(app_id1, kTaskId1, session_id1);

  // Create the window for the app1.
  views::Widget* widget1 = CreateExoWindow("org.chromium.arc.100");
  aura::Window* window1 = widget1->GetNativeWindow();

  // Create the window for the app2. The task id needs to match the
  // |window_app_id| arg of CreateExoWindow.
  int32_t kTaskId2 = 101;
  views::Widget* widget2 = CreateExoWindow("org.chromium.arc.101");
  aura::Window* window2 = widget2->GetNativeWindow();

  // Simulate creating kTaskId2.
  CreateTask(app_id2, kTaskId2, session_id2);
  VerifyObserver(window1, /*launch_count=*/1, /*init_count=*/0);
  VerifyObserver(window2, /*launch_count=*/1, /*init_count=*/0);

  VerifyWindowProperty(window1, kTaskId1, /*restore_window_id*/ 0,
                       /*hidden=*/false);
  VerifyWindowProperty(window2, kTaskId2, /*restore_window_id*/ 0,
                       /*hidden=*/false);

  WaitForAppLaunchInfoSaved();

  int32_t activation_index1 = 11;
  int32_t activation_index2 = 12;
  SaveWindowInfo(window1, activation_index1);
  SaveWindowInfo(window2, activation_index2);

  WaitForAppLaunchInfoSaved();

  Restore();
  widget1->CloseNow();
  widget2->CloseNow();

  app_host()->OnTaskDestroyed(kTaskId1);
  app_host()->OnTaskDestroyed(kTaskId2);

  int32_t session_id3 =
      ::full_restore::kArcSessionIdOffsetForRestoredLaunching + 1;
  int32_t session_id4 =
      ::full_restore::kArcSessionIdOffsetForRestoredLaunching + 2;

  // Create the window to simulate the restoration for the app1. The task id
  // needs to match the |window_app_id| arg of CreateExoWindow.
  int32_t kTaskId3 = 201;
  widget1 = CreateExoWindow("org.chromium.arc.201");
  window1 = widget1->GetNativeWindow();

  VerifyWindowProperty(window1, kTaskId3,
                       ::full_restore::kParentToHiddenContainer,
                       /*hidden=*/true);

  // Simulate creating tasks for apps.
  CreateTask(app_id1, kTaskId3, session_id3);

  int32_t kTaskId4 = 202;
  CreateTask(app_id2, kTaskId4, session_id4);

  // Create the window to simulate the restoration for the app2.
  widget2 = CreateExoWindow("org.chromium.arc.202");
  window2 = widget2->GetNativeWindow();

  VerifyObserver(window1, /*launch_count=*/1, /*init_count=*/2);
  VerifyObserver(window2, /*launch_count=*/1, /*init_count=*/1);
  VerifyWindowProperty(window1, kTaskId3, kTaskId1, /*hidden=*/false);
  VerifyWindowProperty(window2, kTaskId4, kTaskId2, /*hidden=*/false);
  VerifyWindowInfo(window1, activation_index1);
  VerifyWindowInfo(window2, activation_index2);

  // destroy tasks and close windows.
  widget1->CloseNow();
  app_host()->OnTaskDestroyed(kTaskId3);
  app_host()->OnTaskDestroyed(kTaskId4);
  widget2->CloseNow();

  ASSERT_FALSE(GetWindowInfo(kTaskId1));
  ASSERT_FALSE(GetWindowInfo(kTaskId2));

  StopInstance();
}

class AppLaunchHandlerNoBrowserBrowserTest
    : public AppLaunchHandlerBrowserTest {
 public:
  AppLaunchHandlerNoBrowserBrowserTest() = default;
  ~AppLaunchHandlerNoBrowserBrowserTest() override = default;

  // BrowserTestBase:
  void PreRunTestOnMainThread() override {
    set_skip_initial_restore(true);

    AppLaunchHandlerBrowserTest::PreRunTestOnMainThread();
  }
};

IN_PROC_BROWSER_TEST_F(AppLaunchHandlerNoBrowserBrowserTest,
                       NoBrowserOnLaunch) {
  EXPECT_TRUE(BrowserList::GetInstance()->empty());
}

class AppLaunchHandlerSystemWebAppsBrowserTest
    : public SystemWebAppIntegrationTest {
 public:
  AppLaunchHandlerSystemWebAppsBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(ash::features::kFullRestore);
  }
  ~AppLaunchHandlerSystemWebAppsBrowserTest() override = default;

  Browser* LaunchSystemWebApp() {
    WaitForTestSystemAppInstall();

    auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile());
    content::TestNavigationObserver navigation_observer(
        GURL("chrome://help-app/"));
    navigation_observer.StartWatchingNewWebContents();

    proxy->Launch(
        *GetManager().GetAppIdForSystemApp(web_app::SystemAppType::HELP),
        ui::EventFlags::EF_NONE, apps::mojom::LaunchSource::kFromChromeInternal,
        apps::MakeWindowInfo(display::kDefaultDisplayId));

    navigation_observer.Wait();

    return BrowserList::GetInstance()->GetLastActive();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(AppLaunchHandlerSystemWebAppsBrowserTest, LaunchSWA) {
  Browser* app_browser = LaunchSystemWebApp();
  ASSERT_TRUE(app_browser);
  ASSERT_NE(browser(), app_browser);

  // Get the window id.
  aura::Window* window = app_browser->window()->GetNativeWindow();
  int32_t window_id = window->GetProperty(::full_restore::kWindowIdKey);

  WaitForAppLaunchInfoSaved();

  // Create AppLaunchHandler.
  auto app_launch_handler = std::make_unique<AppLaunchHandler>(profile());

  // Close app_browser so that the SWA can be relaunched.
  web_app::CloseAndWait(app_browser);

  // Set should restore.
  app_launch_handler->SetShouldRestore();

  // Wait for the restoration.
  content::RunAllTasksUntilIdle();

  // Get the restored browser for the system web app.
  Browser* restore_app_browser = BrowserList::GetInstance()->GetLastActive();
  ASSERT_TRUE(restore_app_browser);
  ASSERT_NE(browser(), restore_app_browser);

  // Get the restore window id.
  window = restore_app_browser->window()->GetNativeWindow();
  int32_t restore_window_id =
      window->GetProperty(::full_restore::kRestoreWindowIdKey);

  EXPECT_EQ(window_id, restore_window_id);
}

IN_PROC_BROWSER_TEST_P(AppLaunchHandlerSystemWebAppsBrowserTest,
                       WindowProperties) {
  Browser* app_browser = LaunchSystemWebApp();
  ASSERT_TRUE(app_browser);
  ASSERT_NE(browser(), app_browser);

  // Get the window id.
  aura::Window* window = app_browser->window()->GetNativeWindow();
  int32_t window_id = window->GetProperty(::full_restore::kWindowIdKey);

  // Snap |window| to the left and store its window properties.
  // TODO(sammiequon): Store and check desk id and restore bounds.
  auto* window_state = ash::WindowState::Get(window);
  const ash::WMEvent left_snap_event(ash::WM_EVENT_SNAP_LEFT);
  window_state->OnWMEvent(&left_snap_event);
  const WindowStateType pre_save_state_type = window_state->GetStateType();
  EXPECT_EQ(chromeos::WindowStateType::kLeftSnapped, pre_save_state_type);
  const gfx::Rect pre_save_bounds = window->GetBoundsInScreen();

  SaveWindowInfo(window);
  WaitForAppLaunchInfoSaved();

  // Create AppLaunchHandler.
  auto app_launch_handler = std::make_unique<AppLaunchHandler>(profile());

  // Close |app_browser| so that the SWA can be relaunched.
  web_app::CloseAndWait(app_browser);

  // Set should restore.
  app_launch_handler->SetShouldRestore();

  // Wait for the restoration.
  // TODO(chinsenj|nancylingwang): Look into using a more specific signal to
  // detect when restoration is done.
  content::RunAllTasksUntilIdle();

  // Get the restored browser for the system web app.
  Browser* restore_app_browser = BrowserList::GetInstance()->GetLastActive();
  ASSERT_TRUE(restore_app_browser);
  ASSERT_NE(browser(), restore_app_browser);

  // Get the restored browser's window.
  window = restore_app_browser->window()->GetNativeWindow();
  ASSERT_EQ(window_id,
            window->GetProperty(::full_restore::kRestoreWindowIdKey));

  // Check that |window|'s properties match the one's we stored.
  EXPECT_EQ(pre_save_bounds, window->GetBoundsInScreen());
  window_state = ash::WindowState::Get(window);
  EXPECT_EQ(pre_save_state_type, window_state->GetStateType());

  // Verify that |window_state| has viable restore bounds for when the user
  // wants to return to normal window show state. Regression test for
  // https://crbug.com/1188986.
  EXPECT_TRUE(window_state->HasRestoreBounds());
}

INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_REGULAR_PROFILE_P(
    AppLaunchHandlerSystemWebAppsBrowserTest);

}  // namespace full_restore
}  // namespace chromeos
