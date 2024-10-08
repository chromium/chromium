// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/app_list_syncable_service.h"

#include <algorithm>
#include <utility>

#include "ash/app_list/model/app_list_item.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/web_app_id_constants.h"
#include "ash/public/cpp/app_list/app_list_config.h"
#include "base/containers/to_vector.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "cc/base/math_util.h"
#include "chrome/browser/apps/app_preload_service/app_preload_service.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/app_list/app_list_client_impl.h"
#include "chrome/browser/ash/app_list/app_list_model_updater.h"
#include "chrome/browser/ash/app_list/app_list_test_util.h"
#include "chrome/browser/ash/app_list/chrome_app_list_item.h"
#include "chrome/browser/ash/app_list/chrome_app_list_model_updater.h"
#include "chrome/browser/ash/app_list/reorder/app_list_reorder_core.h"
#include "chrome/browser/ash/app_list/test/app_list_syncable_service_test_base.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/app_constants/constants.h"
#include "components/crx_file/id_util.h"
#include "components/sync/protocol/app_list_specifics.pb.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/test/fake_sync_change_processor.h"
#include "components/sync/test/sync_change_processor_wrapper_for_test.h"
#include "extensions/common/constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

using crx_file::id_util::GenerateId;
using testing::ElementsAre;
using testing::ElementsAreArray;
using ItemTestApi = ChromeAppListItem::TestApi;

namespace app_list {

namespace {

const char kOsSettingsUrl[] = "chrome://os-settings/";

// These constants are defined as functions so their values can be derived via
// function calls.  The constant naming scheme is kept to maintain readability.
const std::string kInvalidOrdinalsId() {
  return GenerateId("invalid_ordinals");
}
const std::string kEmptyItemNameId() {
  return GenerateId("empty_item_name");
}
const std::string kEmptyItemNameUnsetId() {
  return GenerateId("empty_item_name_unset");
}
const std::string kEmptyParentId() {
  return GenerateId("empty_parent_id");
}
const std::string kEmptyParentUnsetId() {
  return GenerateId("empty_parent_id_unset");
}
const std::string kEmptyOrdinalsId() {
  return GenerateId("empty_ordinals");
}
const std::string kEmptyOrdinalsUnsetId() {
  return GenerateId("empty_ordinals_unset");
}
const std::string kDupeItemId() {
  return GenerateId("dupe_item_id");
}
const std::string kParentId() {
  return GenerateId("parent_id");
}
const std::string kEmptyPromisePackageId() {
  return GenerateId("empty_package_id");
}
const std::string kEmptyPromisePackageUnsetId() {
  return GenerateId("unset_package_id");
}

syncer::SyncDataList CreateBadAppRemoteData(const std::string& id) {
  syncer::SyncDataList sync_list;
  // Invalid item_ordinal and item_pin_ordinal.
  sync_list.push_back(CreateAppRemoteData(
      id == kDefault ? kInvalidOrdinalsId() : id, "item_name", kParentId(),
      "$$invalid_ordinal$$", "$$invalid_ordinal$$"));
  // Empty item name.
  sync_list.push_back(
      CreateAppRemoteData(id == kDefault ? kEmptyItemNameId() : id, "",
                          kParentId(), "ordinal", "pinordinal"));
  sync_list.push_back(
      CreateAppRemoteData(id == kDefault ? kEmptyItemNameUnsetId() : id, kUnset,
                          kParentId(), "ordinal", "pinordinal"));
  // Empty parent ID.
  sync_list.push_back(
      CreateAppRemoteData(id == kDefault ? kEmptyParentId() : id, "item_name",
                          "", "ordinal", "pinordinal"));
  sync_list.push_back(
      CreateAppRemoteData(id == kDefault ? kEmptyParentUnsetId() : id,
                          "item_name", kUnset, "ordinal", "pinordinal"));
  // Empty item_ordinal and item_pin_ordinal.
  sync_list.push_back(
      CreateAppRemoteData(id == kDefault ? kEmptyOrdinalsId() : id, "item_name",
                          kParentId(), "", ""));
  sync_list.push_back(
      CreateAppRemoteData(id == kDefault ? kEmptyOrdinalsUnsetId() : id,
                          "item_name", kParentId(), kUnset, kUnset));
  // Duplicate item_id.
  sync_list.push_back(CreateAppRemoteData(id == kDefault ? kDupeItemId() : id,
                                          "item_name", kParentId(), "ordinal",
                                          "pinordinal"));
  sync_list.push_back(CreateAppRemoteData(id == kDefault ? kDupeItemId() : id,
                                          "item_name_dupe", kParentId(),
                                          "ordinal", "pinordinal"));
  // Empty item_id.
  sync_list.push_back(CreateAppRemoteData("", "item_name", kParentId(),
                                          "ordinal", "pinordinal"));
  sync_list.push_back(CreateAppRemoteData(kUnset, "item_name", kParentId(),
                                          "ordinal", "pinordinal"));
  // Empty promise_package_id.
  sync_list.push_back(
      CreateAppRemoteData(id == kDefault ? kEmptyPromisePackageId() : id,
                          "item_name", kParentId(), "ordinal", "pinordinal",
                          sync_pb::AppListSpecifics_AppListItemType_TYPE_APP,
                          /*is_user_pinned=*/false, /*promise_package_id=*/""));
  sync_list.push_back(CreateAppRemoteData(
      id == kDefault ? kEmptyPromisePackageUnsetId() : id, "item_name",
      kParentId(), "ordinal", "pinordinal",
      sync_pb::AppListSpecifics_AppListItemType_TYPE_APP,
      /*is_user_pinned=*/false, /*promise_package_id=*/kUnset));

  // All fields empty.
  sync_list.push_back(CreateAppRemoteData(
      "", "", "", "", "", sync_pb::AppListSpecifics_AppListItemType_TYPE_APP,
      std::nullopt, ""));
  sync_list.push_back(
      CreateAppRemoteData(kUnset, kUnset, kUnset, kUnset, kUnset,
                          sync_pb::AppListSpecifics_AppListItemType_TYPE_APP,
                          std::nullopt, kUnset));

  return sync_list;
}

bool AreAllAppAtributesEqualInSync(
    const AppListSyncableService::SyncItem* item1,
    const AppListSyncableService::SyncItem* item2) {
  return item1->parent_id == item2->parent_id &&
         item1->item_ordinal.EqualsOrBothInvalid(item2->item_ordinal) &&
         item1->item_pin_ordinal.EqualsOrBothInvalid(item2->item_pin_ordinal);
}

bool AreAllAppAtributesNotEqualInSync(
    const AppListSyncableService::SyncItem* item1,
    const AppListSyncableService::SyncItem* item2) {
  return item1->parent_id != item2->parent_id &&
         !item1->item_ordinal.EqualsOrBothInvalid(item2->item_ordinal) &&
         !item1->item_pin_ordinal.EqualsOrBothInvalid(item2->item_pin_ordinal);
}

bool AreAllAppAtributesEqualInAppList(const ChromeAppListItem* item1,
                                      const ChromeAppListItem* item2) {
  // Note, there is no pin position in app list.
  return item1->folder_id() == item2->folder_id() &&
         item1->position().EqualsOrBothInvalid(item2->position());
}

bool AreAllAppAtributesNotEqualInAppList(const ChromeAppListItem* item1,
                                         const ChromeAppListItem* item2) {
  // Note, there is no pin position in app list.
  return item1->folder_id() != item2->folder_id() &&
         !item1->position().EqualsOrBothInvalid(item2->position());
}

std::string GetLastPositionString() {
  static syncer::StringOrdinal last_position;
  if (!last_position.IsValid())
    last_position = syncer::StringOrdinal::CreateInitialOrdinal();
  else
    last_position = last_position.CreateAfter();
  return last_position.ToDebugString();
}

}  // namespace

// The class that verifies app list syncable service features. Use a fake app
// list model updater during testing.
class AppListSyncableServiceTest : public test::AppListSyncableServiceTestBase {
 public:
  AppListSyncableServiceTest() {
    feature_list_.InitAndEnableFeature(
        ash::features::kRemoveStalePolicyPinnedAppsFromShelf);
  }
  AppListSyncableServiceTest(const AppListSyncableServiceTest&) = delete;
  AppListSyncableServiceTest& operator=(const AppListSyncableServiceTest&) =
      delete;
  ~AppListSyncableServiceTest() override = default;

  // test::AppListSyncableServiceTestBase:
  void SetUp() override {
    AppListSyncableServiceTestBase::SetUp();
    model_updater_test_api_ =
        std::make_unique<AppListModelUpdater::TestApi>(GetModelUpdater());
  }

  void TearDown() override { app_list_syncable_service_.reset(); }

  AppListModelUpdater::TestApi* model_updater_test_api() {
    return model_updater_test_api_.get();
  }

  // Returns the app list order stored as preference.
  ash::AppListSortOrder GetSortOrderFromPrefs() {
    return static_cast<ash::AppListSortOrder>(
        profile()->GetPrefs()->GetInteger(prefs::kAppListPreferredOrder));
  }

  ash::AppListItem* FindItemForApp(extensions::Extension* app) {
    return GetModelUpdater()->model_for_test()->FindItem(app->id());
  }

