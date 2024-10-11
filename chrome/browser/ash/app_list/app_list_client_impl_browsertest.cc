// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/app_list_client_impl.h"

#include <stddef.h>

#include <memory>
#include <optional>
#include <vector>

#include "ash/app_list/apps_collections_controller.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/constants/web_app_id_constants.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/app_list/app_list_metrics.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/public/cpp/test/app_list_test_api.h"
#include "ash/public/cpp/test/shell_test_api.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/shell.h"
#include "ash/test/active_window_waiter.h"
#include "ash/webui/settings/public/constants/routes.mojom.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/one_shot_event.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/apps/app_service/promise_apps/promise_app.h"
#include "chrome/browser/apps/app_service/promise_apps/promise_app_registry_cache.h"
#include "chrome/browser/apps/platform_apps/app_browsertest_util.h"
#include "chrome/browser/ash/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ash/app_list/app_list_model_updater.h"
#include "chrome/browser/ash/app_list/app_list_model_updater_observer.h"
#include "chrome/browser/ash/app_list/app_list_survey_handler.h"
#include "chrome/browser/ash/app_list/app_list_syncable_service_factory.h"
#include "chrome/browser/ash/app_list/app_list_test_util.h"
#include "chrome/browser/ash/app_list/chrome_app_list_item.h"
#include "chrome/browser/ash/app_list/chrome_app_list_model_updater.h"
#include "chrome/browser/ash/app_list/search/search_controller.h"
#include "chrome/browser/ash/app_list/search/test/app_list_search_test_helper.h"
#include "chrome/browser/ash/app_list/search/test/search_results_changed_waiter.h"
#include "chrome/browser/ash/app_list/test/chrome_app_list_test_support.h"
#include "chrome/browser/ash/hats/hats_config.h"
#include "chrome/browser/ash/hats/hats_notification_controller.h"
#include "chrome/browser/ash/login/demo_mode/demo_mode_test_utils.h"
#include "chrome/browser/ash/login/demo_mode/demo_session.h"
#include "chrome/browser/ash/login/login_manager_test.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/ui/ash/login/user_adding_screen.h"
#include "chrome/browser/ui/ash/shelf/shelf_controller_helper.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "chromeos/ash/components/standalone_browser/feature_refs.h"
#include "components/account_id/account_id.h"
#include "components/app_constants/constants.h"
#include "components/browser_sync/browser_sync_switches.h"
#include "components/prefs/pref_service.h"
#include "components/services/app_service/public/cpp/package_id.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_names.h"
#include "components/user_manager/user_type.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
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

// Browser Test for AppListClientImpl.
using AppListClientImplBrowserTest = extensions::PlatformAppBrowserTest;
using ::testing::Invoke;
using ::testing::NiceMock;

namespace {

const apps::PackageId kTestPackageId =
    apps::PackageId(apps::PackageType::kArc, "com.test.package");

class TestObserver : public app_list::AppListSyncableService::Observer {
 public:
  explicit TestObserver(app_list::AppListSyncableService* syncable_service) {
    observer_.Observe(syncable_service);
  }
  TestObserver(const TestObserver&) = delete;
  TestObserver& operator=(const TestObserver&) = delete;
  ~TestObserver() override = default;

  size_t add_or_update_count() const { return add_or_update_count_; }

  // app_list::AppListSyncableService::Observer:
  void OnSyncModelUpdated() override {}
  void OnAddOrUpdateFromSyncItemForTest() override { ++add_or_update_count_; }

 private:
  base::ScopedObservation<app_list::AppListSyncableService,
                          app_list::AppListSyncableService::Observer>
      observer_{this};
  size_t add_or_update_count_ = 0;
};

// A fake for AppListSyncableService that allows easy modifications.
class AppListSyncableServiceFake : public app_list::AppListSyncableService {
 public:
  AppListSyncableServiceFake(Profile* profile,
                             bool was_first_sync_ever,
                             base::OneShotEvent* on_first_sync)
      : app_list::AppListSyncableService(profile),
        on_first_sync_(on_first_sync),
        was_first_sync_ever_(was_first_sync_ever) {}
  ~AppListSyncableServiceFake() override = default;
  AppListSyncableServiceFake(const AppListSyncableServiceFake&) = delete;
  AppListSyncableServiceFake& operator=(const AppListSyncableServiceFake&) =
      delete;

  void OnFirstSync(
      base::OnceCallback<void(bool was_first_sync_ever)> callback) override {
    on_first_sync_->Post(
        FROM_HERE, base::BindOnce(std::move(callback), was_first_sync_ever_));
  }

  // The event to signal when the first app list sync in the session has been
  // completed.
  const raw_ptr<base::OneShotEvent> on_first_sync_;

