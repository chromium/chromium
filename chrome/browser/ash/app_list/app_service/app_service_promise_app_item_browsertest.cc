// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/app_service/app_service_promise_app_item.h"

#include <memory>
#include <optional>
#include <ranges>
#include <string>
#include <vector>

#include "ash/app_list/app_list_model_provider.h"
#include "ash/app_list/model/app_list_item.h"
#include "ash/components/arc/arc_features.h"
#include "ash/components/arc/mojom/app.mojom.h"
#include "ash/components/arc/session/connection_holder.h"
#include "ash/components/arc/test/arc_util_test_support.h"
#include "ash/components/arc/test/connection_holder_util.h"
#include "ash/components/arc/test/fake_app_instance.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/public/cpp/app_menu_constants.h"
#include "ash/public/cpp/shelf_model.h"
#include "ash/public/cpp/shelf_types.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_base.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_future.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_ash.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/promise_apps/promise_app.h"
#include "chrome/browser/apps/app_service/promise_apps/promise_app_metrics.h"
#include "chrome/browser/apps/app_service/promise_apps/promise_app_registry_cache.h"
#include "chrome/browser/apps/app_service/promise_apps/promise_app_service.h"
#include "chrome/browser/apps/platform_apps/app_browsertest_util.h"
#include "chrome/browser/ash/app_list/app_list_client_impl.h"
#include "chrome/browser/ash/app_list/app_list_model_updater.h"
#include "chrome/browser/ash/app_list/app_list_syncable_service.h"
#include "chrome/browser/ash/app_list/app_list_syncable_service_factory.h"
#include "chrome/browser/ash/app_list/app_list_test_util.h"
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ash/app_list/arc/arc_app_test.h"
#include "chrome/browser/ash/app_list/arc/arc_package_install_priority_handler.h"
#include "chrome/browser/ash/app_list/chrome_app_list_item.h"
#include "chrome/browser/ash/app_list/chrome_app_list_model_updater.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller_util.h"
#include "chrome/browser/ui/ash/shelf/shelf_controller_helper.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/package_id.h"
#include "components/sync/model/string_ordinal.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/test/fake_sync_change_processor.h"
#include "components/sync/test/sync_change_processor_wrapper_for_test.h"
#include "content/public/test/browser_test.h"
#include "net/http/http_status_code.h"
#include "ui/base/models/menu_model.h"

namespace apps {

const apps::PackageId kTestPackageId =
    apps::PackageId(apps::PackageType::kArc, "com.test.package");

ash::AppListItem* GetAppListItem(const std::string& id) {
  return ash::AppListModelProvider::Get()->model()->FindItem(id);
}

bool IsItemPinned(const std::string& item_id) {
  const auto& shelf_items = ash::ShelfModel::Get()->items();
  auto pinned_item =
      base::ranges::find_if(shelf_items, [&item_id](const auto& shelf_item) {
        return shelf_item.id.app_id == item_id;
      });
  return pinned_item != std::ranges::end(shelf_items);
}

class AppServicePromiseAppItemBrowserTest
    : public extensions::PlatformAppBrowserTest,
      public PromiseAppRegistryCache::Observer {
 public:
  AppServicePromiseAppItemBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        {ash::features::kPromiseIcons, arc::kSyncInstallPriority}, {});
  }
  ~AppServicePromiseAppItemBrowserTest() override = default;

  // extensions::PlatformAppBrowserTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    arc::SetArcAvailableCommandLineForTesting(command_line);
  }

  void SetUpInProcessBrowserTestFixture() override {
    arc::ArcSessionManager::SetUiEnabledForTesting(false);
    extensions::PlatformAppBrowserTest::SetUpInProcessBrowserTestFixture();
  }

  void SetUpOnMainThread() override {
    extensions::PlatformAppBrowserTest::SetUpOnMainThread();

    arc::SetArcPlayStoreEnabledForProfile(profile(), true);

    arc_app_list_pref_ = ArcAppListPrefs::Get(profile());
    DCHECK(arc_app_list_pref_);

    base::RunLoop run_loop;
    arc_app_list_pref_->SetDefaultAppsReadyCallback(run_loop.QuitClosure());
    run_loop.Run();

    app_instance_ = std::make_unique<arc::FakeAppInstance>(arc_app_list_pref_);
    arc_app_list_pref_->app_connection_holder()->SetInstance(
        app_instance_.get());
    WaitForInstanceReady(arc_app_list_pref_->app_connection_holder());

    AppListClientImpl* client = AppListClientImpl::GetInstance();
    ASSERT_TRUE(client);
    client->UpdateProfile();
    cache_ = app_service_proxy()->PromiseAppRegistryCache();

    https_server_.RegisterRequestHandler(
        base::BindRepeating(&AppServicePromiseAppItemBrowserTest::HandleRequest,
                            base::Unretained(this)));
    https_server_.AddDefaultHandlers(GetChromeTestDataDir());
    ASSERT_TRUE(https_server_.Start());
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        ash::switches::kAlmanacApiUrl, https_server_.GetURL("/").spec());
  }

  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) {
    // Mock a response to ensure that the test does not stay hanging for an
    // Almanac response. It will be a failure response so the promise app will
    // fall back to a placeholder icon.
    if (base::Contains(request.relative_url, "v1/promise-app/")) {
      auto response = std::make_unique<net::test_server::BasicHttpResponse>();
      response->set_code(net::HTTP_INTERNAL_SERVER_ERROR);
      response->set_content_type("application/x-protobuf");
      response->set_content("");
      return response;
    }
    return nullptr;
  }

  void TearDownOnMainThread() override {
    app_list_syncable_service()->StopSyncing(syncer::APP_LIST);
    arc_app_list_pref_->app_connection_holder()->CloseInstance(
        app_instance_.get());
    app_instance_.reset();
    arc::ArcSessionManager::Get()->Shutdown();
    extensions::PlatformAppBrowserTest::TearDownOnMainThread();
  }

  ChromeAppListModelUpdater* GetChromeAppListModelUpdater() {
    return static_cast<ChromeAppListModelUpdater*>(
        app_list_syncable_service()->GetModelUpdater());
  }

  ChromeAppListItem* GetChromeAppListItem(const std::string& app_id) {
    AppListModelUpdater* model_updater =
        app_list_syncable_service()->GetModelUpdater();
    return model_updater->FindItem(app_id);
  }

  ChromeAppListItem* GetChromeAppListItem(const PackageId& package_id) {
    return GetChromeAppListItem(package_id.ToString());
  }

  apps::PromiseAppRegistryCache* cache() { return cache_; }

  apps::AppServiceProxy* app_service_proxy() {
    return apps::AppServiceProxyFactory::GetForProfile(profile());
  }

  ArcAppListPrefs* arc_app_list_pref() { return arc_app_list_pref_; }
  arc::FakeAppInstance* app_instance() { return app_instance_.get(); }

  app_list::AppListSyncableService* app_list_syncable_service() {
    return app_list::AppListSyncableServiceFactory::GetForProfile(profile());
  }

  void AddArcPackageWithApps(const std::string& package,
                             const std::vector<std::string>& activities) {
    for (const auto& activity : activities) {
      arc::mojom::AppInfoPtr app =
          arc::mojom::AppInfo::New("Fake App", package, activity, false);
      app_instance_->SendAppAdded(*app);
    }

    app_instance_->SendPackageAdded(arc::mojom::ArcPackageInfo::New(
        package,
        /*package_version=*/1, /*last_backup_android_id=*/1,
        /*last_backup_time=*/0, /*sync=*/false));
  }

  // Set the number of updates we expect the Promise App Registry Cache to
  // receive in the test.
  void ExpectNumUpdates(int num_updates) {
    expected_num_updates_ = num_updates;
    current_num_updates_ = 0;
    if (!obs_.IsObserving()) {
      obs_.Observe(cache_);
    }
  }

  void WaitForPromiseAppUpdates() {
    if (expected_num_updates_ == current_num_updates_) {
      return;
    }
    wait_run_loop_ = std::make_unique<base::RunLoop>();
    wait_run_loop_->Run();
  }

  // apps::PromiseAppRegistryCache::Observer:
  void OnPromiseAppUpdate(const PromiseAppUpdate& update) override {
    current_num_updates_++;
    if (wait_run_loop_ && wait_run_loop_->running() &&
        expected_num_updates_ == current_num_updates_) {
      wait_run_loop_->Quit();
    }
  }

  void OnPromiseAppRegistryCacheWillBeDestroyed(
      apps::PromiseAppRegistryCache* cache) override {
    obs_.Reset();
  }

  void SetUpSyncedArcPromiseApp(const std::string& package_name) {
    auto* install_priority_handler =
        arc_app_list_pref()->GetInstallPriorityHandler();
    DCHECK(install_priority_handler);
    install_priority_handler->InstallSyncedPacakge(
        package_name, arc::mojom::InstallPriority::kLow);
    // Test:
    // 1) Start the installation.
    // Expect 2 updates: Promise app registration, then Almanac response update.
    // Note: As the Almanac response is not mocked, the promise icon will
    // fallback to using a placeholder image.
    ExpectNumUpdates(/*num_updates=*/2);
    app_instance()->SendInstallationStarted(package_name);
    WaitForPromiseAppUpdates();
  }

 private:
  raw_ptr<apps::PromiseAppRegistryCache, DanglingUntriaged> cache_;
  raw_ptr<ArcAppListPrefs, DanglingUntriaged> arc_app_list_pref_ = nullptr;

  std::unique_ptr<arc::FakeAppInstance> app_instance_;

  base::ScopedObservation<PromiseAppRegistryCache,
                          PromiseAppRegistryCache::Observer>
      obs_{this};

  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<base::RunLoop> wait_run_loop_;
  net::EmbeddedTestServer https_server_;

  // Tracks how many times we should expect OnPromiseAppUpdate to be called
  // before proceeding with a test.
  int expected_num_updates_;
  int current_num_updates_;
};

