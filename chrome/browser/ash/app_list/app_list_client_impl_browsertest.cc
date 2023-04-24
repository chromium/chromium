// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/app_list/app_list_metrics.h"
#include "ash/public/cpp/test/app_list_test_api.h"
#include "ash/public/cpp/test/shell_test_api.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/shell.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/apps/platform_apps/app_browsertest_util.h"
#include "chrome/browser/ash/app_list/app_list_client_impl.h"
#include "chrome/browser/ash/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ash/app_list/app_list_model_updater.h"
#include "chrome/browser/ash/app_list/app_list_syncable_service_factory.h"
#include "chrome/browser/ash/app_list/chrome_app_list_item.h"
#include "chrome/browser/ash/app_list/search/search_controller.h"
#include "chrome/browser/ash/app_list/search/test/app_list_search_test_helper.h"
#include "chrome/browser/ash/app_list/search/test/search_results_changed_waiter.h"
#include "chrome/browser/ash/app_list/test/chrome_app_list_test_support.h"
#include "chrome/browser/ash/file_manager/app_id.h"
#include "chrome/browser/ash/login/demo_mode/demo_session.h"
#include "chrome/browser/ash/login/demo_mode/demo_setup_test_utils.h"
#include "chrome/browser/ash/login/login_manager_test.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/ui/user_adding_screen.h"
#include "chrome/browser/ash/login/users/chrome_user_manager.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ash/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/browser/ui/webui/settings/chromeos/constants/routes.mojom.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/app_constants/constants.h"
#include "components/browser_sync/browser_sync_switches.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_names.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/common/constants.h"
#include "ui/aura/window.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/display/display.h"
#include "ui/display/scoped_display_for_new_windows.h"
#include "ui/display/screen.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/wm/core/window_util.h"
#include "ui/wm/public/activation_change_observer.h"
#include "ui/wm/public/activation_client.h"

// Browser Test for AppListClientImpl.
using AppListClientImplBrowserTest = extensions::PlatformAppBrowserTest;

namespace {

class TestObserver : public app_list::AppListSyncableService::Observer {
 public:
  explicit TestObserver(app_list::AppListSyncableService* syncable_service)
      : syncable_service_(syncable_service) {
    syncable_service_->AddObserverAndStart(this);
  }
  TestObserver(const TestObserver&) = delete;
  TestObserver& operator=(const TestObserver&) = delete;
  ~TestObserver() override { syncable_service_->RemoveObserver(this); }

  size_t add_or_update_count() const { return add_or_update_count_; }

  // app_list::AppListSyncableService::Observer:
  void OnSyncModelUpdated() override {}
  void OnAddOrUpdateFromSyncItemForTest() override { ++add_or_update_count_; }

 private:
  const raw_ptr<app_list::AppListSyncableService, ExperimentalAsh>
      syncable_service_;
  size_t add_or_update_count_ = 0;
};

class ActiveWindowWaiter : public wm::ActivationChangeObserver {
 public:
  explicit ActiveWindowWaiter(aura::Window* root_window) {
    observation_.Observe(wm::GetActivationClient(root_window));
  }

  ActiveWindowWaiter(const ActiveWindowWaiter&) = delete;
  ActiveWindowWaiter& operator=(const ActiveWindowWaiter&) = delete;

  ~ActiveWindowWaiter() override = default;

  aura::Window* Wait() {
    run_loop_.Run();
    return found_window_;
  }

  void OnWindowActivated(wm::ActivationChangeObserver::ActivationReason reason,
                         aura::Window* gained_active,
                         aura::Window* lost_active) override {
    if (gained_active) {
      found_window_ = gained_active;
      observation_.Reset();
      run_loop_.Quit();
    }
  }

 private:
  base::RunLoop run_loop_;
  raw_ptr<aura::Window, ExperimentalAsh> found_window_ = nullptr;

  base::ScopedObservation<wm::ActivationClient, wm::ActivationChangeObserver>
      observation_{this};
};

}  // namespace

// Test AppListClient::IsAppOpen for extension apps.
IN_PROC_BROWSER_TEST_F(AppListClientImplBrowserTest, IsExtensionAppOpen) {
  AppListControllerDelegate* delegate = AppListClientImpl::GetInstance();
  EXPECT_FALSE(delegate->IsAppOpen("fake_extension_app_id"));

  base::FilePath extension_path = test_data_dir_.AppendASCII("app");
  const extensions::Extension* extension_app = LoadExtension(extension_path);
  ASSERT_NE(nullptr, extension_app);
  EXPECT_FALSE(delegate->IsAppOpen(extension_app->id()));
  {
    content::CreateAndLoadWebContentsObserver app_loaded_observer;
    apps::AppServiceProxyFactory::GetForProfile(profile())->Launch(
        extension_app->id(),
        apps::GetEventFlags(WindowOpenDisposition::NEW_WINDOW,
                            false /* preferred_containner */),
        apps::LaunchSource::kFromTest,
        std::make_unique<apps::WindowInfo>(
            display::Screen::GetScreen()->GetPrimaryDisplay().id()));
    app_loaded_observer.Wait();
  }
  EXPECT_TRUE(delegate->IsAppOpen(extension_app->id()));
}

