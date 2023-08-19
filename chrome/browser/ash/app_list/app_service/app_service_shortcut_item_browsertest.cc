// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/app_service/app_service_shortcut_item.h"

#include "ash/app_list/app_list_model_provider.h"
#include "ash/app_list/model/app_list_item.h"
#include "base/ranges/algorithm.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_ash.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/platform_apps/app_browsertest_util.h"
#include "chrome/browser/ash/app_list/app_list_client_impl.h"
#include "chrome/browser/ash/app_list/app_list_syncable_service.h"
#include "chrome/browser/ash/app_list/app_list_syncable_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/shortcut/shortcut.h"
#include "components/services/app_service/public/cpp/shortcut/shortcut_registry_cache.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/test/fake_sync_change_processor.h"
#include "components/sync/test/sync_change_processor_wrapper_for_test.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace apps {

namespace {

ash::AppListItem* GetAppListItem(const std::string& id) {
  return ash::AppListModelProvider::Get()->model()->FindItem(id);
}

}  // namespace

class AppServiceShortcutItemBrowserTest
    : public extensions::PlatformAppBrowserTest {
 public:
  AppServiceShortcutItemBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(
        features::kCrosWebAppShortcutUiUpdate);
  }
  ~AppServiceShortcutItemBrowserTest() override = default;

  // extensions::PlatformAppBrowserTest:
  void SetUpOnMainThread() override {
    extensions::PlatformAppBrowserTest::SetUpOnMainThread();
    AppListClientImpl* client = AppListClientImpl::GetInstance();
    ASSERT_TRUE(client);
    client->UpdateProfile();
  }

  apps::ShortcutRegistryCache* cache() {
    return apps::AppServiceProxyFactory::GetForProfile(profile())
        ->ShortcutRegistryCache();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(AppServiceShortcutItemBrowserTest,
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

  // Register a shortcut in the shortcut registry cache.
  apps::ShortcutPtr shortcut =
      std::make_unique<Shortcut>("host_app_id", "local_id");
  shortcut->shortcut_source = ShortcutSource::kUser;
  shortcut->name = "Test";
  cache()->UpdateShortcut(std::move(shortcut));

  apps::ShortcutId shortcut_id =
      apps::GenerateShortcutId("host_app_id", "local_id");
  ash::AppListItem* item = GetAppListItem(shortcut_id.value());
  ASSERT_TRUE(item);
  ASSERT_EQ(item->name(), "Test");

  // Verify that the shortcut item is added to local storage.
  const base::Value::Dict& local_items =
      profile()->GetPrefs()->GetDict(prefs::kAppListLocalState);
  const base::Value::Dict* dict_item =
      local_items.FindDict(shortcut_id.value());
  EXPECT_TRUE(dict_item);

  // Verify that shortcut item is uploaded to sync data.

  auto is_shortcut = [&shortcut_id](const syncer::SyncChange& sync_change) {
    return sync_change.sync_data().GetSpecifics().app_list().item_id() ==
           shortcut_id.value();
  };
  EXPECT_TRUE(base::ranges::any_of(sync_processor->changes(), is_shortcut));
}

}  // namespace apps