  bool was_first_sync_ever_;
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
  auto* app_service_proxy =
      apps::AppServiceProxyFactory::GetForProfile(browser()->profile());
  ASSERT_TRUE(app_service_proxy);

  // Bring up the app list.
  EXPECT_FALSE(client->GetAppListWindow());
  client->ShowAppList(ash::AppListShowSource::kSearchKey);
  ash::AppListTestApi().WaitForBubbleWindow(
      /*wait_for_opening_animation=*/false);
  EXPECT_TRUE(client->GetAppListWindow());

  EXPECT_TRUE(wm::GetTransientChildren(client->GetAppListWindow()).empty());

  // Open the uninstall dialog.
  base::RunLoop run_loop;
  app_service_proxy->UninstallForTesting(
      app->id(), client->GetAppListWindow(),
      base::BindLambdaForTesting([&](bool) { run_loop.Quit(); }));
  run_loop.Run();

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
  raw_ptr<AppListModelUpdater> updater_;
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
                       ash::AppListLaunchedFrom::kLaunchedFromGrid,
                       /*is_above_the_fold=*/true);
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
                       ash::AppListLaunchedFrom::kLaunchedFromGrid,
                       /*is_above_the_fold=*/true);
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
                       ash::AppListLaunchedFrom::kLaunchedFromGrid,
                       /*is_above_the_fold=*/true);
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

class AppListClientImplBrowserPromiseAppTest
    : public AppListClientImplBrowserTest,
      public AppListModelUpdaterObserver {
 public:
  AppListClientImplBrowserPromiseAppTest() {
    feature_list_.InitWithFeatures({ash::features::kPromiseIcons}, {});
  }

  // extensions::PlatformAppBrowserTest:
  void SetUpOnMainThread() override {
    extensions::PlatformAppBrowserTest::SetUpOnMainThread();
    AppListClientImpl* client = AppListClientImpl::GetInstance();
    ASSERT_TRUE(client);
    client->UpdateProfile();
    test::GetModelUpdater(client)->AddObserver(this);
  }

  void TearDownOnMainThread() override {
    AppListClientImpl* client = AppListClientImpl::GetInstance();
    ASSERT_TRUE(client);
    test::GetModelUpdater(client)->RemoveObserver(this);
    extensions::PlatformAppBrowserTest::TearDownOnMainThread();
  }

  apps::AppServiceProxy* app_service_proxy() {
    return apps::AppServiceProxyFactory::GetForProfile(profile());
  }

  apps::PromiseAppRegistryCache* cache() {
    return app_service_proxy()->PromiseAppRegistryCache();
  }

  // AppListModelUpdaterObserver:
  void OnAppListItemUpdated(ChromeAppListItem* item) override {
    last_updated_metadata_ = item->CloneMetadata();
    updates_++;
  }

  ash::AppListItemMetadata* GetMetadataFromLastUpdate() {
    return last_updated_metadata_.get();
  }

  int GetAndResetUpdateCount() {
    int cached_updates = updates_;
    updates_ = 0;
    return cached_updates;
  }

 private:
  int updates_ = 0;
  std::unique_ptr<ash::AppListItemMetadata> last_updated_metadata_;
  base::test::ScopedFeatureList feature_list_;
};