IN_PROC_BROWSER_TEST_F(AppServicePromiseAppItemBrowserTest,
                       ShouldShowUpdateCreatesItem) {
  // Sync setup.
  std::unique_ptr<syncer::FakeSyncChangeProcessor> sync_processor =
      std::make_unique<syncer::FakeSyncChangeProcessor>();
  app_list::AppListSyncableService* app_list_syncable_service_ =
      app_list_syncable_service();
  app_list_syncable_service_->MergeDataAndStartSyncing(
      syncer::APP_LIST, {},
      std::make_unique<syncer::SyncChangeProcessorWrapperForTest>(
          sync_processor.get()));
  content::RunAllTasksUntilIdle();

  // Register a promise app in the promise app registry cache.
  apps::PromiseAppPtr promise_app =
      std::make_unique<PromiseApp>(kTestPackageId);
  cache()->OnPromiseApp(std::move(promise_app));

  // Promise app registration in the cache should not result in a promise app
  // launcher item if should_show is false (which it is by default).
  ash::AppListItem* item = GetAppListItem(kTestPackageId.ToString());
  ASSERT_FALSE(item);

  // Update the promise app to allow showing in the Launcher.
  apps::PromiseAppPtr promise_app_update =
      std::make_unique<PromiseApp>(kTestPackageId);
  promise_app_update->should_show = true;
  cache()->OnPromiseApp(std::move(promise_app_update));

  // Promise app item should now exist in the model.
  item = GetAppListItem(kTestPackageId.ToString());
  ASSERT_TRUE(item);

  // Verify that the promise app item is not added to local storage.
  const base::Value::Dict& local_items =
      profile()->GetPrefs()->GetDict(prefs::kAppListLocalState);
  const base::Value::Dict* dict_item =
      local_items.FindDict(kTestPackageId.ToString());
  EXPECT_FALSE(dict_item);

  // Verify that promise app item is not uploaded to sync data.
  for (auto sync_change : sync_processor->changes()) {
    const std::string item_id =
        sync_change.sync_data().GetSpecifics().app_list().item_id();
    EXPECT_NE(item_id, kTestPackageId.ToString());
  }
}

IN_PROC_BROWSER_TEST_F(AppServicePromiseAppItemBrowserTest,
                       PromiseAppItemContextMenu) {
  // Register a promise app in the promise app registry cache.
  apps::PromiseAppPtr promise_app =
      std::make_unique<PromiseApp>(kTestPackageId);
  promise_app->should_show = true;
  cache()->OnPromiseApp(std::move(promise_app));

  // Promise app item should exist in the model.
  ChromeAppListItem* item = GetChromeAppListItem(kTestPackageId);
  ASSERT_TRUE(item);
  ASSERT_EQ(item->name(),
            base::UTF16ToUTF8(ShelfControllerHelper::GetLabelForPromiseStatus(
                apps::PromiseStatus::kPending)));

  // Retrieve the context menu.
  base::RunLoop run_loop;
  std::unique_ptr<ui::SimpleMenuModel> menu_model;
  item->GetContextMenuModel(
      ash::AppListItemContext::kAppsGrid,
      base::BindLambdaForTesting(
          [&](std::unique_ptr<ui::SimpleMenuModel> created_menu) {
            menu_model = std::move(created_menu);
            run_loop.Quit();
          }));
  run_loop.Run();

  // The context menu should have the option to pin to shelf, a separator and
  // the reorder submenu.
  EXPECT_EQ(menu_model->GetItemCount(), 3u);
  EXPECT_EQ(menu_model->GetTypeAt(0), ui::MenuModel::ItemType::TYPE_COMMAND);
  EXPECT_EQ(menu_model->GetCommandIdAt(0), ash::CommandId::TOGGLE_PIN);

  EXPECT_EQ(menu_model->GetTypeAt(1), ui::MenuModel::ItemType::TYPE_SEPARATOR);

  EXPECT_EQ(menu_model->GetTypeAt(2), ui::MenuModel::ItemType::TYPE_SUBMENU);
  EXPECT_EQ(menu_model->GetCommandIdAt(2), ash::CommandId::REORDER_SUBMENU);

  // Reorder context menu should have options to reorder alphabetically and by
  // color.
  auto* reorder_submenu = menu_model->GetSubmenuModelAt(2);
  ASSERT_EQ(reorder_submenu->GetItemCount(), 2u);
  EXPECT_EQ(reorder_submenu->GetCommandIdAt(0),
            ash::CommandId::REORDER_BY_NAME_ALPHABETICAL);
  EXPECT_EQ(reorder_submenu->GetCommandIdAt(1),
            ash::CommandId::REORDER_BY_COLOR);
}

