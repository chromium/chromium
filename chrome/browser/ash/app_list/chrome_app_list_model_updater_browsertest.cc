// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>
#include <string>
#include <vector>

#include "ash/app_list/model/app_list_item.h"
#include "ash/app_list/model/app_list_model.h"
#include "ash/constants/web_app_id_constants.h"
#include "ash/public/cpp/accelerators.h"
#include "ash/public/cpp/shelf_model.h"
#include "ash/public/cpp/test/app_list_test_api.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/app_list/app_list_client_impl.h"
#include "chrome/browser/ash/app_list/app_list_model_updater.h"
#include "chrome/browser/ash/app_list/app_list_syncable_service_factory.h"
#include "chrome/browser/ash/app_list/chrome_app_list_item.h"
#include "chrome/browser/ash/app_list/test/chrome_app_list_test_support.h"
#include "chrome/browser/ash/login/login_manager_test.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "components/account_id/account_id.h"
#include "components/app_constants/constants.h"
#include "components/session_manager/core/session_manager.h"
#include "components/sync/model/string_ordinal.h"
#include "components/sync/test/fake_sync_change_processor.h"
#include "components/user_manager/fake_user_manager.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/extension_system.h"

namespace {

constexpr char kOemAppId[] = "emfkafnhnpcmabnnkckkchdilgeoekbo";

// Gets list of item app IDs in order they appear in shelf model. Only items
// contained in `filter` will be included.
std::vector<std::string> GetOrderedShelfItems(
    const std::set<std::string>& filter) {
  std::vector<std::string> result;
  for (const auto& item : ash::ShelfModel::Get()->items()) {
    if (base::Contains(filter, item.id.app_id))
      result.push_back(item.id.app_id);
  }
  return result;
}

}  // namespace

class OemAppPositionTest : public ash::LoginManagerTest {
 public:
  OemAppPositionTest() { login_mixin_.AppendRegularUsers(1); }
  OemAppPositionTest(const OemAppPositionTest&) = delete;
  OemAppPositionTest& operator=(const OemAppPositionTest&) = delete;
  ~OemAppPositionTest() override = default;

  // LoginManagerTest:
  bool SetUpUserDataDirectory() override {
    // Create test user profile directory and copy extensions and preferences
    // from the test data directory to it.
    base::FilePath user_data_dir;
    base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
    const std::string user_id_hash =
        user_manager::FakeUserManager::GetFakeUsernameHash(
            login_mixin_.users()[0].account_id);
    const base::FilePath user_profile_path = user_data_dir.Append(
        ash::ProfileHelper::GetUserProfileDir(user_id_hash));
    base::CreateDirectory(user_profile_path);

    base::FilePath src_dir;
    base::PathService::Get(chrome::DIR_TEST_DATA, &src_dir);
    src_dir = src_dir.AppendASCII("extensions").AppendASCII("app_list_oem");

    base::CopyFile(src_dir.Append(chrome::kPreferencesFilename),
                   user_profile_path.Append(chrome::kPreferencesFilename));
    base::CopyDirectory(src_dir.AppendASCII("Extensions"), user_profile_path,
                        true);
    return true;
  }

  ash::LoginManagerMixin login_mixin_{&mixin_host_};
};

class ChromeAppListModelUpdaterTest : public extensions::ExtensionBrowserTest {
 public:
  ChromeAppListModelUpdaterTest() = default;
  ~ChromeAppListModelUpdaterTest() override = default;
  ChromeAppListModelUpdaterTest(const ChromeAppListModelUpdaterTest& other) =
      delete;
  ChromeAppListModelUpdaterTest& operator=(
      const ChromeAppListModelUpdaterTest& other) = delete;

 protected:
  void SetUpOnMainThread() override {
    ExtensionBrowserTest::SetUpOnMainThread();
    AppListClientImpl* client = AppListClientImpl::GetInstance();
    ASSERT_TRUE(client);
    client->UpdateProfile();

    // Ensure async callbacks are run.
    base::RunLoop().RunUntilIdle();
  }