// Tests that progress updates from promise apps registry are reflected into the
// launcher.
IN_PROC_BROWSER_TEST_F(AppListClientImplBrowserPromiseAppTest,
                       PromiseAppsInLauncher) {
  std::string app_name = "Long App Name";
  AppListClientImpl* client = AppListClientImpl::GetInstance();
  EXPECT_TRUE(client);

  // Register a promise app in the promise app registry cache.
  apps::PromiseAppPtr promise_app =
      std::make_unique<apps::PromiseApp>(kTestPackageId);
  promise_app->status = apps::PromiseStatus::kPending;
  promise_app->name = app_name;
  promise_app->should_show = true;
  cache()->OnPromiseApp(std::move(promise_app));

  // Show the app list to ensure it has loaded a profile.
  client->ShowAppList(ash::AppListShowSource::kSearchKey);
  AppListModelUpdater* model_updater = test::GetModelUpdater(client);
  EXPECT_TRUE(model_updater);

  ChromeAppListItem* item = model_updater->FindItem(kTestPackageId.ToString());
  ASSERT_TRUE(item);
  EXPECT_EQ(item->progress(), 0);
  EXPECT_EQ(item->app_status(), ash::AppStatus::kPending);
  ASSERT_EQ(item->name(),
            base::UTF16ToUTF8(ShelfControllerHelper::GetLabelForPromiseStatus(
                apps::PromiseStatus::kPending)));
  ASSERT_EQ(item->accessible_name(),
            base::UTF16ToUTF8(
                ShelfControllerHelper::GetAccessibleLabelForPromiseStatus(
                    app_name, apps::PromiseStatus::kPending)));
  GetAndResetUpdateCount();

  // Update the promise app in the promise app registry cache.
  apps::PromiseAppPtr update =
      std::make_unique<apps::PromiseApp>(kTestPackageId);
  update->progress = 0.3;
  update->status = apps::PromiseStatus::kInstalling;
  cache()->OnPromiseApp(std::move(update));

  // Verify that OnAppListItemUpdated was called four times:
  // For accessible name, for name, for progress and for app_status.
  EXPECT_EQ(4, GetAndResetUpdateCount());

  // Promise app item should have updated fields.
  EXPECT_EQ(item->progress(), 0.3f);
  EXPECT_EQ(item->app_status(), ash::AppStatus::kInstalling);
  EXPECT_EQ(item->name(),
            base::UTF16ToUTF8(ShelfControllerHelper::GetLabelForPromiseStatus(
                apps::PromiseStatus::kInstalling)));
  ASSERT_EQ(item->accessible_name(),
            base::UTF16ToUTF8(
                ShelfControllerHelper::GetAccessibleLabelForPromiseStatus(
                    app_name, apps::PromiseStatus::kInstalling)));

  // Register (i.e. "install") an app with a matching package ID. This should
  // trigger removal of the promise app.
  std::string app_id = "asdfghjkl";
  apps::AppPtr app = std::make_unique<apps::App>(apps::AppType::kArc, app_id);
  app->publisher_id = kTestPackageId.identifier();
  app->readiness = apps::Readiness::kReady;

  std::vector<apps::AppPtr> apps;
  apps.push_back(std::move(app));
  app_service_proxy()->OnApps(std::move(apps), apps::AppType::kArc,
                              /*should_notify_initialized=*/false);

  // Verify that the promise app was updated correctly into a successful status
  // before it was removed.
  ash::AppListItemMetadata* metadata_before_removal =
      GetMetadataFromLastUpdate();
  EXPECT_EQ(1, GetAndResetUpdateCount());
  EXPECT_EQ(ash::AppStatus::kInstallSuccess,
            metadata_before_removal->app_status);
  EXPECT_FALSE(model_updater->FindItem(kTestPackageId.ToString()));
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
  // TODO(crbug.com/41459944): Remove after fixing AppLaunchEventLogger.
  content::RunAllTasksUntilIdle();
}

// TODO(crbug.com/335362001): Re-enable this test.
#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_OpenSearchResultOnPrimaryDisplay \
  DISABLED_OpenSearchResultOnPrimaryDisplay
