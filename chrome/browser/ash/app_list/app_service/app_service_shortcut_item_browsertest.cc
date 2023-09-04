// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/app_service/app_service_shortcut_item.h"

#include <memory>

#include "ash/app_list/app_list_model_provider.h"
#include "ash/app_list/model/app_list_item.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/public/cpp/shelf_item.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "base/ranges/algorithm.h"
#include "base/test/test_future.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_ash.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/publishers/shortcut_publisher.h"
#include "chrome/browser/apps/platform_apps/app_browsertest_util.h"
#include "chrome/browser/ash/app_list/app_list_client_impl.h"
#include "chrome/browser/ash/app_list/app_list_model_updater.h"
#include "chrome/browser/ash/app_list/app_list_syncable_service.h"
#include "chrome/browser/ash/app_list/app_list_syncable_service_factory.h"
#include "chrome/browser/ash/app_list/chrome_app_list_model_updater.h"
#include "chrome/browser/ash/app_list/test/chrome_app_list_test_support.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/app_constants/constants.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/shortcut/shortcut.h"
#include "components/services/app_service/public/cpp/shortcut/shortcut_registry_cache.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/test/fake_sync_change_processor.h"
#include "components/sync/test/sync_change_processor_wrapper_for_test.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/menu_model.h"
#include "ui/views/vector_icons.h"

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

  apps::ShortcutId CreateWebAppBasedShortcut(
      const GURL& app_url,
      const std::u16string& shortcut_name) {
    // Create web app based shortcut.
    auto web_app_info = std::make_unique<web_app::WebAppInstallInfo>();
    web_app_info->start_url = app_url;
    web_app_info->title = shortcut_name;
    auto local_shortcut_id =
        web_app::test::InstallWebApp(profile(), std::move(web_app_info));
    return apps::GenerateShortcutId(app_constants::kChromeAppId,
                                    local_shortcut_id);
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

IN_PROC_BROWSER_TEST_F(AppServiceShortcutItemBrowserTest, ContextMenuOpen) {
  GURL app_url = GURL("https://example.org/");
  std::u16string shortcut_name = u"Example";
  apps::ShortcutId shortcut_id =
      CreateWebAppBasedShortcut(app_url, shortcut_name);

  AppListClientImpl* client = AppListClientImpl::GetInstance();
  AppListModelUpdater* model_updater = test::GetModelUpdater(client);
  ChromeAppListItem* item = model_updater->FindItem(shortcut_id.value());
  ASSERT_TRUE(item);

  base::test::TestFuture<std::unique_ptr<ui::SimpleMenuModel>> future;
  item->GetContextMenuModel(ash::AppListItemContext::kNone,
                            future.GetCallback());

  std::unique_ptr<ui::SimpleMenuModel> menu_model = future.Take();

  auto launch_new_command_index =
      menu_model->GetIndexOfCommandId(ash::LAUNCH_NEW);
  ASSERT_TRUE(launch_new_command_index);
  EXPECT_EQ(launch_new_command_index.value(), 0u);

  std::u16string host_app_name;
  apps::AppServiceProxyFactory::GetForProfile(profile())
      ->AppRegistryCache()
      .ForOneApp(app_constants::kChromeAppId,
                 [&host_app_name](const apps::AppUpdate& update) {
                   host_app_name = base::UTF8ToUTF16(update.ShortName());
                 });

  EXPECT_EQ(u"Open " + shortcut_name + u" - " + host_app_name,
            menu_model->GetLabelAt(launch_new_command_index.value()));
  EXPECT_EQ(&vector_icons::kLaunchIcon,
            menu_model->GetIconAt(launch_new_command_index.value())
                .GetVectorIcon()
                .vector_icon());

  ui_test_utils::UrlLoadObserver url_observer(
      app_url, content::NotificationService::AllSources());
  menu_model->ActivatedAt(launch_new_command_index.value());
  url_observer.Wait();
}

IN_PROC_BROWSER_TEST_F(AppServiceShortcutItemBrowserTest, Activate) {
  GURL app_url = GURL("https://example.org/");
  std::u16string shortcut_name = u"Example";
  apps::ShortcutId shortcut_id =
      CreateWebAppBasedShortcut(app_url, shortcut_name);

  AppListClientImpl* client = AppListClientImpl::GetInstance();
  AppListModelUpdater* model_updater = test::GetModelUpdater(client);
  ChromeAppListItem* item = model_updater->FindItem(shortcut_id.value());
  ASSERT_TRUE(item);

  ui_test_utils::UrlLoadObserver url_observer(
      app_url, content::NotificationService::AllSources());
  item->PerformActivate(ui::EF_NONE);
  url_observer.Wait();
}

IN_PROC_BROWSER_TEST_F(AppServiceShortcutItemBrowserTest,
                       ContextMenuTooglePin) {
  GURL app_url = GURL("https://example.org/");
  std::u16string shortcut_name = u"Example";
  apps::ShortcutId shortcut_id =
      CreateWebAppBasedShortcut(app_url, shortcut_name);

  AppListClientImpl* client = AppListClientImpl::GetInstance();
  AppListModelUpdater* model_updater = test::GetModelUpdater(client);
  ChromeAppListItem* item = model_updater->FindItem(shortcut_id.value());
  ASSERT_TRUE(item);

  base::test::TestFuture<std::unique_ptr<ui::SimpleMenuModel>> future;
  item->GetContextMenuModel(ash::AppListItemContext::kNone,
                            future.GetCallback());

  std::unique_ptr<ui::SimpleMenuModel> menu_model = future.Take();

  auto tootle_pin_command_index =
      menu_model->GetIndexOfCommandId(ash::CommandId::TOGGLE_PIN);
  ASSERT_TRUE(tootle_pin_command_index);
  EXPECT_EQ(tootle_pin_command_index.value(), 1u);

  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_APP_LIST_CONTEXT_MENU_PIN),
            menu_model->GetLabelAt(tootle_pin_command_index.value()));
  EXPECT_EQ(&views::kPinIcon,
            menu_model->GetIconAt(tootle_pin_command_index.value())
                .GetVectorIcon()
                .vector_icon());

  menu_model->ActivatedAt(tootle_pin_command_index.value());

  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_APP_LIST_CONTEXT_MENU_UNPIN),
            menu_model->GetLabelAt(tootle_pin_command_index.value()));
  EXPECT_EQ(&views::kUnpinIcon,
            menu_model->GetIconAt(tootle_pin_command_index.value())
                .GetVectorIcon()
                .vector_icon());
  auto* controller = ChromeShelfController::instance();
  auto* shelf_item = controller->GetItem(ash::ShelfID(shortcut_id.value()));
  EXPECT_TRUE(shelf_item);
  EXPECT_EQ(shelf_item->type, ash::ShelfItemType::TYPE_PINNED_APP);
  EXPECT_EQ(shelf_item->title, shortcut_name);

  menu_model->ActivatedAt(tootle_pin_command_index.value());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_APP_LIST_CONTEXT_MENU_PIN),
            menu_model->GetLabelAt(tootle_pin_command_index.value()));
  EXPECT_EQ(&views::kPinIcon,
            menu_model->GetIconAt(tootle_pin_command_index.value())
                .GetVectorIcon()
                .vector_icon());
  EXPECT_FALSE(controller->GetItem(ash::ShelfID(shortcut_id.value())));
}