// Test AppListClient::IsAppOpen for platform apps.
IN_PROC_BROWSER_TEST_F(AppListClientImplBrowserTest, IsPlatformAppOpen) {
  AppListControllerDelegate* delegate = AppListClientImpl::GetInstance();
  EXPECT_FALSE(delegate->IsAppOpen("fake_platform_app_id"));

  const extensions::Extension* app = InstallPlatformApp("minimal");
  EXPECT_FALSE(delegate->IsAppOpen(app->id()));
  {
    content::CreateAndLoadWebContentsObserver app_loaded_observer;
    LaunchPlatformApp(app);
    app_loaded_observer.Wait();
  }
  EXPECT_TRUE(delegate->IsAppOpen(app->id()));
}

// Test UninstallApp for platform apps.
IN_PROC_BROWSER_TEST_F(AppListClientImplBrowserTest, UninstallApp) {
  AppListClientImpl* client = AppListClientImpl::GetInstance();
  const extensions::Extension* app = InstallPlatformApp("minimal");

  // Bring up the app list.
  EXPECT_FALSE(client->GetAppListWindow());
  client->ShowAppList(ash::AppListShowSource::kSearchKey);
  ash::AppListTestApi().WaitForBubbleWindow(
      /*wait_for_opening_animation=*/false);
  EXPECT_TRUE(client->GetAppListWindow());

  EXPECT_TRUE(wm::GetTransientChildren(client->GetAppListWindow()).empty());

  // Open the uninstall dialog.
  base::RunLoop run_loop;
  client->UninstallApp(profile(), app->id());

  run_loop.RunUntilIdle();
  EXPECT_FALSE(wm::GetTransientChildren(client->GetAppListWindow()).empty());

  // The app list should not be dismissed when the dialog is shown.
  EXPECT_TRUE(client->app_list_visible());
  EXPECT_TRUE(client->GetAppListWindow());
}

IN_PROC_BROWSER_TEST_F(AppListClientImplBrowserTest, ShowAppInfo) {
  ash::SystemWebAppManager::GetForTest(profile())
      ->InstallSystemAppsForTesting();

  AppListClientImpl* client = AppListClientImpl::GetInstance();
  const extensions::Extension* app = InstallPlatformApp("minimal");

  // Bring up the app list.
  EXPECT_FALSE(client->GetAppListWindow());
  client->ShowAppList(ash::AppListShowSource::kSearchKey);
  EXPECT_TRUE(client->GetAppListWindow());
  EXPECT_TRUE(wm::GetTransientChildren(client->GetAppListWindow()).empty());

  // Open the app info dialog.
  ui_test_utils::BrowserChangeObserver browser_opened(
      nullptr, ui_test_utils::BrowserChangeObserver::ChangeType::kAdded);
  client->DoShowAppInfoFlow(profile(), app->id());
  browser_opened.Wait();

  Browser* settings_app =
      chrome::SettingsWindowManager::GetInstance()->FindBrowserForProfile(
          profile());
  EXPECT_TRUE(content::WaitForLoadStop(
      settings_app->tab_strip_model()->GetActiveWebContents()));

  EXPECT_EQ(
      chrome::GetOSSettingsUrl(
          base::StrCat({chromeos::settings::mojom::kAppDetailsSubpagePath,
                        "?id=", app->id()})),
      settings_app->tab_strip_model()->GetActiveWebContents()->GetVisibleURL());
  // The app list should be dismissed when the dialog is shown.
  EXPECT_FALSE(client->app_list_visible());
  EXPECT_FALSE(client->GetAppListWindow());
}

// Test the CreateNewWindow function of the controller delegate.
IN_PROC_BROWSER_TEST_F(AppListClientImplBrowserTest, CreateNewWindow) {
  AppListClientImpl* client = AppListClientImpl::GetInstance();
  AppListControllerDelegate* controller = client;
  ASSERT_TRUE(controller);

  EXPECT_EQ(1U, chrome::GetBrowserCount(browser()->profile()));
  EXPECT_EQ(0U,
            chrome::GetBrowserCount(browser()->profile()->GetPrimaryOTRProfile(
                /*create_if_needed=*/true)));

  controller->CreateNewWindow(/*incognito=*/false,
                              /*should_trigger_session_restore=*/true);
  EXPECT_EQ(2U, chrome::GetBrowserCount(browser()->profile()));

  controller->CreateNewWindow(/*incognito=*/true,
                              /*should_trigger_session_restore=*/true);
  EXPECT_EQ(1U,
            chrome::GetBrowserCount(browser()->profile()->GetPrimaryOTRProfile(
                /*create_if_needed=*/true)));
}

// When getting activated, SelfDestroyAppItem has itself removed from the
// model updater.
class SelfDestroyAppItem : public ChromeAppListItem {
 public:
  SelfDestroyAppItem(Profile* profile,
                     const std::string& app_id,
                     AppListModelUpdater* model_updater)
      : ChromeAppListItem(profile, app_id, model_updater),
        updater_(model_updater) {}
  ~SelfDestroyAppItem() override = default;

  // ChromeAppListItem:
  void Activate(int event_flags) override {
    updater_->RemoveItem(id(), /*is_uninstall=*/true);
  }

 private:
  raw_ptr<AppListModelUpdater, ExperimentalAsh> updater_;
};