#else
#define MAYBE_OpenSearchResultOnPrimaryDisplay OpenSearchResultOnPrimaryDisplay
#endif
IN_PROC_BROWSER_TEST_F(AppListClientImplBrowserTest,
                       MAYBE_OpenSearchResultOnPrimaryDisplay) {
  display::test::DisplayManagerTestApi display_manager(
      ash::ShellTestApi().display_manager());
  display_manager.UpdateDisplay("400x300,500x400");

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

  ash::ActiveWindowWaiter window_waiter(primary_root_window);

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
  display_manager.UpdateDisplay("400x300,500x400");

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

  ash::ActiveWindowWaiter window_waiter(secondary_root_window);

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
    feature_list_.InitWithFeatures(ash::standalone_browser::GetFeatureRefs(),
                                   {});
    scoped_command_line_.GetProcessCommandLine()->AppendSwitch(
        ash::switches::kEnableLacrosForTesting);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  base::test::ScopedCommandLine scoped_command_line_;
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

// Verifies that apps visibility is correctly calculated.
IN_PROC_BROWSER_TEST_F(AppListClientImplBrowserTest, AppsVisibility) {
  AppListClientImpl* client = AppListClientImpl::GetInstance();
  EXPECT_TRUE(client);
  client->UpdateProfile();

  // Show the app list to ensure it has loaded a profile.
  client->ShowAppList(ash::AppListShowSource::kSearchKey);
  AppListModelUpdater* model_updater = test::GetModelUpdater(client);
  EXPECT_TRUE(model_updater);

  // Get the webstore hosted app.
  ChromeAppListItem* item = model_updater->FindItem(extensions::kWebStoreAppId);
  EXPECT_TRUE(item);

  // Fetch the correct histogram name.
  base::HistogramTester histogram_tester;
  const std::string apps_collections_state =
      ash::AppsCollectionsController::Get()
          ->GetUserExperimentalArmAsHistogramSuffix();
  const std::string histogram_prefix =
      "Apps.AppListBubble.AppsPage.AppLaunchesByVisibility";

  histogram_tester.ExpectTotalCount(
      base::StrCat({histogram_prefix, ".AboveTheFold", apps_collections_state}),
      0);

  histogram_tester.ExpectTotalCount(
      base::StrCat({histogram_prefix, ".BelowTheFold", apps_collections_state}),
      0);

  // Activates web store as if it was activated below the fold.
  client->ActivateItem(/*profile_id=*/0, item->id(), /*event_flags=*/0,
                       ash::AppListLaunchedFrom::kLaunchedFromGrid,
                       /*is_above_the_fold=*/false);

  histogram_tester.ExpectTotalCount(
      base::StrCat({histogram_prefix, ".AboveTheFold", apps_collections_state}),
      0);

  histogram_tester.ExpectTotalCount(
      base::StrCat({histogram_prefix, ".BelowTheFold", apps_collections_state}),
      1);

  // Activates web store as if it was activated above the fold.
  client->ActivateItem(/*profile_id=*/0, item->id(), /*event_flags=*/0,
                       ash::AppListLaunchedFrom::kLaunchedFromGrid,
                       /*is_above_the_fold=*/true);

  histogram_tester.ExpectTotalCount(
      base::StrCat({histogram_prefix, ".AboveTheFold", apps_collections_state}),
      1);

  histogram_tester.ExpectTotalCount(
      base::StrCat({histogram_prefix, ".BelowTheFold", apps_collections_state}),
      1);
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
  raw_ptr<AppListModelUpdater, DanglingUntriaged> model_updater_;
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
    user_manager::UserManager::Get()->SetIsCurrentUserNew(true);
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

  // Verify that the metric is not recorded.
  ShowAppListAndVerify();
  tester.ExpectTotalCount(
      "Apps.TimeDurationBetweenNewUserSessionActivationAndFirstLauncherOpening."
      "ClamshellMode",
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
}

// Verifies that the duration between login and the first time apps collections
// is shown by a new account is recorded correctly.
class DurationBetweenSeesionActivationAndAppsCollectionsShowingBrowserTest
    : public DurationBetweenSeesionActivationAndFirstLauncherShowingBrowserTest {
 public:
  DurationBetweenSeesionActivationAndAppsCollectionsShowingBrowserTest()
      : DurationBetweenSeesionActivationAndFirstLauncherShowingBrowserTest() {
    feature_list_.InitWithFeatures({app_list_features::kAppsCollections}, {});
  }
  ~DurationBetweenSeesionActivationAndAppsCollectionsShowingBrowserTest()
      override = default;

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(
    DurationBetweenSeesionActivationAndAppsCollectionsShowingBrowserTest,
    MetricRecordedOnNewAccount) {
  base::HistogramTester tester;
  ShowAppListAndVerify();
  tester.ExpectTotalCount(
      "Apps.TimeDurationBetweenNewUserSessionActivationAndAppsCollectionShown",
      1);
}

// The duration between OOBE and the first launcher with apps collections
// showing should not be recorded if the current user is pre-registered.
IN_PROC_BROWSER_TEST_F(
    DurationBetweenSeesionActivationAndAppsCollectionsShowingBrowserTest,
    MetricNotRecordedOnRegisteredAccount) {
  ash::UserAddingScreen::Get()->Start();

  // Verify that the launcher usage state is recorded when switching accounts.
  base::HistogramTester tester;
  AddUser(registered_user_id_);

  // Verify that the metric is not recorded.
  ShowAppListAndVerify();
  tester.ExpectTotalCount(
      "Apps.TimeDurationBetweenNewUserSessionActivationAndAppsCollectionShown",
      0);
}

// The duration between OOBE and the first launcher with apps collections
// showing should not be recorded if a user signs in to a new account, switches
// to another account then switches back to the new account.
IN_PROC_BROWSER_TEST_F(
    DurationBetweenSeesionActivationAndAppsCollectionsShowingBrowserTest,
    MetricNotRecordedAfterUserSwitch) {
  // Switch to a registered user account then switch back.
  ash::UserAddingScreen::Get()->Start();
  AddUser(registered_user_id_);
  user_manager::UserManager::Get()->SwitchActiveUser(new_user_id_);

  // Verify that the metric is not recorded.
  base::HistogramTester tester;
  ShowAppListAndVerify();
  tester.ExpectTotalCount(
      "Apps.TimeDurationBetweenNewUserSessionActivationAndAppsCollectionShown",
      0);
}

class AppListClientNewUserTest : public InProcessBrowserTest,
                                 public testing::WithParamInterface<bool> {
 public:
  AppListClientNewUserTest() = default;
  ~AppListClientNewUserTest() override = default;

 public:
  // Returns the event to signal when the first app list sync in the session has
  // been completed.
  base::OneShotEvent& on_first_sync() { return on_first_sync_; }

  // Returns whether the first app list sync in the session was the first sync
  // ever across all ChromeOS devices and sessions for the given user, based on
  // test parameterization.
  bool was_first_sync_ever() const { return GetParam(); }

  // Returns the `AccountId` for the primary `profile()`.
  const AccountId& account_id() const { return account_id_; }

 private:
  // InProcessBrowserTest:
  void SetUpOnMainThread() override {
    SetUpEnvironment();
    // Inject the testing profile into the client, since once a user session was
    // created, with one browser, the client stops observing the profile
    // manager.
    AppListClientImpl::GetInstance()->OnProfileAdded(profile_);
    InProcessBrowserTest::SetUpOnMainThread();
  }

  // Sets up profile and user manager. Should be called only once on test setup.
  void SetUpEnvironment() {
    account_id_ = AccountId::FromUserEmailGaiaId("test@test-user", "gaia-id");

    TestingProfile::Builder profile_builder;
    profile_builder.AddTestingFactory(
        app_list::AppListSyncableServiceFactory::GetInstance(),
        base::BindLambdaForTesting([&](content::BrowserContext* browser_context)
                                       -> std::unique_ptr<KeyedService> {
          return std::make_unique<AppListSyncableServiceFake>(
              Profile::FromBrowserContext(browser_context),
              was_first_sync_ever(), &on_first_sync_);
        }));
    profile_builder.SetProfileName("test@test-user");
    profile_builder.SetPath(
        ash::BrowserContextHelper::Get()->GetBrowserContextPathByUserIdHash(
            user_manager::FakeUserManager::GetFakeUsernameHash(account_id_)));

    std::unique_ptr<TestingProfile> testing_profile = profile_builder.Build();
    profile_ = testing_profile.get();
    g_browser_process->profile_manager()->RegisterTestingProfile(
        std::move(testing_profile), true);

    auto user_manager = std::make_unique<ash::FakeChromeUserManager>();
    user_manager->AddUserWithAffiliationAndTypeAndProfile(
        account_id_, true, user_manager::UserType::kRegular, profile_);
    user_manager->LoginUser(account_id_);

    user_manager_enabler_ = std::make_unique<user_manager::ScopedUserManager>(
        std::move(user_manager));
  }

  void TearDownOnMainThread() override {
    profile_ = nullptr;
    base::RunLoop().RunUntilIdle();
    user_manager_enabler_.reset();
    InProcessBrowserTest::TearDownOnMainThread();
  }

  std::unique_ptr<user_manager::ScopedUserManager> user_manager_enabler_;
  // The event to signal when the first app list sync in the session has been
  // completed.
  base::OneShotEvent on_first_sync_;
  raw_ptr<TestingProfile> profile_;
  AccountId account_id_;
};

INSTANTIATE_TEST_SUITE_P(All, AppListClientNewUserTest, testing::Bool());

IN_PROC_BROWSER_TEST_P(AppListClientNewUserTest, IsNewUser) {
  // Until the first app list sync in the session has been completed, it is
  // not known whether a given user can be considered new.
  EXPECT_EQ(AppListClientImpl::GetInstance()->IsNewUser(account_id()),
            std::nullopt);

  // Signal that the first app list sync in the session has been completed.
  on_first_sync().Signal();

  // Once the first app list sync in the session has been completed, a task
  // will be posted to the `AppListClient` which will cache whether the given
  // user can be considered new.
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return AppListClientImpl::GetInstance()->IsNewUser(account_id()) ==
           was_first_sync_ever();
  }));
}