  void ShowAppList() {
    ash::AcceleratorController::Get()->PerformActionIfEnabled(
        ash::AcceleratorAction::kToggleAppList, {});
    app_list_test_api_.WaitForBubbleWindow(
        /*wait_for_opening_animation=*/false);
  }

  ash::AppListTestApi app_list_test_api_;
};

// Tests that an Oem app and its folder are created with valid positions after
// sign-in.
IN_PROC_BROWSER_TEST_F(OemAppPositionTest, ValidOemAppPosition) {
  LoginUser(login_mixin_.users()[0].account_id);

  AppListClientImpl* client = AppListClientImpl::GetInstance();
  ASSERT_TRUE(client);
  client->UpdateProfile();
  AppListModelUpdater* model_updater = test::GetModelUpdater(client);

  // Ensure async callbacks are run.
  base::RunLoop().RunUntilIdle();

  const ChromeAppListItem* oem_app = model_updater->FindItem(kOemAppId);
  ASSERT_TRUE(oem_app);
  EXPECT_TRUE(oem_app->position().IsValid());

  const ChromeAppListItem* oem_folder =
      model_updater->FindItem(ash::kOemFolderId);
  ASSERT_TRUE(oem_folder);
  EXPECT_TRUE(oem_folder->position().IsValid());
}

IN_PROC_BROWSER_TEST_F(ChromeAppListModelUpdaterTest,
                       GetPositionBeforeFirstItemTest) {
  AppListClientImpl* client = AppListClientImpl::GetInstance();
  ASSERT_TRUE(client);
  AppListModelUpdater* model_updater = test::GetModelUpdater(client);

  // Default apps will be present but we add an app to guarantee there will be
  // at least 1 app.
  const std::string app1_id =
      LoadExtension(test_data_dir_.AppendASCII("app1"))->id();
  ASSERT_FALSE(app1_id.empty());

  // Create the app list view and show the apps grid.
  ShowAppList();

  std::vector<std::string> top_level_id_list =
      app_list_test_api_.GetTopLevelViewIdList();
  syncer::StringOrdinal position_before_first_item =
      model_updater->GetPositionBeforeFirstItem();

  // Check that position is before all items in the list.
  for (const auto& id : top_level_id_list) {
    ChromeAppListItem* item = model_updater->FindItem(id);
    ASSERT_TRUE(position_before_first_item.LessThan(item->position()));
  }

  // Move app to the front.
  app_list_test_api_.MoveItemToPosition(app1_id, 0);

  std::vector<std::string> reordered_top_level_id_list =
      app_list_test_api_.GetTopLevelViewIdList();
  syncer::StringOrdinal new_position_before_first_item =
      model_updater->GetPositionBeforeFirstItem();

  // Re-check that position is before all items in the list.
  for (const auto& id : reordered_top_level_id_list) {
    ChromeAppListItem* item = model_updater->FindItem(id);
    ASSERT_TRUE(new_position_before_first_item.LessThan(item->position()));
  }
}