// Verifies that activating an app item which destroys itself during activation
// will not cause crash (see https://crbug.com/990282).
IN_PROC_BROWSER_TEST_F(AppListClientImplBrowserTest, ActivateSelfDestroyApp) {
  AppListClientImpl* client = AppListClientImpl::GetInstance();
  client->UpdateProfile();
  ASSERT_TRUE(client);
  AppListModelUpdater* model_updater = test::GetModelUpdater(client);

  // Add an app item which destroys itself during activation.
  const std::string app_id("fake_id");
  model_updater->AddItem(std::make_unique<SelfDestroyAppItem>(
      browser()->profile(), app_id, model_updater));
  ChromeAppListItem* item = model_updater->FindItem(app_id);
  ASSERT_TRUE(item);

  // Activates |item|.
  client->ActivateItem(/*profile_id=*/0, item->id(), /*event_flags=*/0,
                       ash::AppListLaunchedFrom::kLaunchedFromGrid);
}

// Verifies that the first app activation by a new user is recorded.
IN_PROC_BROWSER_TEST_F(AppListClientImplBrowserTest,
                       AppActivationShouldBeRecorded) {
  AppListClientImpl* client = AppListClientImpl::GetInstance();
  client->UpdateProfile();

  // Emulate that the current user is new.
  client->InitializeAsIfNewUserLoginForTest();

  TestObserver syncable_service_observer(
      app_list::AppListSyncableServiceFactory::GetInstance()->GetForProfile(
          profile()));

  // Add an app item.
  AppListModelUpdater* model_updater = test::GetModelUpdater(client);
  const std::string app_id("fake_id");
  auto new_item = std::make_unique<ChromeAppListItem>(browser()->profile(),
                                                      app_id, model_updater);
  new_item->SetChromeName("Fake app");
  model_updater->AddItem(std::move(new_item));

  // Verify that the app addition from the app list client side should not
  // trigger the update recursively, i.e. the client side observers the update
  // in the app list model then reacts to it.
  EXPECT_EQ(0u, syncable_service_observer.add_or_update_count());

  base::HistogramTester histogram_tester;

  // Verify that app activation is recorded.
  client->ShowAppList(ash::AppListShowSource::kSearchKey);
  ChromeAppListItem* item = model_updater->FindItem(app_id);
  ASSERT_TRUE(item);
  client->ActivateItem(/*profile_id=*/0, item->id(), /*event_flags=*/0,
                       ash::AppListLaunchedFrom::kLaunchedFromGrid);
  histogram_tester.ExpectBucketCount(
      "Apps.NewUserFirstLauncherAction.ClamshellMode",
      static_cast<int>(ash::AppListLaunchedFrom::kLaunchedFromGrid),
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectTotalCount(
      "Apps.TimeBetweenNewUserSessionActivationAndFirstLauncherAction."
      "ClamshellMode",
      /*expected_bucket_count=*/1);

  // Verify that only the first app activation is recorded.
  client->ActivateItem(/*profile_id=*/0, item->id(), /*event_flags=*/0,
                       ash::AppListLaunchedFrom::kLaunchedFromGrid);
  histogram_tester.ExpectBucketCount(
      "Apps.NewUserFirstLauncherAction.ClamshellMode",
      static_cast<int>(ash::AppListLaunchedFrom::kLaunchedFromGrid),
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectTotalCount(
      "Apps.TimeBetweenNewUserSessionActivationAndFirstLauncherAction."
      "ClamshellMode",
      /*expected_bucket_count=*/1);
}

// Test that all the items in the context menu for a hosted app have valid
// labels.
IN_PROC_BROWSER_TEST_F(AppListClientImplBrowserTest, ShowContextMenu) {
  AppListClientImpl* client = AppListClientImpl::GetInstance();
  EXPECT_TRUE(client);

  // Show the app list to ensure it has loaded a profile.
  client->ShowAppList(ash::AppListShowSource::kSearchKey);
  AppListModelUpdater* model_updater = test::GetModelUpdater(client);
  EXPECT_TRUE(model_updater);

  // Get the webstore hosted app, which is always present.
  ChromeAppListItem* item = model_updater->FindItem(extensions::kWebStoreAppId);
  EXPECT_TRUE(item);

  base::RunLoop run_loop;
  std::unique_ptr<ui::SimpleMenuModel> menu_model;
  item->GetContextMenuModel(
      ash::AppListItemContext::kNone,
      base::BindLambdaForTesting(
          [&](std::unique_ptr<ui::SimpleMenuModel> created_menu) {
            menu_model = std::move(created_menu);
            run_loop.Quit();
          }));
  run_loop.Run();
  EXPECT_TRUE(menu_model);

  size_t num_items = menu_model->GetItemCount();
  EXPECT_GT(num_items, 0u);

  for (size_t i = 0; i < num_items; i++) {
    if (menu_model->GetTypeAt(i) == ui::MenuModel::TYPE_SEPARATOR)
      continue;

    std::u16string label = menu_model->GetLabelAt(i);
    EXPECT_FALSE(label.empty());
  }
}

// Test that OpenSearchResult that dismisses app list runs fine without
// use-after-free.
IN_PROC_BROWSER_TEST_F(AppListClientImplBrowserTest, OpenSearchResult) {
  AppListClientImpl* client = AppListClientImpl::GetInstance();
  ASSERT_TRUE(client);

  // Emulate that the current user is new.
  client->InitializeAsIfNewUserLoginForTest();

  // Associate |client| with the current profile.
  client->UpdateProfile();

  // Show the launcher.
  client->ShowAppList(ash::AppListShowSource::kSearchKey);
  ash::AppListTestApi().WaitForBubbleWindow(
      /*wait_for_opening_animation=*/false);

  AppListModelUpdater* model_updater = test::GetModelUpdater(client);
  ASSERT_TRUE(model_updater);
  app_list::SearchController* search_controller = client->search_controller();
  ASSERT_TRUE(search_controller);

  // Any app that opens a window to dismiss app list is good enough for this
  // test.
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  const std::string app_title = "chrome";
#else
  const std::string app_title = "chromium";
#endif  // !BUILDFLAG(GOOGLE_CHROME_BRANDING)

  const std::string app_result_id =
      "chrome-extension://mgndgikekgjfcpckkfioiadnlibdjbkf/";

  // Search by title and the app must present in the results.
  ash::AppListTestApi().SimulateSearch(base::UTF8ToUTF16(app_title));
  ASSERT_TRUE(search_controller->FindSearchResult(app_result_id));

  // Expect that the browser window is not minimized.
  ASSERT_FALSE(browser()->window()->IsMinimized());

  // Open the app result.
  base::HistogramTester histogram_tester;
  client->OpenSearchResult(model_updater->model_id(), app_result_id,
                           ui::EF_NONE,
                           ash::AppListLaunchedFrom::kLaunchedFromSearchBox,
                           ash::AppListLaunchType::kAppSearchResult, 0,
                           false /* launch_as_default */);

  // Expect that opening the result from the search box is recorded.
  histogram_tester.ExpectBucketCount(
      "Apps.OpenedAppListSearchResultFromSearchBoxV2."
      "ExistNonAppBrowserWindowOpenAndNotMinimized",
      static_cast<int>(ash::EXTENSION_APP),
      /*expected_bucket_count=*/1);

  // Verify that opening the app result is recorded.
  histogram_tester.ExpectBucketCount(
      "Apps.NewUserFirstLauncherAction.ClamshellMode",
      static_cast<int>(ash::AppListLaunchedFrom::kLaunchedFromSearchBox),
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectTotalCount(
      "Apps.TimeBetweenNewUserSessionActivationAndFirstLauncherAction."
      "ClamshellMode",
      /*expected_bucket_count=*/1);

  // App list should be dismissed.
  EXPECT_FALSE(client->app_list_target_visibility());

  // Minimize the browser. Then show the app list and open the app result.
  browser()->window()->Minimize();
  client->ShowAppList(ash::AppListShowSource::kSearchKey);
  client->OpenSearchResult(model_updater->model_id(), app_result_id,
                           ui::EF_NONE,
                           ash::AppListLaunchedFrom::kLaunchedFromSearchBox,
                           ash::AppListLaunchType::kAppSearchResult, 0,
                           false /* launch_as_default */);

  // Expect that opening the result from the search box is recorded.
  histogram_tester.ExpectBucketCount(
      "Apps.OpenedAppListSearchResultFromSearchBoxV2."
      "NonAppBrowserWindowsEitherClosedOrMinimized",
      static_cast<int>(ash::EXTENSION_APP),
      /*expected_bucket_count=*/1);

  // Needed to let AppLaunchEventLogger finish its work on worker thread.
  // Otherwise, its |weak_factory_| is released on UI thread and causing
  // the bound WeakPtr to fail sequence check on a worker thread.
  // TODO(crbug.com/965065): Remove after fixing AppLaunchEventLogger.
  content::RunAllTasksUntilIdle();
}

IN_PROC_BROWSER_TEST_F(AppListClientImplBrowserTest,
                       OpenSearchResultOnPrimaryDisplay) {
  display::test::DisplayManagerTestApi display_manager(
      ash::ShellTestApi().display_manager());
  display_manager.UpdateDisplay("400x300,500x500");

  const display::Display& primary_display =
      display::Screen::GetScreen()->GetPrimaryDisplay();
  AppListClientImpl* const client = AppListClientImpl::GetInstance();
  ASSERT_TRUE(client);
  // Associate |client| with the current profile.
  client->UpdateProfile();

  EXPECT_EQ(display::kInvalidDisplayId, client->GetAppListDisplayId());

  aura::Window* const primary_root_window =
      ash::Shell::GetRootWindowForDisplayId(primary_display.id());

  client->ShowAppList(ash::AppListShowSource::kSearchKey);
  ash::AppListTestApi().WaitForBubbleWindowInRootWindow(
      primary_root_window,
      /*wait_for_opening_animation=*/true);

  EXPECT_EQ(primary_display.id(), client->GetAppListDisplayId());

  // Simluate search, and verify an activated search result opens on the
  // primary display.
  const std::u16string app_query = u"Chrom";
  const std::string app_id = app_constants::kChromeAppId;
  const std::string app_result_id =
      base::StringPrintf("chrome-extension://%s/", app_id.c_str());

  app_list::SearchResultsChangedWaiter results_changed_waiter(
      AppListClientImpl::GetInstance()->search_controller(),
      {app_list::ResultType::kInstalledApp});
  app_list::ResultsWaiter results_waiter(app_query);

  // Search by title and the app must present in the results.
  ash::AppListTestApi().SimulateSearch(app_query);

  results_changed_waiter.Wait();
  results_waiter.Wait();

  app_list::SearchController* const search_controller =
      client->search_controller();
  ASSERT_TRUE(search_controller);
  ASSERT_TRUE(search_controller->FindSearchResult(app_result_id));

  AppListModelUpdater* model_updater = test::GetModelUpdater(client);
  ASSERT_TRUE(model_updater);

  ActiveWindowWaiter window_waiter(primary_root_window);

  client->OpenSearchResult(model_updater->model_id(), app_result_id,
                           ui::EF_NONE,
                           ash::AppListLaunchedFrom::kLaunchedFromSearchBox,
                           ash::AppListLaunchType::kAppSearchResult, 0,
                           false /* launch_as_default */);

  aura::Window* const app_window = window_waiter.Wait();
  ASSERT_TRUE(app_window);
  EXPECT_EQ(primary_root_window, app_window->GetRootWindow());
  EXPECT_EQ(app_id,
            ash::ShelfID::Deserialize(app_window->GetProperty(ash::kShelfIDKey))
                .app_id);
}

IN_PROC_BROWSER_TEST_F(AppListClientImplBrowserTest,
                       OpenSearchResultOnSecondaryDisplay) {
  display::test::DisplayManagerTestApi display_manager(
      ash::ShellTestApi().display_manager());
  display_manager.UpdateDisplay("400x300,500x500");

  const display::Display& secondary_display =
      display_manager.GetSecondaryDisplay();
  AppListClientImpl* const client = AppListClientImpl::GetInstance();
  ASSERT_TRUE(client);
  // Associate |client| with the current profile.
  client->UpdateProfile();

  EXPECT_EQ(display::kInvalidDisplayId, client->GetAppListDisplayId());

  aura::Window* const secondary_root_window =
      ash::Shell::GetRootWindowForDisplayId(secondary_display.id());

  // Open app list on a secondary display.
  {
    display::ScopedDisplayForNewWindows scoped_display(secondary_display.id());
    client->ShowAppList(ash::AppListShowSource::kSearchKey);
    ash::AppListTestApi().WaitForBubbleWindowInRootWindow(
        secondary_root_window,
        /*wait_for_opening_animation=*/true);
  }

  EXPECT_EQ(secondary_display.id(), client->GetAppListDisplayId());

  // Simluate search, and verify an activated search result opens on the
  // secondary display.
  const std::u16string app_query = u"Chrom";
  const std::string app_id = app_constants::kChromeAppId;
  const std::string app_result_id =
      base::StringPrintf("chrome-extension://%s/", app_id.c_str());

  app_list::SearchResultsChangedWaiter results_changed_waiter(
      AppListClientImpl::GetInstance()->search_controller(),
      {app_list::ResultType::kInstalledApp});
  app_list::ResultsWaiter results_waiter(app_query);

  // Search by title and the app must present in the results.
  ash::AppListTestApi().SimulateSearch(app_query);

  results_changed_waiter.Wait();
  results_waiter.Wait();

  app_list::SearchController* const search_controller =
      client->search_controller();
  ASSERT_TRUE(search_controller);
  ASSERT_TRUE(search_controller->FindSearchResult(app_result_id));

  AppListModelUpdater* model_updater = test::GetModelUpdater(client);
  ASSERT_TRUE(model_updater);

  ActiveWindowWaiter window_waiter(secondary_root_window);

  client->OpenSearchResult(model_updater->model_id(), app_result_id,
                           ui::EF_NONE,
                           ash::AppListLaunchedFrom::kLaunchedFromSearchBox,
                           ash::AppListLaunchType::kAppSearchResult, 0,
                           false /* launch_as_default */);

  aura::Window* const app_window = window_waiter.Wait();
  ASSERT_TRUE(app_window);
  EXPECT_EQ(secondary_root_window, app_window->GetRootWindow());
  EXPECT_EQ(app_id,
            ash::ShelfID::Deserialize(app_window->GetProperty(ash::kShelfIDKey))
                .app_id);

  // Open app list on the primary display, and verify `GetAppListDisplayId()`
  // returns the display where the launcher is shown.

  {
    display::ScopedDisplayForNewWindows scoped_display(
        display::Screen::GetScreen()->GetPrimaryDisplay().id());
    client->ShowAppList(ash::AppListShowSource::kSearchKey);
    ash::AppListTestApi().WaitForBubbleWindow(
        /*wait_for_opening_animation=*/true);
  }
  EXPECT_EQ(display::Screen::GetScreen()->GetPrimaryDisplay().id(),
            client->GetAppListDisplayId());
}

class AppListClientImplLacrosOnlyBrowserTest
    : public AppListClientImplBrowserTest {
 public:
  AppListClientImplLacrosOnlyBrowserTest() {
    feature_list_.InitWithFeatures(
        {ash::features::kLacrosSupport, ash::features::kLacrosPrimary,
         ash::features::kLacrosOnly},
        {});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(AppListClientImplLacrosOnlyBrowserTest, ChromeApp) {
  AppListControllerDelegate* delegate = AppListClientImpl::GetInstance();
  ASSERT_TRUE(delegate);
  ASSERT_TRUE(profile());
  EXPECT_EQ(
      extensions::LAUNCH_TYPE_INVALID,
      delegate->GetExtensionLaunchType(profile(), app_constants::kChromeAppId));
}

IN_PROC_BROWSER_TEST_F(AppListClientImplBrowserTest, ChromeApp) {
  AppListControllerDelegate* delegate = AppListClientImpl::GetInstance();
  ASSERT_TRUE(delegate);
  ASSERT_TRUE(profile());
  EXPECT_EQ(
      extensions::LAUNCH_TYPE_REGULAR,
      delegate->GetExtensionLaunchType(profile(), app_constants::kChromeAppId));
}

// Test that browser launch time is recorded is recorded in preferences.
// This is important for suggested apps sorting.
IN_PROC_BROWSER_TEST_F(AppListClientImplBrowserTest,
                       BrowserLaunchTimeRecorded) {
  AppListClientImpl* client = AppListClientImpl::GetInstance();
  AppListControllerDelegate* controller = client;
  ASSERT_TRUE(controller);

  Profile* profile = browser()->profile();
  Profile* profile_otr =
      profile->GetPrimaryOTRProfile(/*create_if_needed=*/true);

  extensions::ExtensionPrefs* prefs = extensions::ExtensionPrefs::Get(profile);

  // Starting with just one regular browser.
  EXPECT_EQ(1U, chrome::GetBrowserCount(profile));
  EXPECT_EQ(0U, chrome::GetBrowserCount(profile_otr));

  // First browser launch time should be recorded.
  const base::Time time_recorded1 =
      prefs->GetLastLaunchTime(app_constants::kChromeAppId);
  EXPECT_NE(base::Time(), time_recorded1);

  // Create an incognito browser so that we can close the regular one without
  // exiting the test.
  controller->CreateNewWindow(/*incognito=*/true,
                              /*should_trigger_session_restore=*/true);
  EXPECT_EQ(1U, chrome::GetBrowserCount(profile_otr));
  // Creating incognito browser should not update the launch time.
  EXPECT_EQ(time_recorded1,
            prefs->GetLastLaunchTime(app_constants::kChromeAppId));

  // Close the regular browser.
  CloseBrowserSynchronously(chrome::FindBrowserWithProfile(profile));
  EXPECT_EQ(0U, chrome::GetBrowserCount(profile));
  // Recorded the launch time should not update.
  EXPECT_EQ(time_recorded1,
            prefs->GetLastLaunchTime(app_constants::kChromeAppId));

  // Launch another regular browser.
  const base::Time time_before_launch = base::Time::Now();
  controller->CreateNewWindow(/*incognito=*/false,
                              /*should_trigger_session_restore=*/true);
  const base::Time time_after_launch = base::Time::Now();
  EXPECT_EQ(1U, chrome::GetBrowserCount(profile));

  const base::Time time_recorded2 =
      prefs->GetLastLaunchTime(app_constants::kChromeAppId);
  EXPECT_LE(time_before_launch, time_recorded2);
  EXPECT_GE(time_after_launch, time_recorded2);

  // Creating a second regular browser should not update the launch time.
  controller->CreateNewWindow(/*incognito=*/false,
                              /*should_trigger_session_restore=*/true);
  EXPECT_EQ(2U, chrome::GetBrowserCount(profile));
  EXPECT_EQ(time_recorded2,
            prefs->GetLastLaunchTime(app_constants::kChromeAppId));
}

// Browser Test for AppListClient that observes search result changes.
using AppListClientSearchResultsBrowserTest = extensions::ExtensionBrowserTest;

// Test showing search results, and uninstalling one of them while displayed.
IN_PROC_BROWSER_TEST_F(AppListClientSearchResultsBrowserTest,
                       UninstallSearchResult) {
  base::FilePath test_extension_path;
  ASSERT_TRUE(
      base::PathService::Get(chrome::DIR_TEST_DATA, &test_extension_path));
  test_extension_path = test_extension_path.AppendASCII("extensions")
                            .AppendASCII("platform_apps")
                            .AppendASCII("minimal");

  AppListClientImpl* client = AppListClientImpl::GetInstance();
  ASSERT_TRUE(client);
  // Associate |client| with the current profile.
  client->UpdateProfile();

  AppListModelUpdater* model_updater = test::GetModelUpdater(client);
  ASSERT_TRUE(model_updater);
  app_list::SearchController* search_controller = client->search_controller();
  ASSERT_TRUE(search_controller);

  // Install the extension.
  const extensions::Extension* extension = InstallExtension(
      test_extension_path, 1 /* expected_change: new install */);
  ASSERT_TRUE(extension);

  const std::string title = extension->name();

  // Show the app list first, otherwise we won't have a search box to update.
  client->ShowAppList(ash::AppListShowSource::kSearchKey);
  ash::AppListTestApi().WaitForBubbleWindow(
      /*wait_for_opening_animation=*/false);

  // Currently the search box is empty, so we have no result.
  EXPECT_FALSE(search_controller->GetResultByTitleForTest(title));

  // Now a search finds the extension.
  ash::AppListTestApi().SimulateSearch(base::UTF8ToUTF16(title));

  EXPECT_TRUE(search_controller->GetResultByTitleForTest(title));

  // Uninstall the extension.
  UninstallExtension(extension->id());

  // Allow async callbacks to run.
  base::RunLoop().RunUntilIdle();

  // We cannot find the extension any more.
  EXPECT_FALSE(search_controller->GetResultByTitleForTest(title));

  client->DismissView();
}

class AppListClientGuestModeBrowserTest : public InProcessBrowserTest {
 public:
  AppListClientGuestModeBrowserTest() = default;
  AppListClientGuestModeBrowserTest(const AppListClientGuestModeBrowserTest&) =
      delete;
  AppListClientGuestModeBrowserTest& operator=(
      const AppListClientGuestModeBrowserTest&) = delete;

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override;
};

void AppListClientGuestModeBrowserTest::SetUpCommandLine(
    base::CommandLine* command_line) {
  command_line->AppendSwitch(ash::switches::kGuestSession);
  command_line->AppendSwitchASCII(ash::switches::kLoginUser,
                                  user_manager::kGuestUserName);
  command_line->AppendSwitchASCII(ash::switches::kLoginProfile,
                                  TestingProfile::kTestUserProfileDir);
  command_line->AppendSwitch(switches::kIncognito);
}

// Test creating the initial app list in guest mode.
IN_PROC_BROWSER_TEST_F(AppListClientGuestModeBrowserTest, Incognito) {
  AppListClientImpl* client = AppListClientImpl::GetInstance();
  EXPECT_TRUE(client->GetCurrentAppListProfile());

  client->ShowAppList(ash::AppListShowSource::kSearchKey);
  EXPECT_EQ(browser()->profile(), client->GetCurrentAppListProfile());
}

class AppListAppLaunchTest : public extensions::ExtensionBrowserTest {
 protected:
  AppListAppLaunchTest() : extensions::ExtensionBrowserTest() {
    histogram_tester_ = std::make_unique<base::HistogramTester>();
  }
  AppListAppLaunchTest(const AppListAppLaunchTest&) = delete;
  AppListAppLaunchTest& operator=(const AppListAppLaunchTest&) = delete;
  ~AppListAppLaunchTest() override = default;

  // InProcessBrowserTest:
  void SetUpOnMainThread() override {
    extensions::ExtensionBrowserTest::SetUpOnMainThread();

    AppListClientImpl* app_list = AppListClientImpl::GetInstance();
    EXPECT_TRUE(app_list);

    // Need to set the profile to get the model updater.
    app_list->UpdateProfile();
    model_updater_ = app_list->GetModelUpdaterForTest();
    EXPECT_TRUE(model_updater_);
  }

  void LaunchChromeAppListItem(const std::string& id) {
    model_updater_->FindItem(id)->PerformActivate(ui::EF_NONE);
  }

  // Captures histograms.
  std::unique_ptr<base::HistogramTester> histogram_tester_;

 private:
  raw_ptr<AppListModelUpdater, ExperimentalAsh> model_updater_;
};

IN_PROC_BROWSER_TEST_F(AppListAppLaunchTest,
                       NoDemoModeAppLaunchSourceReported) {
  EXPECT_FALSE(ash::DemoSession::IsDeviceInDemoMode());
  LaunchChromeAppListItem(app_constants::kChromeAppId);

  // Should see 0 apps launched from the Launcher in the histogram when not in
  // Demo mode.
  histogram_tester_->ExpectTotalCount("DemoMode.AppLaunchSource", 0);
}

IN_PROC_BROWSER_TEST_F(AppListAppLaunchTest, DemoModeAppLaunchSourceReported) {
  ash::test::LockDemoDeviceInstallAttributes();
  EXPECT_TRUE(ash::DemoSession::IsDeviceInDemoMode());

  // Should see 0 apps launched from the Launcher in the histogram at first.
  histogram_tester_->ExpectTotalCount("DemoMode.AppLaunchSource", 0);

  // Launch chrome browser from the Launcher.  The same mechanism
  // (ChromeAppListItem) is used for all types of apps
  // (ARC, extension, etc), so launching just the browser suffices
  // to test all these cases.
  LaunchChromeAppListItem(app_constants::kChromeAppId);

  // Should see 1 app launched from the Launcher in the histogram.
  histogram_tester_->ExpectUniqueSample(
      "DemoMode.AppLaunchSource", ash::DemoSession::AppLaunchSource::kAppList,
      1);
}

// Verifies that the duration between login and the first launcher showing by
// a new account is recorded correctly.
class DurationBetweenSeesionActivationAndFirstLauncherShowingBrowserTest
    : public ash::LoginManagerTest {
 public:
  DurationBetweenSeesionActivationAndFirstLauncherShowingBrowserTest() {
    login_mixin_.AppendRegularUsers(2);
    new_user_id_ = login_mixin_.users()[0].account_id;
    registered_user_id_ = login_mixin_.users()[1].account_id;
  }
  ~DurationBetweenSeesionActivationAndFirstLauncherShowingBrowserTest()
      override = default;

 protected:
  void ShowAppListAndVerify() {
    auto* client = AppListClientImpl::GetInstance();
    client->ShowAppList(ash::AppListShowSource::kSearchKey);
    ash::AppListTestApi().WaitForBubbleWindow(
        /*wait_for_opening_animation=*/false);
    ASSERT_TRUE(client->app_list_visible());
  }

  // ash::LoginManagerTest:
  void SetUpOnMainThread() override {
    ash::LoginManagerTest::SetUpOnMainThread();
    // Emulate to sign in to a new account. It is time-consuming for an end to
    // end test, i.e. the test covering the whole process from OOBE flow to
    // showing the launcher. Therefore we set the current user to be new
    // explicitly.
    LoginUser(new_user_id_);
    ash::ChromeUserManager::Get()->SetIsCurrentUserNew(true);
    AppListClientImpl::GetInstance()->InitializeAsIfNewUserLoginForTest();
  }

  AccountId new_user_id_;
  AccountId registered_user_id_;
  ash::LoginManagerMixin login_mixin_{&mixin_host_};
};

IN_PROC_BROWSER_TEST_F(
    DurationBetweenSeesionActivationAndFirstLauncherShowingBrowserTest,
    MetricRecordedOnNewAccount) {
  base::HistogramTester tester;
  ShowAppListAndVerify();
  tester.ExpectTotalCount(
      "Apps.TimeDurationBetweenNewUserSessionActivationAndFirstLauncherOpening."
      "ClamshellMode",
      1);
  tester.ExpectBucketCount(
      "Apps.AppListUsageByNewUsers.ClamshellMode",
      static_cast<int>(AppListClientImpl::AppListUsageStateByNewUsers::kUsed),
      1);
}

// The duration between OOBE and the first launcher showing should not be
// recorded if the current user is pre-registered.
IN_PROC_BROWSER_TEST_F(
    DurationBetweenSeesionActivationAndFirstLauncherShowingBrowserTest,
    MetricNotRecordedOnRegisteredAccount) {
  ash::UserAddingScreen::Get()->Start();

  // Verify that the launcher usage state is recorded when switching accounts.
  base::HistogramTester tester;
  AddUser(registered_user_id_);
  tester.ExpectBucketCount(
      "Apps.AppListUsageByNewUsers.ClamshellMode",
      static_cast<int>(AppListClientImpl::AppListUsageStateByNewUsers::
                           kNotUsedBeforeSwitchingAccounts),
      1);

  // Verify that the metric is not recorded.
  ShowAppListAndVerify();
  tester.ExpectTotalCount(
      "Apps.TimeDurationBetweenNewUserSessionActivationAndFirstLauncherOpening."
      "ClamshellMode",
      0);
  tester.ExpectBucketCount(
      "Apps.AppListUsageByNewUsers.ClamshellMode",
      static_cast<int>(AppListClientImpl::AppListUsageStateByNewUsers::kUsed),
      0);
}

// The duration between OOBE and the first launcher showing should not be
// recorded if a user signs in to a new account, switches to another account
// then switches back to the new account.
IN_PROC_BROWSER_TEST_F(
    DurationBetweenSeesionActivationAndFirstLauncherShowingBrowserTest,
    MetricNotRecordedAfterUserSwitch) {
  // Switch to a registered user account then switch back.
  ash::UserAddingScreen::Get()->Start();
  AddUser(registered_user_id_);
  user_manager::UserManager::Get()->SwitchActiveUser(new_user_id_);

  // Verify that the metric is not recorded.
  base::HistogramTester tester;
  ShowAppListAndVerify();
  tester.ExpectTotalCount(
      "Apps.TimeDurationBetweenNewUserSessionActivationAndFirstLauncherOpening."
      "ClamshellMode",
      0);
  tester.ExpectBucketCount(
      "Apps.AppListUsageByNewUsers.ClamshellMode",
      static_cast<int>(AppListClientImpl::AppListUsageStateByNewUsers::kUsed),
      0);
}

class DurationBetweenSeesionActivationAndFirstLauncherShowingShutdownTest
    : public DurationBetweenSeesionActivationAndFirstLauncherShowingBrowserTest {
 public:
  DurationBetweenSeesionActivationAndFirstLauncherShowingShutdownTest() =
      default;
  ~DurationBetweenSeesionActivationAndFirstLauncherShowingShutdownTest()
      override = default;

 protected:
  // DurationBetweenSeesionActivationAndFirstLauncherShowingBrowserTest:
  void SetUpOnMainThread() override {
    DurationBetweenSeesionActivationAndFirstLauncherShowingBrowserTest::
        SetUpOnMainThread();
    histogram_tester_ = std::make_unique<base::HistogramTester>();
  }

  void TearDown() override {
    histogram_tester_->ExpectBucketCount(
        "Apps.AppListUsageByNewUsers.ClamshellMode",
        static_cast<int>(AppListClientImpl::AppListUsageStateByNewUsers::
                             kNotUsedBeforeDestruction),
        1);
    DurationBetweenSeesionActivationAndFirstLauncherShowingBrowserTest::
        TearDown();
  }

  std::unique_ptr<base::HistogramTester> histogram_tester_;
};

// Verify that the launcher usage state is recorded when shutting down.
IN_PROC_BROWSER_TEST_F(
    DurationBetweenSeesionActivationAndFirstLauncherShowingShutdownTest,
    NotUseLauncherBeforeShuttingDown) {
  // Do nothing. Verify the histogram after the browser process is terminated.
}