// An enum identifying the possible combinations for the Launcher HATS survey in
// tests.
enum class AppListSurveyConfiguration {
  // No HATS configurations is selected for this test.
  kNone,
  // ash::kHatsLauncherAppsFindingSurvey
  kAppsFinding,
  // ash::kHatsLauncherAppsNeedingSurvey
  kAppsNeeding,
};

class AppListSurveyTriggerTest
    : public AppListClientImplBrowserTest,
      public testing::WithParamInterface<
          std::tuple<ash::AppsCollectionsController::ExperimentalArm,
                     AppListSurveyConfiguration>> {
 public:
  AppListSurveyTriggerTest() {
    std::vector<base::test::FeatureRefAndParams> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;
    ash::AppsCollectionsController::ExperimentalArm arm = GetExperimentalArm();

    switch (arm) {
      case ash::AppsCollectionsController::ExperimentalArm::kDefaultValue:
      case ash::AppsCollectionsController::ExperimentalArm::kControl:
        disabled_features.push_back(app_list_features::kAppsCollections);
        break;
      case ash::AppsCollectionsController::ExperimentalArm::kEnabled:
        enabled_features.push_back(base::test::FeatureRefAndParams(
            app_list_features::kAppsCollections,
            {{"is-counterfactual", "false"}, {"is-modified-order", "false"}}));
        break;
      case ash::AppsCollectionsController::ExperimentalArm::kCounterfactual:
        enabled_features.push_back(base::test::FeatureRefAndParams(
            app_list_features::kAppsCollections,
            {{"is-counterfactual", "true"}, {"is-modified-order", "false"}}));
        break;
      case ash::AppsCollectionsController::ExperimentalArm::kModifiedOrder:
        enabled_features.push_back(base::test::FeatureRefAndParams(
            app_list_features::kAppsCollections,
            {{"is-counterfactual", "false"}, {"is-modified-order", "true"}}));
        break;
    }

    switch (GetHatsConfig()) {
      case AppListSurveyConfiguration::kNone:
        disabled_features.push_back(
            ash::kHatsLauncherAppsNeedingSurvey.feature);
        disabled_features.push_back(
            ash::kHatsLauncherAppsFindingSurvey.feature);
        break;
      case AppListSurveyConfiguration::kAppsFinding:
        enabled_features.push_back(base::test::FeatureRefAndParams(
            ash::kHatsLauncherAppsFindingSurvey.feature, {}));
        disabled_features.push_back(
            ash::kHatsLauncherAppsNeedingSurvey.feature);
        break;
      case AppListSurveyConfiguration::kAppsNeeding:
        enabled_features.push_back(base::test::FeatureRefAndParams(
            ash::kHatsLauncherAppsNeedingSurvey.feature, {}));
        disabled_features.push_back(
            ash::kHatsLauncherAppsFindingSurvey.feature);
        break;
    }

    scoped_feature_list_.InitWithFeaturesAndParameters(enabled_features,
                                                       disabled_features);
  }
  ~AppListSurveyTriggerTest() override = default;

  // AppListClientImplBrowserTest:
  void SetUpOnMainThread() override {
    AppListClientImplBrowserTest::SetUpOnMainThread();

    display_service_ = std::make_unique<NotificationDisplayServiceTester>(
        browser()->profile());
    user_manager::UserManager::Get()->SetIsCurrentUserNew(true);
    AppListClientImpl::GetInstance()->InitializeAsIfNewUserLoginForTest();
  }

  void SetUpDefaultCommandLine(base::CommandLine* command_line) override {
    AppListClientImplBrowserTest::SetUpDefaultCommandLine(command_line);

    switch (GetHatsConfig()) {
      case AppListSurveyConfiguration::kNone:
        break;
      case AppListSurveyConfiguration::kAppsFinding:
        command_line->AppendSwitchASCII(
            ash::switches::kForceHappinessTrackingSystem,
            ash::kHatsLauncherAppsFindingSurvey.feature.name);
        break;
      case AppListSurveyConfiguration::kAppsNeeding:
        command_line->AppendSwitchASCII(
            ash::switches::kForceHappinessTrackingSystem,
            ash::kHatsLauncherAppsNeedingSurvey.feature.name);
        break;
    }
  }

  bool IsHatsNotificationActive() const {
    return display_service_
        ->GetNotification(ash::HatsNotificationController::kNotificationId)
        .has_value();
  }

  void MaybeWaitForHatsNotification() {
    if (!ShouldShowHatsSurvey()) {
      return;
    }

    base::RunLoop loop;
    display_service_->SetNotificationAddedClosure(loop.QuitClosure());
    loop.Run();
  }

  const ash::HatsNotificationController* GetHatsNotificationController() const {
    return AppListClientImpl::GetInstance()
        ->survey_handler_->GetHatsNotificationControllerForTesting();
  }

  // Returns the HATS Survey that is expected to trigger.
  AppListSurveyConfiguration GetHatsConfig() const {
    return std::get<1>(GetParam());
  }

  // Returns the experimental arm that this test was set up for AppsCollections.
  ash::AppsCollectionsController::ExperimentalArm GetExperimentalArm() const {
    return std::get<0>(GetParam());
  }

  // Returns whether the a HATS survey should trigger for this parameter
  // configuration.
  bool ShouldShowHatsSurvey() {
    return GetExperimentalArm() !=
               ash::AppsCollectionsController::ExperimentalArm::kControl &&
           GetHatsConfig() != AppListSurveyConfiguration::kNone;
  }

  base::test::ScopedFeatureList scoped_feature_list_;

  std::unique_ptr<NotificationDisplayServiceTester> display_service_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    AppListSurveyTriggerTest,
    ::testing::Combine(
        testing::Values(
            ash::AppsCollectionsController::ExperimentalArm::kControl,
            ash::AppsCollectionsController::ExperimentalArm::kCounterfactual,
            ash::AppsCollectionsController::ExperimentalArm::kEnabled,
            ash::AppsCollectionsController::ExperimentalArm::kModifiedOrder),
        testing::Values(AppListSurveyConfiguration::kAppsFinding,
                        AppListSurveyConfiguration::kAppsNeeding,
                        AppListSurveyConfiguration::kNone)));