IN_PROC_BROWSER_TEST_F(ChromeAppListModelUpdaterTest,
                       PRE_ReorderAppPositionInTopLevelAppList) {
  const std::string app1_id =
      LoadExtension(test_data_dir_.AppendASCII("app1"))->id();
  ASSERT_FALSE(app1_id.empty());
  const std::string app2_id =
      LoadExtension(test_data_dir_.AppendASCII("app2"))->id();
  ASSERT_FALSE(app2_id.empty());
  // App3 is the same app as app1 in |test_data_dir_|. Take app4 as the third
  // app in this test.
  const std::string app3_id =
      LoadExtension(test_data_dir_.AppendASCII("app4"))->id();
  ASSERT_FALSE(app3_id.empty());

  // Create the app list view and show the apps grid.
  ShowAppList();

  std::vector<std::string> top_level_id_list =
      app_list_test_api_.GetTopLevelViewIdList();
  size_t top_level_id_list_size = top_level_id_list.size();

  // This test ignores the default apps and we don't test the exact
  // |top_level_id_list| size here.
  ASSERT_GE(top_level_id_list_size, 3u);

  ASSERT_EQ(top_level_id_list[2], app1_id);
  ASSERT_EQ(top_level_id_list[1], app2_id);
  ASSERT_EQ(top_level_id_list[0], app3_id);

  // After the move operation, app3 should be at index 0 and app1 should be at
  // index 1. App2 stays at the last position in the item list.
  app_list_test_api_.MoveItemToPosition(app1_id, 0);
  app_list_test_api_.MoveItemToPosition(app3_id, 0);

  std::vector<std::string> reordered_top_level_id_list =
      app_list_test_api_.GetTopLevelViewIdList();

  EXPECT_EQ(top_level_id_list_size, reordered_top_level_id_list.size());
  EXPECT_EQ(reordered_top_level_id_list[0], app3_id);
  EXPECT_EQ(reordered_top_level_id_list[1], app1_id);
  EXPECT_EQ(reordered_top_level_id_list[2], app2_id);
}

// Tests if the app position changed in the top level persist after the system
// restarts.
IN_PROC_BROWSER_TEST_F(ChromeAppListModelUpdaterTest,
                       ReorderAppPositionInTopLevelAppList) {
  // Create the app list view and show the apps grid.
  ShowAppList();

  const std::string app1_id =
      GetExtensionByPath(extension_registry()->enabled_extensions(),
                         test_data_dir_.AppendASCII("app1"))
          ->id();
  const std::string app2_id =
      GetExtensionByPath(extension_registry()->enabled_extensions(),
                         test_data_dir_.AppendASCII("app2"))
          ->id();
  const std::string app3_id =
      GetExtensionByPath(extension_registry()->enabled_extensions(),
                         test_data_dir_.AppendASCII("app4"))
          ->id();

  std::vector<std::string> reordered_top_level_id_list =
      app_list_test_api_.GetTopLevelViewIdList();

  // This test ignores the default apps and we don't test the exact
  // |reordered_top_level_id_list| size here.
  ASSERT_GE(reordered_top_level_id_list.size(), 3u);

  EXPECT_EQ(reordered_top_level_id_list[0], app3_id);
  EXPECT_EQ(reordered_top_level_id_list[1], app1_id);
  EXPECT_EQ(reordered_top_level_id_list[2], app2_id);
}

IN_PROC_BROWSER_TEST_F(ChromeAppListModelUpdaterTest,
                       PRE_ReorderAppPositionInFolder) {
  const std::string app1_id =
      LoadExtension(test_data_dir_.AppendASCII("app1"))->id();
  ASSERT_FALSE(app1_id.empty());
  const std::string app2_id =
      LoadExtension(test_data_dir_.AppendASCII("app2"))->id();
  ASSERT_FALSE(app2_id.empty());
  // App3 is the same app as app1 in |test_data_dir_|. Take app4 as the third
  // app in this test.
  const std::string app3_id =
      LoadExtension(test_data_dir_.AppendASCII("app4"))->id();
  ASSERT_FALSE(app3_id.empty());

  // Create the app list view and show the apps grid.
  ShowAppList();

  // Create a folder with app1, app2 and app3 in order.
  const std::string folder_id =
      app_list_test_api_.CreateFolderWithApps({app1_id, app2_id, app3_id});

  std::vector<std::string> original_id_list{app1_id, app2_id, app3_id};
  ASSERT_EQ(app_list_test_api_.GetAppIdsInFolder(folder_id), original_id_list);

  // Change an app position in the folder.
  app_list_test_api_.MoveItemToPosition(app1_id, 2);

  std::vector<std::string> reordered_id_list{app2_id, app3_id, app1_id};
  EXPECT_EQ(app_list_test_api_.GetAppIdsInFolder(folder_id), reordered_id_list);
}