IN_PROC_BROWSER_TEST_F(AppServicePromiseAppItemBrowserTest,
                       CompleteAppInstallationRemovesPromiseAppItem) {
  AppType app_type = AppType::kArc;
  std::string identifier = "test.com.example";
  PackageId package_id(PackageType::kArc, identifier);

  // Register a promise app in the promise app registry cache.
  apps::PromiseAppPtr promise_app = std::make_unique<PromiseApp>(package_id);
  promise_app->should_show = true;
  cache()->OnPromiseApp(std::move(promise_app));

  // Promise app item should exist in the model.
  ash::AppListItem* item = GetAppListItem(package_id.ToString());
  ASSERT_TRUE(item);

  // Register (i.e. "install") an app with a matching package ID. This should
  // trigger removal of the promise app.
  std::string app_id = "qwertyuiopasdfghjkl";
  apps::AppPtr app = std::make_unique<apps::App>(app_type, app_id);
  app->publisher_id = identifier;
  app->readiness = apps::Readiness::kReady;
  std::vector<apps::AppPtr> apps;
  apps.push_back(std::move(app));
  app_service_proxy()->OnApps(std::move(apps), app_type,
                              /*should_notify_initialized=*/false);

  // Promise app item should no longer exist in the model.
  item = GetAppListItem(package_id.ToString());
  ASSERT_FALSE(item);
}

IN_PROC_BROWSER_TEST_F(AppServicePromiseAppItemBrowserTest,
                       UpdatedFieldsShowInChromeAppListItem) {
  // Register a promise app in the promise app registry cache.
  apps::PromiseAppPtr promise_app =
      std::make_unique<PromiseApp>(kTestPackageId);
  promise_app->status = PromiseStatus::kPending;
  promise_app->should_show = true;
  cache()->OnPromiseApp(std::move(promise_app));

  // Promise app item should exist in the model.
  ChromeAppListItem* item = GetChromeAppListItem(kTestPackageId);
  ASSERT_TRUE(item);
  EXPECT_EQ(item->progress(), 0);
  EXPECT_EQ(item->app_status(), ash::AppStatus::kPending);
  ASSERT_EQ(item->name(),
            base::UTF16ToUTF8(ShelfControllerHelper::GetLabelForPromiseStatus(
                apps::PromiseStatus::kPending)));

  // Update the promise app in the promise app registry cache.
  apps::PromiseAppPtr update = std::make_unique<PromiseApp>(kTestPackageId);
  update->progress = 0.3;
  update->status = PromiseStatus::kInstalling;
  cache()->OnPromiseApp(std::move(update));

  // Promise app item should have updated fields.
  EXPECT_EQ(item->progress(), 0.3f);
  EXPECT_EQ(item->app_status(), ash::AppStatus::kInstalling);
  EXPECT_EQ(item->name(),
            base::UTF16ToUTF8(ShelfControllerHelper::GetLabelForPromiseStatus(
                apps::PromiseStatus::kInstalling)));
}

IN_PROC_BROWSER_TEST_F(AppServicePromiseAppItemBrowserTest, SetToSyncPosition) {
  syncer::StringOrdinal ordinal = syncer::StringOrdinal::CreateInitialOrdinal();

  const std::string app_activity = "test.com.example.activity";
  const std::string app_id =
      ArcAppListPrefs::GetAppId(kTestPackageId.identifier(), app_activity);

  // Add entry in sync data that has a matching PackageId with the promise app.
  syncer::SyncDataList sync_list;
  sync_list.push_back((app_list::CreateAppRemoteData(
      app_id, "App Name", /*parent_id=*/std::string(),
      ordinal.ToInternalValue(), /*item_pin_ordinal=*/std::string(),
      /*item_type=*/sync_pb::AppListSpecifics_AppListItemType_TYPE_APP,
      /*is_user_pinned=*/std::nullopt,
      /*promise_package_id=*/kTestPackageId.ToString())));
  app_list_syncable_service()->MergeDataAndStartSyncing(
      syncer::APP_LIST, sync_list,
      std::make_unique<syncer::FakeSyncChangeProcessor>());
  content::RunAllTasksUntilIdle();

  // Register a promise app in the promise app registry cache.
  apps::PromiseAppPtr promise_app =
      std::make_unique<PromiseApp>(kTestPackageId);
  promise_app->should_show = true;
  cache()->OnPromiseApp(std::move(promise_app));

  // Promise app item should exist in the model at the correct position.
  ChromeAppListItem* item = GetChromeAppListItem(kTestPackageId);
  ASSERT_TRUE(item);
  EXPECT_EQ(item->position(), ordinal);

  syncer::StringOrdinal ordinal_after_sync = ordinal.CreateAfter();
  syncer::SyncChangeList change_list;
  change_list.push_back(syncer::SyncChange(
      FROM_HERE, syncer::SyncChange::ACTION_ADD,
      app_list::CreateAppRemoteData(
          app_id, "Test App", "", ordinal_after_sync.ToInternalValue(), "",
          sync_pb::AppListSpecifics_AppListItemType_TYPE_APP, std::nullopt,
          kTestPackageId.ToString())));
  app_list_syncable_service()->ProcessSyncChanges(base::Location(),
                                                  change_list);

  // Verify the promise package position gets updaed by sync.
  item = GetChromeAppListItem(kTestPackageId);
  ASSERT_TRUE(item);
  EXPECT_EQ(item->position(), ordinal_after_sync);

  // Register (i.e. "install") an app with a matching package ID. This should
  // trigger removal of the promise app.
  AddArcPackageWithApps(kTestPackageId.identifier(), {app_activity});

  EXPECT_FALSE(GetChromeAppListItem(kTestPackageId));
  ChromeAppListItem* app_item = GetChromeAppListItem(app_id);
  ASSERT_TRUE(app_item);
  EXPECT_EQ(app_item->position(), ordinal_after_sync);
  EXPECT_FALSE(app_item->is_new_install());
}

IN_PROC_BROWSER_TEST_F(AppServicePromiseAppItemBrowserTest,
                       InstalledAppTakesPromiseAppPosition) {
  syncer::StringOrdinal initial_ordinal =
      syncer::StringOrdinal::CreateInitialOrdinal();
  syncer::StringOrdinal app_ordinal = initial_ordinal.CreateAfter();

  const std::string app_activity = "test.com.example.activity";
  const std::string app_id =
      ArcAppListPrefs::GetAppId(kTestPackageId.identifier(), app_activity);

  // Add entry in sync data that has a matching PackageId with the promise app.
  syncer::SyncDataList sync_list;
  sync_list.push_back((app_list::CreateAppRemoteData(
      "different.app", "Other app", /*parent_id=*/std::string(),
      initial_ordinal.ToInternalValue(), /*item_pin_ordinal=*/std::string(),
      /*item_type=*/sync_pb::AppListSpecifics_AppListItemType_TYPE_APP)));
  app_list_syncable_service()->MergeDataAndStartSyncing(
      syncer::APP_LIST, sync_list,
      std::make_unique<syncer::FakeSyncChangeProcessor>());
  content::RunAllTasksUntilIdle();

  // Register a promise app in the promise app registry cache.
  apps::PromiseAppPtr promise_app =
      std::make_unique<PromiseApp>(kTestPackageId);
  promise_app->should_show = true;
  cache()->OnPromiseApp(std::move(promise_app));

  // Promise app item should exist in the model.
  ChromeAppListItem* item = GetChromeAppListItem(kTestPackageId);
  ASSERT_TRUE(item);

  GetChromeAppListModelUpdater()->RequestPositionUpdate(
      kTestPackageId.ToString(), app_ordinal,
      ash::RequestPositionUpdateReason::kMoveItem);
  EXPECT_EQ(item->position(), app_ordinal);

  // Register (i.e. "install") an app with a matching package ID. This should
  // trigger removal of the promise app.
  AddArcPackageWithApps(kTestPackageId.identifier(), {app_activity});

  EXPECT_FALSE(GetChromeAppListItem(kTestPackageId));
  ChromeAppListItem* app_item = GetChromeAppListItem(app_id);
  ASSERT_TRUE(app_item);
  EXPECT_EQ(app_item->position(), app_ordinal);
  EXPECT_TRUE(app_item->is_new_install());
}