IN_PROC_BROWSER_TEST_P(AppListSurveyTriggerTest, ShowSurveySuccess) {
  EXPECT_FALSE(IsHatsNotificationActive());

  AppListClientImpl* client = AppListClientImpl::GetInstance();

  // Bring up the app list.
  EXPECT_FALSE(client->GetAppListWindow());
  client->ShowAppList(ash::AppListShowSource::kSearchKey);
  ash::AppListTestApi().WaitForBubbleWindow(
      /*wait_for_opening_animation=*/false);
  EXPECT_TRUE(client->GetAppListWindow());

  MaybeWaitForHatsNotification();

  EXPECT_EQ(GetHatsNotificationController() != nullptr, ShouldShowHatsSurvey());
  EXPECT_EQ(IsHatsNotificationActive(), ShouldShowHatsSurvey());
}

IN_PROC_BROWSER_TEST_P(AppListSurveyTriggerTest, ShowSurveyOnlyOnce) {
  if (!ShouldShowHatsSurvey()) {
    return;
  }

  EXPECT_FALSE(IsHatsNotificationActive());

  AppListClientImpl* client = AppListClientImpl::GetInstance();

  // Bring up the app list.
  EXPECT_FALSE(client->GetAppListWindow());
  client->ShowAppList(ash::AppListShowSource::kSearchKey);
  ash::AppListTestApi().WaitForBubbleWindow(
      /*wait_for_opening_animation=*/false);
  EXPECT_TRUE(client->GetAppListWindow());

  MaybeWaitForHatsNotification();

  const ash::HatsNotificationController* hats_notification_controller =
      GetHatsNotificationController();
  EXPECT_NE(hats_notification_controller, nullptr);
  EXPECT_TRUE(IsHatsNotificationActive());

  // Bring up the app list again but the controller shouldn't be a new instance.
  client->DismissView();

  EXPECT_FALSE(client->GetAppListWindow());
  client->ShowAppList(ash::AppListShowSource::kSearchKey);
  ash::AppListTestApi().WaitForBubbleWindow(
      /*wait_for_opening_animation=*/false);
  EXPECT_TRUE(client->GetAppListWindow());

  EXPECT_EQ(hats_notification_controller, GetHatsNotificationController());
}