// Tests if the app position changed in a folder persist after the system
// restarts.
IN_PROC_BROWSER_TEST_F(ChromeAppListModelUpdaterTest,
                       ReorderAppPositionInFolder) {
  const std::string app1_id =
      GetExtensionByPath(extension_registry()->enabled_extensions(),
                         test_data_dir_.AppendASCII("app1"))
          ->id();
  const std::string app2_id =
      GetExtensionByPath(extension_registry()->enabled_extensions(),
                         test_data_dir_.AppendASCII("app2"))
          ->id();
  const std::string app3_id =
      GetExtensionByPath(extension_registry()->enabled_extensions(),
                         test_data_dir_.AppendASCII("app4"))
          ->id();

  std::string folder_id = app_list_test_api_.GetFolderId(app1_id);
  // Check if the three apps are still in the same folder.
  ASSERT_FALSE(folder_id.empty());
  ASSERT_EQ(app_list_test_api_.GetFolderId(app2_id), folder_id);
  ASSERT_EQ(app_list_test_api_.GetFolderId(app3_id), folder_id);

  std::vector<std::string> reordered_id_list{app2_id, app3_id, app1_id};
  EXPECT_EQ(app_list_test_api_.GetAppIdsInFolder(folder_id), reordered_id_list);
}

IN_PROC_BROWSER_TEST_F(ChromeAppListModelUpdaterTest,
                       PRE_UnmergeTwoItemFolder) {
  const std::string app1_id =
      LoadExtension(test_data_dir_.AppendASCII("app1"))->id();
  ASSERT_FALSE(app1_id.empty());
  const std::string app2_id =
      LoadExtension(test_data_dir_.AppendASCII("app2"))->id();
  ASSERT_FALSE(app2_id.empty());
  // App3 is the same app as app1 in |test_data_dir_|. Take app4 as the third
  // app in this test.
  const std::string app3_id =
      LoadExtension(test_data_dir_.AppendASCII("app4"))->id();
  ASSERT_FALSE(app3_id.empty());

  // Create the app list view and show the apps grid.
  ShowAppList();

  // Create a folder with app1, app2 and app3 in order.
  const std::string folder_id =
      app_list_test_api_.CreateFolderWithApps({app1_id, app2_id});

  ash::AppListModel* model = app_list_test_api_.GetAppListModel();
  ash::AppListItem* app1_item = model->FindItem(app1_id);
  ASSERT_TRUE(app1_item);

  ash::AppListItem* app2_item = model->FindItem(app2_id);
  ASSERT_TRUE(app2_item);

  ash::AppListItem* app3_item = model->FindItem(app3_id);
  ASSERT_TRUE(app3_item);

  model->MoveItemToRootAt(app2_item, app3_item->position().CreateBefore());

  // Get last 3 items (the grid may have default items, in addition to the ones
  // installed by the test).
  std::vector<std::string> top_level_id_list =
      app_list_test_api_.GetTopLevelViewIdList();
  ASSERT_GT(top_level_id_list.size(), 2u);
  EXPECT_TRUE(base::Contains(top_level_id_list, folder_id));
  model->MoveItemToRootAt(app1_item, app2_item->position().CreateBefore());

  top_level_id_list = app_list_test_api_.GetTopLevelViewIdList();
  EXPECT_FALSE(base::Contains(top_level_id_list, folder_id));

  std::vector<std::string> leading_items = {
      top_level_id_list[0],
      top_level_id_list[1],
      top_level_id_list[2],
  };

  EXPECT_EQ(std::vector<std::string>({app1_id, app2_id, app3_id}),
            leading_items);
}

