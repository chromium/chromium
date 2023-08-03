// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/app_service/app_service_promise_app_item.h"

#include "ash/app_list/app_list_model_provider.h"
#include "ash/app_list/model/app_list_item.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/public/cpp/app_menu_constants.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_ash.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/package_id.h"
#include "chrome/browser/apps/app_service/promise_apps/promise_app.h"
#include "chrome/browser/apps/app_service/promise_apps/promise_app_registry_cache.h"
#include "chrome/browser/apps/platform_apps/app_browsertest_util.h"
#include "chrome/browser/ash/app_list/app_list_client_impl.h"
#include "chrome/browser/ash/app_list/app_list_model_updater.h"
#include "chrome/browser/ash/app_list/app_list_syncable_service.h"
#include "chrome/browser/ash/app_list/app_list_syncable_service_factory.h"
#include "chrome/browser/ash/app_list/chrome_app_list_item.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/test/fake_sync_change_processor.h"
#include "components/sync/test/sync_change_processor_wrapper_for_test.h"
#include "content/public/test/browser_test.h"
#include "ui/base/models/menu_model.h"

namespace apps {

const apps::PackageId kTestPackageId =
    apps::PackageId(apps::AppType::kArc, "com.test.package");

ash::AppListItem* GetAppListItem(const std::string& id) {
  return ash::AppListModelProvider::Get()->model()->FindItem(id);
}

class AppServicePromiseAppItemBrowserTest
    : public extensions::PlatformAppBrowserTest {
 public:
  AppServicePromiseAppItemBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(ash::features::kPromiseIcons);
  }
  ~AppServicePromiseAppItemBrowserTest() override = default;

  // extensions::PlatformAppBrowserTest:
  void SetUpOnMainThread() override {
    extensions::PlatformAppBrowserTest::SetUpOnMainThread();
    AppListClientImpl* client = AppListClientImpl::GetInstance();
    ASSERT_TRUE(client);
    client->UpdateProfile();
    cache_ = apps::AppServiceProxyFactory::GetForProfile(profile())
                 ->PromiseAppRegistryCache();
  }

  ChromeAppListItem* GetChromeAppListItem(const PackageId& package_id) {
    app_list::AppListSyncableService* app_list_syncable_service =
        app_list::AppListSyncableServiceFactory::GetForProfile(profile());
    AppListModelUpdater* model_updater =
        app_list_syncable_service->GetModelUpdater();
    return model_updater->FindItem(package_id.ToString());
  }

  apps::PromiseAppRegistryCache* cache() { return cache_; }

  apps::AppRegistryCache& app_cache() {
    return apps::AppServiceProxyFactory::GetForProfile(profile())
        ->AppRegistryCache();
  }

 private:
  raw_ptr<apps::PromiseAppRegistryCache, ExperimentalAsh> cache_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(AppServicePromiseAppItemBrowserTest,
                       ShouldShowUpdateCreatesItem) {
  // Sync setup.
  std::unique_ptr<syncer::FakeSyncChangeProcessor> sync_processor =
      std::make_unique<syncer::FakeSyncChangeProcessor>();
  app_list::AppListSyncableService* app_list_syncable_service_ =
      app_list::AppListSyncableServiceFactory::GetForProfile(profile());
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
  promise_app_update->name = "Test";
  promise_app_update->should_show = true;
  cache()->OnPromiseApp(std::move(promise_app_update));

  // Promise app item should now exist in the model.
  item = GetAppListItem(kTestPackageId.ToString());
  ASSERT_TRUE(item);
  ASSERT_EQ(item->name(), "Test");

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
  promise_app->name = "Test";
  promise_app->should_show = true;
  cache()->OnPromiseApp(std::move(promise_app));

  // Promise app item should exist in the model.
  ChromeAppListItem* item = GetChromeAppListItem(kTestPackageId);
  ASSERT_TRUE(item);
  ASSERT_EQ(item->name(), "Test");

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
  PackageId package_id(app_type, identifier);

  // Register a promise app in the promise app registry cache.
  apps::PromiseAppPtr promise_app = std::make_unique<PromiseApp>(package_id);
  promise_app->name = "Test";
  promise_app->should_show = true;
  cache()->OnPromiseApp(std::move(promise_app));

  // Promise app item should exist in the model.
  ash::AppListItem* item = GetAppListItem(package_id.ToString());
  ASSERT_TRUE(item);
  ASSERT_EQ(item->name(), "Test");

  // Register (i.e. "install") an app with a matching package ID. This should
  // trigger removal of the promise app.
  std::string app_id = "qwertyuiopasdfghjkl";
  apps::AppPtr app = std::make_unique<apps::App>(app_type, app_id);
  app->publisher_id = identifier;
  app->readiness = apps::Readiness::kReady;
  std::vector<apps::AppPtr> apps;
  apps.push_back(std::move(app));
  app_cache().OnApps(std::move(apps), app_type,
                     /*should_notify_initialized=*/false);

  // Promise app item should no longer exist in the model.
  item = GetAppListItem(package_id.ToString());
  ASSERT_FALSE(item);
}

}  // namespace apps