// A suite for verifying the experimental arm for apps collections experiment
// that modifies the order of apps.
class AppListModifiedDefaultAppOrderTest
    : public AppListClientImplBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  AppListModifiedDefaultAppOrderTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        app_list_features::kAppsCollections,
        {{"is-counterfactual", "false"},
         {"is-modified-order",
          IsModifiedOrderExperimentalArm() ? "true" : "false"}});
  }
  ~AppListModifiedDefaultAppOrderTest() override = default;

  // AppListClientImplBrowserTest:
  void SetUpOnMainThread() override {
    AppListClientImplBrowserTest::SetUpOnMainThread();
    user_manager::UserManager::Get()->SetIsCurrentUserNew(true);
    AppListClientImpl::GetInstance()->InitializeAsIfNewUserLoginForTest();
  }

  bool IsModifiedOrderExperimentalArm() { return GetParam(); }

  void AddSyncedItem(std::string app_id, AppListModelUpdater* model_updater) {
    app_list::AppListSyncableService* syncable_service =
        app_list_syncable_service();
    ASSERT_TRUE(syncable_service);

    syncable_service->set_app_default_positioned_for_new_users_only_for_test(
        app_id);
    auto new_item = std::make_unique<ChromeAppListItem>(browser()->profile(),
                                                        app_id, model_updater);
    new_item->SetChromeName(app_id);
    syncable_service->AddItem(std::move(new_item));
  }

  ChromeAppListModelUpdater* GetChromeAppListModelUpdater() {
    return static_cast<ChromeAppListModelUpdater*>(
        app_list_syncable_service()->GetModelUpdater());
  }

  app_list::AppListSyncableService* app_list_syncable_service() {
    return app_list::AppListSyncableServiceFactory::GetForProfile(profile());
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         AppListModifiedDefaultAppOrderTest,
                         ::testing::Bool());

