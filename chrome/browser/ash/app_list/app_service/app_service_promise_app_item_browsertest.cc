// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/app_service/app_service_promise_app_item.h"

#include "ash/app_list/app_list_model_provider.h"
#include "ash/app_list/model/app_list_item.h"
#include "ash/constants/ash_features.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_ash.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/package_id.h"
#include "chrome/browser/apps/app_service/promise_apps/promise_app.h"
#include "chrome/browser/apps/app_service/promise_apps/promise_app_registry_cache.h"
#include "chrome/browser/apps/platform_apps/app_browsertest_util.h"
#include "chrome/browser/ash/app_list/app_list_client_impl.h"
#include "chrome/browser/ash/app_list/app_list_syncable_service.h"
#include "chrome/browser/ash/app_list/app_list_syncable_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/test/fake_sync_change_processor.h"
#include "components/sync/test/sync_change_processor_wrapper_for_test.h"
#include "content/public/test/browser_test.h"

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

  apps::PromiseAppRegistryCache* cache() { return cache_; }

 private:
  apps::PromiseAppRegistryCache* cache_;
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

}  // namespace apps