IN_PROC_BROWSER_TEST_F(AppServicePromiseAppItemBrowserTest, SetToSyncParent) {
  syncer::StringOrdinal folder_ordinal =
      syncer::StringOrdinal::CreateInitialOrdinal();
  syncer::StringOrdinal item_ordinal = folder_ordinal.CreateAfter();

  const std::string app_activity = "test.com.example.activity";
  const std::string app_id =
      ArcAppListPrefs::GetAppId(kTestPackageId.identifier(), app_activity);

  const std::string kFolderItemId = "folder_id";
  syncer::SyncDataList sync_list;
  sync_list.push_back(app_list::CreateAppRemoteData(
      kFolderItemId, "Folder", "", folder_ordinal.ToInternalValue(), "",
      sync_pb::AppListSpecifics_AppListItemType_TYPE_FOLDER));
  sync_list.push_back(app_list::CreateAppRemoteData(
      app_id, "App name", kFolderItemId, item_ordinal.ToInternalValue(),
      /*item_pin_ordinal=*/std::string(),
      /*item_type=*/sync_pb::AppListSpecifics_AppListItemType_TYPE_APP,
      /*is_user_pinned=*/std::nullopt,
      /*promise_package_id=*/kTestPackageId.ToString()));

  app_list_syncable_service()->MergeDataAndStartSyncing(
      syncer::APP_LIST, sync_list,
      std::make_unique<syncer::FakeSyncChangeProcessor>());
  content::RunAllTasksUntilIdle();

  // Register a promise app in the promise app registry cache.
  apps::PromiseAppPtr promise_app =
      std::make_unique<PromiseApp>(kTestPackageId);
  promise_app->should_show = true;
  cache()->OnPromiseApp(std::move(promise_app));

  // Promise app item should exist in the model within the folder specified in
  // sync data.
  ChromeAppListItem* item = GetChromeAppListItem(kTestPackageId);
  ASSERT_TRUE(item);
  EXPECT_EQ(item->folder_id(), kFolderItemId);

  syncer::SyncChangeList change_list;
  change_list.push_back(syncer::SyncChange(
      FROM_HERE, syncer::SyncChange::ACTION_ADD,
      app_list::CreateAppRemoteData(
          app_id, "App name", "", item_ordinal.ToInternalValue(), "",
          sync_pb::AppListSpecifics_AppListItemType_TYPE_APP, std::nullopt,
          kTestPackageId.ToString())));
  app_list_syncable_service()->ProcessSyncChanges(base::Location(),
                                                  change_list);

  // Verify the promise package position gets updaed by sync.
  item = GetChromeAppListItem(kTestPackageId);
  ASSERT_TRUE(item);
  EXPECT_EQ(item->folder_id(), "");

  // Register (i.e. "install") an app with a matching package ID. This should
  // trigger removal of the promise app.
  AddArcPackageWithApps(kTestPackageId.identifier(), {app_activity});

  EXPECT_FALSE(GetChromeAppListItem(kTestPackageId));
  ChromeAppListItem* app_item = GetChromeAppListItem(app_id);
  ASSERT_TRUE(app_item);
  EXPECT_EQ(app_item->folder_id(), "");
  EXPECT_FALSE(app_item->is_new_install());
}

IN_PROC_BROWSER_TEST_F(AppServicePromiseAppItemBrowserTest,
                       InstalledAppTakesPromiseAppParent) {
  syncer::StringOrdinal folder_ordinal =
      syncer::StringOrdinal::CreateInitialOrdinal();
  syncer::StringOrdinal other_app_ordinal = folder_ordinal.CreateAfter();

  const std::string app_activity = "test.com.example.activity";
  const std::string app_id =
      ArcAppListPrefs::GetAppId(kTestPackageId.identifier(), app_activity);

  const std::string kFolderItemId = "folder_id";

  const std::string other_package = "test.com.other";
  const std::string other_app_activity = "test.com.other.app";
  const std::string other_app_id =
      ArcAppListPrefs::GetAppId(other_package, other_app_activity);

  syncer::SyncDataList sync_list;
  sync_list.push_back(app_list::CreateAppRemoteData(
      kFolderItemId, "Folder", "", folder_ordinal.ToInternalValue(), "",
      sync_pb::AppListSpecifics_AppListItemType_TYPE_FOLDER));
  sync_list.push_back(app_list::CreateAppRemoteData(
      other_app_id, "Other app", kFolderItemId,
      other_app_ordinal.ToInternalValue(),
      /*item_pin_ordinal=*/std::string(),
      /*item_type=*/sync_pb::AppListSpecifics_AppListItemType_TYPE_APP));
  app_list_syncable_service()->MergeDataAndStartSyncing(
      syncer::APP_LIST, sync_list,
      std::make_unique<syncer::FakeSyncChangeProcessor>());
  content::RunAllTasksUntilIdle();

  AddArcPackageWithApps(other_package, {other_app_activity});

  // Register a promise app in the promise app registry cache.
  apps::PromiseAppPtr promise_app =
      std::make_unique<PromiseApp>(kTestPackageId);
  promise_app->should_show = true;
  cache()->OnPromiseApp(std::move(promise_app));

  // Promise app item should exist in the model at the correct position.
  ChromeAppListItem* item = GetChromeAppListItem(kTestPackageId);
  ASSERT_TRUE(item);

  GetChromeAppListModelUpdater()->RequestMoveItemToFolder(
      kTestPackageId.ToString(), kFolderItemId);
  EXPECT_EQ(item->folder_id(), kFolderItemId);

  // Register (i.e. "install") an app with a matching package ID. This should
  // trigger removal of the promise app.
  AddArcPackageWithApps(kTestPackageId.identifier(), {app_activity});

  EXPECT_FALSE(GetChromeAppListItem(kTestPackageId));
  ChromeAppListItem* app_item = GetChromeAppListItem(app_id);
  ASSERT_TRUE(app_item);
  EXPECT_EQ(app_item->folder_id(), kFolderItemId);
  EXPECT_TRUE(app_item->is_new_install());
}

IN_PROC_BROWSER_TEST_F(AppServicePromiseAppItemBrowserTest,
                       LabelMatchesWithStatus) {
  // Register test promise app.
  PromiseAppPtr promise_app = std::make_unique<PromiseApp>(kTestPackageId);
  promise_app->status = PromiseStatus::kPending;
  promise_app->should_show = true;
  cache()->OnPromiseApp(std::move(promise_app));

  // Promise app item should now exist in the model.
  ChromeAppListItem* item = GetChromeAppListItem(kTestPackageId);
  ASSERT_TRUE(item);
  ASSERT_EQ(item->app_status(), ash::AppStatus::kPending);
  ASSERT_EQ(item->name(),
            base::UTF16ToUTF8(ShelfControllerHelper::GetLabelForPromiseStatus(
                PromiseStatus::kPending)));

  // Push a status update to the promise app.
  PromiseAppPtr update = std::make_unique<PromiseApp>(kTestPackageId);
  update->status = PromiseStatus::kInstalling;
  cache()->OnPromiseApp(std::move(update));

  // Item should now reflect the new status and name.
  EXPECT_TRUE(item);
  EXPECT_EQ(item->app_status(), ash::AppStatus::kInstalling);
  EXPECT_EQ(item->name(),
            base::UTF16ToUTF8(ShelfControllerHelper::GetLabelForPromiseStatus(
                PromiseStatus::kInstalling)));
}