IN_PROC_BROWSER_TEST_F(ChromeAppListModelUpdaterTest, UnmergeTwoItemFolder) {
  const std::string app1_id =
      GetExtensionByPath(extension_registry()->enabled_extensions(),
                         test_data_dir_.AppendASCII("app1"))
          ->id();
  const std::string app2_id =
      GetExtensionByPath(extension_registry()->enabled_extensions(),
                         test_data_dir_.AppendASCII("app2"))
          ->id();
  const std::string app3_id =
      GetExtensionByPath(extension_registry()->enabled_extensions(),
                         test_data_dir_.AppendASCII("app4"))
          ->id();

  // Create the app list view and show the apps grid.
  ShowAppList();

  // Get last 3 items (the grid may have default items, in addition to the ones
  // installed by the test).
  std::vector<std::string> top_level_id_list =
      app_list_test_api_.GetTopLevelViewIdList();
  ASSERT_GT(top_level_id_list.size(), 2u);

  std::vector<std::string> leading_items = {
      top_level_id_list[0],
      top_level_id_list[1],
      top_level_id_list[2],
  };

  EXPECT_EQ(std::vector<std::string>({app1_id, app2_id, app3_id}),
            leading_items);
}

// Tests that session restart before a default pinned preinstalled app is
// correctly positioned in the app list if the session restarts before the app
// installation completes.
IN_PROC_BROWSER_TEST_F(ChromeAppListModelUpdaterTest,
                       PRE_SessionRestartDoesntOverrideDefaultAppListPosition) {
  // Simluate installation of an app pinned to shelf by default:
  // App with web_app::kGmailAppId ID.
  auto gmail_info = web_app::WebAppInstallInfo::CreateWithStartUrlForTesting(
      GURL("https://mail.google.com/mail/?usp=installed_webapp"));
  gmail_info->display_mode = blink::mojom::DisplayMode::kMinimalUi;
  web_app::test::InstallWebApp(profile(), std::move(gmail_info));

  std::set<std::string> app_filter({app_constants::kChromeAppId,
                                    web_app::kGmailAppId,
                                    web_app::kMessagesAppId});
  EXPECT_EQ(std::vector<std::string>(
                {app_constants::kChromeAppId, web_app::kGmailAppId}),
            GetOrderedShelfItems(app_filter));
}

IN_PROC_BROWSER_TEST_F(ChromeAppListModelUpdaterTest,
                       SessionRestartDoesntOverrideDefaultAppListPosition) {
  app_list::AppListSyncableService* app_list_syncable_service =
      app_list::AppListSyncableServiceFactory::GetForProfile(profile());
  AppListModelUpdater* app_list_model_updater =
      app_list_syncable_service->GetModelUpdater();
  app_list_model_updater->SetActive(true);

  app_list_syncable_service->MergeDataAndStartSyncing(
      syncer::APP_LIST, syncer::SyncDataList(),
      std::make_unique<syncer::FakeSyncChangeProcessor>());

  // Simluate installation of an app pinned to shelf by default after initial
  // sync data is merged: app with web_app::kMessagesAppId ID.
  auto messages_info = web_app::WebAppInstallInfo::CreateWithStartUrlForTesting(
      GURL("https://messages.google.com/web/"));
  messages_info->display_mode = blink::mojom::DisplayMode::kMinimalUi;
  web_app::test::InstallWebApp(profile(), std::move(messages_info));

  std::set<std::string> app_filter({app_constants::kChromeAppId,
                                    web_app::kGmailAppId,
                                    web_app::kMessagesAppId});
  EXPECT_EQ(
      std::vector<std::string>({app_constants::kChromeAppId,
                                web_app::kGmailAppId, web_app::kMessagesAppId}),
      GetOrderedShelfItems(app_filter));

  // Verify that order of apps in the app list respects default app ordinals
  // (for test apps that have default app list ordinal set).
  ShowAppList();
  std::vector<std::string> top_level_id_list =
      app_list_test_api_.GetTopLevelViewIdList();
  std::vector<std::string> filtered_top_level_id_list;
  for (const auto& item : top_level_id_list) {
    if (base::Contains(app_filter, item))
      filtered_top_level_id_list.push_back(item);
  }

  EXPECT_EQ(
      std::vector<std::string>({app_constants::kChromeAppId,
                                web_app::kGmailAppId, web_app::kMessagesAppId}),
      filtered_top_level_id_list);
}