// Verify that the default order of apps is changed once the recalculation
// happens for the first time in the modified order experimental arm of apps
// collections.
IN_PROC_BROWSER_TEST_P(AppListModifiedDefaultAppOrderTest,
                       DefaultOrdinalsChangeAfterRecalculation) {
  AppListClientImpl* client = AppListClientImpl::GetInstance();
  ASSERT_TRUE(client);
  client->UpdateProfile();
  ChromeAppListModelUpdater* model_updater = GetChromeAppListModelUpdater();
  ASSERT_TRUE(model_updater);
  // Install some default apps by syncing.
  // In the default app order, youtube appears before the camera app. For the
  // apps collections experimental arm, camera appears first.
  AddSyncedItem(web_app::kCameraAppId, model_updater);
  AddSyncedItem(extension_misc::kYoutubeAppId, model_updater);

  ChromeAppListItem* camera_item =
      model_updater->FindItem(web_app::kCameraAppId);
  const syncer::StringOrdinal camera_ordinal = camera_item->position();

  ChromeAppListItem* youtube_item =
      model_updater->FindItem(extension_misc::kYoutubeAppId);
  const syncer::StringOrdinal youtube_ordinal = youtube_item->position();

  // Before calculating the experimental arm, the default apps should be ordered
  // as default, with youtube having a lesser ordinal than camera.
  EXPECT_TRUE(youtube_ordinal.LessThan(camera_ordinal));

  // Trigger a recalculation of the experimental arm and apps position for
  // testing simplicity. This is usually done on first sync.
  client->MaybeRecalculateAppsGridDefaultOrder();
  const syncer::StringOrdinal new_camera_ordinal = camera_item->position();
  const syncer::StringOrdinal new_youtube_ordinal = youtube_item->position();

  // After determining if the user belongs in the
  // experimental arm or not, the default apps may change their ordinals if the
  // user belongs in the experimental modified order. The order of youtube and
  // camera is also changed so that now camera has a lesser ordinal than
  // youtube.
  EXPECT_EQ(camera_ordinal != new_camera_ordinal,
            IsModifiedOrderExperimentalArm());
  EXPECT_EQ(youtube_ordinal != new_youtube_ordinal,
            IsModifiedOrderExperimentalArm());
  EXPECT_EQ(new_camera_ordinal.LessThan(new_youtube_ordinal),
            IsModifiedOrderExperimentalArm());
}

// Verify that the default order of apps is changed once the app list opens for
// the first time in the modified order experimental arm of apps collections.
IN_PROC_BROWSER_TEST_P(AppListModifiedDefaultAppOrderTest,
                       DefaultOrdinalsNotChangeAfterReorder) {
  AppListClientImpl* client = AppListClientImpl::GetInstance();
  ASSERT_TRUE(client);
  client->UpdateProfile();
  ChromeAppListModelUpdater* model_updater = GetChromeAppListModelUpdater();
  ASSERT_TRUE(model_updater);
  // Install some default apps by syncing.
  AddSyncedItem(web_app::kCameraAppId, model_updater);
  AddSyncedItem(extension_misc::kYoutubeAppId, model_updater);
  AddSyncedItem(web_app::kCalculatorAppId, model_updater);

  ChromeAppListItem* camera_item =
      model_updater->FindItem(web_app::kCameraAppId);
  const syncer::StringOrdinal camera_ordinal = camera_item->position();

  ChromeAppListItem* youtube_item =
      model_updater->FindItem(extension_misc::kYoutubeAppId);
  const syncer::StringOrdinal youtube_ordinal = youtube_item->position();

  ChromeAppListItem* calculator_item =
      model_updater->FindItem(web_app::kCalculatorAppId);
  syncer::StringOrdinal calculator_ordinal = calculator_item->position();

  // Before calculating the experimental arm, the default apps should be ordered
  // as default, with youtube having a lesser ordinal than camera, which have a
  // lesser ordinal than calculator.
  EXPECT_TRUE(youtube_ordinal.LessThan(camera_ordinal));
  EXPECT_TRUE(camera_ordinal.LessThan(calculator_ordinal));

  // Move the calculator before the camera
  model_updater->RequestPositionUpdate(
      web_app::kCalculatorAppId, camera_ordinal.CreateBefore(),
      ash::RequestPositionUpdateReason::kMoveItem);
  calculator_ordinal = calculator_item->position();
  EXPECT_TRUE(calculator_ordinal.LessThan(camera_ordinal));

  // Trigger a recalculation of the experimental arm and apps position for
  // testing simplicity. This is usually done on first sync.
  client->MaybeRecalculateAppsGridDefaultOrder();
  const syncer::StringOrdinal new_camera_ordinal = camera_item->position();
  const syncer::StringOrdinal new_youtube_ordinal = youtube_item->position();
  const syncer::StringOrdinal new_calculator_ordinal =
      calculator_item->position();

  // Because there was an app reorder, ordinals should not change.
  EXPECT_EQ(camera_ordinal, new_camera_ordinal);
  EXPECT_EQ(youtube_ordinal, new_youtube_ordinal);
  EXPECT_EQ(calculator_ordinal, new_calculator_ordinal);
}