IN_PROC_BROWSER_TEST_F(AppServiceShortcutItemBrowserTest, ContextMenuRemove) {
  GURL app_url = GURL("https://example.org/");
  std::u16string shortcut_name = u"Example";
  apps::ShortcutId shortcut_id =
      CreateWebAppBasedShortcut(app_url, shortcut_name);

  AppListClientImpl* client = AppListClientImpl::GetInstance();
  AppListModelUpdater* model_updater = test::GetModelUpdater(client);
  ChromeAppListItem* item = model_updater->FindItem(shortcut_id.value());
  ASSERT_TRUE(item);

  base::test::TestFuture<std::unique_ptr<ui::SimpleMenuModel>> future;
  item->GetContextMenuModel(ash::AppListItemContext::kNone,
                            future.GetCallback());

  std::unique_ptr<ui::SimpleMenuModel> menu_model = future.Take();

  auto uninstall_command_index =
      menu_model->GetIndexOfCommandId(ash::UNINSTALL);
  ASSERT_TRUE(uninstall_command_index);
  EXPECT_EQ(uninstall_command_index.value(), 2u);

  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_APP_LIST_REMOVE_SHORTCUT),
            menu_model->GetLabelAt(uninstall_command_index.value()));
  EXPECT_EQ(&views::kUninstallIcon,
            menu_model->GetIconAt(uninstall_command_index.value())
                .GetVectorIcon()
                .vector_icon());

  menu_model->ActivatedAt(uninstall_command_index.value());
  base::RunLoop().RunUntilIdle();
  content::RunAllTasksUntilIdle();
  item = model_updater->FindItem(shortcut_id.value());
  EXPECT_FALSE(item);
}