IN_PROC_BROWSER_TEST_F(ChromeAppListModelUpdaterTest,
                       PRE_UserAppsInEphemeralFoldersMovedToRootAfterRestart) {
  // Install 2 apps.
  const std::string app1_id =
      LoadExtension(test_data_dir_.AppendASCII("app1"))->id();
  ASSERT_FALSE(app1_id.empty());
  const std::string app2_id =
      LoadExtension(test_data_dir_.AppendASCII("app2"))->id();
  ASSERT_FALSE(app2_id.empty());

  // Create the app list view and show the apps grid.
  ShowAppList();

  app_list::AppListSyncableService* app_list_syncable_service =
      app_list::AppListSyncableServiceFactory::GetForProfile(profile());
  AppListModelUpdater* app_list_model_updater =
      app_list_syncable_service->GetModelUpdater();
  app_list_model_updater->SetActive(true);

  // Create ephemeral folder.
  const std::string kEphemeralFolderId = "ephemeral_folder_id";
  syncer::StringOrdinal position =
      syncer::StringOrdinal::CreateInitialOrdinal();
  std::unique_ptr<ChromeAppListItem> ephemeral_folder_item =
      std::make_unique<ChromeAppListItem>(profile(), kEphemeralFolderId,
                                          app_list_model_updater);
  ephemeral_folder_item->SetChromeIsFolder(true);
  ephemeral_folder_item->SetChromeName("Folder");
  ephemeral_folder_item->SetIsSystemFolder(true);
  ephemeral_folder_item->SetIsEphemeral(true);
  app_list_syncable_service->AddItem(std::move(ephemeral_folder_item));

  ash::AppListModel* model = app_list_test_api_.GetAppListModel();

  ash::AppListItem* folder_item = model->FindItem(kEphemeralFolderId);
  ASSERT_TRUE(folder_item);
  EXPECT_TRUE(folder_item->GetMetadata()->is_ephemeral);

  // Move apps to ephemeral folder.
  ash::AppListItem* app1_item = model->FindItem(app1_id);
  ASSERT_TRUE(app1_item);
  model->MoveItemToFolder(app1_item, kEphemeralFolderId);

  ash::AppListItem* app2_item = model->FindItem(app2_id);
  ASSERT_TRUE(app2_item);
  model->MoveItemToFolder(app2_item, kEphemeralFolderId);

  // User apps have the ephemeral folder as a parent.
  app1_item = model->FindItem(app1_id);
  ASSERT_TRUE(app1_item);
  EXPECT_EQ(app1_item->folder_id(), kEphemeralFolderId);

  app2_item = model->FindItem(app2_id);
  ASSERT_TRUE(app2_item);
  EXPECT_EQ(app2_item->folder_id(), kEphemeralFolderId);
}

IN_PROC_BROWSER_TEST_F(ChromeAppListModelUpdaterTest,
                       UserAppsInEphemeralFoldersMovedToRootAfterRestart) {
  // Create the app list view and show the apps grid.
  ShowAppList();

  const std::string app1_id =
      GetExtensionByPath(extension_registry()->enabled_extensions(),
                         test_data_dir_.AppendASCII("app1"))
          ->id();
  const std::string app2_id =
      GetExtensionByPath(extension_registry()->enabled_extensions(),
                         test_data_dir_.AppendASCII("app2"))
          ->id();

  ash::AppListModel* model = app_list_test_api_.GetAppListModel();

  // Ephemeral folder was not synced.
  ash::AppListItem* folder_item = model->FindItem("ephemeral_folder_id");
  ASSERT_FALSE(folder_item);

  // User apps are moved to root after restart.
  ash::AppListItem* app1_item = model->FindItem(app1_id);
  ASSERT_TRUE(app1_item);
  EXPECT_FALSE(app1_item->IsInFolder());

  ash::AppListItem* app2_item = model->FindItem(app2_id);
  ASSERT_TRUE(app2_item);
  EXPECT_FALSE(app2_item->IsInFolder());
}