IN_PROC_BROWSER_TEST_F(AppServicePromiseAppItemBrowserTest,
                       InstalledAppPinnedWhenPinningPromiseApp) {
  std::string identifier = "test.com.example";
  PackageId package_id(PackageType::kArc, identifier);

  // Register a promise app in the promise app registry cache.
  apps::PromiseAppPtr promise_app = std::make_unique<PromiseApp>(package_id);
  promise_app->should_show = true;
  cache()->OnPromiseApp(std::move(promise_app));

  // Promise app item should exist in the model.
  ash::AppListItem* item = GetAppListItem(package_id.ToString());
  ASSERT_TRUE(item);

  // Pin the promise app to shelf.
  AppListClientImpl::GetInstance()->PinApp(package_id.ToString());
  EXPECT_TRUE(IsItemPinned(package_id.ToString()));

  // Register (i.e. "install") an app with a matching package ID. This should
  // trigger removal of the promise app.
  std::string app_activity = "test.com.example.activity";
  AddArcPackageWithApps(identifier, {app_activity});

  // Promise app item should no longer exist in the model.
  item = GetAppListItem(package_id.ToString());
  ASSERT_FALSE(item);
  EXPECT_FALSE(IsItemPinned(package_id.ToString()));

  // Verify that the app installed in place of the promise app is pinned.
  const std::string installed_app_id =
      ArcAppListPrefs::GetAppId(identifier, app_activity);
  EXPECT_TRUE(IsItemPinned(installed_app_id));

  ash::AppListItem* app_item = GetAppListItem(installed_app_id);
  ASSERT_TRUE(app_item);
  EXPECT_TRUE(app_item->is_new_install());
}

IN_PROC_BROWSER_TEST_F(AppServicePromiseAppItemBrowserTest,
                       PromiseAppPinnedIfLinkedToAPinnedSyncedApp) {
  syncer::StringOrdinal ordinal = syncer::StringOrdinal::CreateInitialOrdinal();
  syncer::StringOrdinal pin_ordinal = ordinal.CreateAfter();

  const std::string app_activity = "test.com.example.activity";
  const std::string app_id =
      ArcAppListPrefs::GetAppId(kTestPackageId.identifier(), app_activity);
  // Add entry in sync data that has a matching PackageId with the promise app.
  syncer::SyncDataList sync_list;
  sync_list.push_back((app_list::CreateAppRemoteData(
      app_id, "App Name", /*parent_id=*/std::string(),
      ordinal.ToInternalValue(), pin_ordinal.ToInternalValue(),
      /*item_type=*/sync_pb::AppListSpecifics_AppListItemType_TYPE_APP,
      /*is_user_pinned=*/std::nullopt,
      /*promise_package_id=*/kTestPackageId.ToString())));
  app_list_syncable_service()->MergeDataAndStartSyncing(
      syncer::APP_LIST, sync_list,
      std::make_unique<syncer::FakeSyncChangeProcessor>());
  content::RunAllTasksUntilIdle();

  // Register a promise app in the promise app registry cache.
  apps::PromiseAppPtr promise_app =
      std::make_unique<PromiseApp>(kTestPackageId);
  promise_app->should_show = true;
  cache()->OnPromiseApp(std::move(promise_app));

  // Promise app item should exist in the model, and be pinned.
  const std::string promise_app_id = kTestPackageId.ToString();
  ash::AppListItem* item = GetAppListItem(promise_app_id);
  ASSERT_TRUE(item);

  EXPECT_TRUE(IsItemPinned(promise_app_id));

  // Register (i.e. "install") an app with a matching package ID. This should
  // trigger removal of the promise app.
  AddArcPackageWithApps(kTestPackageId.identifier(), {app_activity});

  // Promise app item should no longer exist in the model.
  item = GetAppListItem(promise_app_id);
  ASSERT_FALSE(item);
  EXPECT_FALSE(IsItemPinned(promise_app_id));

  const std::string installed_app_id =
      ArcAppListPrefs::GetAppId(kTestPackageId.identifier(), app_activity);
  // Verify that the app installed in place of the promise app is pinned.
  EXPECT_TRUE(IsItemPinned(installed_app_id));

  ash::AppListItem* app_item = GetAppListItem(installed_app_id);
  ASSERT_TRUE(app_item);
  EXPECT_FALSE(app_item->is_new_install());
}

IN_PROC_BROWSER_TEST_F(AppServicePromiseAppItemBrowserTest,
                       PromiseItemInstallsMultipleApps) {
  syncer::StringOrdinal ordinal = syncer::StringOrdinal::CreateInitialOrdinal();
  syncer::StringOrdinal pin_ordinal = ordinal.CreateAfter();
  syncer::StringOrdinal promise_app_ordinal = pin_ordinal.CreateAfter();

  const std::string app_activity_in_sync = "test.com.example.activity.1";
  const std::string app_id_in_sync = ArcAppListPrefs::GetAppId(
      kTestPackageId.identifier(), app_activity_in_sync);
  // Add entry in sync data that has a matching PackageId with the promise app.
  syncer::SyncDataList sync_list;
  sync_list.push_back((app_list::CreateAppRemoteData(
      app_id_in_sync, "App Name", /*parent_id=*/std::string(),
      ordinal.ToInternalValue(), std::string(),
      /*item_type=*/sync_pb::AppListSpecifics_AppListItemType_TYPE_APP,
      /*is_user_pinned=*/std::nullopt,
      /*promise_package_id=*/kTestPackageId.ToString())));
  app_list_syncable_service()->MergeDataAndStartSyncing(
      syncer::APP_LIST, sync_list,
      std::make_unique<syncer::FakeSyncChangeProcessor>());
  content::RunAllTasksUntilIdle();

  // Register a promise app in the promise app registry cache.
  apps::PromiseAppPtr promise_app =
      std::make_unique<PromiseApp>(kTestPackageId);
  promise_app->should_show = true;
  cache()->OnPromiseApp(std::move(promise_app));

  // Promise app item should exist in the model, and be pinned.
  const std::string promise_app_id = kTestPackageId.ToString();
  ash::AppListItem* item = GetAppListItem(promise_app_id);
  ASSERT_TRUE(item);

  GetChromeAppListModelUpdater()->RequestPositionUpdate(
      promise_app_id, promise_app_ordinal,
      ash::RequestPositionUpdateReason::kMoveItem);

  AppListClientImpl::GetInstance()->PinApp(promise_app_id);
  EXPECT_TRUE(IsItemPinned(promise_app_id));

  // Register (i.e. "install") an app with a matching package ID. This should
  // trigger removal of the promise app.
  std::string extra_app_activity = "test.com.example.activity.2";
  AddArcPackageWithApps(kTestPackageId.identifier(),
                        {extra_app_activity, app_activity_in_sync});

  // Promise app item should no longer exist in the model.
  item = GetAppListItem(promise_app_id);
  ASSERT_FALSE(item);
  EXPECT_FALSE(IsItemPinned(promise_app_id));

  const std::string extra_app_id = ArcAppListPrefs::GetAppId(
      kTestPackageId.identifier(), extra_app_activity);
  // Verify that the app installed in place of the promise app is pinned.
  EXPECT_TRUE(IsItemPinned(app_id_in_sync));
  EXPECT_FALSE(IsItemPinned(extra_app_id));

  // Verify the installed app positions in app list are as expected.
  const ash::AppListItem* const old_installed_item =
      GetAppListItem(app_id_in_sync);
  ASSERT_TRUE(old_installed_item);
  EXPECT_EQ(promise_app_ordinal, old_installed_item->position());
  EXPECT_FALSE(old_installed_item->is_new_install());

  const ash::AppListItem* const new_installed_item =
      GetAppListItem(extra_app_id);
  ASSERT_TRUE(new_installed_item);
  EXPECT_NE(promise_app_ordinal, new_installed_item->position());
  EXPECT_TRUE(new_installed_item->is_new_install());
}