IN_PROC_BROWSER_TEST_F(AppServiceShortcutItemBrowserTest, ContextMenuReorder) {
  GURL app_url = GURL("https://example.org/");
  std::u16string shortcut_name = u"Example";
  apps::ShortcutId shortcut_id =
      CreateWebAppBasedShortcut(app_url, shortcut_name);

  AppListClientImpl* client = AppListClientImpl::GetInstance();
  AppListModelUpdater* model_updater = test::GetModelUpdater(client);
  ChromeAppListItem* item = model_updater->FindItem(shortcut_id.value());
  ASSERT_TRUE(item);

  base::test::TestFuture<std::unique_ptr<ui::SimpleMenuModel>> future;
  item->GetContextMenuModel(ash::AppListItemContext::kAppsGrid,
                            future.GetCallback());

  std::unique_ptr<ui::SimpleMenuModel> menu_model = future.Take();

  ASSERT_EQ(menu_model->GetTypeAt(3u), ui::MenuModel::TYPE_SEPARATOR);
  EXPECT_EQ(menu_model->GetSeparatorTypeAt(3u),
            ui::MenuSeparatorType::NORMAL_SEPARATOR);

  auto reorder_command_index =
      menu_model->GetIndexOfCommandId(ash::REORDER_SUBMENU);
  ASSERT_TRUE(reorder_command_index);
  EXPECT_EQ(reorder_command_index.value(), 4u);

  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_APP_LIST_CONTEXT_MENU_REORDER_TITLE),
            menu_model->GetLabelAt(reorder_command_index.value()));
  EXPECT_EQ(&ash::kReorderIcon,
            menu_model->GetIconAt(reorder_command_index.value())
                .GetVectorIcon()
                .vector_icon());

  auto* submenu = menu_model->GetSubmenuModelAt(reorder_command_index.value());
  ASSERT_TRUE(submenu);

  size_t name_reorder_command_index = 0u;
  EXPECT_EQ(submenu->GetCommandIdAt(name_reorder_command_index),
            ash::REORDER_BY_NAME_ALPHABETICAL);

  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_APP_LIST_CONTEXT_MENU_REORDER_BY_NAME),
      submenu->GetLabelAt(name_reorder_command_index));
  EXPECT_EQ(&ash::kSortAlphabeticalIcon,
            submenu->GetIconAt(name_reorder_command_index)
                .GetVectorIcon()
                .vector_icon());

  ChromeAppListModelUpdater* chrome_model_updater =
      static_cast<ChromeAppListModelUpdater*>(
          app_list::AppListSyncableServiceFactory::GetForProfile(profile())
              ->GetModelUpdater());
  ASSERT_TRUE(chrome_model_updater);

  submenu->ActivatedAt(name_reorder_command_index);
  EXPECT_EQ(ash::AppListSortOrder::kNameAlphabetical,
            chrome_model_updater->GetTemporarySortOrderForTest());

  size_t colour_reorder_command_index = 1u;
  EXPECT_EQ(submenu->GetCommandIdAt(colour_reorder_command_index),
            ash::REORDER_BY_COLOR);

  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_APP_LIST_CONTEXT_MENU_REORDER_BY_COLOR),
      submenu->GetLabelAt(colour_reorder_command_index));
  EXPECT_EQ(&ash::kSortColorIcon,
            submenu->GetIconAt(colour_reorder_command_index)
                .GetVectorIcon()
                .vector_icon());
  submenu->ActivatedAt(colour_reorder_command_index);
  EXPECT_EQ(ash::AppListSortOrder::kColor,
            chrome_model_updater->GetTemporarySortOrderForTest());
}

}  // namespace apps