IN_PROC_BROWSER_TEST_F(ChromeAppListModelUpdaterTest, IsNewInstall) {
  AppListClientImpl* client = AppListClientImpl::GetInstance();
  ASSERT_TRUE(client);
  AppListModelUpdater* model_updater = test::GetModelUpdater(client);
  ASSERT_TRUE(model_updater);

  // The built-in "Web Store" app is not a new install.
  ChromeAppListItem* web_store_item =
      model_updater->FindItem(extensions::kWebStoreAppId);
  ASSERT_TRUE(web_store_item);
  EXPECT_FALSE(web_store_item->CloneMetadata()->is_new_install);

  // Install 2 apps.
  const std::string app1_id =
      LoadExtension(test_data_dir_.AppendASCII("app1"))->id();
  ASSERT_FALSE(app1_id.empty());
  const std::string app2_id =
      LoadExtension(test_data_dir_.AppendASCII("app2"))->id();
  ASSERT_FALSE(app2_id.empty());

  // Both apps are new installs.
  ChromeAppListItem* item1 = model_updater->FindItem(app1_id);
  ASSERT_TRUE(item1);
  EXPECT_TRUE(item1->CloneMetadata()->is_new_install);

  ChromeAppListItem* item2 = model_updater->FindItem(app2_id);
  ASSERT_TRUE(item2);
  EXPECT_TRUE(item2->CloneMetadata()->is_new_install);

  // Launch the first app.
  item1->Activate(ui::EF_NONE);

  // First app is no longer a new install.
  EXPECT_FALSE(item1->CloneMetadata()->is_new_install);

  // Second app is still a new install.
  EXPECT_TRUE(item2->CloneMetadata()->is_new_install);
}

IN_PROC_BROWSER_TEST_F(ChromeAppListModelUpdaterTest, IsNewInstallInFolder) {
  AppListClientImpl* client = AppListClientImpl::GetInstance();
  ASSERT_TRUE(client);
  AppListModelUpdater* model_updater = test::GetModelUpdater(client);
  ASSERT_TRUE(model_updater);

  // Install 2 apps.
  const std::string app1_id =
      LoadExtension(test_data_dir_.AppendASCII("app1"))->id();
  ASSERT_FALSE(app1_id.empty());
  const std::string app2_id =
      LoadExtension(test_data_dir_.AppendASCII("app2"))->id();
  ASSERT_FALSE(app2_id.empty());

  ShowAppList();

  // Put the apps in a folder.
  const std::string folder_id =
      app_list_test_api_.CreateFolderWithApps({app1_id, app2_id});

  // Both apps are new installs.
  ash::AppListModel* model = app_list_test_api_.GetAppListModel();
  ash::AppListItem* app1_item = model->FindItem(app1_id);
  ASSERT_TRUE(app1_item);
  EXPECT_TRUE(app1_item->is_new_install());

  ash::AppListItem* app2_item = model->FindItem(app2_id);
  ASSERT_TRUE(app2_item);
  EXPECT_TRUE(app2_item->is_new_install());

  // The folder is considered a "new install" because it contains an item that
  // is a new install.
  ash::AppListItem* folder_item = model->FindItem(folder_id);
  ASSERT_TRUE(folder_item);
  EXPECT_TRUE(folder_item->is_new_install());

  // Launching one item clears its new install status, but the folder still
  // contains a new install.
  model_updater->FindItem(app1_id)->Activate(ui::EF_NONE);
  EXPECT_FALSE(app1_item->is_new_install());
  EXPECT_TRUE(app2_item->is_new_install());
  EXPECT_TRUE(folder_item->is_new_install());

  // Launching them other item clears its new install status, and the folder
  // no longer contains a new install.
  model_updater->FindItem(app2_id)->Activate(ui::EF_NONE);
  EXPECT_FALSE(app1_item->is_new_install());
  EXPECT_FALSE(app2_item->is_new_install());
  EXPECT_FALSE(folder_item->is_new_install());
}