IN_PROC_BROWSER_TEST_F(AppServicePromiseAppItemBrowserTest,
                       TargetItemSyncedWhileInstallingPromiseApp) {
  app_list_syncable_service()->MergeDataAndStartSyncing(
      syncer::APP_LIST, syncer::SyncDataList(),
      std::make_unique<syncer::FakeSyncChangeProcessor>());
  content::RunAllTasksUntilIdle();

  // Register a promise app in the promise app registry cache.
  apps::PromiseAppPtr promise_app =
      std::make_unique<PromiseApp>(kTestPackageId);
  promise_app->should_show = true;
  cache()->OnPromiseApp(std::move(promise_app));

  // Promise app item should exist in the model, and be pinned.
  const std::string promise_app_id = kTestPackageId.ToString();
  ash::AppListItem* item = GetAppListItem(promise_app_id);
  ASSERT_TRUE(item);

  AppListClientImpl::GetInstance()->PinApp(promise_app_id);
  EXPECT_TRUE(IsItemPinned(promise_app_id));

  syncer::StringOrdinal app_ordinal =
      syncer::StringOrdinal::CreateInitialOrdinal();
  syncer::StringOrdinal promise_app_ordinal = app_ordinal.CreateAfter();

  GetChromeAppListModelUpdater()->RequestPositionUpdate(
      promise_app_id, promise_app_ordinal,
      ash::RequestPositionUpdateReason::kMoveItem);

  const std::string app_activity = "test.com.example.activity";
  const std::string app_id =
      ArcAppListPrefs::GetAppId(kTestPackageId.identifier(), app_activity);

  syncer::SyncChangeList change_list;
  change_list.push_back(syncer::SyncChange(
      FROM_HERE, syncer::SyncChange::ACTION_ADD,
      app_list::CreateAppRemoteData(
          app_id, "Test App", "", app_ordinal.ToInternalValue(), "",
          sync_pb::AppListSpecifics_AppListItemType_TYPE_APP, std::nullopt,
          kTestPackageId.ToString())));
  app_list_syncable_service()->ProcessSyncChanges(base::Location(),
                                                  change_list);
  AddArcPackageWithApps(kTestPackageId.identifier(), {app_activity});

  // Promise app item should no longer exist in the model.
  item = GetAppListItem(promise_app_id);
  ASSERT_FALSE(item);
  EXPECT_FALSE(IsItemPinned(promise_app_id));

  // Verify that the app installed in place of the promise app is pinned.
  EXPECT_TRUE(IsItemPinned(app_id));

  const ash::AppListItem* const installed_item = GetAppListItem(app_id);
  ASSERT_TRUE(installed_item);
  EXPECT_EQ(promise_app_ordinal, installed_item->position());
  EXPECT_FALSE(installed_item->is_new_install());
}

IN_PROC_BROWSER_TEST_F(AppServicePromiseAppItemBrowserTest,
                       MainLabelAndAccessibleLabelAreCorrect) {
  std::string app_name = "Long Name";

  // Register a promise app in the promise app registry cache.
  apps::PromiseAppPtr promise_app =
      std::make_unique<PromiseApp>(kTestPackageId);
  promise_app->status = PromiseStatus::kPending;
  promise_app->name = app_name;
  promise_app->should_show = true;
  cache()->OnPromiseApp(std::move(promise_app));

  // Promise app item should exist in the model.
  ChromeAppListItem* item = GetChromeAppListItem(kTestPackageId);
  ASSERT_TRUE(item);
  EXPECT_EQ(item->app_status(), ash::AppStatus::kPending);
  ASSERT_EQ(item->name(), "Waiting…");
  ASSERT_EQ(item->accessible_name(), "Long Name, waiting");

  // Update the promise app in the promise app registry cache.
  apps::PromiseAppPtr update = std::make_unique<PromiseApp>(kTestPackageId);
  update->progress = 0.3;
  update->status = PromiseStatus::kInstalling;
  cache()->OnPromiseApp(std::move(update));

  // Promise app item should have updated fields.
  EXPECT_EQ(item->app_status(), ash::AppStatus::kInstalling);
  EXPECT_EQ(item->name(), "Installing…");
  ASSERT_EQ(item->accessible_name(), "Long Name, installing");
}

IN_PROC_BROWSER_TEST_F(AppServicePromiseAppItemBrowserTest,
                       PlaceholderAccessibleLabelUsedWhenNoNameAvailable) {
  // Register a promise app in the promise app registry cache.
  apps::PromiseAppPtr promise_app =
      std::make_unique<PromiseApp>(kTestPackageId);
  promise_app->should_show = true;
  cache()->OnPromiseApp(std::move(promise_app));

  // Promise app item should exist in the model.
  ChromeAppListItem* item = GetChromeAppListItem(kTestPackageId);
  ASSERT_TRUE(item);
  EXPECT_EQ(item->app_status(), ash::AppStatus::kPending);
  ASSERT_EQ(item->accessible_name(), "An app, waiting");

  // Update the promise app in the promise app registry cache.
  apps::PromiseAppPtr update = std::make_unique<PromiseApp>(kTestPackageId);
  update->status = PromiseStatus::kInstalling;
  cache()->OnPromiseApp(std::move(update));

  // Promise app item should have updated fields.
  EXPECT_EQ(item->app_status(), ash::AppStatus::kInstalling);
  ASSERT_EQ(item->accessible_name(), "An app, installing");
}

IN_PROC_BROWSER_TEST_F(AppServicePromiseAppItemBrowserTest,
                       LauncherItemCreationUpdatesMetrics) {
  base::HistogramTester histogram_tester;
  histogram_tester.ExpectBucketCount(
      kPromiseAppLifecycleEventHistogram,
      apps::PromiseAppLifecycleEvent::kCreatedInLauncher, 0);

  apps::PromiseAppPtr promise_app =
      std::make_unique<PromiseApp>(kTestPackageId);
  promise_app->should_show = true;
  cache()->OnPromiseApp(std::move(promise_app));

  ChromeAppListItem* item = GetChromeAppListItem(kTestPackageId);
  ASSERT_TRUE(item);
  histogram_tester.ExpectBucketCount(
      kPromiseAppLifecycleEventHistogram,
      apps::PromiseAppLifecycleEvent::kCreatedInLauncher, 1);
}