  // A hacky way to change an item's name.
  void ChangeItemName(const std::string& id, const std::string& new_name) {
    const_cast<AppListSyncableService::SyncItem*>(
        app_list_syncable_service()->GetSyncItem(id))
        ->item_name = new_name;
    app_list_syncable_service()->GetModelUpdater()->SetItemName(id, new_name);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<AppListModelUpdater::TestApi> model_updater_test_api_;
};

TEST_F(AppListSyncableServiceTest, OEMFolderForConflictingPos) {
  // Create a "web store" app.
  const std::string web_store_app_id(extensions::kWebStoreAppId);
  scoped_refptr<extensions::Extension> store =
      MakeApp("webstore", web_store_app_id,
              extensions::Extension::WAS_INSTALLED_BY_DEFAULT);
  InstallExtension(store.get());

  // Create some app. Note its id should be greater than web store app id in
  // order to move app in case of conflicting pos after web store app.
  const std::string test_app_1_id = CreateNextAppId(extensions::kWebStoreAppId);
  scoped_refptr<extensions::Extension> test_app_1 =
      MakeApp(kSomeAppName, test_app_1_id,
              extensions ::Extension::WAS_INSTALLED_BY_DEFAULT);
  InstallExtension(test_app_1.get());

  const std::string test_app_2_id = CreateNextAppId(test_app_1_id);
  scoped_refptr<extensions::Extension> test_app_2 =
      MakeApp(kSomeAppName, test_app_2_id,
              extensions ::Extension::WAS_INSTALLED_BY_DEFAULT);
  InstallExtension(test_app_2.get());

  AppListModelUpdater* model_updater = GetModelUpdater();
  ChromeAppListItem* web_store_item = model_updater->FindItem(web_store_app_id);
  ASSERT_TRUE(web_store_item);
  ChromeAppListItem* test_app_1_item = model_updater->FindItem(test_app_1_id);
  ASSERT_TRUE(test_app_1_item);

  ChromeAppListItem* test_app_2_item = model_updater->FindItem(test_app_2_id);
  ASSERT_TRUE(test_app_2_item);
  // Simulate position conflict.
  model_updater_test_api()->SetItemPosition(web_store_item->id(),
                                            test_app_1_item->position());

  // Position second test app after the webstore and the first test app.
  model_updater_test_api()->SetItemPosition(
      test_app_2_item->id(), test_app_1_item->position().CreateAfter());

  // Install an OEM app. It must be placed by default after web store app but in
  // case of app of the same position should be shifted next.
  const std::string oem_app_id = CreateNextAppId(test_app_2_id);
  scoped_refptr<extensions::Extension> oem_app = MakeApp(
      kOemAppName, oem_app_id, extensions::Extension::WAS_INSTALLED_BY_OEM);
  InstallExtension(oem_app.get());

  // OEM item is not top level element.
  ChromeAppListItem* oem_app_item = model_updater->FindItem(oem_app_id);
  EXPECT_NE(nullptr, oem_app_item);
  EXPECT_EQ(oem_app_item->folder_id(), ash::kOemFolderId);

  // But OEM folder is.
  ChromeAppListItem* oem_folder = model_updater->FindItem(ash::kOemFolderId);
  ASSERT_NE(nullptr, oem_folder);
  EXPECT_EQ(oem_folder->folder_id(), "");

  EXPECT_TRUE(oem_folder->position().GreaterThan(web_store_item->position()));
  EXPECT_TRUE(oem_folder->position().GreaterThan(test_app_1_item->position()));
  EXPECT_TRUE(oem_folder->position().LessThan(test_app_2_item->position()));

  // Receiving initial sync data does not change the OEM folder position.
  app_list_syncable_service()->MergeDataAndStartSyncing(
      syncer::APP_LIST, syncer::SyncDataList(),
      std::make_unique<syncer::FakeSyncChangeProcessor>());
  content::RunAllTasksUntilIdle();

  EXPECT_TRUE(oem_folder->position().GreaterThan(web_store_item->position()));
  EXPECT_TRUE(oem_folder->position().GreaterThan(test_app_1_item->position()));
  EXPECT_TRUE(oem_folder->position().LessThan(test_app_2_item->position()));
}

// Verifies that OEM folder is positioned at the end of the list if initial sync
// data that contains non-default apps is received after an OEM data is
// received.
TEST_F(AppListSyncableServiceTest,
       OEMFolderPositionUpdatedAfterInitialSyncWithNonDefaultApps) {
  // Create a "web store" app.
  const std::string web_store_app_id(extensions::kWebStoreAppId);
  scoped_refptr<extensions::Extension> store =
      MakeApp("webstore", web_store_app_id,
              extensions::Extension::WAS_INSTALLED_BY_DEFAULT);
  InstallExtension(store.get());
  AppListModelUpdater* model_updater = GetModelUpdater();
  ChromeAppListItem* web_store_item = model_updater->FindItem(web_store_app_id);

  const std::string test_app_id = CreateNextAppId(extensions::kWebStoreAppId);
  scoped_refptr<extensions::Extension> test_app =
      MakeApp(kSomeAppName, test_app_id,
              extensions ::Extension::WAS_INSTALLED_BY_DEFAULT);
  InstallExtension(test_app.get());
  ChromeAppListItem* test_app_item = model_updater->FindItem(test_app_id);
  model_updater_test_api()->SetItemPosition(
      test_app_item->id(), web_store_item->position().CreateAfter());

  // Install an OEM app. It must be placed by default after web store app but in
  // case of app of the same position should be shifted next.
  const std::string oem_app_id = CreateNextAppId(test_app_id);
  scoped_refptr<extensions::Extension> oem_app = MakeApp(
      kOemAppName, oem_app_id, extensions::Extension::WAS_INSTALLED_BY_OEM);
  InstallExtension(oem_app.get());

  syncer::StringOrdinal sync_item_ordinal =
      test_app_item->position().CreateAfter();

  syncer::SyncDataList sync_list;
  sync_list.push_back(CreateAppRemoteData(
      "non-oem-app", "Non OEM app", std::string() /* parent_id */,
      sync_item_ordinal.ToInternalValue(),
      std::string() /* item_pin_ordinal */));
  app_list_syncable_service()->MergeDataAndStartSyncing(
      syncer::APP_LIST, sync_list,
      std::make_unique<syncer::FakeSyncChangeProcessor>());
  content::RunAllTasksUntilIdle();

  ChromeAppListItem* oem_folder = model_updater->FindItem(ash::kOemFolderId);
  ASSERT_NE(nullptr, oem_folder);
  EXPECT_EQ(oem_folder->folder_id(), "");

  EXPECT_TRUE(oem_folder->position().GreaterThan(web_store_item->position()));
  EXPECT_TRUE(oem_folder->position().GreaterThan(test_app_item->position()));
  EXPECT_TRUE(oem_folder->position().GreaterThan(sync_item_ordinal));
}

TEST_F(AppListSyncableServiceTest,
       OEMFolderPositionedAtEndIfNonDefaultAppsSynced) {
  const std::string web_store_app_id(extensions::kWebStoreAppId);
  scoped_refptr<extensions::Extension> store =
      MakeApp("webstore", web_store_app_id,
              extensions::Extension::WAS_INSTALLED_BY_DEFAULT);
  InstallExtension(store.get());
  AppListModelUpdater* model_updater = GetModelUpdater();
  ChromeAppListItem* web_store_item = model_updater->FindItem(web_store_app_id);

  const std::string test_app_id = CreateNextAppId(extensions::kWebStoreAppId);
  scoped_refptr<extensions::Extension> test_app =
      MakeApp(kSomeAppName, test_app_id,
              extensions ::Extension::WAS_INSTALLED_BY_DEFAULT);
  InstallExtension(test_app.get());
  ChromeAppListItem* test_app_item = model_updater->FindItem(test_app_id);
  model_updater_test_api()->SetItemPosition(
      test_app_item->id(), web_store_item->position().CreateAfter());

  syncer::StringOrdinal sync_item_ordinal =
      test_app_item->position().CreateAfter();

  syncer::SyncDataList sync_list;
  sync_list.push_back(CreateAppRemoteData(
      "non-oem-app", "Non OEM app", std::string() /* parent_id */,
      sync_item_ordinal.ToInternalValue(),
      std::string() /* item_pin_ordinal */));
  app_list_syncable_service()->MergeDataAndStartSyncing(
      syncer::APP_LIST, sync_list,
      std::make_unique<syncer::FakeSyncChangeProcessor>());
  content::RunAllTasksUntilIdle();

  // Install an OEM app. It must be placed by default after web store app but in
  // case of app of the same position should be shifted next.
  const std::string oem_app_id = CreateNextAppId(test_app_id);
  scoped_refptr<extensions::Extension> oem_app = MakeApp(
      kOemAppName, oem_app_id, extensions::Extension::WAS_INSTALLED_BY_OEM);
  InstallExtension(oem_app.get());

  ChromeAppListItem* oem_folder = model_updater->FindItem(ash::kOemFolderId);
  ASSERT_NE(nullptr, oem_folder);
  EXPECT_EQ(oem_folder->folder_id(), "");

  EXPECT_TRUE(oem_folder->position().GreaterThan(web_store_item->position()));
  EXPECT_TRUE(oem_folder->position().GreaterThan(test_app_item->position()));
  EXPECT_TRUE(oem_folder->position().GreaterThan(sync_item_ordinal));
}

TEST_F(AppListSyncableServiceTest,
       OEMFolderPositionedAfterWebstoreIfOnlyDefaultAppsSynced) {
  const std::string web_store_app_id(extensions::kWebStoreAppId);
  scoped_refptr<extensions::Extension> store =
      MakeApp("webstore", web_store_app_id,
              extensions::Extension::WAS_INSTALLED_BY_DEFAULT);
  InstallExtension(store.get());
  AppListModelUpdater* model_updater = GetModelUpdater();
  ChromeAppListItem* web_store_item = model_updater->FindItem(web_store_app_id);

  const std::string test_app_id = CreateNextAppId(extensions::kWebStoreAppId);
  scoped_refptr<extensions::Extension> test_app =
      MakeApp(kSomeAppName, test_app_id,
              extensions ::Extension::WAS_INSTALLED_BY_DEFAULT);
  InstallExtension(test_app.get());
  ChromeAppListItem* test_app_item = model_updater->FindItem(test_app_id);
  model_updater_test_api()->SetItemPosition(
      test_app_item->id(), web_store_item->position().CreateAfter());

  app_list_syncable_service()->MergeDataAndStartSyncing(
      syncer::APP_LIST, syncer::SyncDataList(),
      std::make_unique<syncer::FakeSyncChangeProcessor>());
  content::RunAllTasksUntilIdle();

  const std::string oem_app_id = CreateNextAppId(test_app_id);
  scoped_refptr<extensions::Extension> oem_app = MakeApp(
      kOemAppName, oem_app_id, extensions::Extension::WAS_INSTALLED_BY_OEM);
  InstallExtension(oem_app.get());

  ChromeAppListItem* oem_folder = model_updater->FindItem(ash::kOemFolderId);
  ASSERT_NE(nullptr, oem_folder);
  EXPECT_EQ(oem_folder->folder_id(), "");

  EXPECT_TRUE(oem_folder->position().GreaterThan(web_store_item->position()));
  EXPECT_TRUE(oem_folder->position().LessThan(test_app_item->position()));
}

// Verifies that OEM item preserves parent and doesn't change parent in case
// sync change says this.
TEST_F(AppListSyncableServiceTest, OEMItemIgnoreSyncParent) {
  const std::string oem_app_id = CreateNextAppId(extensions::kWebStoreAppId);
  scoped_refptr<extensions::Extension> oem_app = MakeApp(
      kOemAppName, oem_app_id, extensions::Extension::WAS_INSTALLED_BY_OEM);
  InstallExtension(oem_app.get());

  // OEM item is not top level element.
  AppListModelUpdater* model_updater = GetModelUpdater();
  ChromeAppListItem* oem_app_item = model_updater->FindItem(oem_app_id);
  ASSERT_TRUE(oem_app_item);
  EXPECT_EQ(ash::kOemFolderId, oem_app_item->folder_id());

  // Send sync that OEM app is top-level item.
  syncer::SyncDataList sync_list;
  sync_list.push_back(CreateAppRemoteData(
      oem_app_id, kOemAppName, std::string() /* parent_id */,
      oem_app_item->position().ToInternalValue(),
      std::string() /* item_pin_ordinal */));
  app_list_syncable_service()->MergeDataAndStartSyncing(
      syncer::APP_LIST, sync_list,
      std::make_unique<syncer::FakeSyncChangeProcessor>());
  content::RunAllTasksUntilIdle();

  // Parent folder is not changed.
  EXPECT_EQ(ash::kOemFolderId, oem_app_item->folder_id());
}

TEST_F(AppListSyncableServiceTest, OEMAppParentNotOverridenInSync) {
  const std::string oem_app_id = CreateNextAppId(extensions::kWebStoreAppId);
  scoped_refptr<extensions::Extension> oem_app = MakeApp(
      kOemAppName, oem_app_id, extensions::Extension::WAS_INSTALLED_BY_OEM);
  const std::string oem_app_parent_in_sync = "nonoemfolder";

  // Send sync where the OEM app is parented by another folder.
  syncer::SyncDataList sync_list;
  sync_list.push_back(CreateAppRemoteData(
      oem_app_parent_in_sync, "Non OEM folder", std::string() /* parent_id */,
      syncer::StringOrdinal("nonoemfolderposition").ToInternalValue(),
      std::string() /* item_pin_ordinal*/,
      sync_pb::AppListSpecifics_AppListItemType_TYPE_FOLDER));
  sync_list.push_back(CreateAppRemoteData(
      oem_app_id, kOemAppName, oem_app_parent_in_sync,
      syncer::StringOrdinal("appposition").ToInternalValue(),
      std::string() /* item_pin_ordinal */));
  // Add an extra app to the folder to avoid invalid single item folder.
  sync_list.push_back(CreateAppRemoteData(
      "non-oem-app", "Non OEM app", oem_app_parent_in_sync,
      syncer::StringOrdinal("nonoemappposition").ToInternalValue(),
      std::string() /* item_pin_ordinal */));
  sync_list.push_back(CreateAppRemoteData(
      ash::kOemFolderId, "OEM", std::string() /*parent_id*/,
      syncer::StringOrdinal("oemposition").ToInternalValue(),
      std::string() /* item_pin_ordinal*/,
      sync_pb::AppListSpecifics_AppListItemType_TYPE_FOLDER));
  app_list_syncable_service()->MergeDataAndStartSyncing(
      syncer::APP_LIST, sync_list,
      std::make_unique<syncer::FakeSyncChangeProcessor>());
  content::RunAllTasksUntilIdle();

  InstallExtension(oem_app.get());

  // The OEM app should be parented by the OEM folder locally.
  AppListModelUpdater* model_updater = GetModelUpdater();
  ChromeAppListItem* oem_app_item = model_updater->FindItem(oem_app_id);
  ASSERT_TRUE(oem_app_item);
  EXPECT_EQ(ash::kOemFolderId, oem_app_item->folder_id());

  ChromeAppListItem* oem_folder_item =
      model_updater->FindItem(ash::kOemFolderId);
  ASSERT_TRUE(oem_folder_item);
  EXPECT_EQ(oem_folder_item->position(), syncer::StringOrdinal("oemposition"));

  // Verify that the OEM parent has no changed in sync.
  const AppListSyncableService::SyncItem* app_sync_item =
      GetSyncItem(oem_app_id);
  ASSERT_TRUE(app_sync_item);
  EXPECT_EQ(oem_app_parent_in_sync, app_sync_item->parent_id);

  // Verify that the non OEM folder is not removed from sync, even though it's
  // not been created locally.
  EXPECT_FALSE(model_updater->FindItem(oem_app_parent_in_sync));
  EXPECT_TRUE(GetSyncItem(oem_app_parent_in_sync));
}

// Verifies that OEM folder position respects the OEM folder position in sync.
TEST_F(AppListSyncableServiceTest, OEMFolderPositionSync) {
  const std::string oem_app_id = CreateNextAppId(extensions::kWebStoreAppId);
  scoped_refptr<extensions::Extension> oem_app = MakeApp(
      kOemAppName, oem_app_id, extensions::Extension::WAS_INSTALLED_BY_OEM);

  // Send sync with an OEM folder item.
  syncer::SyncDataList sync_list;
  sync_list.push_back(CreateAppRemoteData(
      oem_app_id, kOemAppName, std::string() /* parent_id */,
      syncer::StringOrdinal("appposition").ToInternalValue(),
      std::string() /* item_pin_ordinal */));
  sync_list.push_back(CreateAppRemoteData(
      ash::kOemFolderId, "OEM", std::string() /*parent_id*/,
      syncer::StringOrdinal("oemposition").ToInternalValue(),
      std::string() /* item_pin_ordinal*/,
      sync_pb::AppListSpecifics_AppListItemType_TYPE_FOLDER));
  app_list_syncable_service()->MergeDataAndStartSyncing(
      syncer::APP_LIST, sync_list,
      std::make_unique<syncer::FakeSyncChangeProcessor>());
  content::RunAllTasksUntilIdle();

  InstallExtension(oem_app.get());

  // OEM app should locally be parented by the OEM folder.
  AppListModelUpdater* model_updater = GetModelUpdater();
  ChromeAppListItem* oem_app_item = model_updater->FindItem(oem_app_id);
  ASSERT_TRUE(oem_app_item);
  EXPECT_EQ(ash::kOemFolderId, oem_app_item->folder_id());

  ChromeAppListItem* oem_folder_item =
      model_updater->FindItem(ash::kOemFolderId);
  ASSERT_TRUE(oem_folder_item);
  // The OEM folder folder should be set to the value set by sync.
  EXPECT_EQ(oem_folder_item->position(), syncer::StringOrdinal("oemposition"));
}

// Verifies that non-OEM item is not moved to OEM folder by sync.
TEST_F(AppListSyncableServiceTest, NonOEMItemIgnoreSyncToOEMFolder) {
  const std::string app_id = CreateNextAppId(extensions::kWebStoreAppId);
  scoped_refptr<extensions::Extension> app = MakeApp(
      kSomeAppName, app_id, extensions::Extension::WAS_INSTALLED_BY_DEFAULT);
  InstallExtension(app.get());

  ChromeAppListItem* app_item = GetModelUpdater()->FindItem(app_id);
  ASSERT_TRUE(app_item);
  // It is in the top list.
  EXPECT_EQ(std::string(), app_item->folder_id());

  // Send sync that this app is in OEM folder.
  syncer::SyncDataList sync_list;
  sync_list.push_back(
      CreateAppRemoteData(app_id, kSomeAppName, ash::kOemFolderId,
                          app_item->position().ToInternalValue(),
                          std::string() /* item_pin_ordinal */));
  app_list_syncable_service()->MergeDataAndStartSyncing(
      syncer::APP_LIST, sync_list,
      std::make_unique<syncer::FakeSyncChangeProcessor>());
  content::RunAllTasksUntilIdle();

  // Parent folder is not changed.
  EXPECT_EQ(std::string(), app_item->folder_id());
}

TEST_F(AppListSyncableServiceTest, InitialMerge) {
  const std::string kItemId1 = GenerateId("item_id1");
  const std::string kItemId2 = GenerateId("item_id2");

  syncer::SyncDataList sync_list;
  sync_list.push_back(CreateAppRemoteData(
      kItemId1, "item_name1", GenerateId("parent_id1"), "ordinal", "pinordinal",
      sync_pb::AppListSpecifics_AppListItemType_TYPE_APP, std::nullopt,
      "promise_package_id1"));
  sync_list.push_back(CreateAppRemoteData(
      kItemId2, "item_name2", GenerateId("parent_id2"), "ordinal", "pinordinal",
      sync_pb::AppListSpecifics_AppListItemType_TYPE_APP, std::nullopt,
      "promise_package_id2"));

  app_list_syncable_service()->MergeDataAndStartSyncing(
      syncer::APP_LIST, sync_list,
      std::make_unique<syncer::FakeSyncChangeProcessor>());
  content::RunAllTasksUntilIdle();

  ASSERT_TRUE(GetSyncItem(kItemId1));
  EXPECT_EQ("item_name1", GetSyncItem(kItemId1)->item_name);
  EXPECT_EQ(GenerateId("parent_id1"), GetSyncItem(kItemId1)->parent_id);
  EXPECT_EQ("ordinal", GetSyncItem(kItemId1)->item_ordinal.ToDebugString());
  EXPECT_EQ("pinordinal",
            GetSyncItem(kItemId1)->item_pin_ordinal.ToDebugString());
  EXPECT_EQ("promise_package_id1", GetSyncItem(kItemId1)->promise_package_id);

  ASSERT_TRUE(GetSyncItem(kItemId2));
  EXPECT_EQ("item_name2", GetSyncItem(kItemId2)->item_name);
  EXPECT_EQ(GenerateId("parent_id2"), GetSyncItem(kItemId2)->parent_id);
  EXPECT_EQ("ordinal", GetSyncItem(kItemId2)->item_ordinal.ToDebugString());
  EXPECT_EQ("pinordinal",
            GetSyncItem(kItemId2)->item_pin_ordinal.ToDebugString());
  EXPECT_EQ("promise_package_id2", GetSyncItem(kItemId2)->promise_package_id);
}

class AppListInternalAppSyncableServiceTest
    : public AppListSyncableServiceTest {
 public:
  AppListInternalAppSyncableServiceTest() {
    chrome::SettingsWindowManager::ForceDeprecatedSettingsWindowForTesting();
  }

  void SetUp() override {
    AppListSyncableServiceTest::SetUp();
    web_app::test::InstallDummyWebApp(testing_profile(), kOsSettingsUrl,
                                      GURL(kOsSettingsUrl));
  }

  ~AppListInternalAppSyncableServiceTest() override = default;
};

TEST_F(AppListSyncableServiceTest, InitialMerge_BadData) {
  const syncer::SyncDataList sync_list = CreateBadAppRemoteData(kDefault);

  app_list_syncable_service()->MergeDataAndStartSyncing(
      syncer::APP_LIST, sync_list,
      std::make_unique<syncer::FakeSyncChangeProcessor>());
  content::RunAllTasksUntilIdle();

  // Invalid item_ordinal and item_pin_ordinal.
  // Invalid item_ordinal is fixed up.
  ASSERT_TRUE(GetSyncItem(kInvalidOrdinalsId()));
  EXPECT_EQ("n",
            GetSyncItem(kInvalidOrdinalsId())->item_ordinal.ToDebugString());
  EXPECT_EQ(
      "INVALID[$$invalid_ordinal$$]",
      GetSyncItem(kInvalidOrdinalsId())->item_pin_ordinal.ToDebugString());

  // Empty item name.
  ASSERT_TRUE(GetSyncItem(kEmptyItemNameId()));
  EXPECT_EQ("", GetSyncItem(kEmptyItemNameId())->item_name);
  EXPECT_TRUE(GetSyncItem(kEmptyItemNameUnsetId()));
  EXPECT_EQ("", GetSyncItem(kEmptyItemNameUnsetId())->item_name);

  // Empty parent ID.
  ASSERT_TRUE(GetSyncItem(kEmptyParentId()));
  EXPECT_EQ("", GetSyncItem(kEmptyParentId())->parent_id);
  EXPECT_TRUE(GetSyncItem(kEmptyParentUnsetId()));
  EXPECT_EQ("", GetSyncItem(kEmptyParentUnsetId())->parent_id);

  // Empty item_ordinal and item_pin_ordinal.
  // Empty item_ordinal is fixed up.
  ASSERT_TRUE(GetSyncItem(kEmptyOrdinalsId()));
  EXPECT_EQ("n", GetSyncItem(kEmptyOrdinalsId())->item_ordinal.ToDebugString());
  EXPECT_EQ("INVALID[]",
            GetSyncItem(kEmptyOrdinalsId())->item_pin_ordinal.ToDebugString());
  ASSERT_TRUE(GetSyncItem(kEmptyOrdinalsUnsetId()));
  EXPECT_EQ("n",
            GetSyncItem(kEmptyOrdinalsUnsetId())->item_ordinal.ToDebugString());
  EXPECT_EQ(
      "INVALID[]",
      GetSyncItem(kEmptyOrdinalsUnsetId())->item_pin_ordinal.ToDebugString());

  // Duplicate item_id overrides previous.
  ASSERT_TRUE(GetSyncItem(kDupeItemId()));
  EXPECT_EQ("item_name_dupe", GetSyncItem(kDupeItemId())->item_name);

  // Empty promise_package_id.
  ASSERT_TRUE(GetSyncItem(kEmptyPromisePackageId()));
  EXPECT_TRUE(
      GetSyncItem(kEmptyPromisePackageId())->promise_package_id.empty());
  EXPECT_TRUE(GetSyncItem(kEmptyPromisePackageUnsetId()));
  EXPECT_TRUE(
      GetSyncItem(kEmptyPromisePackageUnsetId())->promise_package_id.empty());
}

TEST_F(AppListSyncableServiceTest, InitialMergeAndUpdate) {
  RemoveAllExistingItems();

  const std::string kItemId1 = GenerateId("item_id1");
  const std::string kItemId2 = GenerateId("item_id2");

  syncer::SyncDataList sync_list;
  sync_list.push_back(CreateAppRemoteData(
      kItemId1, "item_name1", kParentId(), "ordinal", "pinordinal",
      sync_pb::AppListSpecifics_AppListItemType_TYPE_APP, std::nullopt,
      "promise_package_id1"));
  sync_list.push_back(CreateAppRemoteData(kItemId2, "item_name2", kParentId(),
                                          "ordinal", "pinordinal"));

  auto sync_processor = std::make_unique<syncer::FakeSyncChangeProcessor>();
  app_list_syncable_service()->MergeDataAndStartSyncing(
      syncer::APP_LIST, sync_list,
      std::make_unique<syncer::SyncChangeProcessorWrapperForTest>(
          sync_processor.get()));
  content::RunAllTasksUntilIdle();

  ASSERT_TRUE(GetSyncItem(kItemId1));
  ASSERT_TRUE(GetSyncItem(kItemId2));

  syncer::SyncChangeList change_list;
  change_list.push_back(syncer::SyncChange(
      FROM_HERE, syncer::SyncChange::ACTION_UPDATE,
      CreateAppRemoteData(kItemId1, "item_name1x", GenerateId("parent_id1x"),
                          "ordinalx", "pinordinalx",
                          sync_pb::AppListSpecifics_AppListItemType_TYPE_APP,
                          /*is_user_pinned=*/true, "promise_package_id1x")));
  change_list.push_back(syncer::SyncChange(
      FROM_HERE, syncer::SyncChange::ACTION_UPDATE,
      CreateAppRemoteData(kItemId2, "item_name2x", GenerateId("parent_id2x"),
                          "ordinalx", "pinordinalx",
                          sync_pb::AppListSpecifics_AppListItemType_TYPE_APP,
                          /*is_user_pinned=*/false, "promise_package_id2")));

  app_list_syncable_service()->ProcessSyncChanges(base::Location(),
                                                  change_list);
  content::RunAllTasksUntilIdle();

  ASSERT_TRUE(GetSyncItem(kItemId1));
  EXPECT_EQ("item_name1x", GetSyncItem(kItemId1)->item_name);
  EXPECT_EQ(GenerateId("parent_id1x"), GetSyncItem(kItemId1)->parent_id);
  EXPECT_EQ("ordinalx", GetSyncItem(kItemId1)->item_ordinal.ToDebugString());
  EXPECT_EQ("pinordinalx",
            GetSyncItem(kItemId1)->item_pin_ordinal.ToDebugString());
  EXPECT_TRUE(GetSyncItem(kItemId1)->is_user_pinned.has_value());
  EXPECT_TRUE(*GetSyncItem(kItemId1)->is_user_pinned);
  EXPECT_FALSE(GetSyncItem(kItemId1)->promise_package_id.empty());
  EXPECT_EQ("promise_package_id1x", GetSyncItem(kItemId1)->promise_package_id);

  ASSERT_TRUE(GetSyncItem(kItemId2));
  EXPECT_EQ("item_name2x", GetSyncItem(kItemId2)->item_name);
  EXPECT_EQ(GenerateId("parent_id2x"), GetSyncItem(kItemId2)->parent_id);
  EXPECT_EQ("ordinalx", GetSyncItem(kItemId2)->item_ordinal.ToDebugString());
  EXPECT_EQ("pinordinalx",
            GetSyncItem(kItemId2)->item_pin_ordinal.ToDebugString());
  EXPECT_TRUE(GetSyncItem(kItemId2)->is_user_pinned.has_value());
  EXPECT_FALSE(*GetSyncItem(kItemId2)->is_user_pinned);
  EXPECT_FALSE(GetSyncItem(kItemId2)->promise_package_id.empty());
  EXPECT_EQ("promise_package_id2", GetSyncItem(kItemId2)->promise_package_id);
}

TEST_F(AppListSyncableServiceTest, InitialMergeAndUpdate_BadData) {
  const std::string kItemId = GenerateId("item_id");

  syncer::SyncDataList sync_list;
  sync_list.push_back(CreateAppRemoteData(
      kItemId, "item_name", kParentId(), "ordinal", "pinordinal",
      sync_pb::AppListSpecifics_AppListItemType_TYPE_APP,
      /*is_user_pinned=*/false, "promise_package_id"));

  app_list_syncable_service()->MergeDataAndStartSyncing(
      syncer::APP_LIST, sync_list,
      std::make_unique<syncer::FakeSyncChangeProcessor>());
  content::RunAllTasksUntilIdle();

  ASSERT_TRUE(GetSyncItem(kItemId));

  // Validate items with bad data are processed without crashing.
  app_list_syncable_service()->ProcessSyncChanges(
      FROM_HERE, base::ToVector(
                     CreateBadAppRemoteData(kItemId), [](const auto& update) {
                       return syncer::SyncChange(
                           FROM_HERE, syncer::SyncChange::ACTION_UPDATE,
                           update);
                     }));
  content::RunAllTasksUntilIdle();

  ASSERT_TRUE(GetSyncItem(kItemId));
}

TEST_F(AppListSyncableServiceTest, HandlesItemWithNonExistantFolderId) {
  const std::string kFolderItemId = "folder_item_id";
  const std::string kItemId = GenerateId("item_id");
  const std::string kNonInstalledItemId = GenerateId("not_installed_item_id");

  syncer::SyncDataList sync_list;
  sync_list.push_back(CreateAppRemoteData(kItemId, "item_name", kFolderItemId,
                                          "ordinal", "pinordinal"));
  sync_list.push_back(CreateAppRemoteData(kNonInstalledItemId, "item_name",
                                          kFolderItemId, "ordinal2",
                                          "pinordinal"));

  app_list_syncable_service()->MergeDataAndStartSyncing(
      syncer::APP_LIST, sync_list,
      std::make_unique<syncer::FakeSyncChangeProcessor>());
  content::RunAllTasksUntilIdle();

  scoped_refptr<extensions::Extension> app = MakeApp(
      "Test app", kItemId, extensions::Extension::WAS_INSTALLED_BY_DEFAULT);
  InstallExtension(app.get());

  // Verify that the item install does not crash - the app is moved to the root
  // apps grid.
  ASSERT_TRUE(GetSyncItem(kItemId));
  AppListModelUpdater* model_updater = GetModelUpdater();
  ChromeAppListItem* installed_item = model_updater->FindItem(kItemId);
  ASSERT_TRUE(installed_item);
  EXPECT_EQ("", installed_item->folder_id());
}

TEST_F(AppListSyncableServiceTest, AddingFolderChildItemWithInvalidPosition) {
  const std::string kFolderItemId = "folder_item_id";
  const std::string kItemId = GenerateId("item_id");
  const std::string kNonInstalledItemId = GenerateId("not_installed_item_id");

  syncer::SyncDataList sync_list;
  sync_list.push_back(CreateAppRemoteData(
      kFolderItemId, "folder_item_name", "", "ordinal", "pinordinal",
      sync_pb::AppListSpecifics_AppListItemType_TYPE_FOLDER));
  sync_list.push_back(CreateAppRemoteData(kItemId, "item_name", kFolderItemId,
                                          "$$invalid_ordinal$$", "pinordinal"));
  sync_list.push_back(CreateAppRemoteData(kNonInstalledItemId, "item_name",
                                          kFolderItemId, "ordinal",
                                          "pinordinal"));

  app_list_syncable_service()->MergeDataAndStartSyncing(
      syncer::APP_LIST, sync_list,
      std::make_unique<syncer::FakeSyncChangeProcessor>());
  content::RunAllTasksUntilIdle();

  scoped_refptr<extensions::Extension> app = MakeApp(
      "Test app", kItemId, extensions::Extension::WAS_INSTALLED_BY_DEFAULT);
  InstallExtension(app.get());

  ASSERT_TRUE(GetSyncItem(kItemId));
  AppListModelUpdater* model_updater = GetModelUpdater();
  ChromeAppListItem* installed_item = model_updater->FindItem(kItemId);
  ASSERT_TRUE(installed_item);
  EXPECT_EQ(kFolderItemId, installed_item->folder_id());
}

TEST_F(AppListSyncableServiceTest,
       AddingFolderChildItemWithSiblingWithInvalidPosition) {
  const std::string kFolderItemId = "folder_item_id";
  const std::string kItemId1 = GenerateId("item_id_1");
  const std::string kItemId2 = GenerateId("item_id_2");

  syncer::SyncDataList sync_list;
  sync_list.push_back(CreateAppRemoteData(
      kFolderItemId, "folder_item_name", "", "ordinal", "pinordinal",
      sync_pb::AppListSpecifics_AppListItemType_TYPE_FOLDER));
  sync_list.push_back(CreateAppRemoteData(kItemId1, "item1_name", kFolderItemId,
                                          "ordinal1", "pinordinal1"));
  sync_list.push_back(CreateAppRemoteData(kItemId2, "item2_name", kFolderItemId,
                                          "$$invalid_ordinal$$",
                                          "pinordinal2"));

  app_list_syncable_service()->MergeDataAndStartSyncing(
      syncer::APP_LIST, sync_list,
      std::make_unique<syncer::FakeSyncChangeProcessor>());
  content::RunAllTasksUntilIdle();

  scoped_refptr<extensions::Extension> app_2 = MakeApp(
      "Test app", kItemId2, extensions::Extension::WAS_INSTALLED_BY_DEFAULT);
  InstallExtension(app_2.get());

  ASSERT_TRUE(GetSyncItem(kItemId2));
  AppListModelUpdater* model_updater = GetModelUpdater();
  ChromeAppListItem* item_2 = model_updater->FindItem(kItemId2);
  ASSERT_TRUE(item_2);
  EXPECT_EQ(kFolderItemId, item_2->folder_id());

  scoped_refptr<extensions::Extension> app_1 = MakeApp(
      "Test app", kItemId1, extensions::Extension::WAS_INSTALLED_BY_DEFAULT);
  InstallExtension(app_1.get());

  ASSERT_TRUE(GetSyncItem(kItemId1));
  ChromeAppListItem* item_1 = model_updater->FindItem(kItemId1);
  ASSERT_TRUE(item_1);
  EXPECT_EQ(kFolderItemId, item_1->folder_id());

  ASSERT_TRUE(GetSyncItem(kItemId2));
  item_2 = model_updater->FindItem(kItemId2);
  ASSERT_TRUE(item_2);
  EXPECT_EQ(kFolderItemId, item_2->folder_id());
}

TEST_F(AppListSyncableServiceTest, SyncFolderMoveWithInvalidOrdinalInfo) {
  const std::string kFolderItemId = "folder_item_id";
  const std::string kItemId1 = GenerateId("item_id_1");
  const std::string kItemId2 = GenerateId("item_id_2");

  scoped_refptr<extensions::Extension> app_2 = MakeApp(
      "Test app", kItemId2, extensions::Extension::WAS_INSTALLED_BY_DEFAULT);
  InstallExtension(app_2.get());

  scoped_refptr<extensions::Extension> app_1 = MakeApp(
      "Test app", kItemId1, extensions::Extension::WAS_INSTALLED_BY_DEFAULT);
  InstallExtension(app_1.get());

  syncer::SyncDataList sync_list;
  sync_list.push_back(CreateAppRemoteData(
      kFolderItemId, "folder_item_name", "", "ordinal", "pinordinal",
      sync_pb::AppListSpecifics_AppListItemType_TYPE_FOLDER));
  sync_list.push_back(CreateAppRemoteData(kItemId1, "item1_name", kFolderItemId,
                                          "ordinal1", "pinordinal1"));
  sync_list.push_back(CreateAppRemoteData(kItemId2, "item2_name", kFolderItemId,
                                          "$$invalid_ordinal$$",
                                          "pinordinal2"));

  app_list_syncable_service()->MergeDataAndStartSyncing(
      syncer::APP_LIST, sync_list,
      std::make_unique<syncer::FakeSyncChangeProcessor>());
  content::RunAllTasksUntilIdle();

  ASSERT_TRUE(GetSyncItem(kItemId1));
  AppListModelUpdater* model_updater = GetModelUpdater();
  ChromeAppListItem* item_1 = model_updater->FindItem(kItemId1);
  ASSERT_TRUE(item_1);
  EXPECT_EQ(kFolderItemId, item_1->folder_id());

  ASSERT_TRUE(GetSyncItem(kItemId2));
  ChromeAppListItem* item_2 = model_updater->FindItem(kItemId2);
  ASSERT_TRUE(item_2);
  EXPECT_EQ(kFolderItemId, item_2->folder_id());
}

TEST_F(AppListSyncableServiceTest, PruneEmptySyncFolder) {
  // Add a folder item and two items that are parented to the folder item.
  const std::string kFolderItemId = GenerateId("folder_item_id");
  const std::string kItemId1 = GenerateId("item_id_1");
  const std::string kItemId2 = GenerateId("item_id_2");

  syncer::SyncDataList sync_list;
  sync_list.push_back(CreateAppRemoteData(
      kFolderItemId, "folder_item_name", kParentId(), "ordinal", "pinordinal",
      sync_pb::AppListSpecifics_AppListItemType_TYPE_FOLDER));
  sync_list.push_back(CreateAppRemoteData(kItemId1, "item1_name", kFolderItemId,
                                          "ordinal1", "pinordinal1"));
  sync_list.push_back(CreateAppRemoteData(kItemId2, "item2_name", kFolderItemId,
                                          "ordinal2", "pinordinal2"));

  app_list_syncable_service()->MergeDataAndStartSyncing(
      syncer::APP_LIST, sync_list,
      std::make_unique<syncer::FakeSyncChangeProcessor>());
  content::RunAllTasksUntilIdle();

  ASSERT_TRUE(GetSyncItem(kFolderItemId));
  ASSERT_TRUE(GetSyncItem(kItemId1));
  ASSERT_TRUE(GetSyncItem(kItemId2));

  // Remove one of the child item, the folder still has one item in it.
  app_list_syncable_service()->RemoveItem(kItemId1, /*is_uninstall=*/false);
  content::RunAllTasksUntilIdle();

  ASSERT_TRUE(GetSyncItem(kFolderItemId));
  ASSERT_FALSE(GetSyncItem(kItemId1));
  ASSERT_TRUE(GetSyncItem(kItemId2));

  // Remove the other child item, the empty folder should be removed as well.
  app_list_syncable_service()->RemoveItem(kItemId2, /*is_uninstall=*/false);
  content::RunAllTasksUntilIdle();

  ASSERT_FALSE(GetSyncItem(kFolderItemId));
  ASSERT_FALSE(GetSyncItem(kItemId2));
}

TEST_F(AppListSyncableServiceTest,
       CleanUpSingleItemSyncFolderAfterInitialMerge) {
  syncer::SyncDataList sync_list;
  // Add a top level item.
  const std::string kTopItem = GenerateId("top_item_id");
  sync_list.push_back(CreateAppRemoteData(
      kTopItem, "top_item_name", "", GetLastPositionString(), "pinordinal"));

  // Add a single app folder item with only one child app item in it.
  const std::string kFolderId1 = GenerateId("folder_id_1");
  const std::string kChildItemId1 = GenerateId("child_item_id_1");
  sync_list.push_back(CreateAppRemoteData(
      kFolderId1, "folder_1_name", "", GetLastPositionString(), "pinordinal",
      sync_pb::AppListSpecifics_AppListItemType_TYPE_FOLDER));
  sync_list.push_back(CreateAppRemoteData(kChildItemId1, "child_item_1_name",
                                          kFolderId1, GetLastPositionString(),
                                          "pinordinal"));

  // Add a regular folder with two app items in it.
  const std::string kFolderId2 = GenerateId("folder_id_2");
  const std::string kChildItemId2 = GenerateId("child_item_id_2");
  const std::string kChildItemId3 = GenerateId("child_item_id_3");
  sync_list.push_back(CreateAppRemoteData(
      kFolderId2, "folder_2_name", "", GetLastPositionString(), "pinordinal",
      sync_pb::AppListSpecifics_AppListItemType_TYPE_FOLDER));
  sync_list.push_back(CreateAppRemoteData(kChildItemId2, "child_item_2_name",
                                          kFolderId2, GetLastPositionString(),
                                          "pinordinal"));
  sync_list.push_back(CreateAppRemoteData(kChildItemId3, "child_item_3_name",
                                          kFolderId2, GetLastPositionString(),
                                          "pinordinal"));

  app_list_syncable_service()->MergeDataAndStartSyncing(
      syncer::APP_LIST, sync_list,
      std::make_unique<syncer::FakeSyncChangeProcessor>());
  content::RunAllTasksUntilIdle();

  ASSERT_TRUE(GetSyncItem(kFolderId1));
  ASSERT_TRUE(GetSyncItem(kChildItemId1));
  EXPECT_EQ(kFolderId1, GetSyncItem(kChildItemId1)->parent_id);
  EXPECT_TRUE(
      GetSyncItem(kChildItemId1)
          ->item_ordinal.GreaterThan(GetSyncItem(kTopItem)->item_ordinal));

  // Sync items should be created for regular folder.
  ASSERT_TRUE(GetSyncItem(kFolderId2));
  ASSERT_TRUE(GetSyncItem(kChildItemId2));
  EXPECT_EQ(kFolderId2, GetSyncItem(kChildItemId2)->parent_id);
  ASSERT_TRUE(GetSyncItem(kChildItemId3));
  EXPECT_EQ(kFolderId2, GetSyncItem(kChildItemId3)->parent_id);
  EXPECT_TRUE(
      GetSyncItem(kChildItemId1)
          ->item_ordinal.LessThan(GetSyncItem(kFolderId2)->item_ordinal));
}

// Simulates and verifies the fix of the single item folder issue of
// crbug.com/1082530. Here is the repro of the bug.
// When user signs in on a new device for the first time, a folder contains two
// app items, one is installed before another. After the first app is installed,
// user sees a single item folder with the first app. User moves the app out of
// the folder, the folder disappears from UI. Later, when the second app is
// installed, it will show up in a folder with only the second app in it.
// The fix will remove the second app from the folder after user removes the
// first app. Later, when the second app is installed, it will show at the top
// level.
TEST_F(AppListSyncableServiceTest, UpdateSyncItemRemoveLastItemFromFolder) {
  // Create two apps associated with a folder for sync data.
  const std::string kFolderId = GenerateId("folder_id");
  const std::string kChildItemId1 = extensions::kWebStoreAppId;
  const std::string kChildItemId2 = CreateNextAppId(extensions::kWebStoreAppId);
  syncer::SyncDataList sync_list;
  sync_list.push_back(CreateAppRemoteData(
      kFolderId, "folder_name", "", GetLastPositionString(), "pinordinal",
      sync_pb::AppListSpecifics_AppListItemType_TYPE_FOLDER));
  sync_list.push_back(CreateAppRemoteData(kChildItemId1, "child_item_1_name",
                                          kFolderId, GetLastPositionString(),
                                          "pinordinal"));
  sync_list.push_back(CreateAppRemoteData(kChildItemId2, "child_item_2_name",
                                          kFolderId, GetLastPositionString(),
                                          "pinordinal"));

  app_list_syncable_service()->MergeDataAndStartSyncing(
      syncer::APP_LIST, sync_list,
      std::make_unique<syncer::FakeSyncChangeProcessor>());
  content::RunAllTasksUntilIdle();

  ASSERT_TRUE(GetSyncItem(kFolderId));
  ASSERT_TRUE(GetSyncItem(kChildItemId1));
  ASSERT_TRUE(GetSyncItem(kChildItemId2));
  EXPECT_EQ(kFolderId, GetSyncItem(kChildItemId1)->parent_id);
  EXPECT_EQ(kFolderId, GetSyncItem(kChildItemId2)->parent_id);

  // Install the first child app.
  scoped_refptr<extensions::Extension> child_app_1 =
      MakeApp("child_item_1_name", kChildItemId1,
              extensions::Extension::WAS_INSTALLED_BY_DEFAULT);
  InstallExtension(child_app_1.get());

  // Verify the first child app is created in the model updater.
  // The second app is not in the model updater since it is not installed yet.
  AppListModelUpdater* model_updater = GetModelUpdater();
  ChromeAppListItem* child_item_1 = model_updater->FindItem(kChildItemId1);
  ASSERT_TRUE(child_item_1);
  EXPECT_EQ(kFolderId, child_item_1->folder_id());
  ASSERT_FALSE(model_updater->FindItem(kChildItemId2));

  // Move the child_item_1 out of the folder.
  model_updater->SetItemFolderId(child_item_1->id(), "");

  ASSERT_TRUE(GetSyncItem(kChildItemId1));
  EXPECT_EQ("", GetSyncItem(kChildItemId1)->parent_id);
  ASSERT_TRUE(GetSyncItem(kChildItemId2));
  EXPECT_EQ(kFolderId, GetSyncItem(kChildItemId2)->parent_id);
  EXPECT_EQ("", model_updater->FindItem(kChildItemId1)->folder_id());

  // Install the second child app.
  scoped_refptr<extensions::Extension> child_app_2 =
      MakeApp("child_item_2_name", kChildItemId2,
              extensions::Extension::WAS_INSTALLED_BY_DEFAULT);
  InstallExtension(child_app_2.get());

  ChromeAppListItem* child_item_2 = model_updater->FindItem(kChildItemId2);
  ASSERT_TRUE(child_item_2);
  EXPECT_EQ(kFolderId, child_item_2->folder_id());
}

TEST_F(AppListSyncableServiceTest, PruneRedundantPageBreakItems) {
  RemoveAllExistingItems();

  // Populate item list with items and leading, trailing and duplicate "page
  // break" items.
  const std::string kPageBreakItemId1 = GenerateId("page_break_item_id1");
  const std::string kItemId1 = GenerateId("item_id1");
  const std::string kFolderItemId = GenerateId("folder_item_id");
  const std::string kPageBreakItemId2 = GenerateId("page_break_item_id2");
  const std::string kItemInFolderId = GenerateId("item_in_folder_id");
  const std::string kPageBreakItemId3 = GenerateId("page_break_item_id3");
  const std::string kPageBreakItemId4 = GenerateId("page_break_item_id4");
  const std::string kItemId2 = GenerateId("item_id2");
  const std::string kPageBreakItemId5 = GenerateId("page_break_item_id5");

  syncer::SyncDataList sync_list;
  sync_list.push_back(CreateAppRemoteData(
      kPageBreakItemId1, "page_break_item_name", "" /* parent_id */,
      "b" /* ordinal */, "pinordinal",
      sync_pb::AppListSpecifics_AppListItemType_TYPE_PAGE_BREAK));
  sync_list.push_back(CreateAppRemoteData(kItemId1, "item_name",
                                          "" /* parent_id */, "c" /* ordinal */,
                                          "pinordinal"));
  sync_list.push_back(CreateAppRemoteData(kFolderItemId, "folder_item_name",
                                          "" /* parent_id */, "d" /* ordinal */,
                                          "pinordinal"));
  sync_list.push_back(CreateAppRemoteData(
      kPageBreakItemId2, "page_break_item_name", "" /* parent_id */,
      "e" /* ordinal */, "pinordinal",
      sync_pb::AppListSpecifics_AppListItemType_TYPE_PAGE_BREAK));
  sync_list.push_back(CreateAppRemoteData(
      kItemInFolderId, "item_in_folder_name", kFolderItemId /* parent_id */,
      "f" /* ordinal */, "pinordinal"));
  sync_list.push_back(CreateAppRemoteData(
      kPageBreakItemId3, "page_break_item_name", "" /* parent_id */,
      "g" /* ordinal */, "pinordinal",
      sync_pb::AppListSpecifics_AppListItemType_TYPE_PAGE_BREAK));
  sync_list.push_back(CreateAppRemoteData(
      kPageBreakItemId4, "page_break_item_name", "" /* parent_id */,
      "h" /* ordinal */, "pinordinal",
      sync_pb::AppListSpecifics_AppListItemType_TYPE_PAGE_BREAK));
  sync_list.push_back(CreateAppRemoteData(kItemId2, "item_name",
                                          "" /* parent_id */, "i" /* ordinal */,
                                          "pinordinal"));
  sync_list.push_back(CreateAppRemoteData(
      kPageBreakItemId5, "page_break_item_name", "" /* parent_id */,
      "j" /* ordinal */, "pinordinal",
      sync_pb::AppListSpecifics_AppListItemType_TYPE_PAGE_BREAK));

  app_list_syncable_service()->MergeDataAndStartSyncing(
      syncer::APP_LIST, sync_list,
      std::make_unique<syncer::FakeSyncChangeProcessor>());
  content::RunAllTasksUntilIdle();

  ASSERT_TRUE(GetSyncItem(kPageBreakItemId1));
  ASSERT_TRUE(GetSyncItem(kItemId1));
  ASSERT_TRUE(GetSyncItem(kFolderItemId));
  ASSERT_TRUE(GetSyncItem(kPageBreakItemId2));
  ASSERT_TRUE(GetSyncItem(kItemInFolderId));
  ASSERT_TRUE(GetSyncItem(kPageBreakItemId3));
  ASSERT_TRUE(GetSyncItem(kPageBreakItemId4));
  ASSERT_TRUE(GetSyncItem(kItemId2));
  ASSERT_TRUE(GetSyncItem(kPageBreakItemId5));

  // Remove a item, which triggers removing redundant "page break" items.
  app_list_syncable_service()->RemoveItem(kItemId1, /*is_uninstall=*/false);
  content::RunAllTasksUntilIdle();

  ASSERT_FALSE(GetSyncItem(kPageBreakItemId1));
  ASSERT_FALSE(GetSyncItem(kItemId1));
  ASSERT_TRUE(GetSyncItem(kFolderItemId));
  ASSERT_TRUE(GetSyncItem(kPageBreakItemId2));
  ASSERT_TRUE(GetSyncItem(kItemInFolderId));
  ASSERT_FALSE(GetSyncItem(kPageBreakItemId3));
  ASSERT_FALSE(GetSyncItem(kPageBreakItemId4));
  ASSERT_TRUE(GetSyncItem(kItemId2));
  ASSERT_FALSE(GetSyncItem(kPageBreakItemId5));
}

// This test simulates the following overflow case. Assume the maximum items
// on each page is three. Both device 1 and device 2 have the identical apps,
// the apps layout looks like the following.
// page 1: A1, A2 [page break]
// page 2: B1, B2, B3 [page break]
// page 3: C1 [page break]
// On device 1, move A1 from page 1 to page 2 and insert between B1 and B2.
// After the move, the apps layout should look like the following
// page 1: A2 [page break]
// page 2: B1, A1, B2 [page break]
// page 3: B3, C1 [page break]
// Notice that B3 is overflowed from page 2 to page 3 and placed before C1.
// After the changes are synced to device 2, it should have the same app layout
// as shown on device 1.
// This test simulates that device 2 gets the sync changes from device 1, and
// applies the changes in model updater and the apps should have the same layout
// as the ones on the device 1. It verifies the fix for the repro issue
// described in http://crbug.com/938098#c15.
TEST_F(AppListSyncableServiceTest, PageBreakWithOverflowItem) {
  RemoveAllExistingItems();

  // Create 2 apps on the page 1.
  syncer::SyncDataList sync_list;
  const std::string kItemIdA1 = extensions::kWebStoreAppId;
  const std::string kItemIdA2 = CreateNextAppId(extensions::kWebStoreAppId);
  const std::string kPageBreakItemId1 = GenerateId("page_break_item_id1");
  sync_list.push_back(CreateAppRemoteData(
      kItemIdA1, "A1", "", GetLastPositionString(), "pinordinal"));
  sync_list.push_back(CreateAppRemoteData(
      kItemIdA2, "A2", "", GetLastPositionString(), "pinordinal"));
  sync_list.push_back(CreateAppRemoteData(
      kPageBreakItemId1, "page_break_item1_name", "" /* parent_id */,
      GetLastPositionString(), "pinordinal",
      sync_pb::AppListSpecifics_AppListItemType_TYPE_PAGE_BREAK));

  // Create 3 apps on the page 2, which makes it a full page.
  const std::string kItemIdB1 = CreateNextAppId(kItemIdA2);
  const std::string kItemIdB2 = CreateNextAppId(kItemIdB1);
  const std::string kItemIdB3 = CreateNextAppId(kItemIdB2);
  const std::string kPageBreakItemId2 = GenerateId("page_break_item_id2");
  sync_list.push_back(CreateAppRemoteData(
      kItemIdB1, "B1", "", GetLastPositionString(), "pinordinal"));
  sync_list.push_back(CreateAppRemoteData(
      kItemIdB2, "B2", "", GetLastPositionString(), "pinordinal"));
  sync_list.push_back(CreateAppRemoteData(
      kItemIdB3, "B3", "", GetLastPositionString(), "pinordinal"));
  sync_list.push_back(CreateAppRemoteData(
      kPageBreakItemId2, "page_break_item2_name", "" /* parent_id */,
      GetLastPositionString(), "pinordinal",
      sync_pb::AppListSpecifics_AppListItemType_TYPE_PAGE_BREAK));

  // Create 1 app on page 3.
  const std::string kItemIdC1 = CreateNextAppId(kItemIdB3);
  const std::string kPageBreakItemId3 = GenerateId("page_break_item_id3");
  sync_list.push_back(CreateAppRemoteData(
      kItemIdC1, "C1", "", GetLastPositionString(), "pinordinal"));
  sync_list.push_back(CreateAppRemoteData(
      kPageBreakItemId3, "page_break_item3_name", "" /* parent_id */,
      GetLastPositionString(), "pinordinal",
      sync_pb::AppListSpecifics_AppListItemType_TYPE_PAGE_BREAK));

  app_list_syncable_service()->MergeDataAndStartSyncing(
      syncer::APP_LIST, sync_list,
      std::make_unique<syncer::FakeSyncChangeProcessor>());
  content::RunAllTasksUntilIdle();

  // Verify the sync items on page 1.
  ASSERT_TRUE(GetSyncItem(kItemIdA1));
  ASSERT_TRUE(GetSyncItem(kItemIdA2));
  ASSERT_TRUE(GetSyncItem(kPageBreakItemId1));

  // Verify the sync items on page 2.
  ASSERT_TRUE(GetSyncItem(kItemIdB1));
  ASSERT_TRUE(GetSyncItem(kItemIdB2));
  ASSERT_TRUE(GetSyncItem(kItemIdB3));
  ASSERT_TRUE(GetSyncItem(kPageBreakItemId2));

  // Verify the sync items on page 3.
  ASSERT_TRUE(GetSyncItem(kItemIdC1));
  ASSERT_TRUE(GetSyncItem(kPageBreakItemId3));

  // Install all the apps.
  scoped_refptr<extensions::Extension> app_A1 =
      MakeApp("app_A1_name", kItemIdA1,
              extensions::Extension::WAS_INSTALLED_BY_DEFAULT);
  InstallExtension(app_A1.get());
  scoped_refptr<extensions::Extension> app_A2 =
      MakeApp("app_A2_name", kItemIdA2,
              extensions::Extension::WAS_INSTALLED_BY_DEFAULT);
  InstallExtension(app_A2.get());
  scoped_refptr<extensions::Extension> app_B1 =
      MakeApp("app_B1_name", kItemIdB1,
              extensions::Extension::WAS_INSTALLED_BY_DEFAULT);
  InstallExtension(app_B1.get());
  scoped_refptr<extensions::Extension> app_B2 =
      MakeApp("app_B2_name", kItemIdB2,
              extensions::Extension::WAS_INSTALLED_BY_DEFAULT);
  InstallExtension(app_B2.get());
  scoped_refptr<extensions::Extension> app_B3 =
      MakeApp("app_B3_name", kItemIdB3,
              extensions::Extension::WAS_INSTALLED_BY_DEFAULT);
  InstallExtension(app_B3.get());
  scoped_refptr<extensions::Extension> app_C1 =
      MakeApp("app_C1_name", kItemIdC1,
              extensions::Extension::WAS_INSTALLED_BY_DEFAULT);
  InstallExtension(app_C1.get());

  auto ordered_items = GetOrderedItemIdsFromModelUpdater();
  EXPECT_THAT(ordered_items, ElementsAre(kItemIdA1, kItemIdA2, kItemIdB1,
                                         kItemIdB2, kItemIdB3, kItemIdC1));

  // On device 1, move A1 from page 1 to page 2 and insert it between B1 and B2.
  // Device 2 should get the following 3 sync changes from device 1:
  //    1. Remove the previous page break after B3.
  //    2. Add a new page break after B2.
  //    3. Update A1 for position change to move it between B1 and B2.
  syncer::SyncChangeList change_list;
  // Sync change for removing the previous page break after B3.
  AppListModelUpdater* model_updater = GetModelUpdater();
  ChromeAppListItem* app_item_B1 = model_updater->FindItem(kItemIdB1);
  ChromeAppListItem* app_item_B2 = model_updater->FindItem(kItemIdB2);
  ChromeAppListItem* app_item_B3 = model_updater->FindItem(kItemIdB3);
  change_list.push_back(syncer::SyncChange(
      FROM_HERE, syncer::SyncChange::ACTION_DELETE,
      CreateAppRemoteData(
          kPageBreakItemId2, "page_break_item2_name", "" /* parent_id */,
          GetSyncItem(kPageBreakItemId2)->item_ordinal.ToDebugString(),
          "pinordinal")));
  // Sync change for adding a new page break after B2.
  const std::string kNewPageBreakItemId = GenerateId("new_page_break_item_id");
  change_list.push_back(syncer::SyncChange(
      FROM_HERE, syncer::SyncChange::ACTION_ADD,
      CreateAppRemoteData(
          kNewPageBreakItemId, "new_page_break_item_name", "" /* parent_id */,
          app_item_B2->position()
              .CreateBetween(app_item_B3->position())
              .ToDebugString(),
          "pinordinal",
          sync_pb::AppListSpecifics_AppListItemType_TYPE_PAGE_BREAK)));
  // Sync change for moving A1 between B1 and B2.
  change_list.push_back(syncer::SyncChange(
      FROM_HERE, syncer::SyncChange::ACTION_UPDATE,
      CreateAppRemoteData(kItemIdA1, "A1", "" /* parent_id */,
                          app_item_B1->position()
                              .CreateBetween(app_item_B2->position())
                              .ToDebugString(),
                          "pinordinal")));

  app_list_syncable_service()->ProcessSyncChanges(base::Location(),
                                                  change_list);
  content::RunAllTasksUntilIdle();

  // Verify the original page break item after B3 is removed.
  EXPECT_FALSE(GetSyncItem(kPageBreakItemId2));
  EXPECT_FALSE(model_updater->FindItem(kPageBreakItemId2));

  // Verify a new page break sync item is created.
  EXPECT_TRUE(GetSyncItem(kNewPageBreakItemId));

  auto ordered_items_after_sync = GetOrderedItemIdsFromModelUpdater();
  EXPECT_THAT(ordered_items_after_sync,
              ElementsAre(kItemIdA2, kItemIdB1, kItemIdA1, kItemIdB2, kItemIdB3,
                          kItemIdC1));
}

TEST_F(AppListSyncableServiceTest, FirstAvailablePosition) {
  RemoveAllExistingItems();

  // Populate the first page with items and leave 1 empty slot at the end.
  const int max_items_in_first_page =
      ash::SharedAppListConfig::instance().GetMaxNumOfItemsPerPage();
  syncer::StringOrdinal last_app_position =
      syncer::StringOrdinal::CreateInitialOrdinal();
  AppListModelUpdater* model_updater = GetModelUpdater();
  for (int i = 0; i < max_items_in_first_page - 1; ++i) {
    std::unique_ptr<ChromeAppListItem> item =
        std::make_unique<ChromeAppListItem>(
            profile_.get(), GenerateId("item_id" + base::NumberToString(i)),
            model_updater);
    ItemTestApi(item.get()).SetPosition(last_app_position);
    model_updater->AddItem(std::move(item));
    if (i < max_items_in_first_page - 2)
      last_app_position = last_app_position.CreateAfter();
  }
  EXPECT_TRUE(last_app_position.CreateAfter().Equals(
      model_updater->GetFirstAvailablePosition()));

  EXPECT_TRUE(last_app_position.CreateAfter().Equals(
      model_updater->GetFirstAvailablePosition()));

  // Fill up the first page.
  std::unique_ptr<ChromeAppListItem> app_item =
      std::make_unique<ChromeAppListItem>(
          profile_.get(),
          GenerateId("item_id" + base::NumberToString(max_items_in_first_page)),
          model_updater);
  const syncer::StringOrdinal new_item_position =
      last_app_position.CreateAfter();
  ItemTestApi(app_item.get()).SetPosition(new_item_position);
  model_updater->AddItem(std::move(app_item));
  EXPECT_TRUE(new_item_position.CreateAfter().Equals(
      model_updater->GetFirstAvailablePosition()));
}

// Test that verifies app attributes are transferred to the existing app and to
// to the app which will be installed later.
TEST_F(AppListSyncableServiceTest, TransferItem) {
  // Webstore app in this test is source app.
  scoped_refptr<extensions::Extension> webstore =
      MakeApp(kSomeAppName, extensions::kWebStoreAppId,
              extensions::Extension::WAS_INSTALLED_BY_DEFAULT);
  InstallExtension(webstore.get());

  // Chrome is an existing app to transfer attributes to.
  scoped_refptr<extensions::Extension> chrome =
      MakeApp(kSomeAppName, app_constants::kChromeAppId,
              extensions::Extension::WAS_INSTALLED_BY_DEFAULT);
  InstallExtension(chrome.get());

  // Youtube is a future app to be installed.
  scoped_refptr<extensions::Extension> youtube =
      MakeApp(kSomeAppName, extension_misc::kYoutubeAppId,
              extensions::Extension::WAS_INSTALLED_BY_DEFAULT);

  // Webstore and Chrome items should exist now in sync and in model but not
  // Youtube.
  const AppListSyncableService::SyncItem* webstore_sync_item =
      GetSyncItem(extensions::kWebStoreAppId);
  AppListModelUpdater* model_updater = GetModelUpdater();
  const ChromeAppListItem* webstore_item =
      model_updater->FindItem(extensions::kWebStoreAppId);
  ASSERT_TRUE(webstore_item);
  ASSERT_TRUE(webstore_sync_item);

  const AppListSyncableService::SyncItem* chrome_sync_item =
      GetSyncItem(app_constants::kChromeAppId);
  const ChromeAppListItem* chrome_item =
      model_updater->FindItem(app_constants::kChromeAppId);
  ASSERT_TRUE(chrome_item);
  ASSERT_TRUE(chrome_sync_item);

  EXPECT_FALSE(GetSyncItem(extension_misc::kYoutubeAppId));
  EXPECT_FALSE(model_updater->FindItem(extension_misc::kYoutubeAppId));

  // Modify Webstore app with non-default attributes.
  model_updater->SetItemPosition(extensions::kWebStoreAppId,
                                 syncer::StringOrdinal("position"));
  model_updater->SetItemFolderId(extensions::kWebStoreAppId, "folderid");
  app_list_syncable_service()->SetPinPosition(extensions::kWebStoreAppId,
                                              syncer::StringOrdinal("pin"),
                                              /*pinned_by_policy=*/false);

  // Before transfer attributes are different in both, app item and in sync.
  EXPECT_TRUE(AreAllAppAtributesNotEqualInAppList(webstore_item, chrome_item));
  EXPECT_TRUE(
      AreAllAppAtributesNotEqualInSync(webstore_sync_item, chrome_sync_item));

  // Perform attributes transfer to existing Chrome app.
  EXPECT_TRUE(app_list_syncable_service()->TransferItemAttributes(
      extensions::kWebStoreAppId, app_constants::kChromeAppId));
  // Perform attributes transfer to the future Youtube app.
  EXPECT_TRUE(app_list_syncable_service()->TransferItemAttributes(
      extensions::kWebStoreAppId, extension_misc::kYoutubeAppId));
  // No sync item is created due to transfer to the future app.
  EXPECT_FALSE(GetSyncItem(extension_misc::kYoutubeAppId));
  // Attributes transfer from non-existing app fails.
  EXPECT_FALSE(app_list_syncable_service()->TransferItemAttributes(
      "NonExistingId", extension_misc::kYoutubeAppId));

  // Now Chrome app attributes match Webstore app.
  EXPECT_TRUE(AreAllAppAtributesEqualInAppList(webstore_item, chrome_item));
  EXPECT_TRUE(
      AreAllAppAtributesEqualInSync(webstore_sync_item, chrome_sync_item));

  // Install Youtube now.
  InstallExtension(youtube.get());

  const AppListSyncableService::SyncItem* youtube_sync_item =
      GetSyncItem(extension_misc::kYoutubeAppId);
  const ChromeAppListItem* youtube_item =
      model_updater->FindItem(extension_misc::kYoutubeAppId);
  ASSERT_TRUE(youtube_item);
  ASSERT_TRUE(youtube_sync_item);

  EXPECT_TRUE(AreAllAppAtributesEqualInAppList(webstore_item, youtube_item));
  EXPECT_TRUE(
      AreAllAppAtributesEqualInSync(webstore_sync_item, youtube_sync_item));
}

TEST_F(AppListSyncableServiceTest, EphemeralAppsNotSynced) {
  RemoveAllExistingItems();

  auto sync_processor = std::make_unique<syncer::FakeSyncChangeProcessor>();
  app_list_syncable_service()->MergeDataAndStartSyncing(
      syncer::APP_LIST, {},
      std::make_unique<syncer::SyncChangeProcessorWrapperForTest>(
          sync_processor.get()));
  content::RunAllTasksUntilIdle();

  const std::string ephemeral_app_id =
      CreateNextAppId(extensions::kWebStoreAppId);
  auto* model_updater = GetModelUpdater();
  auto* app_item = model_updater->FindItem(ephemeral_app_id);
  EXPECT_FALSE(app_item);
  EXPECT_FALSE(GetSyncItem(ephemeral_app_id));

  std::unique_ptr<ChromeAppListItem> ephemeral_app_item =
      std::make_unique<ChromeAppListItem>(profile_.get(), ephemeral_app_id,
                                          model_updater);
  ephemeral_app_item->SetIsEphemeral(true);
  // Can't use InstallExtension() because it calls AppRegistryCache::OnApps()
  // with the same app ID but type kChromeApp.
  app_list_syncable_service()->AddItem(std::move(ephemeral_app_item));

  app_item = model_updater->FindItem(ephemeral_app_id);
  ASSERT_TRUE(app_item);
  EXPECT_TRUE(app_item->is_ephemeral());

  auto* sync_item = GetSyncItem(ephemeral_app_id);
  ASSERT_TRUE(sync_item);
  EXPECT_TRUE(sync_item->is_ephemeral);

  // Ephemeral sync items are not added to the local storage.
  const base::Value::Dict& local_items =
      profile_->GetPrefs()->GetDict(prefs::kAppListLocalState);

  const base::Value::Dict* dict_item = local_items.FindDict(ephemeral_app_id);
  EXPECT_FALSE(dict_item);

  // Ephemeral sync items are not uploaded to sync data.
  for (auto sync_change : sync_processor->changes()) {
    const std::string item_id =
        sync_change.sync_data().GetSpecifics().app_list().item_id();
    EXPECT_NE(item_id, ephemeral_app_id);
  }
}

TEST_F(AppListSyncableServiceTest, EphemeralFoldersNotSynced) {
  RemoveAllExistingItems();

  std::unique_ptr<syncer::FakeSyncChangeProcessor> sync_processor =
      std::make_unique<syncer::FakeSyncChangeProcessor>();
  app_list_syncable_service()->MergeDataAndStartSyncing(
      syncer::APP_LIST, {},
      std::unique_ptr<syncer::SyncChangeProcessor>(
          new syncer::SyncChangeProcessorWrapperForTest(sync_processor.get())));
  content::RunAllTasksUntilIdle();

  const std::string ephemeral_folder_id = GenerateId("folder_id");
  auto* model_updater = GetModelUpdater();
  auto* folder_item = model_updater->FindItem(ephemeral_folder_id);
  EXPECT_FALSE(folder_item);
  EXPECT_FALSE(GetSyncItem(ephemeral_folder_id));

  syncer::StringOrdinal position =
      syncer::StringOrdinal::CreateInitialOrdinal();
  std::unique_ptr<ChromeAppListItem> ephemeral_folder_item =
      std::make_unique<ChromeAppListItem>(profile_.get(), ephemeral_folder_id,
                                          model_updater);
  ephemeral_folder_item->SetChromeIsFolder(true);
  ephemeral_folder_item->SetChromeName("Folder");
  ephemeral_folder_item->SetIsSystemFolder(true);
  ephemeral_folder_item->SetIsEphemeral(true);
  app_list_syncable_service()->AddItem(std::move(ephemeral_folder_item));

  folder_item = model_updater->FindItem(ephemeral_folder_id);
  ASSERT_TRUE(folder_item);
  EXPECT_TRUE(folder_item->is_ephemeral());

  auto* sync_item = GetSyncItem(ephemeral_folder_id);
  ASSERT_TRUE(sync_item);
  EXPECT_TRUE(sync_item->is_ephemeral);

  // Ephemeral sync items are not added to the local storage.
  const base::Value::Dict& local_items =
      profile_->GetPrefs()->GetDict(prefs::kAppListLocalState);
  const base::Value::Dict* dict_item =
      local_items.FindDict(ephemeral_folder_id);
  EXPECT_FALSE(dict_item);

  // Ephemeral sync items are not uploaded to sync data.
  for (auto sync_change : sync_processor->changes()) {
    const std::string item_id =
        sync_change.sync_data().GetSpecifics().app_list().item_id();
    EXPECT_NE(item_id, ephemeral_folder_id);
  }
}

TEST_F(AppListSyncableServiceTest, SanitizePagesOnItemAdditionAndRemoval) {
  RemoveAllExistingItems();

  // Add enough items to fill up a legacy app list page.
  std::vector<scoped_refptr<extensions::Extension>> initial_apps;
  syncer::SyncDataList sync_list;
  std::string last_item_id = extensions::kWebStoreAppId;
  for (int i = 0; i < 20; ++i) {
    last_item_id = CreateNextAppId(last_item_id);
    const std::string item_name = base::StringPrintf("Item %d", i);
    sync_list.push_back(CreateAppRemoteData(last_item_id, item_name,
                                            /*folder_id=*/"",
                                            GetLastPositionString(), kUnset));
    initial_apps.push_back(
        MakeApp(item_name, last_item_id, extensions::Extension::NO_FLAGS));
  }

  app_list_syncable_service()->MergeDataAndStartSyncing(
      syncer::APP_LIST, sync_list,
      std::make_unique<syncer::FakeSyncChangeProcessor>());
  content::RunAllTasksUntilIdle();

  for (auto app : initial_apps)
    InstallExtension(app.get());

  ASSERT_EQ(GetNamesOfSortedItemsPerPageFromSyncableService(),
            std::vector<std::vector<std::string>>(
                {{"Item 0",  "Item 1",  "Item 2",  "Item 3",  "Item 4",
                  "Item 5",  "Item 6",  "Item 7",  "Item 8",  "Item 9",
                  "Item 10", "Item 11", "Item 12", "Item 13", "Item 14",
                  "Item 15", "Item 16", "Item 17", "Item 18", "Item 19"}}));

  // Install another app - with productivity launcher enabled, the app gets
  // added to front.
  last_item_id = CreateNextAppId(last_item_id);
  scoped_refptr<extensions::Extension> test_app_1 =
      MakeApp("Test app 1", last_item_id, extensions::Extension::NO_FLAGS);
  InstallExtension(test_app_1.get());

  // Verify a page break was added to sync data.
  ASSERT_EQ(GetNamesOfSortedItemsPerPageFromSyncableService(),
            std::vector<std::vector<std::string>>(
                {{"Test app 1", "Item 0",  "Item 1",  "Item 2",  "Item 3",
                  "Item 4",     "Item 5",  "Item 6",  "Item 7",  "Item 8",
                  "Item 9",     "Item 10", "Item 11", "Item 12", "Item 13",
                  "Item 14",    "Item 15", "Item 16", "Item 17", "Item 18"},
                 {"Item 19"}}));

  // Installing another app will not create a new page - the last item from
  // first page will instead be moved to the second page.
  last_item_id = CreateNextAppId(last_item_id);
  scoped_refptr<extensions::Extension> test_app_2 =
      MakeApp("Test app 2", last_item_id, extensions::Extension::NO_FLAGS);
  InstallExtension(test_app_2.get());

  ASSERT_EQ(GetNamesOfSortedItemsPerPageFromSyncableService(),
            std::vector<std::vector<std::string>>(
                {{"Test app 2", "Test app 1", "Item 0",  "Item 1",  "Item 2",
                  "Item 3",     "Item 4",     "Item 5",  "Item 6",  "Item 7",
                  "Item 8",     "Item 9",     "Item 10", "Item 11", "Item 12",
                  "Item 13",    "Item 14",    "Item 15", "Item 16", "Item 17"},
                 {"Item 18", "Item 19"}}));

  // Remove an app from the first page and verify first app from second page is
  // moved back to the first page.
  RemoveExtension(initial_apps[2]->id());

  ASSERT_EQ(GetNamesOfSortedItemsPerPageFromSyncableService(),
            std::vector<std::vector<std::string>>(
                {{"Test app 2", "Test app 1", "Item 0",  "Item 1",  "Item 3",
                  "Item 4",     "Item 5",     "Item 6",  "Item 7",  "Item 8",
                  "Item 9",     "Item 10",    "Item 11", "Item 12", "Item 13",
                  "Item 14",    "Item 15",    "Item 16", "Item 17", "Item 18"},
                 {"Item 19"}}));

  // Removing another extension from the first page removes the second page.
  RemoveExtension(initial_apps[11]->id());

  EXPECT_EQ(
      GetNamesOfSortedItemsPerPageFromSyncableService(),
      std::vector<std::vector<std::string>>(
          {{"Test app 2", "Test app 1", "Item 0",  "Item 1",  "Item 3",
            "Item 4",     "Item 5",     "Item 6",  "Item 7",  "Item 8",
            "Item 9",     "Item 10",    "Item 12", "Item 13", "Item 14",
            "Item 15",    "Item 16",    "Item 17", "Item 18", "Item 19"}}));
}

TEST_F(AppListSyncableServiceTest, SanitizationKeepsUserAddedPageBreaks) {
  RemoveAllExistingItems();

  // Add enough items to have two pages with legacy max app list page size,
  // and add a page break, which is treated as a user added page break after the
  // first page.
  std::vector<scoped_refptr<extensions::Extension>> initial_apps;
  syncer::SyncDataList sync_list;
  std::string last_item_id = extensions::kWebStoreAppId;
  for (int i = 0; i < 21; ++i) {
    if (i == 20) {
      sync_list.push_back(CreateAppRemoteData(
          "page_break", "page_break", "", GetLastPositionString(), kUnset,
          sync_pb::AppListSpecifics_AppListItemType_TYPE_PAGE_BREAK));
    }
    last_item_id = CreateNextAppId(last_item_id);
    const std::string item_name = base::StringPrintf("Item %d", i);
    sync_list.push_back(CreateAppRemoteData(last_item_id, item_name,
                                            /*folder_id=*/"",
                                            GetLastPositionString(), kUnset));
    initial_apps.push_back(
        MakeApp(item_name, last_item_id, extensions::Extension::NO_FLAGS));
  }

  app_list_syncable_service()->MergeDataAndStartSyncing(
      syncer::APP_LIST, sync_list,
      std::make_unique<syncer::FakeSyncChangeProcessor>());
  content::RunAllTasksUntilIdle();

  for (auto app : initial_apps)
    InstallExtension(app.get());

  ASSERT_EQ(GetNamesOfSortedItemsPerPageFromSyncableService(),
            std::vector<std::vector<std::string>>(
                {{"Item 0",  "Item 1",  "Item 2",  "Item 3",  "Item 4",
                  "Item 5",  "Item 6",  "Item 7",  "Item 8",  "Item 9",
                  "Item 10", "Item 11", "Item 12", "Item 13", "Item 14",
                  "Item 15", "Item 16", "Item 17", "Item 18", "Item 19"},
                 {"Item 20"}}));

  // Install another app - with productivity launcher enabled, the app gets
  // added to front.
  last_item_id = CreateNextAppId(last_item_id);
  scoped_refptr<extensions::Extension> test_app_1 =
      MakeApp("Test app 1", last_item_id, extensions::Extension::NO_FLAGS);
  InstallExtension(test_app_1.get());

  // Verify a page break was added to sync data.
  ASSERT_EQ(GetNamesOfSortedItemsPerPageFromSyncableService(),
            std::vector<std::vector<std::string>>(
                {{"Test app 1", "Item 0",  "Item 1",  "Item 2",  "Item 3",
                  "Item 4",     "Item 5",  "Item 6",  "Item 7",  "Item 8",
                  "Item 9",     "Item 10", "Item 11", "Item 12", "Item 13",
                  "Item 14",    "Item 15", "Item 16", "Item 17", "Item 18"},
                 {"Item 19"},
                 {"Item 20"}}));

  // Installing another app will not create a new page - the last item from
  // first page will instead be moved to the second page.
  last_item_id = CreateNextAppId(last_item_id);
  scoped_refptr<extensions::Extension> test_app_2 =
      MakeApp("Test app 2", last_item_id, extensions::Extension::NO_FLAGS);
  InstallExtension(test_app_2.get());

  ASSERT_EQ(GetNamesOfSortedItemsPerPageFromSyncableService(),
            std::vector<std::vector<std::string>>(
                {{"Test app 2", "Test app 1", "Item 0",  "Item 1",  "Item 2",
                  "Item 3",     "Item 4",     "Item 5",  "Item 6",  "Item 7",
                  "Item 8",     "Item 9",     "Item 10", "Item 11", "Item 12",
                  "Item 13",    "Item 14",    "Item 15", "Item 16", "Item 17"},
                 {"Item 18", "Item 19"},
                 {"Item 20"}}));

  // Remove an app from the first page and verify first app from second page is
  // moved back to the first page.
  RemoveExtension(initial_apps[2]->id());

  ASSERT_EQ(GetNamesOfSortedItemsPerPageFromSyncableService(),
            std::vector<std::vector<std::string>>(
                {{"Test app 2", "Test app 1", "Item 0",  "Item 1",  "Item 3",
                  "Item 4",     "Item 5",     "Item 6",  "Item 7",  "Item 8",
                  "Item 9",     "Item 10",    "Item 11", "Item 12", "Item 13",
                  "Item 14",    "Item 15",    "Item 16", "Item 17", "Item 18"},
                 {"Item 19"},
                 {"Item 20"}}));

  // Removing another extension from the first page removes the second page.
  RemoveExtension(initial_apps[11]->id());

  ASSERT_EQ(GetNamesOfSortedItemsPerPageFromSyncableService(),
            std::vector<std::vector<std::string>>(
                {{"Test app 2", "Test app 1", "Item 0",  "Item 1",  "Item 3",
                  "Item 4",     "Item 5",     "Item 6",  "Item 7",  "Item 8",
                  "Item 9",     "Item 10",    "Item 12", "Item 13", "Item 14",
                  "Item 15",    "Item 16",    "Item 17", "Item 18", "Item 19"},
                 {"Item 20"}}));

  // Remove another app, and verify the app from the second page remains on the
  // second page.
  RemoveExtension(initial_apps[12]->id());

  EXPECT_EQ(GetNamesOfSortedItemsPerPageFromSyncableService(),
            std::vector<std::vector<std::string>>(
                {{"Test app 2", "Test app 1", "Item 0", "Item 1", "Item 3",
                  "Item 4", "Item 5", "Item 6", "Item 7", "Item 8", "Item 9",
                  "Item 10", "Item 13", "Item 14", "Item 15", "Item 16",
                  "Item 17", "Item 18", "Item 19"},
                 {"Item 20"}}));
}

TEST_F(AppListSyncableServiceTest, SanitizePageSizesWhenMovingApps) {
  RemoveAllExistingItems();

  app_list_syncable_service()->MergeDataAndStartSyncing(
      syncer::APP_LIST, {},
      std::make_unique<syncer::FakeSyncChangeProcessor>());
  content::RunAllTasksUntilIdle();

  // Add enough items to have two pages with legacy max app list page size.
  std::vector<scoped_refptr<extensions::Extension>> initial_apps;
  syncer::SyncDataList sync_list;
  std::string last_item_id = extensions::kWebStoreAppId;
  for (int i = 0; i < 21; ++i) {
    last_item_id = CreateNextAppId(last_item_id);
    initial_apps.push_back(MakeApp(base::StringPrintf("Item %d", i),
                                   last_item_id,
                                   extensions::Extension::NO_FLAGS));
  }

  for (auto app : initial_apps)
    InstallExtension(app.get());

  ASSERT_EQ(GetNamesOfSortedItemsPerPageFromSyncableService(),
            std::vector<std::vector<std::string>>(
                {{"Item 20", "Item 19", "Item 18", "Item 17", "Item 16",
                  "Item 15", "Item 14", "Item 13", "Item 12", "Item 11",
                  "Item 10", "Item 9",  "Item 8",  "Item 7",  "Item 6",
                  "Item 5",  "Item 4",  "Item 3",  "Item 2",  "Item 1"},
                 {"Item 0"}}));

  // Move the item from the second page to the first page, and verify the number
  // of the pages remains the same (and the last item on the first page moves to
  // the second page).
  ash::AppListItem* item_5 = FindItemForApp(initial_apps[5].get());
  ASSERT_TRUE(item_5);
  ash::AppListItem* item_6 = FindItemForApp(initial_apps[6].get());
  ASSERT_TRUE(item_6);
  syncer::StringOrdinal target_position =
      item_5->position().CreateBetween(item_6->position());

  GetModelUpdater()->RequestPositionUpdate(
      initial_apps[0]->id(), target_position,
      ash::RequestPositionUpdateReason::kMoveItem);

  ASSERT_EQ(GetNamesOfSortedItemsPerPageFromSyncableService(),
            std::vector<std::vector<std::string>>(
                {{"Item 20", "Item 19", "Item 18", "Item 17", "Item 16",
                  "Item 15", "Item 14", "Item 13", "Item 12", "Item 11",
                  "Item 10", "Item 9",  "Item 8",  "Item 7",  "Item 6",
                  "Item 0",  "Item 5",  "Item 4",  "Item 3",  "Item 2"},
                 {"Item 1"}}));

  // Install another app, and verify this does not add an extra page.
  last_item_id = CreateNextAppId(last_item_id);
  scoped_refptr<extensions::Extension> test_app_1 =
      MakeApp("Test app 1", last_item_id, extensions::Extension::NO_FLAGS);
  InstallExtension(test_app_1.get());

  ASSERT_EQ(GetNamesOfSortedItemsPerPageFromSyncableService(),
            std::vector<std::vector<std::string>>(
                {{"Test app 1", "Item 20", "Item 19", "Item 18", "Item 17",
                  "Item 16",    "Item 15", "Item 14", "Item 13", "Item 12",
                  "Item 11",    "Item 10", "Item 9",  "Item 8",  "Item 7",
                  "Item 6",     "Item 0",  "Item 5",  "Item 4",  "Item 3"},
                 {"Item 2", "Item 1"}}));

  // Move an app from the first page to the second page, and verify no extra
  // pages are created.
  ash::AppListItem* item_2 = FindItemForApp(initial_apps[2].get());
  ASSERT_TRUE(item_2);
  ash::AppListItem* item_1 = FindItemForApp(initial_apps[1].get());
  ASSERT_TRUE(item_1);
  target_position = item_2->position().CreateBetween(item_1->position());

  GetModelUpdater()->RequestPositionUpdate(
      initial_apps[10]->id(), target_position,
      ash::RequestPositionUpdateReason::kMoveItem);

  EXPECT_EQ(GetNamesOfSortedItemsPerPageFromSyncableService(),
            std::vector<std::vector<std::string>>(
                {{"Test app 1", "Item 20", "Item 19", "Item 18", "Item 17",
                  "Item 16",    "Item 15", "Item 14", "Item 13", "Item 12",
                  "Item 11",    "Item 9",  "Item 8",  "Item 7",  "Item 6",
                  "Item 0",     "Item 5",  "Item 4",  "Item 3",  "Item 2"},
                 {"Item 10", "Item 1"}}));
}

TEST_F(AppListSyncableServiceTest,
       SanitizePageSizesWhenCreatingAndRemovingFolders) {
  RemoveAllExistingItems();
  app_list_syncable_service()->MergeDataAndStartSyncing(
      syncer::APP_LIST, {},
      std::make_unique<syncer::FakeSyncChangeProcessor>());
  content::RunAllTasksUntilIdle();

  // Add enough items to have two pages with legacy max app list page size.
  std::vector<scoped_refptr<extensions::Extension>> initial_apps;
  syncer::SyncDataList sync_list;
  std::string last_item_id = extensions::kWebStoreAppId;
  for (int i = 0; i < 21; ++i) {
    last_item_id = CreateNextAppId(last_item_id);
    initial_apps.push_back(MakeApp(base::StringPrintf("Item %d", i),
                                   last_item_id,
                                   extensions::Extension::NO_FLAGS));
  }

  for (auto app : initial_apps)
    InstallExtension(app.get());

  ASSERT_EQ(GetNamesOfSortedItemsPerPageFromSyncableService(),
            std::vector<std::vector<std::string>>(
                {{"Item 20", "Item 19", "Item 18", "Item 17", "Item 16",
                  "Item 15", "Item 14", "Item 13", "Item 12", "Item 11",
                  "Item 10", "Item 9",  "Item 8",  "Item 7",  "Item 6",
                  "Item 5",  "Item 4",  "Item 3",  "Item 2",  "Item 1"},
                 {"Item 0"}}));

  // Merge 2 items on the first page, and verify the second page gets removed.
  const std::string folder_id = GetModelUpdater()->model_for_test()->MergeItems(
      initial_apps[5]->id(), initial_apps[6]->id());
  ASSERT_EQ(
      GetNamesOfSortedItemsPerPageFromSyncableService(),
      std::vector<std::vector<std::string>>(
          {{"Item 20", "Item 19", "Item 18", "Item 17", "Item 16",
            "Item 15", "Item 14", "Item 13", "Item 12", "Item 11",
            "Item 10", "Item 9",  "Item 8",  "Item 7",  "" /*unnamed folder*/,
            "Item 4",  "Item 3",  "Item 2",  "Item 1",  "Item 0"}}));

  // Move an item from the created folder, and verify another page gets
  // created.
  ash::AppListItem* item_2 = FindItemForApp(initial_apps[2].get());
  ASSERT_TRUE(item_2);
  ash::AppListItem* item_1 = FindItemForApp(initial_apps[1].get());
  ASSERT_TRUE(item_1);
  syncer::StringOrdinal target_position =
      item_2->position().CreateBetween(item_1->position());

  ash::AppListItem* item_6 = FindItemForApp(initial_apps[6].get());
  ASSERT_TRUE(item_6);

  GetModelUpdater()->model_for_test()->MoveItemToRootAt(item_6,
                                                        target_position);

  EXPECT_EQ(GetNamesOfSortedItemsPerPageFromSyncableService(),
            std::vector<std::vector<std::string>>(
                {{"Item 20", "Item 19", "Item 18", "Item 17", "Item 16",
                  "Item 15", "Item 14", "Item 13", "Item 12", "Item 11",
                  "Item 10", "Item 9",  "Item 8",  "Item 7",  "",
                  "Item 4",  "Item 3",  "Item 2",  "Item 6",  "Item 1"},
                 {"Item 0"}}));

  // Move item 5 out of folder to the second page, and verify the item on the
  // second page fills the empty space left by the folder removal.
  // Note that when productivity launcher is enabled, single item folders are
  // allowed, so Item 5 is expected to still be in the folder at this point.
  ash::AppListItem* item_5 = FindItemForApp(initial_apps[5].get());
  ASSERT_TRUE(item_5);

  ash::AppListItem* item_0 = FindItemForApp(initial_apps[0].get());
  ASSERT_TRUE(item_0);
  GetModelUpdater()->model_for_test()->MoveItemToRootAt(
      item_5, item_0->position().CreateAfter());

  EXPECT_EQ(
      GetNamesOfSortedItemsPerPageFromSyncableService(),
      std::vector<std::vector<std::string>>(
          {{"Item 20", "Item 19", "Item 18", "Item 17", "Item 16", "Item 15",
            "Item 14", "Item 13", "Item 12", "Item 11", "Item 10", "Item 9",
            "Item 8",  "Item 7",  "",        "Item 4",  "Item 3",  "Item 2",
            "Item 6",  "Item 1",  "Item 0"},
           {"Item 5"}}));
}

TEST_F(AppListSyncableServiceTest, SanitizePageSizesWhenReparentingItems) {
  RemoveAllExistingItems();

  // Create two pages of apps, where the first page is partial, and the second
  // page is full and contains a folder.
  std::vector<scoped_refptr<extensions::Extension>> initial_apps;
  const std::string kFolderId = GenerateId("folder_id");
  syncer::SyncDataList sync_list;
  std::string last_item_id = extensions::kWebStoreAppId;
  for (int i = 0; i <= 33; ++i) {
    if (i == 10) {
      sync_list.push_back(CreateAppRemoteData(
          "page_break_1", "page_break_1", "", GetLastPositionString(), kUnset,
          sync_pb::AppListSpecifics_AppListItemType_TYPE_PAGE_BREAK));
    }

    if (i == 15) {
      sync_list.push_back(CreateAppRemoteData(
          kFolderId, "Folder", "", GetLastPositionString(), "",
          sync_pb::AppListSpecifics_AppListItemType_TYPE_FOLDER));
    }

    last_item_id = CreateNextAppId(last_item_id);
    const std::string item_name = base::StringPrintf("Item %d", i);
    sync_list.push_back(CreateAppRemoteData(last_item_id, item_name,
                                            i >= 15 && i < 20 ? kFolderId : "",
                                            GetLastPositionString(), kUnset));
    initial_apps.push_back(
        MakeApp(item_name, last_item_id, extensions::Extension::NO_FLAGS));
  }

  app_list_syncable_service()->MergeDataAndStartSyncing(
      syncer::APP_LIST, sync_list,
      std::make_unique<syncer::FakeSyncChangeProcessor>());
  content::RunAllTasksUntilIdle();

  for (auto app : initial_apps)
    InstallExtension(app.get());

  // Note that items Item 15 - Item 19 are in the folder.
  ASSERT_EQ(GetNamesOfSortedItemsPerPageFromSyncableService(),
            std::vector<std::vector<std::string>>(
                {{"Item 0", "Item 1", "Item 2", "Item 3", "Item 4", "Item 5",
                  "Item 6", "Item 7", "Item 8", "Item 9"},
                 {"Item 10", "Item 11", "Item 12", "Item 13", "Item 14",
                  "Folder",  "Item 20", "Item 21", "Item 22", "Item 23",
                  "Item 24", "Item 25", "Item 26", "Item 27", "Item 28",
                  "Item 29", "Item 30", "Item 31", "Item 32", "Item 33"}}));

  // Move an item from the folder to second page.
  ash::AppListItem* item_12 = FindItemForApp(initial_apps[12].get());
  ASSERT_TRUE(item_12);
  ash::AppListItem* item_11 = FindItemForApp(initial_apps[11].get());
  ASSERT_TRUE(item_11);
  syncer::StringOrdinal target_position =
      item_12->position().CreateBetween(item_11->position());

  ash::AppListItem* item_15 = FindItemForApp(initial_apps[15].get());
  ASSERT_TRUE(item_15);

  GetModelUpdater()->model_for_test()->MoveItemToRootAt(item_15,
                                                        target_position);

  // Verify that the last item from the second page moves to a new page.
  ASSERT_EQ(GetNamesOfSortedItemsPerPageFromSyncableService(),
            std::vector<std::vector<std::string>>(
                {{"Item 0", "Item 1", "Item 2", "Item 3", "Item 4", "Item 5",
                  "Item 6", "Item 7", "Item 8", "Item 9"},
                 {"Item 10", "Item 11", "Item 15", "Item 12", "Item 13",
                  "Item 14", "Folder",  "Item 20", "Item 21", "Item 22",
                  "Item 23", "Item 24", "Item 25", "Item 26", "Item 27",
                  "Item 28", "Item 29", "Item 30", "Item 31", "Item 32"},
                 {"Item 33"}}));

  // Move an item from the folder to the first page, and verify the page count
  // remains the same.
  ash::AppListItem* item_1 = FindItemForApp(initial_apps[1].get());
  ASSERT_TRUE(item_1);
  ash::AppListItem* item_2 = FindItemForApp(initial_apps[2].get());
  ASSERT_TRUE(item_2);
  target_position = item_2->position().CreateBetween(item_1->position());

  ash::AppListItem* item_16 = FindItemForApp(initial_apps[16].get());
  ASSERT_TRUE(item_16);

  GetModelUpdater()->model_for_test()->MoveItemToRootAt(item_16,
                                                        target_position);

  ASSERT_EQ(GetNamesOfSortedItemsPerPageFromSyncableService(),
            std::vector<std::vector<std::string>>(
                {{"Item 0", "Item 1", "Item 16", "Item 2", "Item 3", "Item 4",
                  "Item 5", "Item 6", "Item 7", "Item 8", "Item 9"},
                 {"Item 10", "Item 11", "Item 15", "Item 12", "Item 13",
                  "Item 14", "Folder",  "Item 20", "Item 21", "Item 22",
                  "Item 23", "Item 24", "Item 25", "Item 26", "Item 27",
                  "Item 28", "Item 29", "Item 30", "Item 31", "Item 32"},
                 {"Item 33"}}));

  // Move an item from second page to the folder, and verify the item from the
  // last page moves back to the second page.
  GetModelUpdater()->model_for_test()->MergeItems(kFolderId,
                                                  initial_apps[20]->id());
  EXPECT_EQ(GetNamesOfSortedItemsPerPageFromSyncableService(),
            std::vector<std::vector<std::string>>(
                {{"Item 0", "Item 1", "Item 16", "Item 2", "Item 3", "Item 4",
                  "Item 5", "Item 6", "Item 7", "Item 8", "Item 9"},
                 {"Item 10", "Item 11", "Item 15", "Item 12", "Item 13",
                  "Item 14", "Folder",  "Item 21", "Item 22", "Item 23",
                  "Item 24", "Item 25", "Item 26", "Item 27", "Item 28",
                  "Item 29", "Item 30", "Item 31", "Item 32", "Item 33"}}));
}

TEST_F(AppListSyncableServiceTest,
       NonInstalledItemsIgnoredWhenSanitizingPageSizes) {
  RemoveAllExistingItems();

  // Create two pages of apps, where the first page is partial, and the second
  // page contains items not installed locally and enough installed apps to just
  // fill out the second page.
  std::vector<scoped_refptr<extensions::Extension>> initial_apps;
  const std::string kFolderId = GenerateId("folder_id");
  syncer::SyncDataList sync_list;
  std::string last_item_id = extensions::kWebStoreAppId;
  for (int i = 0; i <= 41; ++i) {
    if (i == 10) {
      sync_list.push_back(CreateAppRemoteData(
          "page_break_1", "page_break_1", "", GetLastPositionString(), kUnset,
          sync_pb::AppListSpecifics_AppListItemType_TYPE_PAGE_BREAK));
    }

    if (i == 15) {
      sync_list.push_back(CreateAppRemoteData(
          kFolderId, "Folder", "", GetLastPositionString(), "",
          sync_pb::AppListSpecifics_AppListItemType_TYPE_FOLDER));
    }

    last_item_id = CreateNextAppId(last_item_id);
    const std::string item_name = base::StringPrintf("Item %d", i);
    sync_list.push_back(CreateAppRemoteData(last_item_id, item_name,
                                            i >= 15 && i < 20 ? kFolderId : "",
                                            GetLastPositionString(), kUnset));
    if (i % 3) {
      initial_apps.push_back(
          MakeApp(item_name, last_item_id, extensions::Extension::NO_FLAGS));
    }
  }

  app_list_syncable_service()->MergeDataAndStartSyncing(
      syncer::APP_LIST, sync_list,
      std::make_unique<syncer::FakeSyncChangeProcessor>());
  content::RunAllTasksUntilIdle();

  for (auto app : initial_apps)
    InstallExtension(app.get());

  // Note that items Item 15 - Item 19 are in the folder. And items with index
  // divisible by 3 are not installed, and don't count towards the total page
  // size.
  ASSERT_EQ(
      GetNamesOfSortedItemsPerPageFromSyncableService(),
      std::vector<std::vector<std::string>>(
          {{"Item 0", "Item 1", "Item 2", "Item 3", "Item 4", "Item 5",
            "Item 6", "Item 7", "Item 8", "Item 9"},
           {"Item 10", "Item 11", "Item 12", "Item 13", "Item 14", "Folder",
            "Item 20", "Item 21", "Item 22", "Item 23", "Item 24", "Item 25",
            "Item 26", "Item 27", "Item 28", "Item 29", "Item 30", "Item 31",
            "Item 32", "Item 33", "Item 34", "Item 35", "Item 36", "Item 37",
            "Item 38", "Item 39", "Item 40", "Item 41"}}));

  auto get_app_for_item_index =
      [&initial_apps](int i) -> extensions::Extension* {
    DCHECK(i % 3);
    // Assumes that `initial_apps` contains items whose index is not
    // divisible by 3.
    return initial_apps[i - i / 3 - 1].get();
  };

  // Move an item from first page to the second page - verify the page break is
  // added so the second page contains only 20 installed items.
  ash::AppListItem* item_10 = FindItemForApp(get_app_for_item_index(10));
  ASSERT_TRUE(item_10);
  ash::AppListItem* item_11 = FindItemForApp(get_app_for_item_index(11));
  ASSERT_TRUE(item_11);
  syncer::StringOrdinal target_position =
      item_10->position().CreateBetween(item_11->position());

  GetModelUpdater()->RequestPositionUpdate(
      get_app_for_item_index(2)->id(), target_position,
      ash::RequestPositionUpdateReason::kMoveItem);

  ASSERT_EQ(
      GetNamesOfSortedItemsPerPageFromSyncableService(),
      std::vector<std::vector<std::string>>(
          {{"Item 0", "Item 1", "Item 3", "Item 4", "Item 5", "Item 6",
            "Item 7", "Item 8", "Item 9"},
           {"Item 10", "Item 2",  "Item 11", "Item 12", "Item 13", "Item 14",
            "Folder",  "Item 20", "Item 21", "Item 22", "Item 23", "Item 24",
            "Item 25", "Item 26", "Item 27", "Item 28", "Item 29", "Item 30",
            "Item 31", "Item 32", "Item 33", "Item 34", "Item 35", "Item 36",
            "Item 37", "Item 38", "Item 39", "Item 40"},
           {"Item 41"}}));

  // Move an app from the folder to the second page.
  ash::AppListItem* item_2 = FindItemForApp(get_app_for_item_index(2));
  ASSERT_TRUE(item_2);
  target_position = item_10->position().CreateBetween(item_2->position());
  ash::AppListItem* item_16 = FindItemForApp(get_app_for_item_index(16));
  ASSERT_TRUE(item_16);

  GetModelUpdater()->model_for_test()->MoveItemToRootAt(item_16,
                                                        target_position);

  ASSERT_EQ(
      GetNamesOfSortedItemsPerPageFromSyncableService(),
      std::vector<std::vector<std::string>>(
          {{"Item 0", "Item 1", "Item 3", "Item 4", "Item 5", "Item 6",
            "Item 7", "Item 8", "Item 9"},
           {"Item 10", "Item 16", "Item 2",  "Item 11", "Item 12", "Item 13",
            "Item 14", "Folder",  "Item 20", "Item 21", "Item 22", "Item 23",
            "Item 24", "Item 25", "Item 26", "Item 27", "Item 28", "Item 29",
            "Item 30", "Item 31", "Item 32", "Item 33", "Item 34", "Item 35",
            "Item 36", "Item 37", "Item 38", "Item 39"},
           {"Item 40", "Item 41"}}));

  // Move another app to the second page.
  target_position = item_10->position().CreateBetween(item_16->position());
  GetModelUpdater()->RequestPositionUpdate(
      get_app_for_item_index(4)->id(), target_position,
      ash::RequestPositionUpdateReason::kMoveItem);

  EXPECT_EQ(
      GetNamesOfSortedItemsPerPageFromSyncableService(),
      std::vector<std::vector<std::string>>(
          {{"Item 0", "Item 1", "Item 3", "Item 5", "Item 6", "Item 7",
            "Item 8", "Item 9"},
           {"Item 10", "Item 4",  "Item 16", "Item 2",  "Item 11", "Item 12",
            "Item 13", "Item 14", "Folder",  "Item 20", "Item 21", "Item 22",
            "Item 23", "Item 24", "Item 25", "Item 26", "Item 27", "Item 28",
            "Item 29", "Item 30", "Item 31", "Item 32", "Item 33", "Item 34",
            "Item 35", "Item 36", "Item 37"},
           {"Item 38", "Item 39", "Item 40", "Item 41"}}));

  // Remove three items from the second page, and verify items from the third
  // page get moved to the second page.
  GetModelUpdater()->model_for_test()->MergeItems(
      kFolderId, get_app_for_item_index(20)->id());
  RemoveExtension(get_app_for_item_index(22)->id());
  RemoveExtension(get_app_for_item_index(28)->id());

  EXPECT_EQ(
      GetNamesOfSortedItemsPerPageFromSyncableService(),
      std::vector<std::vector<std::string>>(
          {{"Item 0", "Item 1", "Item 3", "Item 5", "Item 6", "Item 7",
            "Item 8", "Item 9"},
           {"Item 10", "Item 4",  "Item 16", "Item 2",  "Item 11", "Item 12",
            "Item 13", "Item 14", "Folder",  "Item 21", "Item 23", "Item 24",
            "Item 25", "Item 26", "Item 27", "Item 29", "Item 30", "Item 31",
            "Item 32", "Item 33", "Item 34", "Item 35", "Item 36", "Item 37",
            "Item 38", "Item 39", "Item 40", "Item 41"}}));
}

TEST_F(AppListSyncableServiceTest,
       DontDuplicatePageBreakBetweenUninstalledItems) {
  RemoveAllExistingItems();

  std::vector<scoped_refptr<extensions::Extension>> initial_apps;
  syncer::SyncDataList sync_list;
  std::string last_item_id = extensions::kWebStoreAppId;
  for (int i = 0; i <= 25; ++i) {
    if (i == 20) {
      sync_list.push_back(CreateAppRemoteData(
          "page_break_1", "page_break_1", "", GetLastPositionString(), kUnset,
          sync_pb::AppListSpecifics_AppListItemType_TYPE_PAGE_BREAK));
    }

    last_item_id = CreateNextAppId(last_item_id);
    const std::string item_name = base::StringPrintf("Item %d", i);
    sync_list.push_back(CreateAppRemoteData(last_item_id, item_name, "",
                                            GetLastPositionString(), kUnset));
    if (i < 19 || i > 22) {
      initial_apps.push_back(
          MakeApp(item_name, last_item_id, extensions::Extension::NO_FLAGS));
    }
  }

  app_list_syncable_service()->MergeDataAndStartSyncing(
      syncer::APP_LIST, sync_list,
      std::make_unique<syncer::FakeSyncChangeProcessor>());
  content::RunAllTasksUntilIdle();

  for (auto app : initial_apps)
    InstallExtension(app.get());

  ASSERT_EQ(GetNamesOfSortedItemsPerPageFromSyncableService(),
            std::vector<std::vector<std::string>>(
                {{"Item 0",  "Item 1",  "Item 2",  "Item 3",  "Item 4",
                  "Item 5",  "Item 6",  "Item 7",  "Item 8",  "Item 9",
                  "Item 10", "Item 11", "Item 12", "Item 13", "Item 14",
                  "Item 15", "Item 16", "Item 17", "Item 18", "Item 19"},
                 {"Item 20", "Item 21", "Item 22", "Item 23", "Item 24",
                  "Item 25"}}));

  // Install an app, and verify Item 19, which is not installed locally does not
  // get moved to a new page.
  last_item_id = CreateNextAppId(last_item_id);
  scoped_refptr<extensions::Extension> test_app_1 =
      MakeApp("Test app 1", last_item_id, extensions::Extension::NO_FLAGS);
  InstallExtension(test_app_1.get());

  ASSERT_EQ(
      GetNamesOfSortedItemsPerPageFromSyncableService(),
      std::vector<std::vector<std::string>>(
          {{"Test app 1", "Item 0",  "Item 1",  "Item 2",  "Item 3",  "Item 4",
            "Item 5",     "Item 6",  "Item 7",  "Item 8",  "Item 9",  "Item 10",
            "Item 11",    "Item 12", "Item 13", "Item 14", "Item 15", "Item 16",
            "Item 17",    "Item 18", "Item 19"},
           {"Item 20", "Item 21", "Item 22", "Item 23", "Item 24",
            "Item 25"}}));

  // Install another app, and verify that Items 18 and 19 get moved to a new
  // page (as number of installed apps on the first page would otherwise
  // overflow legacy max page size).
  last_item_id = CreateNextAppId(last_item_id);
  scoped_refptr<extensions::Extension> test_app_2 =
      MakeApp("Test app 2", last_item_id, extensions::Extension::NO_FLAGS);
  InstallExtension(test_app_2.get());

  ASSERT_EQ(GetNamesOfSortedItemsPerPageFromSyncableService(),
            std::vector<std::vector<std::string>>(
                {{"Test app 2", "Test app 1", "Item 0",  "Item 1",  "Item 2",
                  "Item 3",     "Item 4",     "Item 5",  "Item 6",  "Item 7",
                  "Item 8",     "Item 9",     "Item 10", "Item 11", "Item 12",
                  "Item 13",    "Item 14",    "Item 15", "Item 16", "Item 17"},
                 {"Item 18", "Item 19"},
                 {"Item 20", "Item 21", "Item 22", "Item 23", "Item 24",
                  "Item 25"}}));

  // Remove an app from the first page, and verify page brake before Item 18 is
  // removed, as it can fit into the first page again.
  RemoveExtension(initial_apps[3]->id());
  ASSERT_EQ(GetNamesOfSortedItemsPerPageFromSyncableService(),
            std::vector<std::vector<std::string>>(
                {{"Test app 2", "Test app 1", "Item 0",  "Item 1",  "Item 2",
                  "Item 4",     "Item 5",     "Item 6",  "Item 7",  "Item 8",
                  "Item 9",     "Item 10",    "Item 11", "Item 12", "Item 13",
                  "Item 14",    "Item 15",    "Item 16", "Item 17", "Item 18",
                  "Item 19"},
                 {"Item 20", "Item 21", "Item 22", "Item 23", "Item 24",
                  "Item 25"}}));

  // Remove enough apps so Item 20 can fit into the first page, and verify the
  // page break before Item 20 does not get removed, as it's treated as a page
  // brake added via explicit user action in pre-productivity launcher app list.
  RemoveExtension(initial_apps[4]->id());
  RemoveExtension(initial_apps[5]->id());

  EXPECT_EQ(GetNamesOfSortedItemsPerPageFromSyncableService(),
            std::vector<std::vector<std::string>>(
                {{"Test app 2", "Test app 1", "Item 0", "Item 1", "Item 2",
                  "Item 6", "Item 7", "Item 8", "Item 9", "Item 10", "Item 11",
                  "Item 12", "Item 13", "Item 14", "Item 15", "Item 16",
                  "Item 17", "Item 18", "Item 19"},
                 {"Item 20", "Item 21", "Item 22", "Item 23", "Item 24",
                  "Item 25"}}));
}

// Verifies that app list model sanitizer gracefully handles the case when page
// break has to be added between sync items that have duplicate item ordinals.
TEST_F(AppListSyncableServiceTest,
       PageBreakSanitizationHandlesDuplicateOrdinalsAtPageBreakLocation) {
  RemoveAllExistingItems();

  std::vector<scoped_refptr<extensions::Extension>> initial_apps;
  syncer::SyncDataList sync_list;
  std::string last_item_id = extensions::kWebStoreAppId;
  std::string last_position_string;
  for (int i = 0; i < 30; ++i) {
    if (i < 18 || i >= 22)
      last_position_string = GetLastPositionString();
    last_item_id = CreateNextAppId(last_item_id);
    const std::string item_name = base::StringPrintf("Item %d", i);
    sync_list.push_back(CreateAppRemoteData(last_item_id, item_name,
                                            /*folder_id=*/"",
                                            last_position_string, kUnset));
    initial_apps.push_back(
        MakeApp(item_name, last_item_id, extensions::Extension::NO_FLAGS));
  }

  app_list_syncable_service()->MergeDataAndStartSyncing(
      syncer::APP_LIST, sync_list,
      std::make_unique<syncer::FakeSyncChangeProcessor>());
  content::RunAllTasksUntilIdle();

  for (auto app : initial_apps)
    InstallExtension(app.get());

  // Install another app - with productivity launcher enabled, the app gets
  // added to front.
  last_item_id = CreateNextAppId(last_item_id);
  scoped_refptr<extensions::Extension> test_app_1 =
      MakeApp("Test app 1", last_item_id, extensions::Extension::NO_FLAGS);
  InstallExtension(test_app_1.get());

  std::vector<std::vector<std::string>> items_per_page =
      GetNamesOfSortedItemsPerPageFromSyncableService();
  // Verify a page break was added to sync data.
  EXPECT_EQ(2u, items_per_page.size());
  EXPECT_EQ(20u, items_per_page[0].size());
  EXPECT_EQ(11u, items_per_page[1].size());

  // Verify that the apps whose order is well defined (i.e. that don't have
  // identical string ordinals) is preserved.
  EXPECT_EQ(
      std::vector<std::string>(items_per_page[0].begin(),
                               items_per_page[0].begin() + 18),
      std::vector<std::string>(
          {"Test app 1", "Item 0", "Item 1", "Item 2", "Item 3", "Item 4",
           "Item 5", "Item 6", "Item 7", "Item 8", "Item 9", "Item 10",
           "Item 11", "Item 12", "Item 13", "Item 14", "Item 15", "Item 16"}));

  EXPECT_EQ(
      std::vector<std::string>(items_per_page[1].begin() + 3,
                               items_per_page[1].end()),
      std::vector<std::string>({"Item 22", "Item 23", "Item 24", "Item 25",
                                "Item 26", "Item 27", "Item 28", "Item 29"}));

  // Installing another app will not create a new page - the last item from
  // first page will instead be moved to the second page.
  last_item_id = CreateNextAppId(last_item_id);
  scoped_refptr<extensions::Extension> test_app_2 =
      MakeApp("Test app 2", last_item_id, extensions::Extension::NO_FLAGS);
  InstallExtension(test_app_2.get());

  items_per_page = GetNamesOfSortedItemsPerPageFromSyncableService();
  // Verify a page break was added to sync data.
  EXPECT_EQ(2u, items_per_page.size());
  EXPECT_EQ(20u, items_per_page[0].size());
  EXPECT_EQ(12u, items_per_page[1].size());

  // Verify that the apps whose order is well defined (i.e. that don't have
  // identical string ordinals) is preserved.
  EXPECT_EQ(std::vector<std::string>(items_per_page[0].begin(),
                                     items_per_page[0].begin() + 19),
            std::vector<std::string>(
                {"Test app 2", "Test app 1", "Item 0", "Item 1", "Item 2",
                 "Item 3", "Item 4", "Item 5", "Item 6", "Item 7", "Item 8",
                 "Item 9", "Item 10", "Item 11", "Item 12", "Item 13",
                 "Item 14", "Item 15", "Item 16"}));

  EXPECT_EQ(
      std::vector<std::string>(items_per_page[1].begin() + 4,
                               items_per_page[1].end()),
      std::vector<std::string>({"Item 22", "Item 23", "Item 24", "Item 25",
                                "Item 26", "Item 27", "Item 28", "Item 29"}));
}

// Verifies that app list model sanitizer gracefully handles the case when page
// break has to be added between sync items that have duplicate item ordinals,
// where all trailing items have the same ordinal.
TEST_F(AppListSyncableServiceTest,
       PageBreakSanitizationHandlesTrailingDuplicateOrdinals) {
  RemoveAllExistingItems();

  std::vector<scoped_refptr<extensions::Extension>> initial_apps;
  syncer::SyncDataList sync_list;
  std::string last_item_id = extensions::kWebStoreAppId;
  std::string last_position_string;
  for (int i = 0; i < 30; ++i) {
    if (i < 18)
      last_position_string = GetLastPositionString();
    last_item_id = CreateNextAppId(last_item_id);
    const std::string item_name = base::StringPrintf("Item %d", i);
    sync_list.push_back(CreateAppRemoteData(last_item_id, item_name,
                                            /*folder_id=*/"",
                                            last_position_string, kUnset));
    initial_apps.push_back(
        MakeApp(item_name, last_item_id, extensions::Extension::NO_FLAGS));
  }

  app_list_syncable_service()->MergeDataAndStartSyncing(
      syncer::APP_LIST, sync_list,
      std::make_unique<syncer::FakeSyncChangeProcessor>());
  content::RunAllTasksUntilIdle();

  for (auto app : initial_apps)
    InstallExtension(app.get());

  // Install another app - with productivity launcher enabled, the app gets
  // added to front.
  last_item_id = CreateNextAppId(last_item_id);
  scoped_refptr<extensions::Extension> test_app_1 =
      MakeApp("Test app 1", last_item_id, extensions::Extension::NO_FLAGS);
  InstallExtension(test_app_1.get());

  std::vector<std::vector<std::string>> items_per_page =
      GetNamesOfSortedItemsPerPageFromSyncableService();
  // Verify a page break was added to sync data.
  EXPECT_EQ(2u, items_per_page.size());
  EXPECT_EQ(20u, items_per_page[0].size());
  EXPECT_EQ(11u, items_per_page[1].size());

  // Verify that the apps whose order is well defined (i.e. that don't have
  // identical string ordinals) is preserved.
  EXPECT_EQ(
      std::vector<std::string>(items_per_page[0].begin(),
                               items_per_page[0].begin() + 18),
      std::vector<std::string>(
          {"Test app 1", "Item 0", "Item 1", "Item 2", "Item 3", "Item 4",
           "Item 5", "Item 6", "Item 7", "Item 8", "Item 9", "Item 10",
           "Item 11", "Item 12", "Item 13", "Item 14", "Item 15", "Item 16"}));

  // Installing another app will not create a new page - the last item from
  // first page will instead be moved to the second page.
  last_item_id = CreateNextAppId(last_item_id);
  scoped_refptr<extensions::Extension> test_app_2 =
      MakeApp("Test app 2", last_item_id, extensions::Extension::NO_FLAGS);
  InstallExtension(test_app_2.get());

  items_per_page = GetNamesOfSortedItemsPerPageFromSyncableService();
  EXPECT_EQ(2u, items_per_page.size());
  EXPECT_EQ(20u, items_per_page[0].size());
  EXPECT_EQ(12u, items_per_page[1].size());
  EXPECT_EQ(std::vector<std::string>(items_per_page[0].begin(),
                                     items_per_page[0].begin() + 19),
            std::vector<std::string>(
                {"Test app 2", "Test app 1", "Item 0", "Item 1", "Item 2",
                 "Item 3", "Item 4", "Item 5", "Item 6", "Item 7", "Item 8",
                 "Item 9", "Item 10", "Item 11", "Item 12", "Item 13",
                 "Item 14", "Item 15", "Item 16"}));
}

TEST_F(AppListSyncableServiceTest,
       PageBreakSanitizationHandlesDuplicateOrdinalsAtTwoBreaks) {
  RemoveAllExistingItems();

  std::vector<scoped_refptr<extensions::Extension>> initial_apps;
  syncer::SyncDataList sync_list;
  std::string last_item_id = extensions::kWebStoreAppId;
  std::string last_position_string;
  for (int i = 0; i < 45; ++i) {
    if (i < 18 || (i >= 25 && i < 35) || i >= 42)
      last_position_string = GetLastPositionString();
    last_item_id = CreateNextAppId(last_item_id);
    const std::string item_name = base::StringPrintf("Item %d", i);
    sync_list.push_back(CreateAppRemoteData(last_item_id, item_name,
                                            /*folder_id=*/"",
                                            last_position_string, kUnset));
    initial_apps.push_back(
        MakeApp(item_name, last_item_id, extensions::Extension::NO_FLAGS));
  }

  app_list_syncable_service()->MergeDataAndStartSyncing(
      syncer::APP_LIST, sync_list,
      std::make_unique<syncer::FakeSyncChangeProcessor>());
  content::RunAllTasksUntilIdle();

  for (auto app : initial_apps)
    InstallExtension(app.get());

  // Install another app - with productivity launcher enabled, the app gets
  // added to front.
  last_item_id = CreateNextAppId(last_item_id);
  scoped_refptr<extensions::Extension> test_app_1 =
      MakeApp("Test app 1", last_item_id, extensions::Extension::NO_FLAGS);
  InstallExtension(test_app_1.get());

  std::vector<std::vector<std::string>> items_per_page =
      GetNamesOfSortedItemsPerPageFromSyncableService();
  EXPECT_EQ(3u, items_per_page.size());
  EXPECT_EQ(20u, items_per_page[0].size());
  EXPECT_EQ(20u, items_per_page[1].size());
  EXPECT_EQ(6u, items_per_page[2].size());

  // Verify that the apps whose order is well defined (i.e. that don't have
  // identical string ordinals) is preserved.
  EXPECT_EQ(
      std::vector<std::string>(items_per_page[0].begin(),
                               items_per_page[0].begin() + 18),
      std::vector<std::string>(
          {"Test app 1", "Item 0", "Item 1", "Item 2", "Item 3", "Item 4",
           "Item 5", "Item 6", "Item 7", "Item 8", "Item 9", "Item 10",
           "Item 11", "Item 12", "Item 13", "Item 14", "Item 15", "Item 16"}));
  EXPECT_EQ(std::vector<std::string>(items_per_page[1].begin() + 6,
                                     items_per_page[1].begin() + 15),
            std::vector<std::string>({"Item 25", "Item 26", "Item 27",
                                      "Item 28", "Item 29", "Item 30",
                                      "Item 31", "Item 32", "Item 33"}));
  EXPECT_EQ(std::vector<std::string>(items_per_page[2].begin() + 3,
                                     items_per_page[2].end()),
            std::vector<std::string>({"Item 42", "Item 43", "Item 44"}));
}

TEST_F(AppListSyncableServiceTest,
       PageBreakSanitizationHandlesFullPageOfDuplicateOrdinals) {
  RemoveAllExistingItems();

  std::vector<scoped_refptr<extensions::Extension>> initial_apps;
  syncer::SyncDataList sync_list;
  std::string last_item_id = extensions::kWebStoreAppId;
  std::string last_position_string;
  for (int i = 0; i < 45; ++i) {
    if (i < 18 || i >= 42)
      last_position_string = GetLastPositionString();
    last_item_id = CreateNextAppId(last_item_id);
    const std::string item_name = base::StringPrintf("Item %d", i);
    sync_list.push_back(CreateAppRemoteData(last_item_id, item_name,
                                            /*folder_id=*/"",
                                            last_position_string, kUnset));
    initial_apps.push_back(
        MakeApp(item_name, last_item_id, extensions::Extension::NO_FLAGS));
  }

  app_list_syncable_service()->MergeDataAndStartSyncing(
      syncer::APP_LIST, sync_list,
      std::make_unique<syncer::FakeSyncChangeProcessor>());
  content::RunAllTasksUntilIdle();

  for (auto app : initial_apps)
    InstallExtension(app.get());

  // Install another app - with productivity launcher enabled, the app gets
  // added to front.
  last_item_id = CreateNextAppId(last_item_id);
  scoped_refptr<extensions::Extension> test_app_1 =
      MakeApp("Test app 1", last_item_id, extensions::Extension::NO_FLAGS);
  InstallExtension(test_app_1.get());

  std::vector<std::vector<std::string>> items_per_page =
      GetNamesOfSortedItemsPerPageFromSyncableService();
  // Verify a page break was added to sync data.
  EXPECT_EQ(3u, items_per_page.size());
  EXPECT_EQ(20u, items_per_page[0].size());
  EXPECT_EQ(20u, items_per_page[1].size());
  EXPECT_EQ(6u, items_per_page[2].size());

  // Verify that the apps whose order is well defined (i.e. that don't have
  // identical string ordinals) is preserved.
  EXPECT_EQ(
      std::vector<std::string>(items_per_page[0].begin(),
                               items_per_page[0].begin() + 18),
      std::vector<std::string>(
          {"Test app 1", "Item 0", "Item 1", "Item 2", "Item 3", "Item 4",
           "Item 5", "Item 6", "Item 7", "Item 8", "Item 9", "Item 10",
           "Item 11", "Item 12", "Item 13", "Item 14", "Item 15", "Item 16"}));
  EXPECT_EQ(std::vector<std::string>(items_per_page[2].begin() + 3,
                                     items_per_page[2].end()),
            std::vector<std::string>({"Item 42", "Item 43", "Item 44"}));
}

TEST_F(AppListSyncableServiceTest,
       PageBreakSanitizationHandlesPairOfDuplicateOrdinals) {
  RemoveAllExistingItems();

  std::vector<scoped_refptr<extensions::Extension>> initial_apps;
  syncer::SyncDataList sync_list;
  std::string last_item_id = extensions::kWebStoreAppId;
  std::string last_position_string;
  for (int i = 0; i < 25; ++i) {
    if (i != 19)
      last_position_string = GetLastPositionString();
    last_item_id = CreateNextAppId(last_item_id);
    const std::string item_name = base::StringPrintf("Item %d", i);
    sync_list.push_back(CreateAppRemoteData(last_item_id, item_name,
                                            /*folder_id=*/"",
                                            last_position_string, kUnset));
    initial_apps.push_back(
        MakeApp(item_name, last_item_id, extensions::Extension::NO_FLAGS));
  }

  app_list_syncable_service()->MergeDataAndStartSyncing(
      syncer::APP_LIST, sync_list,
      std::make_unique<syncer::FakeSyncChangeProcessor>());
  content::RunAllTasksUntilIdle();

  for (auto app : initial_apps)
    InstallExtension(app.get());

  // Install another app - with productivity launcher enabled, the app gets
  // added to front.
  last_item_id = CreateNextAppId(last_item_id);
  scoped_refptr<extensions::Extension> test_app_1 =
      MakeApp("Test app 1", last_item_id, extensions::Extension::NO_FLAGS);
  InstallExtension(test_app_1.get());

  std::vector<std::vector<std::string>> items_per_page =
      GetNamesOfSortedItemsPerPageFromSyncableService();
  // Verify a page break was added to sync data.
  EXPECT_EQ(2u, items_per_page.size());
  EXPECT_EQ(20u, items_per_page[0].size());
  EXPECT_EQ(6u, items_per_page[1].size());

  // Verify that the apps whose order is well defined (i.e. that don't have
  // identical string ordinals) is preserved.
  EXPECT_EQ(std::vector<std::string>(items_per_page[0].begin(),
                                     items_per_page[0].begin() + 19),
            std::vector<std::string>(
                {"Test app 1", "Item 0", "Item 1", "Item 2", "Item 3", "Item 4",
                 "Item 5", "Item 6", "Item 7", "Item 8", "Item 9", "Item 10",
                 "Item 11", "Item 12", "Item 13", "Item 14", "Item 15",
                 "Item 16", "Item 17"}));
  EXPECT_EQ(std::vector<std::string>(items_per_page[1].begin() + 1,
                                     items_per_page[1].end()),
            std::vector<std::string>(
                {"Item 20", "Item 21", "Item 22", "Item 23", "Item 24"}));
}

TEST_F(AppListSyncableServiceTest,
       PageBreakSanitizationHandlesAllDuplicateOrdinals) {
  RemoveAllExistingItems();

  std::vector<scoped_refptr<extensions::Extension>> initial_apps;
  syncer::SyncDataList sync_list;
  std::string last_item_id = extensions::kWebStoreAppId;
  std::string last_position_string = GetLastPositionString();
  for (int i = 0; i < 30; ++i) {
    last_item_id = CreateNextAppId(last_item_id);
    const std::string item_name = base::StringPrintf("Item %d", i);
    sync_list.push_back(CreateAppRemoteData(last_item_id, item_name,
                                            /*folder_id=*/"",
                                            last_position_string, kUnset));
    initial_apps.push_back(
        MakeApp(item_name, last_item_id, extensions::Extension::NO_FLAGS));
  }

  app_list_syncable_service()->MergeDataAndStartSyncing(
      syncer::APP_LIST, sync_list,
      std::make_unique<syncer::FakeSyncChangeProcessor>());
  content::RunAllTasksUntilIdle();

  for (auto app : initial_apps)
    InstallExtension(app.get());

  // Install another app - with productivity launcher enabled, the app gets
  // added to front.
  last_item_id = CreateNextAppId(last_item_id);
  scoped_refptr<extensions::Extension> test_app_1 =
      MakeApp("Test app 1", last_item_id, extensions::Extension::NO_FLAGS);
  InstallExtension(test_app_1.get());

  // Verify a page break was added to sync data.
  std::vector<std::vector<std::string>> items_per_page =
      GetNamesOfSortedItemsPerPageFromSyncableService();
  EXPECT_EQ(2u, items_per_page.size());
  EXPECT_EQ(20u, items_per_page[0].size());
  EXPECT_EQ(11u, items_per_page[1].size());
  EXPECT_EQ("Test app 1", items_per_page[0][0]);

  // Installing another app will not create a new page - the last item from
  // first page will instead be moved to the second page.
  last_item_id = CreateNextAppId(last_item_id);
  scoped_refptr<extensions::Extension> test_app_2 =
      MakeApp("Test app 2", last_item_id, extensions::Extension::NO_FLAGS);
  InstallExtension(test_app_2.get());

  items_per_page = GetNamesOfSortedItemsPerPageFromSyncableService();
  EXPECT_EQ(2u, items_per_page.size());
  EXPECT_EQ(20u, items_per_page[0].size());
  EXPECT_EQ(12u, items_per_page[1].size());
  EXPECT_EQ("Test app 2", items_per_page[0][0]);
  EXPECT_EQ("Test app 1", items_per_page[0][1]);
}

// Verifies that sorting works for the mixture of valid and invalid positions.
TEST_F(AppListSyncableServiceTest, SortMixedPositionValidityItems) {
  RemoveAllExistingItems();

  using SyncItem = AppListSyncableService::SyncItem;
  const std::string kItemId1 = CreateNextAppId(extensions::kWebStoreAppId);
  auto item1 = std::make_unique<SyncItem>(
      kItemId1, sync_pb::AppListSpecifics::TYPE_APP, /*is_new=*/false);
  item1->item_name = "a";
  item1->item_ordinal = syncer::StringOrdinal(GetLastPositionString());

  const std::string kItemId2 = CreateNextAppId(kItemId1);
  auto item2 = std::make_unique<SyncItem>(
      kItemId2, sync_pb::AppListSpecifics::TYPE_APP, /*is_new=*/true);
  item2->item_name = "b";

  const std::string kItemId3 = CreateNextAppId(kItemId2);
  auto item3 = std::make_unique<SyncItem>(
      kItemId3, sync_pb::AppListSpecifics::TYPE_APP, /*is_new=*/false);
  item3->item_name = "c";

  std::vector<std::unique_ptr<SyncItem>> sync_items;
  sync_items.push_back(std::move(item1));
  sync_items.push_back(std::move(item2));
  sync_items.push_back(std::move(item3));

  // Populate items and verify their validity. Only `item1` has the valid
  // position.
  app_list_syncable_service()->PopulateSyncItemsForTest(std::move(sync_items));
  EXPECT_TRUE(app_list_syncable_service()
                  ->GetSyncItem(kItemId1)
                  ->item_ordinal.IsValid());
  EXPECT_FALSE(app_list_syncable_service()
                   ->GetSyncItem(kItemId2)
                   ->item_ordinal.IsValid());
  EXPECT_FALSE(app_list_syncable_service()
                   ->GetSyncItem(kItemId3)
                   ->item_ordinal.IsValid());

  app_list_syncable_service()->SetAppListPreferredOrder(
      ash::AppListSortOrder::kNameReverseAlphabetical);
  EXPECT_EQ(GetOrderedItemIdsFromSyncableService(),
            std::vector<std::string>({kItemId3, kItemId2, kItemId1}));

  app_list_syncable_service()->SetAppListPreferredOrder(
      ash::AppListSortOrder::kNameAlphabetical);
  EXPECT_EQ(GetOrderedItemIdsFromSyncableService(),
            std::vector<std::string>({kItemId1, kItemId2, kItemId3}));
}

// Verifies that sorting works if all item positions are invalid.
TEST_F(AppListSyncableServiceTest, SortInvalidPositionItems) {
  RemoveAllExistingItems();

  using SyncItem = AppListSyncableService::SyncItem;
  const std::string kItemId1 = CreateNextAppId(extensions::kWebStoreAppId);
  auto item1 = std::make_unique<SyncItem>(
      kItemId1, sync_pb::AppListSpecifics::TYPE_APP, /*is_new=*/false);
  item1->item_name = "a";

  const std::string kItemId2 = CreateNextAppId(kItemId1);
  auto item2 = std::make_unique<SyncItem>(
      kItemId2, sync_pb::AppListSpecifics::TYPE_APP, /*is_new=*/false);
  item2->item_name = "b";

  const std::string kItemId3 = CreateNextAppId(kItemId2);
  auto item3 = std::make_unique<SyncItem>(
      kItemId3, sync_pb::AppListSpecifics::TYPE_APP, /*is_new=*/true);
  item3->item_name = "c";

  std::vector<std::unique_ptr<SyncItem>> sync_items;
  sync_items.push_back(std::move(item1));
  sync_items.push_back(std::move(item2));
  sync_items.push_back(std::move(item3));

  // Verify the validity of sync item positions.
  app_list_syncable_service()->PopulateSyncItemsForTest(std::move(sync_items));
  EXPECT_FALSE(app_list_syncable_service()
                   ->GetSyncItem(kItemId1)
                   ->item_ordinal.IsValid());
  EXPECT_FALSE(app_list_syncable_service()
                   ->GetSyncItem(kItemId2)
                   ->item_ordinal.IsValid());
  EXPECT_FALSE(app_list_syncable_service()
                   ->GetSyncItem(kItemId3)
                   ->item_ordinal.IsValid());

  app_list_syncable_service()->SetAppListPreferredOrder(
      ash::AppListSortOrder::kNameReverseAlphabetical);
  EXPECT_EQ(GetOrderedItemIdsFromSyncableService(),
            std::vector<std::string>({kItemId3, kItemId2, kItemId1}));

  app_list_syncable_service()->SetAppListPreferredOrder(
      ash::AppListSortOrder::kNameAlphabetical);
  EXPECT_EQ(GetOrderedItemIdsFromSyncableService(),
            std::vector<std::string>({kItemId1, kItemId2, kItemId3}));
}

// Verifies that sorting with alphateical order works as expected for both
// folder items and app items.
TEST_F(AppListSyncableServiceTest, VerifyAlphabeticalOrderForFolderItems) {
  RemoveAllExistingItems();
  syncer::SyncDataList sync_list;

  // Add two apps.
  const std::string kItemId1 = CreateNextAppId(extensions::kWebStoreAppId);
  sync_list.push_back(
      CreateAppRemoteData(kItemId1, "a", "", GetLastPositionString(), kUnset));
  const std::string kItemId2 = CreateNextAppId(kItemId1);
  sync_list.push_back(
      CreateAppRemoteData(kItemId2, "b", "", GetLastPositionString(), kUnset));

  // Add one folder containing two apps.
  const std::string kFolderId1 = GenerateId("FolderId1");
  const std::string kChildItemId1_1 = CreateNextAppId(kItemId2);
  const std::string kChildItemId1_2 = CreateNextAppId(kChildItemId1_1);
  sync_list.push_back(CreateAppRemoteData(
      kFolderId1, "Folder1", "", GetLastPositionString(), kUnset,
      sync_pb::AppListSpecifics_AppListItemType_TYPE_FOLDER));
  sync_list.push_back(CreateAppRemoteData(kChildItemId1_1, "folder1_child1",
                                          kFolderId1, GetLastPositionString(),
                                          kUnset));
  sync_list.push_back(CreateAppRemoteData(kChildItemId1_2, "folder1_child2",
                                          kFolderId1, GetLastPositionString(),
                                          kUnset));

  // Add one folder containing three apps.
  const std::string kFolderId2 = GenerateId("FolderId2");
  const std::string kChildItemId2_1 = CreateNextAppId(kChildItemId1_2);
  const std::string kChildItemId2_2 = CreateNextAppId(kChildItemId2_1);
  const std::string kChildItemId2_3 = CreateNextAppId(kChildItemId2_2);
  sync_list.push_back(CreateAppRemoteData(
      kFolderId2, "Folder2", "", GetLastPositionString(), kUnset,
      sync_pb::AppListSpecifics_AppListItemType_TYPE_FOLDER));
  sync_list.push_back(CreateAppRemoteData(kChildItemId2_1, "folder2_child1",
                                          kFolderId2, GetLastPositionString(),
                                          kUnset));
  sync_list.push_back(CreateAppRemoteData(kChildItemId2_2, "folder2_child2",
                                          kFolderId2, GetLastPositionString(),
                                          kUnset));
  sync_list.push_back(CreateAppRemoteData(kChildItemId2_3, "folder2_child3",
                                          kFolderId2, GetLastPositionString(),
                                          kUnset));

  app_list_syncable_service()->MergeDataAndStartSyncing(
      syncer::APP_LIST, sync_list,
      std::make_unique<syncer::FakeSyncChangeProcessor>());
  content::RunAllTasksUntilIdle();

  // Check the default status before sorting.
  EXPECT_EQ(
      GetOrderedItemIdsFromSyncableService(),
      std::vector<std::string>({kItemId1, kItemId2, kFolderId1, kChildItemId1_1,
                                kChildItemId1_2, kFolderId2, kChildItemId2_1,
                                kChildItemId2_2, kChildItemId2_3}));

  app_list_syncable_service()->SetAppListPreferredOrder(
      ash::AppListSortOrder::kNameReverseAlphabetical);

  // Folders should be sorted alphabetically with apps.
  EXPECT_EQ(
      GetOrderedItemIdsFromSyncableService(),
      std::vector<std::string>(
          {kChildItemId2_3, kChildItemId2_2, kChildItemId2_1, kFolderId2,
           kChildItemId1_2, kChildItemId1_1, kFolderId1, kItemId2, kItemId1}));
}

// Verifies that sorting app items with the alphabetical order should work as
// expected. Meanwhile, sorting should incur the minimum orinal changes.
TEST_F(AppListSyncableServiceTest, VerifyAlphabeticalOrderSort) {
  RemoveAllExistingItems();
  syncer::SyncDataList sync_list;
  const std::string kItemId1 = CreateNextAppId(extensions::kWebStoreAppId);
  sync_list.push_back(
      CreateAppRemoteData(kItemId1, "A", "", GetLastPositionString(), kUnset));
  const std::string kItemId2 = CreateNextAppId(kItemId1);
  sync_list.push_back(
      CreateAppRemoteData(kItemId2, "B", "", GetLastPositionString(), kUnset));
  const std::string kItemId3 = CreateNextAppId(kItemId2);
  sync_list.push_back(
      CreateAppRemoteData(kItemId3, "C", "", GetLastPositionString(), kUnset));
  const std::string kItemId4 = CreateNextAppId(kItemId3);
  sync_list.push_back(
      CreateAppRemoteData(kItemId4, "D", "", GetLastPositionString(), kUnset));
  const std::string kItemId5 = CreateNextAppId(kItemId4);
  sync_list.push_back(
      CreateAppRemoteData(kItemId5, "E", "", GetLastPositionString(), kUnset));

  app_list_syncable_service()->MergeDataAndStartSyncing(
      syncer::APP_LIST, sync_list,
      std::make_unique<syncer::FakeSyncChangeProcessor>());
  content::RunAllTasksUntilIdle();

  // Record the mappings between ids and default ordinals.
  std::unordered_map<std::string, syncer::StringOrdinal> id_ordinal_mappings;
  for (const auto& id_item_pair : app_list_syncable_service()->sync_items()) {
    id_ordinal_mappings[id_item_pair.first] = id_item_pair.second->item_ordinal;
  }

  // Sorting in alphabetical order should not change any ordinal. Because apps
  // are already in order.
  EXPECT_EQ(GetOrderedItemIdsFromSyncableService(),
            std::vector<std::string>(
                {kItemId1, kItemId2, kItemId3, kItemId4, kItemId5}));
  app_list_syncable_service()->SetAppListPreferredOrder(
      ash::AppListSortOrder::kNameAlphabetical);
  for (const auto& id_item_pair : app_list_syncable_service()->sync_items()) {
    EXPECT_EQ(id_ordinal_mappings[id_item_pair.first],
              id_item_pair.second->item_ordinal);
  }

  // Sort in reverse alphabetical order. Verify the app order after sorting.
  app_list_syncable_service()->SetAppListPreferredOrder(
      ash::AppListSortOrder::kNameReverseAlphabetical);
  EXPECT_EQ(GetOrderedItemIdsFromSyncableService(),
            std::vector<std::string>(
                {kItemId5, kItemId4, kItemId3, kItemId2, kItemId1}));

  const auto* sync_item = app_list_syncable_service()->GetSyncItem(kItemId4);
  syncer::SyncChangeList change_list{syncer::SyncChange(
      FROM_HERE, syncer::SyncChange::ACTION_UPDATE,
      CreateAppRemoteData(kItemId4, sync_item->item_name, sync_item->parent_id,
                          app_list_syncable_service()
                              ->GetSyncItem(kItemId1)
                              ->item_ordinal.CreateAfter()
                              .ToDebugString(),
                          sync_item->item_pin_ordinal.ToDebugString()))};
  app_list_syncable_service()->ProcessSyncChanges(base::Location(),
                                                  change_list);
  content::RunAllTasksUntilIdle();

  // Move Item 4 to the end. Record the mappings between ids and ordinals.
  EXPECT_EQ(GetOrderedItemIdsFromSyncableService(),
            std::vector<std::string>(
                {kItemId5, kItemId3, kItemId2, kItemId1, kItemId4}));
  id_ordinal_mappings.clear();
  for (const auto& id_item_pair : app_list_syncable_service()->sync_items()) {
    id_ordinal_mappings[id_item_pair.first] = id_item_pair.second->item_ordinal;
  }

  // Sort and then verify the app order.
  app_list_syncable_service()->SetAppListPreferredOrder(
      ash::AppListSortOrder::kNameReverseAlphabetical);
  EXPECT_EQ(GetOrderedItemIdsFromSyncableService(),
            std::vector<std::string>(
                {kItemId5, kItemId4, kItemId3, kItemId2, kItemId1}));

  // Verify that only Item 4's ordinal changes.
  for (const auto& id_item_pair : app_list_syncable_service()->sync_items()) {
    if (id_item_pair.first == kItemId4) {
      EXPECT_NE(id_ordinal_mappings[id_item_pair.first],
                id_item_pair.second->item_ordinal);
    } else {
      EXPECT_EQ(id_ordinal_mappings[id_item_pair.first],
                id_item_pair.second->item_ordinal);
    }
  }
}

// Verifies that sorting app items with the alphabetical order should work for
// the apps with the duplicate names.
TEST_F(AppListSyncableServiceTest, VerifyAlphabeticalSortWithDuplicateNames) {
  RemoveAllExistingItems();
  syncer::SyncDataList sync_list;
  const std::string kItemId1 = CreateNextAppId(extensions::kWebStoreAppId);
  sync_list.push_back(
      CreateAppRemoteData(kItemId1, "B", "", GetLastPositionString(), kUnset));
  const std::string kItemId2 = CreateNextAppId(kItemId1);
  sync_list.push_back(
      CreateAppRemoteData(kItemId2, "A", "", GetLastPositionString(), kUnset));
  const std::string kItemId3 = CreateNextAppId(kItemId2);
  sync_list.push_back(
      CreateAppRemoteData(kItemId3, "C", "", GetLastPositionString(), kUnset));
  const std::string kItemId4 = CreateNextAppId(kItemId3);
  sync_list.push_back(
      CreateAppRemoteData(kItemId4, "D", "", GetLastPositionString(), kUnset));
  const std::string kItemId5 = CreateNextAppId(kItemId4);
  sync_list.push_back(
      CreateAppRemoteData(kItemId5, "C", "", GetLastPositionString(), kUnset));
  const std::string kItemId6 = CreateNextAppId(kItemId5);
  sync_list.push_back(
      CreateAppRemoteData(kItemId6, "A", "", GetLastPositionString(), kUnset));

  app_list_syncable_service()->MergeDataAndStartSyncing(
      syncer::APP_LIST, sync_list,
      std::make_unique<syncer::FakeSyncChangeProcessor>());
  content::RunAllTasksUntilIdle();

  app_list_syncable_service()->SetAppListPreferredOrder(
      ash::AppListSortOrder::kNameAlphabetical);
  EXPECT_EQ(GetOrderedNamesFromSyncableService(),
            std::vector<std::string>({"A", "A", "B", "C", "C", "D"}));

  app_list_syncable_service()->SetAppListPreferredOrder(
      ash::AppListSortOrder::kNameReverseAlphabetical);
  EXPECT_EQ(GetOrderedNamesFromSyncableService(),
            std::vector<std::string>({"D", "C", "C", "B", "A", "A"}));
}

// Verifies that a new app is placed at the correct place when the launcher is
// in (reverse) alphabetical order.
TEST_F(AppListSyncableServiceTest, NewAppPlacement) {
  RemoveAllExistingItems();
  EXPECT_EQ(ash::AppListSortOrder::kCustom, GetSortOrderFromPrefs());

  const std::string kItemId1 = CreateNextAppId(extensions::kWebStoreAppId);
  const std::string kItemId2 = CreateNextAppId(kItemId1);
  const std::string kItemId3 = CreateNextAppId(kItemId2);
  const std::string kItemId4 = CreateNextAppId(kItemId3);

  scoped_refptr<extensions::Extension> app1 =
      MakeApp("A", kItemId1, extensions::Extension::NO_FLAGS);
  InstallExtension(app1.get());
  scoped_refptr<extensions::Extension> app2 =
      MakeApp("B", kItemId2, extensions::Extension::NO_FLAGS);
  InstallExtension(app2.get());
  scoped_refptr<extensions::Extension> app3 =
      MakeApp("C", kItemId3, extensions::Extension::NO_FLAGS);
  InstallExtension(app3.get());
  scoped_refptr<extensions::Extension> app4 =
      MakeApp("E", kItemId4, extensions::Extension::NO_FLAGS);
  InstallExtension(app4.get());

  app_list_syncable_service()->SetAppListPreferredOrder(
      ash::AppListSortOrder::kNameReverseAlphabetical);
  EXPECT_EQ(ash::AppListSortOrder::kNameReverseAlphabetical,
            GetSortOrderFromPrefs());
  EXPECT_EQ(GetOrderedNamesFromSyncableService(),
            std::vector<std::string>({"E", "C", "B", "A"}));

  // Insert another app. Verify the order.
  const std::string kItemId5 = CreateNextAppId(kItemId4);
  scoped_refptr<extensions::Extension> app5 =
      MakeApp("D", kItemId5, extensions::Extension::NO_FLAGS);
  InstallExtension(app5.get());
  EXPECT_EQ(GetOrderedNamesFromSyncableService(),
            std::vector<std::string>({"E", "D", "C", "B", "A"}));

  // The longest subsequence in reverse alphabetical order is the whole
  // sequence. Therefore the entropy is (1 - 5/5), which is 0.
  AppListModelUpdater* model_updater = GetModelUpdater();
  EXPECT_TRUE(cc::MathUtil::IsWithinEpsilon(
      0.f,
      reorder::CalculateEntropyForTest(
          ash::AppListSortOrder::kNameReverseAlphabetical, model_updater)));

  // The longest subsequence in alphabetical order has one element so the
  // entropy is (1 - 1/5) which is 0.8.
  EXPECT_EQ(0.8f, reorder::CalculateEntropyForTest(
                      ash::AppListSortOrder::kNameAlphabetical, model_updater));

  app_list_syncable_service()->SetAppListPreferredOrder(
      ash::AppListSortOrder::kNameAlphabetical);
  EXPECT_EQ(ash::AppListSortOrder::kNameAlphabetical, GetSortOrderFromPrefs());
  EXPECT_EQ(GetOrderedNamesFromSyncableService(),
            std::vector<std::string>({"A", "B", "C", "D", "E"}));

  ChangeItemName(kItemId3, "Z");
  EXPECT_EQ(GetOrderedNamesFromSyncableService(),
            std::vector<std::string>({"A", "B", "Z", "D", "E"}));

  // The longest subsequence in order is ["A", "B", "D", "E"] so the entropy is
  // (1 - 4/5) which is 0.2.
  EXPECT_TRUE(cc::MathUtil::IsWithinEpsilon(
      0.2f, reorder::CalculateEntropyForTest(
                ash::AppListSortOrder::kNameAlphabetical, model_updater)));

  // Install a new app. Verify its location.
  const std::string kItemId6 = CreateNextAppId(kItemId5);
  scoped_refptr<extensions::Extension> app6 =
      MakeApp("C", kItemId6, extensions::Extension::NO_FLAGS);
  InstallExtension(app6.get());
  EXPECT_EQ(GetOrderedNamesFromSyncableService(),
            std::vector<std::string>({"A", "B", "C", "Z", "D", "E"}));

  // Change another app's name.
  ChangeItemName(kItemId2, "F");
  EXPECT_EQ(GetOrderedNamesFromSyncableService(),
            std::vector<std::string>({"A", "F", "C", "Z", "D", "E"}));

  // The longest subsequence in order is ["A", "C", "D", "E"] so the entropy is
  // (1 - 4/6).
  EXPECT_TRUE(cc::MathUtil::IsWithinEpsilon(
      1 / 3.f, reorder::CalculateEntropyForTest(
                   ash::AppListSortOrder::kNameAlphabetical, model_updater)));

  // Install a new app.
  EXPECT_EQ(ash::AppListSortOrder::kNameAlphabetical, GetSortOrderFromPrefs());
  const std::string kItemId7 = CreateNextAppId(kItemId6);
  scoped_refptr<extensions::Extension> app7 =
      MakeApp("G", kItemId7, extensions::Extension::NO_FLAGS);
  InstallExtension(app7.get());

  // The entropy is too high so the new app is inserted at the front.
  EXPECT_EQ(GetOrderedNamesFromSyncableService(),
            std::vector<std::string>({"G", "A", "F", "C", "Z", "D", "E"}));

  // The sort order is reset.
  EXPECT_EQ(ash::AppListSortOrder::kCustom, GetSortOrderFromPrefs());

  // Install a new app.
  const std::string kItemId8 = CreateNextAppId(kItemId7);
  scoped_refptr<extensions::Extension> app8 =
      MakeApp("H", kItemId8, extensions::Extension::NO_FLAGS);
  InstallExtension(app8.get());

  // Because the sort order is kCustom, the new app is placed at the front.
  EXPECT_EQ(GetOrderedNamesFromSyncableService(),
            std::vector<std::string>({"H", "G", "A", "F", "C", "Z", "D", "E"}));
}

// Verifies that a new app is placed at the correct place when initially all of
// top level items are folders.
TEST_F(AppListSyncableServiceTest, NewAppPlacementInitiallyOnlyFolders) {
  RemoveAllExistingItems();

  // Add three folders.
  syncer::StringOrdinal position =
      syncer::StringOrdinal::CreateInitialOrdinal();
  const std::string kFolderItemId1 = GenerateId("folder_id1");
  AppListModelUpdater* model_updater = GetModelUpdater();
  std::unique_ptr<ChromeAppListItem> folder_item1 =
      std::make_unique<ChromeAppListItem>(profile_.get(), kFolderItemId1,
                                          model_updater);
  folder_item1->SetChromeIsFolder(true);
  ItemTestApi(folder_item1.get()).SetPosition(position);
  ItemTestApi(folder_item1.get()).SetName("Folder1");
  app_list_syncable_service()->AddItem(std::move(folder_item1));
  position = position.CreateBefore();

  const std::string kFolderItemId2 = GenerateId("folder_id2");
  std::unique_ptr<ChromeAppListItem> folder_item2 =
      std::make_unique<ChromeAppListItem>(profile_.get(), kFolderItemId2,
                                          model_updater);
  folder_item2->SetChromeIsFolder(true);
  ItemTestApi(folder_item2.get()).SetPosition(position);
  ItemTestApi(folder_item2.get()).SetName("Folder2");
  app_list_syncable_service()->AddItem(std::move(folder_item2));
  position = position.CreateBefore();

  const std::string kFolderItemId3 = GenerateId("folder_id3");
  std::unique_ptr<ChromeAppListItem> folder_item3 =
      std::make_unique<ChromeAppListItem>(profile_.get(), kFolderItemId3,
                                          model_updater);
  folder_item3->SetChromeIsFolder(true);
  ItemTestApi(folder_item3.get()).SetPosition(position);
  // Use an empty folder name. Note that empty folder name will be interpreted
  // as "Unnamed" during sorting, which is the same as the name showing to the
  // users.
  ItemTestApi(folder_item3.get()).SetName("");
  app_list_syncable_service()->AddItem(std::move(folder_item3));

  // Sort sync items then verify the item order.
  app_list_syncable_service()->SetAppListPreferredOrder(
      ash::AppListSortOrder::kNameAlphabetical);
  EXPECT_EQ(GetOrderedNamesFromSyncableService(),
            std::vector<std::string>({"Folder1", "Folder2", ""}));

  // Install a new app.
  const std::string kNewAppId = CreateNextAppId(GenerateId("app_id"));
  scoped_refptr<extensions::Extension> app =
      MakeApp("G", kNewAppId, extensions::Extension::NO_FLAGS);
  InstallExtension(app.get());

  // Verify that the app is placed alphabetically - note that empty folder name
  // is treated as "Unnamed".
  EXPECT_EQ(GetOrderedNamesFromSyncableService(),
            std::vector<std::string>({"Folder1", "Folder2", "G", ""}));

  // Verify that the entropy is zero.
  EXPECT_TRUE(cc::MathUtil::IsWithinEpsilon(
      0.f, reorder::CalculateEntropyForTest(
               ash::AppListSortOrder::kNameAlphabetical, model_updater)));

  // Install the second app.
  const std::string kNewAppId2 = CreateNextAppId(GenerateId("app_id2"));
  scoped_refptr<extensions::Extension> app2 =
      MakeApp("c", kNewAppId2, extensions::Extension::NO_FLAGS);
  InstallExtension(app2.get());

  // Verify that the app is placed alphabetically again.
  EXPECT_EQ(GetOrderedNamesFromSyncableService(),
            std::vector<std::string>({"c", "Folder1", "Folder2", "G", ""}));

  // Change folders' names so that folders are out of order.
  ChangeItemName(kFolderItemId1, "Folder2");
  ChangeItemName(kFolderItemId2, "Folder1");
  EXPECT_EQ(GetOrderedNamesFromSyncableService(),
            std::vector<std::string>({"c", "Folder2", "Folder1", "G", ""}));

  // There is one folder item out of order so the entropy should be 1/5 = 0.2.
  EXPECT_TRUE(cc::MathUtil::IsWithinEpsilon(
      0.2f, reorder::CalculateEntropyForTest(
                ash::AppListSortOrder::kNameAlphabetical, model_updater)));

  // Install the third app. Verify the item order after installation.
  const std::string kNewAppId3 = CreateNextAppId(GenerateId("app_id3"));
  scoped_refptr<extensions::Extension> app3 =
      MakeApp("Fs", kNewAppId3, extensions::Extension::NO_FLAGS);
  InstallExtension(app3.get());
  EXPECT_EQ(
      GetOrderedNamesFromSyncableService(),
      std::vector<std::string>({"c", "Folder2", "Folder1", "Fs", "G", ""}));

  // Install the forth app. Verify that the new item is inserted in alphabetical
  // order.
  const std::string kNewAppId4 = CreateNextAppId(GenerateId("app_id4"));
  scoped_refptr<extensions::Extension> app4 =
      MakeApp("z", kNewAppId4, extensions::Extension::NO_FLAGS);
  InstallExtension(app4.get());
  EXPECT_EQ(GetOrderedNamesFromSyncableService(),
            std::vector<std::string>(
                {"c", "Folder2", "Folder1", "Fs", "G", "", "z"}));
}

// Verifies that the new app's position maintains the launcher sort order among
// sync items (including the apps not enabled on the local device).
TEST_F(AppListSyncableServiceTest, VerifyNewAppPositionInGlobalScope) {
  RemoveAllExistingItems();

  const std::string kItemId1 = CreateNextAppId(GenerateId("app_id1"));
  scoped_refptr<extensions::Extension> app1 =
      MakeApp("C", kItemId1, extensions::Extension::NO_FLAGS);
  InstallExtension(app1.get());

  const std::string kItemId2 = CreateNextAppId(GenerateId("app_id2"));
  scoped_refptr<extensions::Extension> app2 =
      MakeApp("A", kItemId2, extensions::Extension::NO_FLAGS);
  InstallExtension(app2.get());

  const std::string kItemId3 = CreateNextAppId(GenerateId("app_id3"));
  scoped_refptr<extensions::Extension> app3 =
      MakeApp("D", kItemId3, extensions::Extension::NO_FLAGS);
  InstallExtension(app3.get());

  // The sort order is not set. Therefore an app is always placed at the front.
  EXPECT_EQ(GetOrderedNamesFromSyncableService(),
            std::vector<std::string>({"D", "A", "C"}));

  app_list_syncable_service()->SetAppListPreferredOrder(
      ash::AppListSortOrder::kNameAlphabetical);
  EXPECT_EQ(ash::AppListSortOrder::kNameAlphabetical, GetSortOrderFromPrefs());
  EXPECT_EQ(GetOrderedNamesFromSyncableService(),
            std::vector<std::string>({"A", "C", "D"}));

  // A hacky way to emulate that an item is disabled locally (in other words,
  // the app's sync data exists but its app list item data is missing).
  AppListModelUpdater* model_updater = GetModelUpdater();
  model_updater->RemoveItem(kItemId1, /*is_uninstall=*/true);

  // Install a new app and verify the app order.
  const std::string kItemId4 = CreateNextAppId(GenerateId("app_id4"));
  scoped_refptr<extensions::Extension> app4 =
      MakeApp("B", kItemId4, extensions::Extension::NO_FLAGS);
  InstallExtension(app4.get());
  EXPECT_EQ(GetOrderedNamesFromSyncableService(),
            std::vector<std::string>({"A", "B", "C", "D"}));

  // Remove another item from the model. Now only "A" and "B" are in the model.
  model_updater->RemoveItem(kItemId3, /*is_uninstall=*/true);

  // Install a new app and verify the app order.
  const std::string kItemId5 = CreateNextAppId(GenerateId("app_id5"));
  scoped_refptr<extensions::Extension> app5 =
      MakeApp("F", kItemId5, extensions::Extension::NO_FLAGS);
  InstallExtension(app5.get());
  EXPECT_EQ(GetOrderedNamesFromSyncableService(),
            std::vector<std::string>({"A", "B", "C", "D", "F"}));

  // Remove another item from the model. Now only "A" and "B" are in the model.
  model_updater->RemoveItem(kItemId5, /*is_uninstall=*/true);

  // Install a new app and verify the app order.
  const std::string kItemId6 = CreateNextAppId(GenerateId("app_id6"));
  scoped_refptr<extensions::Extension> app6 =
      MakeApp("E", kItemId6, extensions::Extension::NO_FLAGS);
  InstallExtension(app6.get());
  EXPECT_EQ(GetOrderedNamesFromSyncableService(),
            std::vector<std::string>({"A", "B", "C", "D", "E", "F"}));
}

TEST_F(AppListSyncableServiceTest, RemovePageBreaksIfAppsDontFillUpAPage) {
  RemoveAllExistingItems();
  syncer::SyncDataList sync_list;
  const std::string kItemId1 = CreateNextAppId(extensions::kWebStoreAppId);
  sync_list.push_back(
      CreateAppRemoteData(kItemId1, "B", "", GetLastPositionString(), kUnset));
  const std::string kItemId2 = CreateNextAppId(kItemId1);
  sync_list.push_back(
      CreateAppRemoteData(kItemId2, "A", "", GetLastPositionString(), kUnset));
  const std::string kPageBreak1 = CreateNextAppId(kItemId2);
  sync_list.push_back(CreateAppRemoteData(
      kPageBreak1, "page_break_1", "", GetLastPositionString(), kUnset,
      sync_pb::AppListSpecifics_AppListItemType_TYPE_PAGE_BREAK));
  const std::string kItemId3 = CreateNextAppId(kPageBreak1);
  sync_list.push_back(
      CreateAppRemoteData(kItemId3, "C", "", GetLastPositionString(), kUnset));
  const std::string kPageBreak2 = CreateNextAppId(kItemId3);
  sync_list.push_back(CreateAppRemoteData(
      kPageBreak2, "page_break_2", "", GetLastPositionString(), kUnset,
      sync_pb::AppListSpecifics_AppListItemType_TYPE_PAGE_BREAK));
  const std::string kItemId4 = CreateNextAppId(kPageBreak2);
  sync_list.push_back(
      CreateAppRemoteData(kItemId4, "D", "", GetLastPositionString(), kUnset));

  app_list_syncable_service()->MergeDataAndStartSyncing(
      syncer::APP_LIST, sync_list,
      std::make_unique<syncer::FakeSyncChangeProcessor>());
  content::RunAllTasksUntilIdle();

  scoped_refptr<extensions::Extension> app1 =
      MakeApp("B", kItemId1, extensions::Extension::NO_FLAGS);
  InstallExtension(app1.get());

  scoped_refptr<extensions::Extension> app3 =
      MakeApp("C", kItemId3, extensions::Extension::NO_FLAGS);
  InstallExtension(app1.get());

  app_list_syncable_service()->SetAppListPreferredOrder(
      ash::AppListSortOrder::kNameAlphabetical);
  EXPECT_EQ(ash::AppListSortOrder::kNameAlphabetical, GetSortOrderFromPrefs());
  EXPECT_EQ(GetOrderedNamesFromSyncableService(),
            std::vector<std::string>({"A", "B", "C", "D"}));
}

TEST_F(AppListSyncableServiceTest,
       RemovePageBreaksIfAppCountMatchesLegacyPageSize) {
  RemoveAllExistingItems();
  syncer::SyncDataList sync_list;
  std::string last_item_id = extensions::kWebStoreAppId;
  for (int i = 0; i < 20; ++i) {
    if (i == 10) {
      sync_list.push_back(CreateAppRemoteData(
          "page_break_1", "page_break_1", "", GetLastPositionString(), kUnset,
          sync_pb::AppListSpecifics_AppListItemType_TYPE_PAGE_BREAK));
    }

    last_item_id = CreateNextAppId(last_item_id);
    const std::string item_name = base::StringPrintf("Item %d", i);
    sync_list.push_back(CreateAppRemoteData(last_item_id, item_name, "",
                                            GetLastPositionString(), kUnset));
    if (i % 2) {
      scoped_refptr<extensions::Extension> app =
          MakeApp(item_name, last_item_id, extensions::Extension::NO_FLAGS);
      InstallExtension(app.get());
    }
  }
  app_list_syncable_service()->MergeDataAndStartSyncing(
      syncer::APP_LIST, sync_list,
      std::make_unique<syncer::FakeSyncChangeProcessor>());
  content::RunAllTasksUntilIdle();

  app_list_syncable_service()->SetAppListPreferredOrder(
      ash::AppListSortOrder::kNameAlphabetical);
  EXPECT_EQ(ash::AppListSortOrder::kNameAlphabetical, GetSortOrderFromPrefs());
  EXPECT_EQ(GetNamesOfSortedItemsPerPageFromSyncableService(),
            std::vector<std::vector<std::string>>(
                {{"Item 0",  "Item 1",  "Item 10", "Item 11", "Item 12",
                  "Item 13", "Item 14", "Item 15", "Item 16", "Item 17",
                  "Item 18", "Item 19", "Item 2",  "Item 3",  "Item 4",
                  "Item 5",  "Item 6",  "Item 7",  "Item 8",  "Item 9"}}));
}

TEST_F(AppListSyncableServiceTest, PageBreaksAfterSortWithTwoPagesInSync) {
  RemoveAllExistingItems();
  syncer::SyncDataList sync_list;
  std::string last_item_id = extensions::kWebStoreAppId;
  for (int i = 24; i >= 0; --i) {
    if (i == 10) {
      sync_list.push_back(CreateAppRemoteData(
          "page_break_1", "page_break_1", "", GetLastPositionString(), kUnset,
          sync_pb::AppListSpecifics_AppListItemType_TYPE_PAGE_BREAK));
    }

    last_item_id = CreateNextAppId(last_item_id);
    const std::string item_name = base::StringPrintf("Item %d", i);
    sync_list.push_back(CreateAppRemoteData(last_item_id, item_name, "",
                                            GetLastPositionString(), kUnset));
    scoped_refptr<extensions::Extension> app =
        MakeApp(item_name, last_item_id, extensions::Extension::NO_FLAGS);
    InstallExtension(app.get());
  }

  app_list_syncable_service()->MergeDataAndStartSyncing(
      syncer::APP_LIST, sync_list,
      std::make_unique<syncer::FakeSyncChangeProcessor>());
  content::RunAllTasksUntilIdle();

  app_list_syncable_service()->SetAppListPreferredOrder(
      ash::AppListSortOrder::kNameAlphabetical);
  EXPECT_EQ(ash::AppListSortOrder::kNameAlphabetical, GetSortOrderFromPrefs());
  EXPECT_EQ(GetNamesOfSortedItemsPerPageFromSyncableService(),
            std::vector<std::vector<std::string>>(
                {{"Item 0",  "Item 1",  "Item 10", "Item 11", "Item 12",
                  "Item 13", "Item 14", "Item 15", "Item 16", "Item 17",
                  "Item 18", "Item 19", "Item 2",  "Item 20", "Item 21",
                  "Item 22", "Item 23", "Item 24", "Item 3",  "Item 4"},
                 {"Item 5", "Item 6", "Item 7", "Item 8", "Item 9"}}));
}

TEST_F(AppListSyncableServiceTest,
       PageBreaksAfterSortWithTwoPagesAndNonInstalledItemsInSync) {
  RemoveAllExistingItems();
  syncer::SyncDataList sync_list;
  std::string last_item_id = extensions::kWebStoreAppId;
  for (int i = 33; i >= 0; --i) {
    if (i == 10) {
      sync_list.push_back(CreateAppRemoteData(
          "page_break_1", "page_break_1", "", GetLastPositionString(), kUnset,
          sync_pb::AppListSpecifics_AppListItemType_TYPE_PAGE_BREAK));
    }

    last_item_id = CreateNextAppId(last_item_id);
    const std::string item_name = base::StringPrintf("Item %d", i);
    sync_list.push_back(CreateAppRemoteData(last_item_id, item_name, "",
                                            GetLastPositionString(), kUnset));
    scoped_refptr<extensions::Extension> app =
        MakeApp(item_name, last_item_id, extensions::Extension::NO_FLAGS);

    // Leave subset of apps non-installed.
    if (i % 3)
      InstallExtension(app.get());
  }

  app_list_syncable_service()->MergeDataAndStartSyncing(
      syncer::APP_LIST, sync_list,
      std::make_unique<syncer::FakeSyncChangeProcessor>());
  content::RunAllTasksUntilIdle();

  app_list_syncable_service()->SetAppListPreferredOrder(
      ash::AppListSortOrder::kNameAlphabetical);
  EXPECT_EQ(ash::AppListSortOrder::kNameAlphabetical, GetSortOrderFromPrefs());
  EXPECT_EQ(
      GetNamesOfSortedItemsPerPageFromSyncableService(),
      std::vector<std::vector<std::string>>(
          {{"Item 0",  "Item 1",  "Item 10", "Item 11", "Item 12", "Item 13",
            "Item 14", "Item 15", "Item 16", "Item 17", "Item 18", "Item 19",
            "Item 2",  "Item 20", "Item 21", "Item 22", "Item 23", "Item 24",
            "Item 25", "Item 26", "Item 27", "Item 28", "Item 29", "Item 3",
            "Item 30", "Item 31", "Item 32", "Item 33", "Item 4",  "Item 5",
            "Item 6"},
           {"Item 7", "Item 8", "Item 9"}}));
}

TEST_F(AppListSyncableServiceTest,
       PageBreaksAfterSortWithTwoPagesAndAFolderInSync) {
  RemoveAllExistingItems();

  const std::string kFolderId = GenerateId("folder_id");

  std::vector<scoped_refptr<extensions::Extension>> initial_apps;
  syncer::SyncDataList sync_list;
  std::string last_item_id = extensions::kWebStoreAppId;
  for (int i = 24; i >= 0; --i) {
    if (i == 10) {
      sync_list.push_back(CreateAppRemoteData(
          "page_break_1", "page_break_1", "", GetLastPositionString(), kUnset,
          sync_pb::AppListSpecifics_AppListItemType_TYPE_PAGE_BREAK));
    }

    if (i == 20) {
      sync_list.push_back(CreateAppRemoteData(
          kFolderId, "Folder", "", GetLastPositionString(), "",
          sync_pb::AppListSpecifics_AppListItemType_TYPE_FOLDER));
    }

    last_item_id = CreateNextAppId(last_item_id);
    const std::string item_name = base::StringPrintf("Item %d", i);
    sync_list.push_back(CreateAppRemoteData(last_item_id, item_name,
                                            i >= 15 && i < 20 ? kFolderId : "",
                                            GetLastPositionString(), kUnset));
    initial_apps.push_back(
        MakeApp(item_name, last_item_id, extensions::Extension::NO_FLAGS));
  }

  for (auto app : initial_apps)
    InstallExtension(app.get());

  app_list_syncable_service()->MergeDataAndStartSyncing(
      syncer::APP_LIST, sync_list,
      std::make_unique<syncer::FakeSyncChangeProcessor>());
  content::RunAllTasksUntilIdle();

  app_list_syncable_service()->SetAppListPreferredOrder(
      ash::AppListSortOrder::kNameAlphabetical);
  EXPECT_EQ(ash::AppListSortOrder::kNameAlphabetical, GetSortOrderFromPrefs());
  // Note that items Item 15 - Item 19 are in the folder.
  EXPECT_EQ(GetNamesOfSortedItemsPerPageFromSyncableService(),
            std::vector<std::vector<std::string>>(
                {{"Folder",  "Item 0",  "Item 1",  "Item 10", "Item 11",
                  "Item 12", "Item 13", "Item 14", "Item 2",  "Item 20",
                  "Item 21", "Item 22", "Item 23", "Item 24", "Item 3",
                  "Item 4",  "Item 5",  "Item 6",  "Item 7",  "Item 8"},
                 {"Item 9"}}));
}

TEST_F(AppListSyncableServiceTest, PageBreaksAfterSortWithTwoFullPagesInSync) {
  RemoveAllExistingItems();
  syncer::SyncDataList sync_list;
  std::string last_item_id = extensions::kWebStoreAppId;
  for (int i = 39; i >= 0; --i) {
    if (i == 10) {
      sync_list.push_back(CreateAppRemoteData(
          "page_break_1", "page_break_1", "", GetLastPositionString(), kUnset,
          sync_pb::AppListSpecifics_AppListItemType_TYPE_PAGE_BREAK));
    }
    if (i == 25) {
      sync_list.push_back(CreateAppRemoteData(
          "page_break_2", "page_break_2", "", GetLastPositionString(), kUnset,
          sync_pb::AppListSpecifics_AppListItemType_TYPE_PAGE_BREAK));
    }

    last_item_id = CreateNextAppId(last_item_id);
    const std::string item_name = base::StringPrintf("Item %d", i);
    sync_list.push_back(CreateAppRemoteData(last_item_id, item_name, "",
                                            GetLastPositionString(), kUnset));
    scoped_refptr<extensions::Extension> app =
        MakeApp(item_name, last_item_id, extensions::Extension::NO_FLAGS);
    InstallExtension(app.get());
  }

  app_list_syncable_service()->MergeDataAndStartSyncing(
      syncer::APP_LIST, sync_list,
      std::make_unique<syncer::FakeSyncChangeProcessor>());
  content::RunAllTasksUntilIdle();

  app_list_syncable_service()->SetAppListPreferredOrder(
      ash::AppListSortOrder::kNameAlphabetical);
  EXPECT_EQ(ash::AppListSortOrder::kNameAlphabetical, GetSortOrderFromPrefs());
  EXPECT_EQ(GetNamesOfSortedItemsPerPageFromSyncableService(),
            std::vector<std::vector<std::string>>(
                {{"Item 0",  "Item 1",  "Item 10", "Item 11", "Item 12",
                  "Item 13", "Item 14", "Item 15", "Item 16", "Item 17",
                  "Item 18", "Item 19", "Item 2",  "Item 20", "Item 21",
                  "Item 22", "Item 23", "Item 24", "Item 25", "Item 26"},
                 {"Item 27", "Item 28", "Item 29", "Item 3",  "Item 30",
                  "Item 31", "Item 32", "Item 33", "Item 34", "Item 35",
                  "Item 36", "Item 37", "Item 38", "Item 39", "Item 4",
                  "Item 5",  "Item 6",  "Item 7",  "Item 8",  "Item 9"}}));
}

TEST_F(AppListSyncableServiceTest, DefaultPositionOfContainerApp) {
  // Use youtube as a stand-in for container app - a default app that takes
  // default app position for new users only.
  scoped_refptr<extensions::Extension> youtube =
      MakeApp(kSomeAppName, extension_misc::kYoutubeAppId,
              extensions::Extension::WAS_INSTALLED_BY_DEFAULT);

  app_list_syncable_service()
      ->set_app_default_positioned_for_new_users_only_for_test(
          extension_misc::kYoutubeAppId);

  // Merge empty sync data, and verify the app item ordinal matches the default
  // item ordinal.
  app_list_syncable_service()->MergeDataAndStartSyncing(
      syncer::APP_LIST, syncer::SyncDataList(),
      std::make_unique<syncer::FakeSyncChangeProcessor>());
  content::RunAllTasksUntilIdle();

  InstallExtension(youtube.get());

  std::vector<std::string> app_ids = GetOrderedItemIdsFromSyncableService();
  ASSERT_GE(app_ids.size(), 0u);
  EXPECT_NE(extension_misc::kYoutubeAppId, app_ids[0]);

  const AppListSyncableService::SyncItem* youtube_sync_item =
      GetSyncItem(extension_misc::kYoutubeAppId);
  ChromeAppListItem* youtube_item =
      GetModelUpdater()->FindItem(extension_misc::kYoutubeAppId);

  EXPECT_EQ(youtube_sync_item->item_ordinal,
            youtube_item->CalculateDefaultPositionIfApplicable());
}

TEST_F(AppListSyncableServiceTest,
       DefaultPositionOfContainerAppWithDelayedInitialSync) {
  // Use youtube as a stand-in for container app - a default app that takes
  // default app position for new users only.
  scoped_refptr<extensions::Extension> youtube =
      MakeApp(kSomeAppName, extension_misc::kYoutubeAppId,
              extensions::Extension::WAS_INSTALLED_BY_DEFAULT);

  app_list_syncable_service()
      ->set_app_default_positioned_for_new_users_only_for_test(
          extension_misc::kYoutubeAppId);

  InstallExtension(youtube.get());

  std::vector<std::string> app_ids = GetOrderedItemIdsFromSyncableService();
  ASSERT_GE(app_ids.size(), 0u);
  EXPECT_NE(extension_misc::kYoutubeAppId, app_ids[0]);

  const AppListSyncableService::SyncItem* youtube_sync_item =
      GetSyncItem(extension_misc::kYoutubeAppId);
  ChromeAppListItem* youtube_item =
      GetModelUpdater()->FindItem(extension_misc::kYoutubeAppId);

  EXPECT_EQ(youtube_sync_item->item_ordinal,
            youtube_item->CalculateDefaultPositionIfApplicable());

  // Merge empty sync data, and verify the app item ordinal matches the default
  // item ordinal.
  app_list_syncable_service()->MergeDataAndStartSyncing(
      syncer::APP_LIST, syncer::SyncDataList(),
      std::make_unique<syncer::FakeSyncChangeProcessor>());
  content::RunAllTasksUntilIdle();

  app_ids = GetOrderedItemIdsFromSyncableService();
  ASSERT_GE(app_ids.size(), 0u);
  EXPECT_NE(extension_misc::kYoutubeAppId, app_ids[0]);

  youtube_sync_item = GetSyncItem(extension_misc::kYoutubeAppId);
  youtube_item = GetModelUpdater()->FindItem(extension_misc::kYoutubeAppId);

  EXPECT_EQ(youtube_sync_item->item_ordinal,
            youtube_item->CalculateDefaultPositionIfApplicable());
}

TEST_F(AppListSyncableServiceTest,
       PositionOfContainerAppWithNonEmptyLocalState) {
  // Make sure the local app list state is non-empty, and restart app list
  // syncable service.
  scoped_refptr<extensions::Extension> webstore =
      MakeApp(kSomeAppName, extensions::kWebStoreAppId,
              extensions::Extension::WAS_INSTALLED_BY_DEFAULT);
  InstallExtension(webstore.get());

  RestartSyncableService();

  // Use youtube as a stand-in for container app - a default app that takes
  // default app position for new users only.
  scoped_refptr<extensions::Extension> youtube =
      MakeApp(kSomeAppName, extension_misc::kYoutubeAppId,
              extensions::Extension::WAS_INSTALLED_BY_DEFAULT);

  app_list_syncable_service()
      ->set_app_default_positioned_for_new_users_only_for_test(
          extension_misc::kYoutubeAppId);
  // Install the test app, and verify that it's positioned as the first app in
  // the app list (as it's installed in session where app list syncable service
  // local state was not initially empty).
  InstallExtension(youtube.get());

  std::vector<std::string> app_ids = GetOrderedItemIdsFromSyncableService();
  ASSERT_GE(app_ids.size(), 0u);
  EXPECT_EQ(extension_misc::kYoutubeAppId, app_ids[0]);

  const AppListSyncableService::SyncItem* youtube_sync_item =
      GetSyncItem(extension_misc::kYoutubeAppId);
  ChromeAppListItem* youtube_item =
      GetModelUpdater()->FindItem(extension_misc::kYoutubeAppId);

  EXPECT_NE(youtube_sync_item->item_ordinal,
            youtube_item->CalculateDefaultPositionIfApplicable());

  // Verify that the test app position does not changes after empty sync.
  app_list_syncable_service()->MergeDataAndStartSyncing(
      syncer::APP_LIST, syncer::SyncDataList(),
      std::make_unique<syncer::FakeSyncChangeProcessor>());
  content::RunAllTasksUntilIdle();

  app_ids = GetOrderedItemIdsFromSyncableService();
  ASSERT_GE(app_ids.size(), 0u);
  EXPECT_EQ(extension_misc::kYoutubeAppId, app_ids[0]);

  youtube_sync_item = GetSyncItem(extension_misc::kYoutubeAppId);
  youtube_item = GetModelUpdater()->FindItem(extension_misc::kYoutubeAppId);

  EXPECT_NE(youtube_sync_item->item_ordinal,
            youtube_item->CalculateDefaultPositionIfApplicable());
}

TEST_F(AppListSyncableServiceTest, PositionOfContainerAppWithNonEmptySyncData) {
  // Use youtube as a stand-in for container app - a default app that takes
  // default app position for new users only.
  scoped_refptr<extensions::Extension> youtube =
      MakeApp(kSomeAppName, extension_misc::kYoutubeAppId,
              extensions::Extension::WAS_INSTALLED_BY_DEFAULT);

  app_list_syncable_service()
      ->set_app_default_positioned_for_new_users_only_for_test(
          extension_misc::kYoutubeAppId);

  // Merge non-empty sync data, and verify the test app is added to front of the
  // app list.
  syncer::SyncDataList sync_data_list;
  sync_data_list.push_back(
      CreateAppRemoteData(GenerateId("item_id"), "item_name",
                          GenerateId("parent_id"), "ordinal", "pin_ordinal"));
  app_list_syncable_service()->MergeDataAndStartSyncing(
      syncer::APP_LIST, sync_data_list,
      std::make_unique<syncer::FakeSyncChangeProcessor>());
  content::RunAllTasksUntilIdle();

  InstallExtension(youtube.get());

  std::vector<std::string> app_ids = GetOrderedItemIdsFromSyncableService();
  ASSERT_GE(app_ids.size(), 0u);
  EXPECT_EQ(extension_misc::kYoutubeAppId, app_ids[0]);

  const AppListSyncableService::SyncItem* youtube_sync_item =
      GetSyncItem(extension_misc::kYoutubeAppId);
  ChromeAppListItem* youtube_item =
      GetModelUpdater()->FindItem(extension_misc::kYoutubeAppId);

  EXPECT_NE(youtube_sync_item->item_ordinal,
            youtube_item->CalculateDefaultPositionIfApplicable());
}

TEST_F(AppListSyncableServiceTest, RespectContainerAppPositionInSync) {
  // Use youtube as a stand-in for container app - a default app that takes
  // default app position for new users only.
  scoped_refptr<extensions::Extension> youtube =
      MakeApp(kSomeAppName, extension_misc::kYoutubeAppId,
              extensions::Extension::WAS_INSTALLED_BY_DEFAULT);

  app_list_syncable_service()
      ->set_app_default_positioned_for_new_users_only_for_test(
          extension_misc::kYoutubeAppId);
  InstallExtension(youtube.get());

  std::vector<std::string> app_ids = GetOrderedItemIdsFromSyncableService();
  ASSERT_GE(app_ids.size(), 0u);
  EXPECT_NE(extension_misc::kYoutubeAppId, app_ids[0]);

  const AppListSyncableService::SyncItem* youtube_sync_item =
      GetSyncItem(extension_misc::kYoutubeAppId);
  ChromeAppListItem* youtube_item =
      GetModelUpdater()->FindItem(extension_misc::kYoutubeAppId);

  EXPECT_EQ(youtube_sync_item->item_ordinal,
            youtube_item->CalculateDefaultPositionIfApplicable());

  // Merge non-empty sync data, and verify the test app is added to front of the
  // app list.
  syncer::SyncDataList sync_data_list;
  sync_data_list.push_back(CreateAppRemoteData(
      GenerateId("item_id"), "item_name", GenerateId("parent_id"),
      GetLastPositionString(), "pin_ordinal"));
  auto youtube_sync_position = GetLastPositionString();
  sync_data_list.push_back(CreateAppRemoteData(extension_misc::kYoutubeAppId,
                                               "item_name", "",
                                               youtube_sync_position, kUnset));
  app_list_syncable_service()->MergeDataAndStartSyncing(
      syncer::APP_LIST, sync_data_list,
      std::make_unique<syncer::FakeSyncChangeProcessor>());
  content::RunAllTasksUntilIdle();

  app_ids = GetOrderedItemIdsFromSyncableService();
  ASSERT_GE(app_ids.size(), 0u);
  EXPECT_NE(extension_misc::kYoutubeAppId, app_ids[0]);

  youtube_sync_item = GetSyncItem(extension_misc::kYoutubeAppId);

  EXPECT_EQ(youtube_sync_position,
            youtube_sync_item->item_ordinal.ToDebugString());
}

TEST_F(AppListSyncableServiceTest,
       RespectContainerAppPositionInSyncWithDelayedSync) {
  // Use youtube as a stand-in for container app - a default app that takes
  // default app position for new users only.
  scoped_refptr<extensions::Extension> youtube =
      MakeApp(kSomeAppName, extension_misc::kYoutubeAppId,
              extensions::Extension::WAS_INSTALLED_BY_DEFAULT);

  app_list_syncable_service()
      ->set_app_default_positioned_for_new_users_only_for_test(
          extension_misc::kYoutubeAppId);

  // Merge non-empty sync data, and verify the test app is added to front of the
  // app list.
  syncer::SyncDataList sync_data_list;
  sync_data_list.push_back(CreateAppRemoteData(
      GenerateId("item_id"), "item_name", GenerateId("parent_id"),
      GetLastPositionString(), "pin_ordinal"));
  auto youtube_sync_position = GetLastPositionString();
  sync_data_list.push_back(CreateAppRemoteData(extension_misc::kYoutubeAppId,
                                               "item_name", "",
                                               youtube_sync_position, kUnset));
  app_list_syncable_service()->MergeDataAndStartSyncing(
      syncer::APP_LIST, sync_data_list,
      std::make_unique<syncer::FakeSyncChangeProcessor>());
  content::RunAllTasksUntilIdle();

  InstallExtension(youtube.get());

  std::vector<std::string> app_ids = GetOrderedItemIdsFromSyncableService();
  ASSERT_GE(app_ids.size(), 0u);
  EXPECT_NE(extension_misc::kYoutubeAppId, app_ids[0]);

  const AppListSyncableService::SyncItem* youtube_sync_item =
      GetSyncItem(extension_misc::kYoutubeAppId);

  EXPECT_EQ(youtube_sync_position,
            youtube_sync_item->item_ordinal.ToDebugString());
}

TEST_F(AppListSyncableServiceTest,
       PositionOfContainerAppUpdatedAferNonEmptySyncData) {
  // Use youtube as a stand-in for container app - a default app that takes
  // default app position for new users only.
  scoped_refptr<extensions::Extension> youtube =
      MakeApp(kSomeAppName, extension_misc::kYoutubeAppId,
              extensions::Extension::WAS_INSTALLED_BY_DEFAULT);

  app_list_syncable_service()
      ->set_app_default_positioned_for_new_users_only_for_test(
          extension_misc::kYoutubeAppId);
  InstallExtension(youtube.get());

  std::vector<std::string> app_ids = GetOrderedItemIdsFromSyncableService();
  ASSERT_GE(app_ids.size(), 0u);
  EXPECT_NE(extension_misc::kYoutubeAppId, app_ids[0]);

  const AppListSyncableService::SyncItem* youtube_sync_item =
      GetSyncItem(extension_misc::kYoutubeAppId);
  ChromeAppListItem* youtube_item =
      GetModelUpdater()->FindItem(extension_misc::kYoutubeAppId);

  EXPECT_EQ(youtube_sync_item->item_ordinal,
            youtube_item->CalculateDefaultPositionIfApplicable());

  // Merge non-empty sync data, and verify the test app is added to front of the
  // app list.
  syncer::SyncDataList sync_data_list;
  sync_data_list.push_back(
      CreateAppRemoteData(GenerateId("item_id"), "item_name",
                          GenerateId("parent_id"), "ordinal", "pin_ordinal"));
  app_list_syncable_service()->MergeDataAndStartSyncing(
      syncer::APP_LIST, sync_data_list,
      std::make_unique<syncer::FakeSyncChangeProcessor>());
  content::RunAllTasksUntilIdle();

  app_ids = GetOrderedItemIdsFromSyncableService();
  ASSERT_GE(app_ids.size(), 0u);
  EXPECT_EQ(extension_misc::kYoutubeAppId, app_ids[0]);

  youtube_sync_item = GetSyncItem(extension_misc::kYoutubeAppId);
  youtube_item = GetModelUpdater()->FindItem(extension_misc::kYoutubeAppId);

  EXPECT_NE(youtube_sync_item->item_ordinal,
            youtube_item->CalculateDefaultPositionIfApplicable());
}

class AppListSyncableServiceAppPreloadTest
    : public test::AppListSyncableServiceTestBase {
 public:
  AppListSyncableServiceAppPreloadTest() {
    feature_list_.InitAndEnableFeature(
        apps::kAppPreloadServiceEnableLauncherOrder);
  }
  AppListSyncableServiceAppPreloadTest(
      const AppListSyncableServiceAppPreloadTest&) = delete;
  AppListSyncableServiceAppPreloadTest& operator=(
      const AppListSyncableServiceAppPreloadTest&) = delete;
  ~AppListSyncableServiceAppPreloadTest() override = default;

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(AppListSyncableServiceAppPreloadTest, LauncherOrdering) {
  const std::map<apps::LauncherItem, syncer::StringOrdinal>& ordinals =
      app_list_syncable_service()->GetDefaultOrdinalsForTest();
  auto ordinals_to_string = [&]() {
    std::vector<std::pair<apps::LauncherItem, syncer::StringOrdinal>> ordered;
    std::copy(ordinals.begin(), ordinals.end(), std::back_inserter(ordered));
    std::sort(
        ordered.begin(), ordered.end(),
        [](std::pair<apps::LauncherItem, syncer::StringOrdinal> const& lhs,
           std::pair<apps::LauncherItem, syncer::StringOrdinal> const& rhs) {
          return lhs.second.LessThan(rhs.second);
        });
    std::vector<std::string> result;
    for (const auto& item : ordered) {
      std::string first =
          absl::holds_alternative<std::string>(item.first)
              ? absl::get<std::string>(item.first)
              : absl::get<apps::PackageId>(item.first).ToString();
      result.push_back(first + "=" + item.second.ToDebugString());
    }
    return result;
  };

  // Validate default order.
  EXPECT_THAT(
      ordinals_to_string(),
      ElementsAreArray({
          "chromeapp:mgndgikekgjfcpckkfioiadnlibdjbkf=n",
          "system:lacros-chrome=t",
          "chromeapp:cnbgggchhmkkdmeppjobngjoejnihlei=w",
          "system:file_manager=x",
          "web:https://mail.google.com/mail/?usp=installed_webapp=y",
          "web:https://docs.google.com/document/?usp=installed_webapp=yn",
          "web:https://docs.google.com/presentation/?usp=installed_webapp=z",
          "web:https://docs.google.com/spreadsheets/?usp=installed_webapp=zm",
          "web:https://drive.google.com/?lfhs=2=zs",
          "web:https://www.youtube.com/?feature=ytca=zv",
          "system:camera=zx",
          "system:settings=zy",
          "system:help=zyn",
          "system:app_mall=zz",
          "system:media=zzm",
          "system:projector=zzs",
          "system:print_management=zzv",
          "system:scanning=zzx",
          "system:shortcut_customization=zzy",
          "system:terminal=zzyn",
      }));
  EXPECT_EQ(app_list_syncable_service()->GetOemFolderNameForTest(),
            "OEM folder");

  // App Preload Server ordering should be merged into defaults.
  auto p = [](std::string s) { return *apps::PackageId::FromString(s); };
  auto type_chrome =
      apps::proto::AppPreloadListResponse_LauncherType_LAUNCHER_TYPE_CHROME;
  auto type_app =
      apps::proto::AppPreloadListResponse_LauncherType_LAUNCHER_TYPE_APP;
  auto type_oem_folder =
      apps::proto::AppPreloadListResponse_LauncherType_LAUNCHER_TYPE_FOLDER_OEM;
  auto type_folder =
      apps::proto::AppPreloadListResponse_LauncherType_LAUNCHER_TYPE_FOLDER;
  apps::LauncherOrdering ordering;
  std::string empty_root_folder_name;
  ordering[empty_root_folder_name] = apps::LauncherItemMap({
      // app1 should come before chrome.
      {p("chromeapp:app1"), {type_app, 1}},
      {p("chromeapp:mgndgikekgjfcpckkfioiadnlibdjbkf"), {type_chrome, 2}},
      {p("system:lacros-chrome"), {type_chrome, 2}},
      // OEM folder name should get set as 'aps-oem-folder'.
      // aps-oem-folder, aps-folder, and app2 should come after chrome.
      {"aps-oem-folder", {type_oem_folder, 3}},
      {"aps-folder", {type_folder, 4}},
      {p("chromeapp:app2"), {type_app, 5}},
      {p("system:settings"), {type_app, 6}},
      // app3 should come after settings.
      {p("chromeapp:app3"), {type_app, 7}},
      // file_manager should remain unchanged before settings.
      {p("system:file_manager"), {type_app, 8}},
      // app4 should be after app3, not after file-manager.
      {p("chromeapp:app4"), {type_app, 9}},
      {p("system:terminal"), {type_app, 10}},
      // app4 should come after terminal and be the last item.
      {p("chromeapp:app5"), {type_app, 11}},
  });
  ordering["aps-oem-folder"] = apps::LauncherItemMap({
      {p("chromeapp:oem1"), {type_app, 1}},
      {p("chromeapp:oem2"), {type_app, 2}},
  });
  ordering["aps-folder"] = apps::LauncherItemMap({
      {p("chromeapp:folderapp1"), {type_app, 1}},
      {p("chromeapp:folderapp2"), {type_app, 2}},
  });

  app_list_syncable_service()->OnGetLauncherOrdering(ordering);
  EXPECT_THAT(
      ordinals_to_string(),
      ElementsAreArray({
          "chromeapp:app1=h",  // app1 before chrome.
          "chromeapp:folderapp1=n",
          "chromeapp:oem1=n",
          "chromeapp:mgndgikekgjfcpckkfioiadnlibdjbkf=n",
          "system:lacros-chrome=t",
          "chromeapp:oem2=t",
          "chromeapp:folderapp2=t",
          "aps-oem-folder=u",  // folders and app2 after chrome.
          "aps-folder=v",
          "chromeapp:app2=vn",
          "chromeapp:cnbgggchhmkkdmeppjobngjoejnihlei=w",
          "system:file_manager=x",  // file-manager unchanged.
          "web:https://mail.google.com/mail/?usp=installed_webapp=y",
          "web:https://docs.google.com/document/?usp=installed_webapp=yn",
          "web:https://docs.google.com/presentation/?usp=installed_webapp=z",
          "web:https://docs.google.com/spreadsheets/?usp=installed_webapp=zm",
          "web:https://drive.google.com/?lfhs=2=zs",
          "web:https://www.youtube.com/?feature=ytca=zv",
          "system:camera=zx",
          "system:settings=zy",
          "chromeapp:app3=zyg",  // app3 after settings.
          "chromeapp:app4=zyj",  // app4 after settings, not after file_manager.
          "system:help=zyn",
          "system:app_mall=zz",
          "system:media=zzm",
          "system:projector=zzs",
          "system:print_management=zzv",
          "system:scanning=zzx",
          "system:shortcut_customization=zzy",
          "system:terminal=zzyn",
          "chromeapp:app5=zzz",  // app5 after terminal, last item.
      }));
  EXPECT_EQ(app_list_syncable_service()->GetOemFolderNameForTest(),
            "aps-oem-folder");

  auto items_to_string = [&]() {
    std::vector<std::string> result;
    for (const auto& item : GetModelUpdater()->GetItems()) {
      result.push_back(
          base::JoinString({item->id(), item->name(), item->folder_id(),
                            item->position().ToDebugString()},
                           "|"));
    }
    return result;
  };

  auto add_item = [&](apps::PackageId package_id) {
    apps::AppPtr app = std::make_unique<apps::App>(apps::AppType::kChromeApp,
                                                   package_id.identifier());
    app->readiness = apps::Readiness::kReady;
    app->installer_package_id = package_id;
    std::vector<apps::AppPtr> deltas;
    deltas.push_back(std::move(app));
    apps::AppServiceProxyFactory::GetForProfile(profile_.get())
        ->OnApps(std::move(deltas), apps::AppType::kUnknown,
                 /*should_notify_initialized=*/false);
    auto item = std::make_unique<ChromeAppListItem>(
        profile_.get(), package_id.identifier(), GetModelUpdater());
    ItemTestApi(item.get()).SetName(package_id.identifier());
    app_list_syncable_service()->AddItem(std::move(item));
  };

  // The 3 default test apps should exist at first.
  EXPECT_THAT(items_to_string(),
              ElementsAreArray({
                  "dceacbkfkmllgmjmbhgkpjegnodmildf|Hosted App||n",
                  "emfkafnhnpcmabnnkckkchdilgeoekbo|Packaged App 1||h",
                  "jlklkagmeajbjiobondfhiekepofmljl|Packaged App 2||e",
              }));

  // Positions should be set from APS, folderapp1 should create 'aps-folder',
  // and oem1 should create 'aps-oem-folder'.
  add_item(p("chromeapp:app1"));
  add_item(p("chromeapp:folderapp1"));
  add_item(p("chromeapp:oem1"));
  EXPECT_THAT(items_to_string(),
              ElementsAreArray({
                  "app1|app1||h",
                  "dceacbkfkmllgmjmbhgkpjegnodmildf|Hosted App||n",
                  "ddb1da55-d478-4243-8642-56d3041f0263|aps-oem-folder||u",
                  "emfkafnhnpcmabnnkckkchdilgeoekbo|Packaged App 1||h",
                  "folder:aps-folder|aps-folder||v",
                  "folderapp1|folderapp1|folder:aps-folder|n",
                  "jlklkagmeajbjiobondfhiekepofmljl|Packaged App 2||e",
                  "oem1|oem1|ddb1da55-d478-4243-8642-56d3041f0263|n",
              }));
}

// Base class for tests of `AppListSyncableService::OnFirstSync()` parameterized
// by whether the first sync in the session is the first sync ever across all
// ChromeOS devices and sessions for the associated user.
class AppListSyncableServiceOnFirstSyncTest
    : public AppListSyncableServiceTest,
      public testing::WithParamInterface<
          /*first_sync_was_first_sync_ever=*/bool> {
 public:
  // Returns whether the first sync in the session is the first sync ever across
  // all ChromeOS devices and sessions for the associated user given test
  // parameterization.
  bool first_sync_was_first_sync_ever() const { return GetParam(); }
};

INSTANTIATE_TEST_SUITE_P(All,
                         AppListSyncableServiceOnFirstSyncTest,
                         ::testing::Bool());

// Verifies that `AppListSyncableService::OnFirstSync()` runs callbacks at
// the expected times and with the expected values.
TEST_P(AppListSyncableServiceOnFirstSyncTest, OnFirstSync) {
  syncer::SyncDataList sync_data_list;

  // Populate `sync_data_list` when the first sync in the session should *not*
  // be the first sync ever across all ChromeOS devices and sessions for the
  // associated user.
  if (!first_sync_was_first_sync_ever()) {
    sync_data_list.push_back(
        CreateAppRemoteData(GenerateId("item_id"), "item_name",
                            GenerateId("parent_id"), "ordinal", "pin_ordinal"));
  }

  // Create a test future for a callback to register *before* the first sync
  // in the session is completed, and another to register *after*.
  base::test::TestFuture<bool> before_first_sync_future;
  base::test::TestFuture<bool> after_first_sync_future;

  // Register a callback *before* the first sync in the session is completed.
  app_list_syncable_service()->OnFirstSync(
      before_first_sync_future.GetCallback());

  // Complete the first sync in the session.
  app_list_syncable_service()->MergeDataAndStartSyncing(
      syncer::APP_LIST, sync_data_list,
      std::make_unique<syncer::FakeSyncChangeProcessor>());

  // Register a callback *after* the first sync in the session is completed.
  app_list_syncable_service()->OnFirstSync(
      after_first_sync_future.GetCallback());

  // Neither callback should have run since callbacks are posted.
  EXPECT_FALSE(before_first_sync_future.IsReady());
  EXPECT_FALSE(after_first_sync_future.IsReady());

  // When run, callbacks should reflect whether the first sync in the session
  // was the first sync ever across all ChromeOS devices and sessions for the
  // associated user.
  EXPECT_EQ(before_first_sync_future.Get(), first_sync_was_first_sync_ever());
  EXPECT_EQ(after_first_sync_future.Get(), first_sync_was_first_sync_ever());
}

}  // namespace app_list