IN_PROC_BROWSER_TEST_F(AppServicePromiseAppItemBrowserTest,
                       ReinstallRemovedDefaultApp) {
  const std::string app_activity = "test.com.example.activity";
  const std::string app_id =
      ArcAppListPrefs::GetAppId(kTestPackageId.identifier(), app_activity);

  syncer::StringOrdinal ordinal = syncer::StringOrdinal::CreateInitialOrdinal();
  syncer::StringOrdinal pin_ordinal = ordinal.CreateAfter();
  // Add entry in sync data that has a matching PackageId with the promise app.
  syncer::SyncDataList sync_list;
  sync_list.push_back((app_list::CreateAppRemoteData(
      app_id, "App Name", /*parent_id=*/std::string(),
      ordinal.ToInternalValue(), pin_ordinal.ToInternalValue(),
      /*item_type=*/
      sync_pb::AppListSpecifics_AppListItemType_TYPE_REMOVE_DEFAULT_APP,
      /*is_user_pinned=*/std::nullopt,
      /*promise_package_id=*/kTestPackageId.ToString())));
  app_list_syncable_service()->MergeDataAndStartSyncing(
      syncer::APP_LIST, sync_list,
      std::make_unique<syncer::FakeSyncChangeProcessor>());
  content::RunAllTasksUntilIdle();

  // Register a promise app in the promise app registry cache.
  apps::PromiseAppPtr promise_app =
      std::make_unique<PromiseApp>(kTestPackageId);
  promise_app->should_show = true;
  cache()->OnPromiseApp(std::move(promise_app));

  const std::string promise_app_id = kTestPackageId.ToString();
  ash::AppListItem* item = GetAppListItem(promise_app_id);
  ASSERT_TRUE(item);

  syncer::StringOrdinal promise_app_ordinal = pin_ordinal.CreateAfter();
  GetChromeAppListModelUpdater()->RequestPositionUpdate(
      promise_app_id, promise_app_ordinal,
      ash::RequestPositionUpdateReason::kMoveItem);
  AppListClientImpl::GetInstance()->PinApp(promise_app_id);

  AddArcPackageWithApps(kTestPackageId.identifier(), {app_activity});

  EXPECT_FALSE(GetAppListItem(promise_app_id));
  EXPECT_FALSE(IsItemPinned(promise_app_id));
  ash::AppListItem* app_item = GetAppListItem(app_id);
  ASSERT_TRUE(app_item);
  EXPECT_EQ(promise_app_ordinal, app_item->position());
  EXPECT_TRUE(IsItemPinned(app_id));
}

IN_PROC_BROWSER_TEST_F(AppServicePromiseAppItemBrowserTest, ContextMenu) {
  apps::PromiseAppPtr promise_app =
      std::make_unique<PromiseApp>(kTestPackageId);
  promise_app->should_show = true;
  cache()->OnPromiseApp(std::move(promise_app));

  // Promise app item should exist in the model.
  ChromeAppListItem* item = GetChromeAppListItem(kTestPackageId);
  ASSERT_TRUE(item);
  base::test::TestFuture<std::unique_ptr<ui::SimpleMenuModel>> future;
  item->GetContextMenuModel(ash::AppListItemContext::kNone,
                            future.GetCallback());
  std::unique_ptr<ui::SimpleMenuModel> menu_model = future.Take();

  // The context menu should only have the option to pin to shelf.
  EXPECT_EQ(menu_model->GetItemCount(), 1u);
  EXPECT_EQ(menu_model->GetTypeAt(0), ui::MenuModel::ItemType::TYPE_COMMAND);
  EXPECT_EQ(menu_model->GetCommandIdAt(0), ash::CommandId::TOGGLE_PIN);
}

// Test the full promise icon lifecycle where promise icon changes are triggered
// by ARC mojom updates.
IN_PROC_BROWSER_TEST_F(AppServicePromiseAppItemBrowserTest,
                       SyncedArcAppPromiseIconLifecycleInLauncherAndShelf) {
  // Test package details.
  std::string package_name = "com.test.app";
  const std::string app_name = "TestApp";
  const std::string activity_name = "TestActivity";
  const apps::PackageId package_id =
      apps::PackageId(apps::PackageType::kArc, package_name);
  const std::string app_id =
      ArcAppListPrefs::GetAppId(package_name, activity_name);

  // Set Up: Add entry in sync data.
  syncer::StringOrdinal launcher_ordinal =
      syncer::StringOrdinal::CreateInitialOrdinal();
  syncer::StringOrdinal shelf_ordinal = launcher_ordinal.CreateAfter();
  syncer::SyncDataList sync_list;
  sync_list.push_back((app_list::CreateAppRemoteData(
      app_id, app_name, /*parent_id=*/std::string(),
      launcher_ordinal.ToInternalValue(), shelf_ordinal.ToInternalValue(),
      /*item_type=*/sync_pb::AppListSpecifics_AppListItemType_TYPE_APP,
      /*is_user_pinned=*/true,
      /*promise_package_id=*/package_id.ToString())));
  app_list_syncable_service()->MergeDataAndStartSyncing(
      syncer::APP_LIST, sync_list,
      std::make_unique<syncer::FakeSyncChangeProcessor>());
  content::RunAllTasksUntilIdle();

  // Skip check for official API key.
  app_service_proxy()->PromiseAppService()->SetSkipApiKeyCheckForTesting(true);

  // Test:
  // 1) Start the installation.
  // Expect 2 updates: Promise app registration, then Almanac response update.
  // Note: As the Almanac response is not mocked, the promise icon will fallback
  // to using a placeholder image.
  ExpectNumUpdates(/*num_updates=*/2);
  app_instance()->SendInstallationStarted(package_name);
  WaitForPromiseAppUpdates();

  // Confirm that the promise icon gets generated with the correct label and
  // icon in the positions indicated by the sync data.
  ash::AppListItem* launcher_item = GetAppListItem(package_id.ToString());
  ASSERT_TRUE(launcher_item);
  EXPECT_EQ(launcher_item->name(), "Waiting…");
  EXPECT_EQ(launcher_item->progress(), 0);
  EXPECT_EQ(launcher_item->position(), launcher_ordinal);
  EXPECT_TRUE(IsItemPinned(package_id.ToString()));
  auto* shelf_controller = ChromeShelfController::instance();
  const ash::ShelfItem* shelf_item =
      shelf_controller->GetItem(ash::ShelfID(package_id.ToString()));
  EXPECT_EQ(shelf_item->progress, 0);
  EXPECT_EQ(app_list_syncable_service()->GetPinPosition(package_id.ToString()),
            shelf_ordinal);

  // 2) Send a progress update.
  app_instance()->SendInstallationProgressChanged(package_name, 0.2);

  // Confirm the promise icon fields.
  launcher_item = GetAppListItem(package_id.ToString());
  EXPECT_EQ(launcher_item->name(), "Installing…");
  EXPECT_FLOAT_EQ(launcher_item->progress(), 0.2f);
  EXPECT_EQ(launcher_item->position(), launcher_ordinal);
  EXPECT_TRUE(IsItemPinned(package_id.ToString()));
  EXPECT_EQ(app_list_syncable_service()->GetPinPosition(package_id.ToString()),
            shelf_ordinal);
  shelf_item = shelf_controller->GetItem(ash::ShelfID(package_id.ToString()));
  EXPECT_FLOAT_EQ(shelf_item->progress, 0.2f);

  // 3) Finish the installation.
  std::vector<arc::mojom::AppInfoPtr> apps;
  arc::mojom::AppInfoPtr app_info = arc::mojom::AppInfo::New(
      app_name, package_name, activity_name, /*sticky=*/false);
  apps.emplace_back(std::move(app_info));
  app_instance()->SendRefreshAppList(apps);

  // Confirm that the promise icon no longer exists.
  launcher_item = GetAppListItem(package_id.ToString());
  EXPECT_FALSE(launcher_item);
  EXPECT_FALSE(IsItemPinned(package_id.ToString()));

  // Confirm that the installed app has replaced the promise icon in the correct
  // Launcher and Shelf position.
  ash::AppListItem* installed_launcher_item = GetAppListItem(app_id);
  EXPECT_TRUE(installed_launcher_item);
  EXPECT_EQ(installed_launcher_item->name(), app_name);
  EXPECT_EQ(installed_launcher_item->position(), launcher_ordinal);
  EXPECT_TRUE(IsItemPinned(app_id));
  EXPECT_EQ(app_list_syncable_service()->GetPinPosition(app_id), shelf_ordinal);
}

IN_PROC_BROWSER_TEST_F(AppServicePromiseAppItemBrowserTest,
                       ActivatePromiseArcAppWhilePending) {
  // Test package details.
  std::string package_name = "com.test.app";
  const std::string app_name = "TestApp";
  const std::string activity_name = "TestActivity";
  const apps::PackageId package_id =
      apps::PackageId(apps::PackageType::kArc, package_name);

  // Skip check for official API key.
  app_service_proxy()->PromiseAppService()->SetSkipApiKeyCheckForTesting(true);
  SetUpSyncedArcPromiseApp(package_name);

  ASSERT_EQ(arc::mojom::InstallPriority::kLow,
            arc_app_list_pref()
                ->GetInstallPriorityHandler()
                ->GetInstallPriorityForTesting(package_name));

  ash::AppListItem* launcher_item = GetAppListItem(package_id.ToString());
  ASSERT_TRUE(launcher_item);
  EXPECT_EQ(launcher_item->name(), "Waiting…");

  ChromeAppListItem* item = GetChromeAppListItem(package_id);
  ASSERT_TRUE(item);
  item->Activate(ui::EF_NONE);

  ASSERT_EQ(arc::mojom::InstallPriority::kMedium,
            arc_app_list_pref()
                ->GetInstallPriorityHandler()
                ->GetInstallPriorityForTesting(package_name));
}

IN_PROC_BROWSER_TEST_F(AppServicePromiseAppItemBrowserTest,
                       ActivatePromiseArcAppWhileInstalling) {
  // Test package details.
  std::string package_name = "com.test.app";
  const std::string app_name = "TestApp";
  const std::string activity_name = "TestActivity";
  const apps::PackageId package_id =
      apps::PackageId(apps::PackageType::kArc, package_name);

  // Skip check for official API key.
  app_service_proxy()->PromiseAppService()->SetSkipApiKeyCheckForTesting(true);
  SetUpSyncedArcPromiseApp(package_name);

  ASSERT_EQ(arc::mojom::InstallPriority::kLow,
            arc_app_list_pref()
                ->GetInstallPriorityHandler()
                ->GetInstallPriorityForTesting(package_name));

  ash::AppListItem* launcher_item = GetAppListItem(package_id.ToString());
  ASSERT_TRUE(launcher_item);
  EXPECT_EQ(launcher_item->name(), "Waiting…");

  // Send a progress update.
  app_instance()->SendInstallationProgressChanged(package_name, 0.2);

  // Confirm the promise icon fields.
  EXPECT_EQ(launcher_item->name(), "Installing…");

  ChromeAppListItem* item = GetChromeAppListItem(package_id);
  ASSERT_TRUE(item);
  item->Activate(ui::EF_NONE);

  // Install priority should not change if the installation has already started.
  ASSERT_EQ(arc::mojom::InstallPriority::kLow,
            arc_app_list_pref()
                ->GetInstallPriorityHandler()
                ->GetInstallPriorityForTesting(package_name));
}

IN_PROC_BROWSER_TEST_F(AppServicePromiseAppItemBrowserTest,
                       SelectPromiseArcAppFromShelfWhilePending) {
  // Test package details.
  std::string package_name = "com.test.app";
  const std::string app_name = "TestApp";
  const std::string activity_name = "TestActivity";
  const apps::PackageId package_id =
      apps::PackageId(apps::PackageType::kArc, package_name);

  // Skip check for official API key.
  app_service_proxy()->PromiseAppService()->SetSkipApiKeyCheckForTesting(true);
  SetUpSyncedArcPromiseApp(package_name);

  ASSERT_EQ(arc::mojom::InstallPriority::kLow,
            arc_app_list_pref()
                ->GetInstallPriorityHandler()
                ->GetInstallPriorityForTesting(package_name));

  ash::ShelfModel* shelf_model = ash::ShelfModel::Get();
  PinAppWithIDToShelf(package_id.ToString());

  ash::ShelfItemDelegate* delegate =
      shelf_model->GetShelfItemDelegate(ash::ShelfID(package_id.ToString()));

  DCHECK(delegate);
  delegate->ItemSelected(/*event=*/nullptr, display::kInvalidDisplayId,
                         ash::LAUNCH_FROM_UNKNOWN,
                         /*callback=*/base::DoNothing(),
                         /*filter_predicate=*/base::NullCallback());

  ASSERT_EQ(arc::mojom::InstallPriority::kMedium,
            arc_app_list_pref()
                ->GetInstallPriorityHandler()
                ->GetInstallPriorityForTesting(package_name));
}

IN_PROC_BROWSER_TEST_F(AppServicePromiseAppItemBrowserTest,
                       SelectPromiseArcAppFromShelfWhileInstalling) {
  // Test package details.
  std::string package_name = "com.test.app";
  const std::string app_name = "TestApp";
  const std::string activity_name = "TestActivity";
  const apps::PackageId package_id =
      apps::PackageId(apps::PackageType::kArc, package_name);

  // Skip check for official API key.
  app_service_proxy()->PromiseAppService()->SetSkipApiKeyCheckForTesting(true);
  SetUpSyncedArcPromiseApp(package_name);

  ASSERT_EQ(arc::mojom::InstallPriority::kLow,
            arc_app_list_pref()
                ->GetInstallPriorityHandler()
                ->GetInstallPriorityForTesting(package_name));

  // Send a progress update.
  app_instance()->SendInstallationProgressChanged(package_name, 0.2);

  ash::ShelfModel* shelf_model = ash::ShelfModel::Get();
  PinAppWithIDToShelf(package_id.ToString());

  ash::ShelfItemDelegate* delegate =
      shelf_model->GetShelfItemDelegate(ash::ShelfID(package_id.ToString()));

  DCHECK(delegate);
  delegate->ItemSelected(/*event=*/nullptr, display::kInvalidDisplayId,
                         ash::LAUNCH_FROM_UNKNOWN,
                         /*callback=*/base::DoNothing(),
                         /*filter_predicate=*/base::NullCallback());

  // Install priority should not be updated if the package installation has
  // started.
  ASSERT_EQ(arc::mojom::InstallPriority::kLow,
            arc_app_list_pref()
                ->GetInstallPriorityHandler()
                ->GetInstallPriorityForTesting(package_name));
}

IN_PROC_BROWSER_TEST_F(AppServicePromiseAppItemBrowserTest,
                       CheckArcPromiseAppIconWhenArcConnectionClosed) {
  // Test package details.
  std::string package_name = "com.test.app";
  const std::string app_name = "TestApp";
  const std::string activity_name = "TestActivity";
  const apps::PackageId package_id =
      apps::PackageId(apps::PackageType::kArc, package_name);
  const std::string app_id =
      ArcAppListPrefs::GetAppId(package_name, activity_name);

  // Skip check for official API key.
  app_service_proxy()->PromiseAppService()->SetSkipApiKeyCheckForTesting(true);

  // Test:
  // 1) Start the installation.
  // Expect 2 updates: Promise app registration, then Almanac response update.
  // Note: As the Almanac response is not mocked, the promise icon will fallback
  // to using a placeholder image.
  ExpectNumUpdates(/*num_updates=*/2);
  app_instance()->SendInstallationStarted(package_name);
  WaitForPromiseAppUpdates();

  // Confirm that the promise icon gets generated with the correct label.
  ash::AppListItem* launcher_item = GetAppListItem(package_id.ToString());
  ASSERT_TRUE(launcher_item);
  EXPECT_EQ(launcher_item->name(), "Waiting…");
  EXPECT_EQ(launcher_item->progress(), 0);
  // Close ARC connection.
  arc_app_list_pref()->app_connection_holder()->CloseInstance(app_instance());

  // Confirm that the promise icon is deleted when ARC connection is closed.
  launcher_item = GetAppListItem(package_id.ToString());
  ASSERT_FALSE(launcher_item);
}

}  // namespace apps
