// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/model/app_list_model.h"
#include "chrome/browser/ash/app_list/app_list_model_updater.h"
#include "chrome/browser/ash/app_list/app_list_test_util.h"
#include "chrome/browser/ash/app_list/chrome_app_list_model_updater.h"
#include "chrome/browser/ash/app_list/test/app_list_syncable_service_test_base.h"
#include "chrome/browser/ash/app_list/test/test_app_list_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/crx_file/id_util.h"
#include "components/sync/test/fake_sync_change_processor.h"
#include "ui/display/test/test_screen.h"

namespace app_list {

using crx_file::id_util::GenerateId;

class TemporaryAppListSortTest : public test::AppListSyncableServiceTestBase {
 public:
  TemporaryAppListSortTest() = default;
  ~TemporaryAppListSortTest() override = default;

  void SetUp() override {
    AppListSyncableServiceTestBase::SetUp();
    app_list_controller_ = std::make_unique<test::TestAppListController>();
    GetModelUpdater()->SetActive(true);
    content::RunAllTasksUntilIdle();
  }

  ChromeAppListModelUpdater* GetChromeModelUpdater() {
    return static_cast<ChromeAppListModelUpdater*>(GetModelUpdater());
  }

  ash::AppListSortOrder GetTemporarySortOrder() {
    return GetChromeModelUpdater()->GetTemporarySortOrderForTest();
  }

  // Returns the app list order stored as preference.
  ash::AppListSortOrder GetSortOrderFromPrefs() {
    return static_cast<ash::AppListSortOrder>(
        profile()->GetPrefs()->GetInteger(prefs::kAppListPreferredOrder));
  }

  syncer::StringOrdinal GetPositionFromModelUpdater(const std::string& id) {
    return GetModelUpdater()->FindItem(id)->position();
  }

  void Commit() {
    static_cast<ChromeAppListModelUpdater*>(GetModelUpdater())
        ->EndTemporarySortAndTakeAction(
            ChromeAppListModelUpdater::EndAction::kCommit);
  }

  bool IsUnderTemporarySort() {
    return static_cast<ChromeAppListModelUpdater*>(GetModelUpdater())
        ->is_under_temporary_sort();
  }

 private:
  display::test::TestScreen test_screen_{/*create_dispay=*/true,
                                         /*register_screen=*/true};
  std::unique_ptr<test::TestAppListController> app_list_controller_;
};

// Verifies that sorting by app names is case insensitive.
TEST_F(TemporaryAppListSortTest, SortIsCaseInsensitive) {
  RemoveAllExistingItems();

  // Add three apps.
  const std::string kItemId1 = CreateNextAppId(GenerateId("app_id1"));
  scoped_refptr<extensions::Extension> app1 =
      MakeApp("aaa", kItemId1, extensions::Extension::NO_FLAGS);
  InstallExtension(app1.get());

  const std::string kItemId2 = CreateNextAppId(GenerateId("app_id2"));
  scoped_refptr<extensions::Extension> app2 =
      MakeApp("BBB", kItemId2, extensions::Extension::NO_FLAGS);
  InstallExtension(app2.get());

  const std::string kItemId3 = CreateNextAppId(GenerateId("app_id3"));
  scoped_refptr<extensions::Extension> app3 =
      MakeApp("ccc", kItemId3, extensions::Extension::NO_FLAGS);
  InstallExtension(app3.get());

  // Verify the default status. Note that when the order is kCustom, a new app
  // should be placed at the front.
  EXPECT_EQ(ash::AppListSortOrder::kCustom, GetSortOrderFromPrefs());
  EXPECT_EQ(GetOrderedItemIdsFromModelUpdater(),
            std::vector<std::string>({kItemId3, kItemId2, kItemId1}));

  // Sort apps with name alphabetical order.
  GetChromeModelUpdater()->RequestAppListSort(
      ash::AppListSortOrder::kNameAlphabetical);

  // The app positions stored in the model updater change, where the order of
  // app names is case insensitive.
  EXPECT_EQ(GetOrderedItemIdsFromModelUpdater(),
            std::vector<std::string>({kItemId1, kItemId2, kItemId3}));
}

TEST_F(TemporaryAppListSortTest, AppInsertionInSortedAppListCaseInsensitive) {
  RemoveAllExistingItems();

  // Add three apps.
  const std::string kItemId1 = CreateNextAppId(GenerateId("app_id1"));
  scoped_refptr<extensions::Extension> app1 =
      MakeApp("aaa", kItemId1, extensions::Extension::NO_FLAGS);
  InstallExtension(app1.get());

  const std::string kItemId2 = CreateNextAppId(GenerateId("app_id2"));
  scoped_refptr<extensions::Extension> app2 =
      MakeApp("BBB", kItemId2, extensions::Extension::NO_FLAGS);
  InstallExtension(app2.get());

  const std::string kItemId3 = CreateNextAppId(GenerateId("app_id3"));
  scoped_refptr<extensions::Extension> app3 =
      MakeApp("ccc", kItemId3, extensions::Extension::NO_FLAGS);
  InstallExtension(app3.get());

  // Verify the default status. Note that when the order is kCustom, a new app
  // should be placed at the front.
  EXPECT_EQ(ash::AppListSortOrder::kCustom, GetSortOrderFromPrefs());
  EXPECT_EQ(GetOrderedItemIdsFromModelUpdater(),
            std::vector<std::string>({kItemId3, kItemId2, kItemId1}));

  // Sort apps with name alphabetical order.
  GetChromeModelUpdater()->RequestAppListSort(
      ash::AppListSortOrder::kNameAlphabetical);

  // The app positions stored in the model updater change, where the order of
  // app names is case insensitive.
  EXPECT_EQ(GetOrderedItemIdsFromModelUpdater(),
            std::vector<std::string>({kItemId1, kItemId2, kItemId3}));

  // Install an additional app.
  const std::string kItemId4 = GenerateId("app_id4");
  scoped_refptr<extensions::Extension> app4 =
      MakeApp("abc", kItemId4, extensions::Extension::NO_FLAGS);
  InstallExtension(app4.get());

  // The app positions stored in the model updater change, where the order of
  // app names is case insensitive.
  EXPECT_EQ(GetOrderedItemIdsFromModelUpdater(),
            std::vector<std::string>({kItemId1, kItemId4, kItemId2, kItemId3}));
}

// Verifies sorting works as expected when the app list is under temporary sort.
TEST_F(TemporaryAppListSortTest, SortUponTemporaryOrder) {
  RemoveAllExistingItems();

  // Add three apps.
  const std::string kItemId1 = CreateNextAppId(GenerateId("app_id1"));
  scoped_refptr<extensions::Extension> app1 =
      MakeApp("A", kItemId1, extensions::Extension::NO_FLAGS);
  InstallExtension(app1.get());

  const std::string kItemId2 = CreateNextAppId(GenerateId("app_id2"));
  scoped_refptr<extensions::Extension> app2 =
      MakeApp("B", kItemId2, extensions::Extension::NO_FLAGS);
  InstallExtension(app2.get());

  const std::string kItemId3 = CreateNextAppId(GenerateId("app_id3"));
  scoped_refptr<extensions::Extension> app3 =
      MakeApp("C", kItemId3, extensions::Extension::NO_FLAGS);
  InstallExtension(app3.get());

  // Verify the default status. Note that when the order is kCustom, a new app
  // should be placed at the front.
  EXPECT_EQ(ash::AppListSortOrder::kCustom, GetSortOrderFromPrefs());
  EXPECT_EQ(GetOrderedItemIdsFromModelUpdater(),
            std::vector<std::string>({kItemId3, kItemId2, kItemId1}));

  // Sort apps with name alphabetical order.
  GetChromeModelUpdater()->RequestAppListSort(
      ash::AppListSortOrder::kNameAlphabetical);

  // The permanent sort order does not change while the temporary order updates.
  EXPECT_EQ(ash::AppListSortOrder::kCustom, GetSortOrderFromPrefs());
  EXPECT_EQ(ash::AppListSortOrder::kNameAlphabetical, GetTemporarySortOrder());

  // The app positions stored in the model updater change.
  EXPECT_EQ(GetOrderedItemIdsFromModelUpdater(),
            std::vector<std::string>({kItemId1, kItemId2, kItemId3}));

  // Permanent positions (i.e. the positions stored in sync data) do not change.
  EXPECT_EQ(GetOrderedItemIdsFromSyncableService(),
            std::vector<std::string>({kItemId3, kItemId2, kItemId1}));

  // Sort again without exiting temporary sort.
  GetChromeModelUpdater()->RequestAppListSort(
      ash::AppListSortOrder::kNameAlphabetical);

  // Verify sort orders and app positions.
  EXPECT_EQ(ash::AppListSortOrder::kCustom, GetSortOrderFromPrefs());
  EXPECT_EQ(ash::AppListSortOrder::kNameAlphabetical, GetTemporarySortOrder());
  EXPECT_EQ(GetOrderedItemIdsFromModelUpdater(),
            std::vector<std::string>({kItemId1, kItemId2, kItemId3}));
  EXPECT_EQ(GetOrderedItemIdsFromSyncableService(),
            std::vector<std::string>({kItemId3, kItemId2, kItemId1}));

  // Sort again without exiting temporary sort.
  GetChromeModelUpdater()->RequestAppListSort(
      ash::AppListSortOrder::kNameAlphabetical);

  // Verify sort orders and app positions.
  EXPECT_EQ(ash::AppListSortOrder::kCustom, GetSortOrderFromPrefs());
  EXPECT_EQ(ash::AppListSortOrder::kNameAlphabetical, GetTemporarySortOrder());
  EXPECT_EQ(GetOrderedItemIdsFromModelUpdater(),
            std::vector<std::string>({kItemId1, kItemId2, kItemId3}));
  EXPECT_EQ(GetOrderedItemIdsFromSyncableService(),
            std::vector<std::string>({kItemId3, kItemId2, kItemId1}));
  EXPECT_TRUE(IsUnderTemporarySort());
}

// Verifies that committing name sort order works as expected.
TEST_F(TemporaryAppListSortTest, CommitNameOrder) {
  RemoveAllExistingItems();

  // Add three apps.
  const std::string kItemId1 = CreateNextAppId(GenerateId("app_id1"));
  scoped_refptr<extensions::Extension> app1 =
      MakeApp("A", kItemId1, extensions::Extension::NO_FLAGS);
  InstallExtension(app1.get());

  const std::string kItemId2 = CreateNextAppId(GenerateId("app_id2"));
  scoped_refptr<extensions::Extension> app2 =
      MakeApp("B", kItemId2, extensions::Extension::NO_FLAGS);
  InstallExtension(app2.get());

  const std::string kItemId3 = CreateNextAppId(GenerateId("app_id3"));
  scoped_refptr<extensions::Extension> app3 =
      MakeApp("C", kItemId3, extensions::Extension::NO_FLAGS);
  InstallExtension(app3.get());

  // Sort with name alphabetical order.
  GetChromeModelUpdater()->RequestAppListSort(
      ash::AppListSortOrder::kNameAlphabetical);

  // Verify that the permanent sort order and the permanent app positions do
  // not change.
  EXPECT_EQ(ash::AppListSortOrder::kCustom, GetSortOrderFromPrefs());
  EXPECT_EQ(GetOrderedItemIdsFromSyncableService(),
            std::vector<std::string>({kItemId3, kItemId2, kItemId1}));

  // Commit the temporary sort order. Verify that permanent data update.
  Commit();
  EXPECT_FALSE(IsUnderTemporarySort());
  EXPECT_EQ(ash::AppListSortOrder::kNameAlphabetical, GetSortOrderFromPrefs());
  EXPECT_EQ(GetOrderedItemIdsFromSyncableService(),
            std::vector<std::string>({kItemId1, kItemId2, kItemId3}));
}

// Verifies that moving an item on the app list that is under temporary sort
// works as expected.
TEST_F(TemporaryAppListSortTest, HandleMoveItem) {
  RemoveAllExistingItems();

  // Install four apps.
  const std::string kItemId1 = CreateNextAppId(GenerateId("app_id1"));
  scoped_refptr<extensions::Extension> app1 =
      MakeApp("A", kItemId1, extensions::Extension::NO_FLAGS);
  InstallExtension(app1.get());

  const std::string kItemId2 = CreateNextAppId(GenerateId("app_id2"));
  scoped_refptr<extensions::Extension> app2 =
      MakeApp("B", kItemId2, extensions::Extension::NO_FLAGS);
  InstallExtension(app2.get());

  const std::string kItemId3 = CreateNextAppId(GenerateId("app_id3"));
  scoped_refptr<extensions::Extension> app3 =
      MakeApp("C", kItemId3, extensions::Extension::NO_FLAGS);
  InstallExtension(app3.get());

  const std::string kItemId4 = CreateNextAppId(GenerateId("app_id4"));
  scoped_refptr<extensions::Extension> app4 =
      MakeApp("D", kItemId4, extensions::Extension::NO_FLAGS);
  InstallExtension(app4.get());

  // Sort apps with name alphabetical order, commit the temporary order then
  // verify the state.
  ChromeAppListModelUpdater* model_updater = GetChromeModelUpdater();
  model_updater->RequestAppListSort(ash::AppListSortOrder::kNameAlphabetical);
  Commit();
  EXPECT_EQ(ash::AppListSortOrder::kNameAlphabetical, GetSortOrderFromPrefs());
  EXPECT_EQ(GetOrderedItemIdsFromSyncableService(),
            std::vector<std::string>({kItemId1, kItemId2, kItemId3, kItemId4}));

  // Sort with name reverse alphabetical order without committing.
  model_updater->RequestAppListSort(
      ash::AppListSortOrder::kNameReverseAlphabetical);
  EXPECT_EQ(GetOrderedItemIdsFromModelUpdater(),
            std::vector<std::string>({kItemId4, kItemId3, kItemId2, kItemId1}));
  EXPECT_EQ(GetOrderedItemIdsFromSyncableService(),
            std::vector<std::string>({kItemId1, kItemId2, kItemId3, kItemId4}));

  // Move `app4` to the position between `app2` and `app3`.
  const syncer::StringOrdinal target_position =
      model_updater->FindItem(kItemId2)->position().CreateBetween(
          model_updater->FindItem(kItemId3)->position());
  model_updater->RequestPositionUpdate(
      kItemId4, target_position, ash::RequestPositionUpdateReason::kMoveItem);

  // Verify the following things:
  // (1) The app list is not under temporary sort.
  // (2) The permanent sort order is cleared.
  // (3) The positions in both model updater and sync data are expected.
  EXPECT_FALSE(IsUnderTemporarySort());
  EXPECT_EQ(ash::AppListSortOrder::kCustom, GetSortOrderFromPrefs());
  EXPECT_EQ(GetOrderedItemIdsFromModelUpdater(),
            std::vector<std::string>({kItemId3, kItemId4, kItemId2, kItemId1}));
  EXPECT_EQ(GetOrderedItemIdsFromSyncableService(),
            std::vector<std::string>({kItemId3, kItemId4, kItemId2, kItemId1}));
}

// Verifies that moving an item within the app list resets the nominal app list
// sort order (if the app list is sorted at the time).
TEST_F(TemporaryAppListSortTest, MovingItemsResetsSortOrder) {
  RemoveAllExistingItems();

  std::vector<scoped_refptr<extensions::Extension>> apps;
  for (int i = 0; i < 10; ++i) {
    const std::string id = GenerateId(base::StringPrintf("app_id_%d", i));
    const std::string name = base::StringPrintf("Item %d", i);
    scoped_refptr<extensions::Extension> app =
        MakeApp(name, id, extensions::Extension::NO_FLAGS);
    apps.push_back(app);
    InstallExtension(app.get());
  }

  // Sort with name alphabetical order.
  GetChromeModelUpdater()->RequestAppListSort(
      ash::AppListSortOrder::kNameAlphabetical);
  Commit();
  EXPECT_FALSE(IsUnderTemporarySort());
  EXPECT_EQ(ash::AppListSortOrder::kNameAlphabetical, GetSortOrderFromPrefs());
  EXPECT_EQ(GetOrderedNamesFromSyncableService(),
            std::vector<std::string>({"Item 0", "Item 1", "Item 2", "Item 3",
                                      "Item 4", "Item 5", "Item 6", "Item 7",
                                      "Item 8", "Item 9"}));

  // Move an item within the app list.
  ChromeAppListModelUpdater* model_updater = GetChromeModelUpdater();
  const syncer::StringOrdinal target_position =
      model_updater->FindItem(apps[1]->id())
          ->position()
          .CreateBetween(model_updater->FindItem(apps[2]->id())->position());
  model_updater->RequestPositionUpdate(
      apps[7]->id(), target_position,
      ash::RequestPositionUpdateReason::kMoveItem);

  // Verify that the app list is no longer considered sorted - new items are
  // added to the first position within the app list.
  EXPECT_FALSE(IsUnderTemporarySort());
  EXPECT_EQ(ash::AppListSortOrder::kCustom, GetSortOrderFromPrefs());
  EXPECT_EQ(GetOrderedNamesFromSyncableService(),
            std::vector<std::string>({"Item 0", "Item 1", "Item 7", "Item 2",
                                      "Item 3", "Item 4", "Item 5", "Item 6",
                                      "Item 8", "Item 9"}));

  scoped_refptr<extensions::Extension> new_app = MakeApp(
      "Item 10", GenerateId("new_install"), extensions::Extension::NO_FLAGS);
  InstallExtension(new_app.get());

  EXPECT_EQ(ash::AppListSortOrder::kCustom, GetSortOrderFromPrefs());
  EXPECT_EQ(GetOrderedNamesFromSyncableService(),
            std::vector<std::string>({"Item 10", "Item 0", "Item 1", "Item 7",
                                      "Item 2", "Item 3", "Item 4", "Item 5",
                                      "Item 6", "Item 8", "Item 9"}));
}

// Verifies that moving an item from a folder to root apps grid resets the
// nominal app list sort order (if the app list is sorted at the time).
TEST_F(TemporaryAppListSortTest, ReparentingItemToRootResetsSortOrder) {
  RemoveAllExistingItems();

  std::vector<scoped_refptr<extensions::Extension>> apps;
  for (int i = 0; i < 10; ++i) {
    const std::string id = GenerateId(base::StringPrintf("app_id_%d", i));
    const std::string name = base::StringPrintf("Item %d", i);
    scoped_refptr<extensions::Extension> app =
        MakeApp(name, id, extensions::Extension::NO_FLAGS);
    apps.push_back(app);
    InstallExtension(app.get());
  }

  // Create a folder that contains three items.
  const std::string kFolderItemId = GenerateId("folder_id");
  syncer::SyncDataList sync_list;
  sync_list.push_back(CreateAppRemoteData(
      kFolderItemId, "Folder", "",
      syncer::StringOrdinal::CreateInitialOrdinal().ToInternalValue(), kUnset,
      sync_pb::AppListSpecifics_AppListItemType_TYPE_FOLDER));
  syncer::StringOrdinal child_position =
      syncer::StringOrdinal::CreateInitialOrdinal();
  sync_list.push_back(
      CreateAppRemoteData(apps[1]->id(), "Item 1", kFolderItemId,
                          child_position.ToInternalValue(), kUnset));
  child_position = child_position.CreateAfter();
  sync_list.push_back(
      CreateAppRemoteData(apps[2]->id(), "Item 2", kFolderItemId,
                          child_position.ToInternalValue(), kUnset));
  child_position = child_position.CreateAfter();
  sync_list.push_back(
      CreateAppRemoteData(apps[3]->id(), "Item 3", kFolderItemId,
                          child_position.ToInternalValue(), kUnset));

  app_list_syncable_service()->MergeDataAndStartSyncing(
      syncer::APP_LIST, sync_list,
      std::make_unique<syncer::FakeSyncChangeProcessor>());
  content::RunAllTasksUntilIdle();

  // Sort with name alphabetical order.
  GetChromeModelUpdater()->RequestAppListSort(
      ash::AppListSortOrder::kNameAlphabetical);
  Commit();
  EXPECT_FALSE(IsUnderTemporarySort());
  EXPECT_EQ(ash::AppListSortOrder::kNameAlphabetical, GetSortOrderFromPrefs());
  EXPECT_EQ(GetOrderedNamesFromSyncableService(),
            std::vector<std::string>({"Folder", "Item 0", "Item 1", "Item 2",
                                      "Item 3", "Item 4", "Item 5", "Item 6",
                                      "Item 7", "Item 8", "Item 9"}));

  // Move an from the folder to root apps grid.
  ChromeAppListModelUpdater* model_updater = GetChromeModelUpdater();
  const syncer::StringOrdinal target_position =
      model_updater->FindItem(apps[6]->id())
          ->position()
          .CreateBetween(model_updater->FindItem(apps[7]->id())->position());
  model_updater->RequestMoveItemToRoot(apps[1]->id(), target_position);

  // Verify that the app list is no longer considered sorted - new items are
  // added to the first position within the app list.
  EXPECT_FALSE(IsUnderTemporarySort());
  EXPECT_EQ(ash::AppListSortOrder::kCustom, GetSortOrderFromPrefs());
  EXPECT_EQ(GetOrderedNamesFromSyncableService(),
            std::vector<std::string>({"Folder", "Item 0", "Item 2", "Item 3",
                                      "Item 4", "Item 5", "Item 6", "Item 1",
                                      "Item 7", "Item 8", "Item 9"}));

  scoped_refptr<extensions::Extension> new_app = MakeApp(
      "Item 10", GenerateId("new_install"), extensions::Extension::NO_FLAGS);
  InstallExtension(new_app.get());

  EXPECT_EQ(ash::AppListSortOrder::kCustom, GetSortOrderFromPrefs());
  EXPECT_EQ(GetOrderedNamesFromSyncableService(),
            std::vector<std::string>({"Item 10", "Folder", "Item 0", "Item 2",
                                      "Item 3", "Item 4", "Item 5", "Item 6",
                                      "Item 1", "Item 7", "Item 8", "Item 9"}));
}

// Verifies that merging two items to form a folder keeps the nominal app list
// sort order (if the app list is sorted at the time) and positions the new
// folder into sorted order.
TEST_F(TemporaryAppListSortTest, MergingItemsKeepsSortOrder) {
  RemoveAllExistingItems();

  std::vector<scoped_refptr<extensions::Extension>> apps;
  for (int i = 0; i < 10; ++i) {
    const std::string id = GenerateId(base::StringPrintf("app_id_%d", i));
    const std::string name = base::StringPrintf("Item %d", i);
    scoped_refptr<extensions::Extension> app =
        MakeApp(name, id, extensions::Extension::NO_FLAGS);
    apps.push_back(app);
    InstallExtension(app.get());
  }

  // Sort with name alphabetical order.
  GetChromeModelUpdater()->RequestAppListSort(
      ash::AppListSortOrder::kNameAlphabetical);
  Commit();
  EXPECT_FALSE(IsUnderTemporarySort());
  EXPECT_EQ(ash::AppListSortOrder::kNameAlphabetical, GetSortOrderFromPrefs());
  EXPECT_EQ(GetOrderedNamesFromSyncableService(),
            std::vector<std::string>({"Item 0", "Item 1", "Item 2", "Item 3",
                                      "Item 4", "Item 5", "Item 6", "Item 7",
                                      "Item 8", "Item 9"}));

  // Merge two items into a folder.
  ChromeAppListModelUpdater* model_updater = GetChromeModelUpdater();
  const std::string folder_id =
      model_updater->model_for_test()->MergeItems(apps[8]->id(), apps[9]->id());

  // Verify that the app list is still considered sorted, and that new installs
  // keep getting added in the sorted order.
  EXPECT_FALSE(IsUnderTemporarySort());
  EXPECT_EQ(ash::AppListSortOrder::kNameAlphabetical, GetSortOrderFromPrefs());
  std::vector<std::string> ordered_names = GetOrderedNamesFromSyncableService();
  EXPECT_EQ(ordered_names, std::vector<std::string>(
                               {"Item 0", "Item 1", "Item 2", "Item 3",
                                "Item 4", "Item 5", "Item 6", "Item 7",
                                "Item 8", "Item 9", "" /*"Unnamed" folder*/}));

  scoped_refptr<extensions::Extension> new_app = MakeApp(
      "Item 10", GenerateId("new_install"), extensions::Extension::NO_FLAGS);
  InstallExtension(new_app.get());

  EXPECT_EQ(ash::AppListSortOrder::kNameAlphabetical, GetSortOrderFromPrefs());
  ordered_names = GetOrderedNamesFromSyncableService();
  EXPECT_EQ(ordered_names,
            std::vector<std::string>({"Item 0", "Item 1", "Item 10", "Item 2",
                                      "Item 3", "Item 4", "Item 5", "Item 6",
                                      "Item 7", "Item 8", "Item 9",
                                      "" /*"Unnamed" folder*/}));
}

// Verifies that moving an item from a folder to root apps grid resets the
// nominal app list sort order (if the app list is sorted at the time).
TEST_F(TemporaryAppListSortTest, ReparentingItemToFolderDoesNotResetSortOrder) {
  RemoveAllExistingItems();

  std::vector<scoped_refptr<extensions::Extension>> apps;
  for (int i = 0; i < 10; ++i) {
    const std::string id = GenerateId(base::StringPrintf("app_id_%d", i));
    const std::string name = base::StringPrintf("Item %d", i);
    scoped_refptr<extensions::Extension> app =
        MakeApp(name, id, extensions::Extension::NO_FLAGS);
    apps.push_back(app);
    InstallExtension(app.get());
  }

  // Create a folder that contains three items.
  const std::string kFolderItemId = GenerateId("folder_id");
  syncer::SyncDataList sync_list;
  sync_list.push_back(CreateAppRemoteData(
      kFolderItemId, "Folder", "",
      syncer::StringOrdinal::CreateInitialOrdinal().ToInternalValue(), kUnset,
      sync_pb::AppListSpecifics_AppListItemType_TYPE_FOLDER));

  // Add three apps to the folder.
  syncer::StringOrdinal child_position =
      syncer::StringOrdinal::CreateInitialOrdinal();
  sync_list.push_back(
      CreateAppRemoteData(apps[1]->id(), "Item 1", kFolderItemId,
                          child_position.ToInternalValue(), kUnset));
  child_position = child_position.CreateAfter();
  sync_list.push_back(
      CreateAppRemoteData(apps[2]->id(), "Item 2", kFolderItemId,
                          child_position.ToInternalValue(), kUnset));
  child_position = child_position.CreateAfter();
  sync_list.push_back(
      CreateAppRemoteData(apps[3]->id(), "Item 3", kFolderItemId,
                          child_position.ToInternalValue(), kUnset));

  app_list_syncable_service()->MergeDataAndStartSyncing(
      syncer::APP_LIST, sync_list,
      std::make_unique<syncer::FakeSyncChangeProcessor>());
  content::RunAllTasksUntilIdle();

  // Sort with name alphabetical order.
  GetChromeModelUpdater()->RequestAppListSort(
      ash::AppListSortOrder::kNameAlphabetical);
  Commit();
  EXPECT_FALSE(IsUnderTemporarySort());
  EXPECT_EQ(ash::AppListSortOrder::kNameAlphabetical, GetSortOrderFromPrefs());
  EXPECT_EQ(GetOrderedNamesFromSyncableService(),
            std::vector<std::string>({"Folder", "Item 0", "Item 1", "Item 2",
                                      "Item 3", "Item 4", "Item 5", "Item 6",
                                      "Item 7", "Item 8", "Item 9"}));

  // Move an from the folder to root apps grid.
  ChromeAppListModelUpdater* model_updater = GetChromeModelUpdater();
  model_updater->RequestMoveItemToFolder(apps[7]->id(), kFolderItemId);

  // Verify that the app list is still considered sorted - new items are
  // added to the app list to maintain sorted order.
  EXPECT_FALSE(IsUnderTemporarySort());
  EXPECT_EQ(ash::AppListSortOrder::kNameAlphabetical, GetSortOrderFromPrefs());
  EXPECT_EQ(GetOrderedNamesFromSyncableService(),
            std::vector<std::string>({"Folder", "Item 0", "Item 1", "Item 2",
                                      "Item 3", "Item 4", "Item 5", "Item 6",
                                      "Item 7", "Item 8", "Item 9"}));

  scoped_refptr<extensions::Extension> new_app = MakeApp(
      "Item 10", GenerateId("new_install"), extensions::Extension::NO_FLAGS);
  InstallExtension(new_app.get());

  EXPECT_EQ(ash::AppListSortOrder::kNameAlphabetical, GetSortOrderFromPrefs());
  EXPECT_EQ(GetOrderedNamesFromSyncableService(),
            std::vector<std::string>({"Folder", "Item 0", "Item 1", "Item 10",
                                      "Item 2", "Item 3", "Item 4", "Item 5",
                                      "Item 6", "Item 7", "Item 8", "Item 9"}));
}
// Verifies that reverting the temporary name sort order works as expected.
TEST_F(TemporaryAppListSortTest, RevertNameOrder) {
  RemoveAllExistingItems();

  // Install Three apps.
  const std::string kItemId1 = CreateNextAppId(GenerateId("app_id1"));
  scoped_refptr<extensions::Extension> app1 =
      MakeApp("A", kItemId1, extensions::Extension::NO_FLAGS);
  InstallExtension(app1.get());

  const std::string kItemId2 = CreateNextAppId(GenerateId("app_id2"));
  scoped_refptr<extensions::Extension> app2 =
      MakeApp("B", kItemId2, extensions::Extension::NO_FLAGS);
  InstallExtension(app2.get());

  const std::string kItemId3 = CreateNextAppId(GenerateId("app_id3"));
  scoped_refptr<extensions::Extension> app3 =
      MakeApp("C", kItemId3, extensions::Extension::NO_FLAGS);
  InstallExtension(app3.get());

  // Sort apps with name alphabetical order, commit the temporary order then
  // verify the state.
  ChromeAppListModelUpdater* model_updater = GetChromeModelUpdater();
  model_updater->RequestAppListSort(ash::AppListSortOrder::kNameAlphabetical);
  Commit();
  EXPECT_EQ(ash::AppListSortOrder::kNameAlphabetical, GetSortOrderFromPrefs());
  EXPECT_EQ(GetOrderedItemIdsFromSyncableService(),
            std::vector<std::string>({kItemId1, kItemId2, kItemId3}));

  // Sort with name reverse alphabetical order without committing.
  model_updater->RequestAppListSort(
      ash::AppListSortOrder::kNameReverseAlphabetical);
  EXPECT_EQ(GetOrderedItemIdsFromModelUpdater(),
            std::vector<std::string>({kItemId3, kItemId2, kItemId1}));
  EXPECT_EQ(GetOrderedItemIdsFromSyncableService(),
            std::vector<std::string>({kItemId1, kItemId2, kItemId3}));

  // Revert the temporary sort order.
  model_updater->RequestAppListSortRevert();

  // Verify the following things:
  // (1) The app list is not under temporary sort.
  // (2) The permanent order does not change.
  // (3) The permanent item positions do not change.
  // (4) The positions in the model updater are reset with the permanent
  // positions.
  EXPECT_FALSE(IsUnderTemporarySort());
  EXPECT_EQ(ash::AppListSortOrder::kNameAlphabetical, GetSortOrderFromPrefs());
  EXPECT_EQ(GetOrderedItemIdsFromModelUpdater(),
            std::vector<std::string>({kItemId1, kItemId2, kItemId3}));
  EXPECT_EQ(GetOrderedItemIdsFromSyncableService(),
            std::vector<std::string>({kItemId1, kItemId2, kItemId3}));
}

// Verifies that the app list under temporary sort works as expected when the
// app list is hidden.
TEST_F(TemporaryAppListSortTest, AppListHidden) {
  RemoveAllExistingItems();

  // Install Three apps.
  const std::string kItemId1 = CreateNextAppId(GenerateId("app_id1"));
  scoped_refptr<extensions::Extension> app1 =
      MakeApp("A", kItemId1, extensions::Extension::NO_FLAGS);
  InstallExtension(app1.get());

  const std::string kItemId2 = CreateNextAppId(GenerateId("app_id2"));
  scoped_refptr<extensions::Extension> app2 =
      MakeApp("B", kItemId2, extensions::Extension::NO_FLAGS);
  InstallExtension(app2.get());

  const std::string kItemId3 = CreateNextAppId(GenerateId("app_id3"));
  scoped_refptr<extensions::Extension> app3 =
      MakeApp("C", kItemId3, extensions::Extension::NO_FLAGS);
  InstallExtension(app3.get());

  // Sort with name alphabetical order.
  ChromeAppListModelUpdater* model_updater = GetChromeModelUpdater();
  model_updater->RequestAppListSort(ash::AppListSortOrder::kNameAlphabetical);

  // Verify that the permanent sort order and the permanent app positions do
  // not change.
  EXPECT_EQ(ash::AppListSortOrder::kCustom, GetSortOrderFromPrefs());
  EXPECT_EQ(GetOrderedItemIdsFromSyncableService(),
            std::vector<std::string>({kItemId3, kItemId2, kItemId1}));

  // Emulate that the app list is hidden.
  model_updater->OnAppListHidden();

  // Verify the following things:
  // (1) The temporary sort ends, and
  // (2) The sort order is committed, and
  // (3) The item positions are committed.
  EXPECT_FALSE(IsUnderTemporarySort());
  EXPECT_EQ(ash::AppListSortOrder::kNameAlphabetical, GetSortOrderFromPrefs());
  EXPECT_EQ(GetOrderedItemIdsFromSyncableService(),
            std::vector<std::string>({kItemId1, kItemId2, kItemId3}));
}

// Verifies the temporary sort behavior when handling item move on a synced
// remote device.
TEST_F(TemporaryAppListSortTest, HandlePositionSyncUpdate) {
  // Start syncing.
  app_list_syncable_service()->MergeDataAndStartSyncing(
      syncer::APP_LIST, syncer::SyncDataList(),
      std::make_unique<syncer::FakeSyncChangeProcessor>());
  content::RunAllTasksUntilIdle();
  RemoveAllExistingItems();

  // Install four apps.
  const std::string kItemId1 = CreateNextAppId(GenerateId("app_id1"));
  scoped_refptr<extensions::Extension> app1 =
      MakeApp("A", kItemId1, extensions::Extension::NO_FLAGS);
  InstallExtension(app1.get());

  const std::string kItemId2 = CreateNextAppId(GenerateId("app_id2"));
  scoped_refptr<extensions::Extension> app2 =
      MakeApp("B", kItemId2, extensions::Extension::NO_FLAGS);
  InstallExtension(app2.get());

  const std::string kItemId3 = CreateNextAppId(GenerateId("app_id3"));
  scoped_refptr<extensions::Extension> app3 =
      MakeApp("C", kItemId3, extensions::Extension::NO_FLAGS);
  InstallExtension(app3.get());

  const std::string kItemId4 = CreateNextAppId(GenerateId("app_id4"));
  scoped_refptr<extensions::Extension> app4 =
      MakeApp("D", kItemId4, extensions::Extension::NO_FLAGS);
  InstallExtension(app4.get());

  // Sort with the name alphabetical order.
  ChromeAppListModelUpdater* model_updater = GetChromeModelUpdater();
  model_updater->RequestAppListSort(ash::AppListSortOrder::kNameAlphabetical);
  Commit();

  // Sort with the name reverse alphabetical order without committing.
  model_updater->RequestAppListSort(
      ash::AppListSortOrder::kNameReverseAlphabetical);
  EXPECT_EQ(GetOrderedItemIdsFromSyncableService(),
            std::vector<std::string>({kItemId1, kItemId2, kItemId3, kItemId4}));
  EXPECT_EQ(GetOrderedItemIdsFromModelUpdater(),
            std::vector<std::string>({kItemId4, kItemId3, kItemId2, kItemId1}));

  // Emulate that `app4` is moved to the position between `app1` and `app2` on
  // a remote device.
  ChromeAppListItem* app4_item = model_updater->FindItem(kItemId4);
  const syncer::StringOrdinal target_position =
      GetPositionFromSyncData(kItemId1).CreateBetween(
          GetPositionFromSyncData(kItemId2));
  syncer::SyncChangeList change_list;
  change_list.push_back(syncer::SyncChange(
      FROM_HERE, syncer::SyncChange::ACTION_UPDATE,
      CreateAppRemoteData(kItemId4, app4_item->name(), app4_item->folder_id(),
                          target_position.ToInternalValue(), "")));
  app_list_syncable_service()->ProcessSyncChanges(base::Location(),
                                                  change_list);
  content::RunAllTasksUntilIdle();

  // Verify that the temporary sort is reverted.
  EXPECT_FALSE(IsUnderTemporarySort());
  EXPECT_EQ(GetOrderedItemIdsFromSyncableService(),
            std::vector<std::string>({kItemId1, kItemId4, kItemId2, kItemId3}));
  EXPECT_EQ(GetOrderedItemIdsFromModelUpdater(),
            std::vector<std::string>({kItemId1, kItemId4, kItemId2, kItemId3}));
}

// Verifies that the app list under temporary sort works as expected when two
// items merge into a folder.
TEST_F(TemporaryAppListSortTest, HandleItemMerge) {
  RemoveAllExistingItems();

  // Install four apps.
  const std::string kItemId1 = CreateNextAppId(GenerateId("app_id1"));
  scoped_refptr<extensions::Extension> app1 =
      MakeApp("A", kItemId1, extensions::Extension::NO_FLAGS);
  InstallExtension(app1.get());

  const std::string kItemId2 = CreateNextAppId(GenerateId("app_id2"));
  scoped_refptr<extensions::Extension> app2 =
      MakeApp("B", kItemId2, extensions::Extension::NO_FLAGS);
  InstallExtension(app2.get());

  const std::string kItemId3 = CreateNextAppId(GenerateId("app_id3"));
  scoped_refptr<extensions::Extension> app3 =
      MakeApp("C", kItemId3, extensions::Extension::NO_FLAGS);
  InstallExtension(app3.get());

  const std::string kItemId4 = CreateNextAppId(GenerateId("app_id4"));
  scoped_refptr<extensions::Extension> app4 =
      MakeApp("D", kItemId4, extensions::Extension::NO_FLAGS);
  InstallExtension(app4.get());

  // Sort with the name alphabetical order.
  ChromeAppListModelUpdater* model_updater = GetChromeModelUpdater();
  model_updater->RequestAppListSort(ash::AppListSortOrder::kNameAlphabetical);
  Commit();

  // Sort with the name reverse alphabetical order without committing. The
  // permanent sort order and the permanent item positions should not change.
  model_updater->RequestAppListSort(
      ash::AppListSortOrder::kNameReverseAlphabetical);
  EXPECT_EQ(GetOrderedItemIdsFromSyncableService(),
            std::vector<std::string>({kItemId1, kItemId2, kItemId3, kItemId4}));
  EXPECT_EQ(ash::AppListSortOrder::kNameAlphabetical, GetSortOrderFromPrefs());

  // Emulate to merge two items into a folder.
  syncer::StringOrdinal position =
      model_updater->FindItem(kItemId4)->position().CreateBefore();
  const std::string kFolderItemId = GenerateId("folder_id1");
  const std::string folder_item_id =
      model_updater->model_for_test()->MergeItems(kItemId4, kItemId3);
  model_updater->RequestFolderRename(folder_item_id, "Folder1");

  // Verify that:
  // (1) Temporary sort ends.
  // (2) Sort order is committed.
  // (3) Local positions are committed.
  EXPECT_FALSE(IsUnderTemporarySort());
  EXPECT_EQ(ash::AppListSortOrder::kNameReverseAlphabetical,
            GetSortOrderFromPrefs());
  EXPECT_EQ(GetOrderedItemIdsFromSyncableService(),
            std::vector<std::string>(
                {folder_item_id, kItemId4, kItemId3, kItemId2, kItemId1}));
  EXPECT_EQ(GetOrderedItemIdsFromModelUpdater(),
            std::vector<std::string>(
                {folder_item_id, kItemId4, kItemId3, kItemId2, kItemId1}));
}

// Verifies that the app list under temporary sort works as expected when a
// folder gets renamed.
TEST_F(TemporaryAppListSortTest, HandleFolderRename) {
  RemoveAllExistingItems();

  // Configure sunc data with a folder containing two apps.
  const std::string kFolderItemId = "folder_id";
  syncer::SyncDataList sync_list;
  sync_list.push_back(CreateAppRemoteData(
      kFolderItemId, "Folder", "",
      syncer::StringOrdinal::CreateInitialOrdinal().ToInternalValue(), kUnset,
      sync_pb::AppListSpecifics_AppListItemType_TYPE_FOLDER));
  const std::string kItemId1 = GenerateId("app_id1");
  const std::string kItemId2 = GenerateId("app_id2");

  syncer::StringOrdinal child_position =
      syncer::StringOrdinal::CreateInitialOrdinal();
  sync_list.push_back(CreateAppRemoteData(
      kItemId1, "A", kFolderItemId, child_position.ToInternalValue(), kUnset));
  child_position = child_position.CreateAfter();
  sync_list.push_back(CreateAppRemoteData(
      kItemId2, "B", kFolderItemId, child_position.ToInternalValue(), kUnset));
  app_list_syncable_service()->MergeDataAndStartSyncing(
      syncer::APP_LIST, sync_list,
      std::make_unique<syncer::FakeSyncChangeProcessor>());
  content::RunAllTasksUntilIdle();

  // Install four apps.
  scoped_refptr<extensions::Extension> app1 =
      MakeApp("A", kItemId1, extensions::Extension::NO_FLAGS);
  InstallExtension(app1.get());

  scoped_refptr<extensions::Extension> app2 =
      MakeApp("B", kItemId2, extensions::Extension::NO_FLAGS);
  InstallExtension(app2.get());

  const std::string kItemId3 = CreateNextAppId(GenerateId("app_id3"));
  scoped_refptr<extensions::Extension> app3 =
      MakeApp("C", kItemId3, extensions::Extension::NO_FLAGS);
  InstallExtension(app3.get());

  const std::string kItemId4 = CreateNextAppId(GenerateId("app_id4"));
  scoped_refptr<extensions::Extension> app4 =
      MakeApp("D", kItemId4, extensions::Extension::NO_FLAGS);
  InstallExtension(app4.get());

  // Sort with the name alphabetical order.
  ChromeAppListModelUpdater* model_updater = GetChromeModelUpdater();
  model_updater->RequestAppListSort(ash::AppListSortOrder::kNameAlphabetical);
  Commit();

  // Sort with the name reverse alphabetical order without committing. The
  // permanent sort order and the permanent item positions should not change.
  model_updater->RequestAppListSort(
      ash::AppListSortOrder::kNameReverseAlphabetical);
  EXPECT_EQ(GetOrderedItemIdsFromSyncableService(),
            std::vector<std::string>(
                {kItemId1, kItemId2, kItemId3, kItemId4, kFolderItemId}));
  EXPECT_EQ(ash::AppListSortOrder::kNameAlphabetical, GetSortOrderFromPrefs());

  // Rename the test folder.
  model_updater->RequestFolderRename(kFolderItemId, "A new folder name");

  // Verify that:
  // (1) Temporary sort ends.
  // (2) Sort order is commited.
  // (3) Local positions are committed.
  EXPECT_FALSE(IsUnderTemporarySort());
  EXPECT_EQ(ash::AppListSortOrder::kNameReverseAlphabetical,
            GetSortOrderFromPrefs());
  EXPECT_EQ(GetOrderedItemIdsFromSyncableService(),
            std::vector<std::string>(
                {kItemId4, kItemId3, kItemId2, kFolderItemId, kItemId1}));
  EXPECT_EQ(GetOrderedItemIdsFromModelUpdater(),
            std::vector<std::string>(
                {kItemId4, kItemId3, kItemId2, kFolderItemId, kItemId1}));
}

// Verifies that the app list under temporary sort works as expected when moving
// an item to an existed folder.
TEST_F(TemporaryAppListSortTest, HandleMoveItemToFolder) {
  RemoveAllExistingItems();

  // Add one folder containing two apps.
  // Emulate to merge two items into a folder.
  const std::string kFolderItemId = GenerateId("folder_id");
  ChromeAppListModelUpdater* model_updater = GetChromeModelUpdater();
  std::unique_ptr<ChromeAppListItem> folder_item =
      std::make_unique<ChromeAppListItem>(profile_.get(), kFolderItemId,
                                          model_updater);
  folder_item->SetChromeIsFolder(true);
  ChromeAppListItem::TestApi(folder_item.get())
      .SetPosition(syncer::StringOrdinal::CreateInitialOrdinal());
  ChromeAppListItem::TestApi(folder_item.get()).SetName("Folder1");
  app_list_syncable_service()->AddItem(std::move(folder_item));

  const std::string kChildItemId1_1 = GenerateId("folder_child1");
  const std::string kChildItemId1_2 = GenerateId("folder_child2");
  syncer::SyncDataList sync_list;
  sync_list.push_back(CreateAppRemoteData(
      kFolderItemId, "Folder", "",
      syncer::StringOrdinal::CreateInitialOrdinal().ToInternalValue(), kUnset,
      sync_pb::AppListSpecifics_AppListItemType_TYPE_FOLDER));

  syncer::StringOrdinal child_position =
      syncer::StringOrdinal::CreateInitialOrdinal();
  sync_list.push_back(CreateAppRemoteData(kChildItemId1_1, "D", kFolderItemId,
                                          child_position.ToInternalValue(),
                                          kUnset));
  child_position = child_position.CreateAfter();
  sync_list.push_back(CreateAppRemoteData(kChildItemId1_2, "E", kFolderItemId,
                                          child_position.ToInternalValue(),
                                          kUnset));

  app_list_syncable_service()->MergeDataAndStartSyncing(
      syncer::APP_LIST, sync_list,
      std::make_unique<syncer::FakeSyncChangeProcessor>());
  content::RunAllTasksUntilIdle();

  // Install three apps.
  const std::string kItemId1 = CreateNextAppId(GenerateId("app_id1"));
  scoped_refptr<extensions::Extension> app1 =
      MakeApp("A", kItemId1, extensions::Extension::NO_FLAGS);
  InstallExtension(app1.get());

  const std::string kItemId2 = CreateNextAppId(GenerateId("app_id2"));
  scoped_refptr<extensions::Extension> app2 =
      MakeApp("B", kItemId2, extensions::Extension::NO_FLAGS);
  InstallExtension(app2.get());

  const std::string kItemId3 = CreateNextAppId(GenerateId("app_id3"));
  scoped_refptr<extensions::Extension> app3 =
      MakeApp("C", kItemId3, extensions::Extension::NO_FLAGS);
  InstallExtension(app3.get());

  // Sort with the name alphabetical order and commit.
  model_updater->RequestAppListSort(ash::AppListSortOrder::kNameAlphabetical);
  Commit();
  EXPECT_EQ(std::vector<std::string>({"A", "B", "C", "D", "E", "Folder"}),
            GetOrderedNamesFromSyncableService());

  // Sort with the name reverse alphabetical order without committing.
  model_updater->RequestAppListSort(
      ash::AppListSortOrder::kNameReverseAlphabetical);
  EXPECT_EQ(ash::AppListSortOrder::kNameAlphabetical, GetSortOrderFromPrefs());
  EXPECT_EQ(std::vector<std::string>({"A", "B", "C", "D", "E", "Folder"}),
            GetOrderedNamesFromSyncableService());

  // Move `app3` to the folder.
  model_updater->RequestMoveItemToFolder(kItemId3, kFolderItemId);

  // Verify that:
  // (1) Temporary sort ends.
  // (2) Sort order is committed.
  EXPECT_FALSE(IsUnderTemporarySort());
  EXPECT_EQ(ash::AppListSortOrder::kNameReverseAlphabetical,
            GetSortOrderFromPrefs());
  EXPECT_EQ(std::vector<std::string>({"Folder", "E", "D", "C", "B", "A"}),
            GetOrderedNamesFromSyncableService());
}

// Verifies that the app list under temporary sort works as expected when moving
// an item from a folder to root apps grid.
TEST_F(TemporaryAppListSortTest, HandleMoveItemToRootGrid) {
  RemoveAllExistingItems();

  // Add one folder containing three apps.
  const std::string kFolderItemId = GenerateId("folder_id");
  syncer::SyncDataList sync_list;
  sync_list.push_back(CreateAppRemoteData(
      kFolderItemId, "Folder", "",
      syncer::StringOrdinal::CreateInitialOrdinal().ToInternalValue(), kUnset,
      sync_pb::AppListSpecifics_AppListItemType_TYPE_FOLDER));
  const std::string kItemId1 = GenerateId("app_id1");
  const std::string kItemId2 = GenerateId("app_id2");
  const std::string kItemId3 = GenerateId("app_id3");

  syncer::StringOrdinal child_position =
      syncer::StringOrdinal::CreateInitialOrdinal();
  sync_list.push_back(CreateAppRemoteData(
      kItemId1, "A", kFolderItemId, child_position.ToInternalValue(), kUnset));
  child_position = child_position.CreateAfter();
  sync_list.push_back(CreateAppRemoteData(
      kItemId2, "B", kFolderItemId, child_position.ToInternalValue(), kUnset));
  child_position = child_position.CreateAfter();
  sync_list.push_back(CreateAppRemoteData(
      kItemId3, "C", kFolderItemId, child_position.ToInternalValue(), kUnset));

  app_list_syncable_service()->MergeDataAndStartSyncing(
      syncer::APP_LIST, sync_list,
      std::make_unique<syncer::FakeSyncChangeProcessor>());
  content::RunAllTasksUntilIdle();

  // Install test apps that were added to the folder.
  scoped_refptr<extensions::Extension> app1 =
      MakeApp("A", kItemId1, extensions::Extension::NO_FLAGS);
  InstallExtension(app1.get());
  scoped_refptr<extensions::Extension> app2 =
      MakeApp("B", kItemId2, extensions::Extension::NO_FLAGS);
  InstallExtension(app2.get());
  scoped_refptr<extensions::Extension> app3 =
      MakeApp("C", kItemId3, extensions::Extension::NO_FLAGS);
  InstallExtension(app3.get());

  // Install an additional app.
  const std::string kItemId4 = GenerateId("app_id4");
  scoped_refptr<extensions::Extension> app4 =
      MakeApp("G", kItemId4, extensions::Extension::NO_FLAGS);
  InstallExtension(app4.get());

  // Sort with the reverse alphabetical name order and commit.
  ChromeAppListModelUpdater* model_updater = GetChromeModelUpdater();
  model_updater->RequestAppListSort(
      ash::AppListSortOrder::kNameReverseAlphabetical);
  Commit();
  EXPECT_EQ(std::vector<std::string>({"G", "Folder", "C", "B", "A"}),
            GetOrderedNamesFromSyncableService());

  // Sort with the name alphabetical order without committing.
  model_updater->RequestAppListSort(ash::AppListSortOrder::kNameAlphabetical);
  EXPECT_EQ(ash::AppListSortOrder::kNameReverseAlphabetical,
            GetSortOrderFromPrefs());
  EXPECT_EQ(std::vector<std::string>({"G", "Folder", "C", "B", "A"}),
            GetOrderedNamesFromSyncableService());

  // Move an folder item to root apps grid.
  model_updater->RequestMoveItemToRoot(
      kItemId1, model_updater->FindItem(kItemId4)->position().CreateAfter());

  // Verify that:
  // (1) Temporary sort ends.
  // (2) Sort order pref reverts to custom.
  EXPECT_FALSE(IsUnderTemporarySort());
  EXPECT_EQ(ash::AppListSortOrder::kCustom, GetSortOrderFromPrefs());
  EXPECT_EQ(std::vector<std::string>({"B", "C", "Folder", "G", "A"}),
            GetOrderedNamesFromSyncableService());
}

// Verifies the temporary sorting behavior with local app installation.
TEST_F(TemporaryAppListSortTest, InstallAppLocally) {
  RemoveAllExistingItems();

  // Install three apps.
  const std::string kItemId1 = CreateNextAppId(GenerateId("app_id1"));
  scoped_refptr<extensions::Extension> app1 =
      MakeApp("B", kItemId1, extensions::Extension::NO_FLAGS);
  InstallExtension(app1.get());

  const std::string kItemId2 = CreateNextAppId(GenerateId("app_id2"));
  scoped_refptr<extensions::Extension> app2 =
      MakeApp("C", kItemId2, extensions::Extension::NO_FLAGS);
  InstallExtension(app2.get());

  const std::string kItemId3 = CreateNextAppId(GenerateId("app_id3"));
  scoped_refptr<extensions::Extension> app3 =
      MakeApp("E", kItemId3, extensions::Extension::NO_FLAGS);
  InstallExtension(app3.get());

  // Sort with the name alphabetical order then commit the order.
  ChromeAppListModelUpdater* model_updater = GetChromeModelUpdater();
  model_updater->RequestAppListSort(ash::AppListSortOrder::kNameAlphabetical);
  Commit();

  // Sort with the name reverse alphabetical order without committing. The
  // permanent sort order and the permanent item positions should not change.
  model_updater->RequestAppListSort(
      ash::AppListSortOrder::kNameReverseAlphabetical);
  EXPECT_EQ(GetOrderedNamesFromSyncableService(),
            std::vector<std::string>({"B", "C", "E"}));
  EXPECT_EQ(GetOrderedNamesFromModelUpdater(),
            std::vector<std::string>({"E", "C", "B"}));
  EXPECT_EQ(ash::AppListSortOrder::kNameAlphabetical, GetSortOrderFromPrefs());

  // Install the forth app.
  const std::string kItemId4 = CreateNextAppId(GenerateId("app_id4"));
  scoped_refptr<extensions::Extension> app4 =
      MakeApp("A", kItemId4, extensions::Extension::NO_FLAGS);
  InstallExtension(app4.get());

  // Verify that the temporary sorting order is committed.
  EXPECT_FALSE(IsUnderTemporarySort());
  EXPECT_EQ(ash::AppListSortOrder::kNameReverseAlphabetical,
            GetSortOrderFromPrefs());
  EXPECT_EQ(GetOrderedNamesFromSyncableService(),
            std::vector<std::string>({"E", "C", "B", "A"}));
  EXPECT_EQ(GetOrderedNamesFromModelUpdater(),
            std::vector<std::string>({"E", "C", "B", "A"}));

  // Sort with the name alphabetical order without committing.
  model_updater->RequestAppListSort(ash::AppListSortOrder::kNameAlphabetical);
  EXPECT_EQ(GetOrderedNamesFromSyncableService(),
            std::vector<std::string>({"E", "C", "B", "A"}));
  EXPECT_EQ(GetOrderedNamesFromModelUpdater(),
            std::vector<std::string>({"A", "B", "C", "E"}));
  EXPECT_EQ(ash::AppListSortOrder::kNameReverseAlphabetical,
            GetSortOrderFromPrefs());

  // Install the fifth app.
  const std::string kItemId5 = CreateNextAppId(GenerateId("app_id5"));
  scoped_refptr<extensions::Extension> app5 =
      MakeApp("D", kItemId5, extensions::Extension::NO_FLAGS);
  InstallExtension(app5.get());

  // Verify that the temporary sorting order is committed.
  EXPECT_FALSE(IsUnderTemporarySort());
  EXPECT_EQ(ash::AppListSortOrder::kNameAlphabetical, GetSortOrderFromPrefs());
  EXPECT_EQ(GetOrderedNamesFromSyncableService(),
            std::vector<std::string>({"A", "B", "C", "D", "E"}));
  EXPECT_EQ(GetOrderedNamesFromModelUpdater(),
            std::vector<std::string>({"A", "B", "C", "D", "E"}));
}

// Verifies the temporary sorting behavior with remote installation.
TEST_F(TemporaryAppListSortTest, InstallAppRemotely) {
  // Start syncing.
  app_list_syncable_service()->MergeDataAndStartSyncing(
      syncer::APP_LIST, syncer::SyncDataList(),
      std::make_unique<syncer::FakeSyncChangeProcessor>());
  content::RunAllTasksUntilIdle();
  RemoveAllExistingItems();

  // Install three apps.
  const std::string kItemId1 = CreateNextAppId(GenerateId("app_id1"));
  scoped_refptr<extensions::Extension> app1 =
      MakeApp("B", kItemId1, extensions::Extension::NO_FLAGS);
  InstallExtension(app1.get());

  const std::string kItemId2 = CreateNextAppId(GenerateId("app_id2"));
  scoped_refptr<extensions::Extension> app2 =
      MakeApp("C", kItemId2, extensions::Extension::NO_FLAGS);
  InstallExtension(app2.get());

  const std::string kItemId3 = CreateNextAppId(GenerateId("app_id3"));
  scoped_refptr<extensions::Extension> app3 =
      MakeApp("E", kItemId3, extensions::Extension::NO_FLAGS);
  InstallExtension(app3.get());

  // Sort with the name alphabetical order then commit the order.
  ChromeAppListModelUpdater* model_updater = GetChromeModelUpdater();
  model_updater->RequestAppListSort(ash::AppListSortOrder::kNameAlphabetical);
  Commit();

  // Sort with the name reverse alphabetical order without committing. The
  // permanent sort order and the permanent item positions should not change.
  model_updater->RequestAppListSort(
      ash::AppListSortOrder::kNameReverseAlphabetical);
  EXPECT_EQ(GetOrderedNamesFromSyncableService(),
            std::vector<std::string>({"B", "C", "E"}));
  EXPECT_EQ(GetOrderedNamesFromModelUpdater(),
            std::vector<std::string>({"E", "C", "B"}));
  EXPECT_EQ(ash::AppListSortOrder::kNameAlphabetical, GetSortOrderFromPrefs());

  // Install an app remotely in the following steps:
  // step 1: create a new sync data through sync service.
  // step 2: install the new app that matches the data created in the step 1.
  syncer::SyncChangeList change_list;
  const std::string kItemId4 = CreateNextAppId(GenerateId("app_id4"));
  const syncer::StringOrdinal target_position =
      GetPositionFromSyncData(kItemId1).CreateBefore();
  change_list.push_back(syncer::SyncChange(
      FROM_HERE, syncer::SyncChange::ACTION_UPDATE,
      CreateAppRemoteData(kItemId4, "A", std::string(),
                          target_position.ToInternalValue(), "")));
  app_list_syncable_service()->ProcessSyncChanges(base::Location(),
                                                  change_list);
  content::RunAllTasksUntilIdle();
  scoped_refptr<extensions::Extension> app4 =
      MakeApp("A", kItemId4, extensions::Extension::NO_FLAGS);
  InstallExtension(app4.get());

  // Verify that the app list is still under temporary sorting order.
  EXPECT_TRUE(IsUnderTemporarySort());
  EXPECT_EQ(ash::AppListSortOrder::kNameAlphabetical, GetSortOrderFromPrefs());
  EXPECT_EQ(GetOrderedNamesFromSyncableService(),
            std::vector<std::string>({"A", "B", "C", "E"}));
  EXPECT_EQ(GetOrderedNamesFromModelUpdater(),
            std::vector<std::string>({"E", "C", "B", "A"}));

  // Revert the temporary sorting order. Verify that the new app is placed at
  // the position that is specified by `change_list`.
  model_updater->RequestAppListSortRevert();
  EXPECT_FALSE(IsUnderTemporarySort());
  EXPECT_EQ(ash::AppListSortOrder::kNameAlphabetical, GetSortOrderFromPrefs());
  EXPECT_EQ(GetOrderedNamesFromSyncableService(),
            std::vector<std::string>({"A", "B", "C", "E"}));
  EXPECT_EQ(GetOrderedNamesFromModelUpdater(),
            std::vector<std::string>({"A", "B", "C", "E"}));
}

// Verifies the temporary sorting behavior when an item is deleted due to the
// removal on a remote device.
TEST_F(TemporaryAppListSortTest, RemoveItemRemotely) {
  // Start syncing.
  app_list_syncable_service()->MergeDataAndStartSyncing(
      syncer::APP_LIST, syncer::SyncDataList(),
      std::make_unique<syncer::FakeSyncChangeProcessor>());
  content::RunAllTasksUntilIdle();
  RemoveAllExistingItems();

  // Install four apps.
  const std::string kItemId1 = CreateNextAppId(GenerateId("app_id1"));
  scoped_refptr<extensions::Extension> app1 =
      MakeApp("A", kItemId1, extensions::Extension::NO_FLAGS);
  InstallExtension(app1.get());

  const std::string kItemId2 = CreateNextAppId(GenerateId("app_id2"));
  scoped_refptr<extensions::Extension> app2 =
      MakeApp("B", kItemId2, extensions::Extension::NO_FLAGS);
  InstallExtension(app2.get());

  const std::string kItemId3 = CreateNextAppId(GenerateId("app_id3"));
  scoped_refptr<extensions::Extension> app3 =
      MakeApp("C", kItemId3, extensions::Extension::NO_FLAGS);
  InstallExtension(app3.get());

  const std::string kItemId4 = CreateNextAppId(GenerateId("app_id4"));
  scoped_refptr<extensions::Extension> app4 =
      MakeApp("D", kItemId4, extensions::Extension::NO_FLAGS);
  InstallExtension(app4.get());

  // Sort with the name alphabetical order then commit the order.
  ChromeAppListModelUpdater* model_updater = GetChromeModelUpdater();
  model_updater->RequestAppListSort(ash::AppListSortOrder::kNameAlphabetical);
  Commit();

  // Sort with the name reverse alphabetical order without committing. The
  // permanent sort order and the permanent item positions should not change.
  model_updater->RequestAppListSort(
      ash::AppListSortOrder::kNameReverseAlphabetical);
  EXPECT_EQ(GetOrderedNamesFromSyncableService(),
            std::vector<std::string>({"A", "B", "C", "D"}));
  EXPECT_EQ(GetOrderedNamesFromModelUpdater(),
            std::vector<std::string>({"D", "C", "B", "A"}));
  EXPECT_EQ(ash::AppListSortOrder::kNameAlphabetical, GetSortOrderFromPrefs());

  // Delete `app4` remotely.
  syncer::SyncChangeList change_list;
  ChromeAppListItem* item_to_delete = model_updater->FindItem(kItemId4);
  change_list.push_back(syncer::SyncChange(
      FROM_HERE, syncer::SyncChange::ACTION_DELETE,
      CreateAppRemoteData(
          kItemId4, item_to_delete->name(), item_to_delete->folder_id(),
          item_to_delete->position().ToInternalValue(), std::string())));
  app_list_syncable_service()->ProcessSyncChanges(base::Location(),
                                                  change_list);
  content::RunAllTasksUntilIdle();

  // Verify that the app list is under temporary sorting.
  EXPECT_EQ(GetOrderedNamesFromSyncableService(),
            std::vector<std::string>({"A", "B", "C"}));
  EXPECT_EQ(GetOrderedNamesFromModelUpdater(),
            std::vector<std::string>({"C", "B", "A"}));
  EXPECT_EQ(ash::AppListSortOrder::kNameAlphabetical, GetSortOrderFromPrefs());

  // Revert the temporary sorting order.
  model_updater->RequestAppListSortRevert();
  EXPECT_FALSE(IsUnderTemporarySort());
  EXPECT_EQ(ash::AppListSortOrder::kNameAlphabetical, GetSortOrderFromPrefs());
  EXPECT_EQ(GetOrderedNamesFromSyncableService(),
            std::vector<std::string>({"A", "B", "C"}));
  EXPECT_EQ(GetOrderedNamesFromModelUpdater(),
            std::vector<std::string>({"A", "B", "C"}));
}

// Verifies that ephemeral apps sorting moves all ephemeral items (apps and
// folders) to the front, in alphabetical, case insensitive order, followed by
// native items (apps and folders) also in alphabetical, case insensitive order.
TEST_F(TemporaryAppListSortTest, AlphabeticalEphemeralAppFirstSort) {
  RemoveAllExistingItems();
  ChromeAppListModelUpdater* model_updater = GetChromeModelUpdater();

  std::vector<scoped_refptr<extensions::Extension>> apps;
  for (int i = 0; i < 2; ++i) {
    const std::string id =
        GenerateId(base::StringPrintf("folder_app_id_%d", i));
    const std::string name = base::StringPrintf("Folder Item %d", i);
    scoped_refptr<extensions::Extension> app =
        MakeApp(name, id, extensions::Extension::NO_FLAGS);
    apps.push_back(app);
    InstallExtension(app.get());
  }

  // Add a native folder with two items.
  const std::string kFolderId1 = GenerateId("folder_id_1");
  syncer::SyncDataList sync_list;
  syncer::StringOrdinal child_position =
      syncer::StringOrdinal::CreateInitialOrdinal();
  sync_list.push_back(CreateAppRemoteData(
      kFolderId1, "folder 1", "", child_position.ToInternalValue(), kUnset,
      sync_pb::AppListSpecifics_AppListItemType_TYPE_FOLDER));
  child_position = child_position.CreateAfter();
  sync_list.push_back(
      CreateAppRemoteData(apps[0]->id(), "Folder Item 0", kFolderId1,
                          child_position.ToInternalValue(), kUnset));
  child_position = child_position.CreateAfter();
  sync_list.push_back(
      CreateAppRemoteData(apps[1]->id(), "Folder Item 1", kFolderId1,
                          child_position.ToInternalValue(), kUnset));
  child_position = child_position.CreateAfter();

  // Add two native apps.
  const std::string kItemId1 = GenerateId("app_id1");
  scoped_refptr<extensions::Extension> app1 =
      MakeApp("app 1", kItemId1, extensions::Extension::NO_FLAGS);
  InstallExtension(app1.get());
  sync_list.push_back(CreateAppRemoteData(
      kItemId1, "app 1", "", child_position.ToInternalValue(), kUnset));
  child_position = child_position.CreateAfter();

  const std::string kItemId2 = GenerateId("app_id2");
  scoped_refptr<extensions::Extension> app2 =
      MakeApp("App 2", kItemId2, extensions::Extension::NO_FLAGS);
  InstallExtension(app2.get());
  child_position = child_position.CreateAfter();
  sync_list.push_back(CreateAppRemoteData(
      kItemId2, "App 2", "", child_position.ToInternalValue(), kUnset));
  child_position = child_position.CreateAfter();

  // Add an ephemeral app and an ephemeral folder with two ephemeral apps
  // inside.
  std::unique_ptr<ChromeAppListItem> app3_item =
      std::make_unique<ChromeAppListItem>(profile_.get(), GenerateId("app_id3"),
                                          model_updater);
  app3_item->SetIsEphemeral(true);
  ChromeAppListItem::TestApi(app3_item.get()).SetPosition(child_position);
  ChromeAppListItem::TestApi(app3_item.get()).SetName("app 3");
  app_list_syncable_service()->AddItem(std::move(app3_item));
  child_position = child_position.CreateAfter();

  const std::string kFolderId2 = GenerateId("folder_id_2");
  std::unique_ptr<ChromeAppListItem> folder2_item =
      std::make_unique<ChromeAppListItem>(profile_.get(), kFolderId2,
                                          model_updater);
  folder2_item->SetChromeIsFolder(true);
  folder2_item->SetIsEphemeral(true);
  ChromeAppListItem::TestApi(folder2_item.get()).SetPosition(child_position);
  ChromeAppListItem::TestApi(folder2_item.get()).SetName("folder 2");
  app_list_syncable_service()->AddItem(std::move(folder2_item));
  child_position = child_position.CreateAfter();

  std::unique_ptr<ChromeAppListItem> app_folder2_item =
      std::make_unique<ChromeAppListItem>(
          profile_.get(), GenerateId("folder_app_id_2"), model_updater);
  app_folder2_item->SetIsEphemeral(true);
  app_folder2_item->SetChromeFolderId(kFolderId2);
  ChromeAppListItem::TestApi(app_folder2_item.get())
      .SetPosition(child_position);
  ChromeAppListItem::TestApi(app_folder2_item.get()).SetName("Folder Item 2");
  app_list_syncable_service()->AddItem(std::move(app_folder2_item));
  child_position = child_position.CreateAfter();

  std::unique_ptr<ChromeAppListItem> app_folder3_item =
      std::make_unique<ChromeAppListItem>(
          profile_.get(), GenerateId("folder_app_id_3"), model_updater);
  app_folder3_item->SetIsEphemeral(true);
  app_folder3_item->SetChromeFolderId(kFolderId2);
  ChromeAppListItem::TestApi(app_folder3_item.get())
      .SetPosition(child_position);
  ChromeAppListItem::TestApi(app_folder3_item.get()).SetName("Folder Item 3");
  app_list_syncable_service()->AddItem(std::move(app_folder3_item));

  app_list_syncable_service()->MergeDataAndStartSyncing(
      syncer::APP_LIST, sync_list,
      std::make_unique<syncer::FakeSyncChangeProcessor>());
  content::RunAllTasksUntilIdle();

  // Verify the default order.
  EXPECT_EQ(ash::AppListSortOrder::kCustom, GetSortOrderFromPrefs());
  EXPECT_EQ(GetOrderedNamesFromSyncableService(),
            std::vector<std::string>(
                {"folder 1", "Folder Item 0", "Folder Item 1", "app 1", "App 2",
                 "app 3", "folder 2", "Folder Item 2", "Folder Item 3"}));

  // Sort apps with ephemeral apps first order.
  model_updater->RequestAppListSort(
      ash::AppListSortOrder::kAlphabeticalEphemeralAppFirst);

  // All ephemeral items (apps and folders) are sorted to the front, in
  // alphabetical, case insensitive order, followed by native items (apps and
  // folders) also in alphabetical, case insensitive order:
  // Ephemeral items: [app 3, folder 2, Folder Item 2, Folder Item 3],
  // Native items: [app 1, App 2, folder 1, Folder Item 0, Folder Item 1]
  EXPECT_EQ(ash::AppListSortOrder::kCustom, GetSortOrderFromPrefs());
  EXPECT_EQ(ash::AppListSortOrder::kAlphabeticalEphemeralAppFirst,
            GetTemporarySortOrder());
  EXPECT_EQ(GetOrderedNamesFromModelUpdater(),
            std::vector<std::string>(
                {"app 3", "folder 2", "Folder Item 2", "Folder Item 3", "app 1",
                 "App 2", "folder 1", "Folder Item 0", "Folder Item 1"}));

  // Add a new ephemeral app.
  const std::string kItemId6 = GenerateId("app_id6");
  std::unique_ptr<ChromeAppListItem> kItemId6_item =
      std::make_unique<ChromeAppListItem>(profile_.get(), kItemId6,
                                          model_updater);
  kItemId6_item->SetIsEphemeral(true);
  ChromeAppListItem::TestApi(kItemId6_item.get()).SetName("App 4");
  app_list_syncable_service()->AddItem(std::move(kItemId6_item));

  // Verify that the app is added to the correct spot, with the other ephemeral
  // items.
  EXPECT_EQ(
      GetOrderedNamesFromModelUpdater(),
      std::vector<std::string>({"app 3", "App 4", "folder 2", "Folder Item 2",
                                "Folder Item 3", "app 1", "App 2", "folder 1",
                                "Folder Item 0", "Folder Item 1"}));
}

// The test class used to verify local uninstallation.
class TemporaryAppListSortLocalUninstallationTest
    : public TemporaryAppListSortTest,
      public testing::WithParamInterface<bool> {
 public:
  TemporaryAppListSortLocalUninstallationTest()
      : TemporaryAppListSortTest(), uninstall_through_service_(GetParam()) {}

  TemporaryAppListSortLocalUninstallationTest(
      const TemporaryAppListSortLocalUninstallationTest&) = delete;
  TemporaryAppListSortLocalUninstallationTest& operator=(
      const TemporaryAppListSortLocalUninstallationTest&) = delete;

  ~TemporaryAppListSortLocalUninstallationTest() override = default;

 protected:
  // If true, an app should be uninstalled through the syncable service;
  // otherwise, an app should be deleted by the model updater.
  const bool uninstall_through_service_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         TemporaryAppListSortLocalUninstallationTest,
                         testing::Bool());

// Verifies the temporary sorting behavior with local app uninstallation.
TEST_P(TemporaryAppListSortLocalUninstallationTest, Basics) {
  RemoveAllExistingItems();

  // Install four apps.
  const std::string kItemId1 = CreateNextAppId(GenerateId("app_id1"));
  scoped_refptr<extensions::Extension> app1 =
      MakeApp("A", kItemId1, extensions::Extension::NO_FLAGS);
  InstallExtension(app1.get());

  const std::string kItemId2 = CreateNextAppId(GenerateId("app_id2"));
  scoped_refptr<extensions::Extension> app2 =
      MakeApp("B", kItemId2, extensions::Extension::NO_FLAGS);
  InstallExtension(app2.get());

  const std::string kItemId3 = CreateNextAppId(GenerateId("app_id3"));
  scoped_refptr<extensions::Extension> app3 =
      MakeApp("C", kItemId3, extensions::Extension::NO_FLAGS);
  InstallExtension(app3.get());

  const std::string kItemId4 = CreateNextAppId(GenerateId("app_id4"));
  scoped_refptr<extensions::Extension> app4 =
      MakeApp("D", kItemId4, extensions::Extension::NO_FLAGS);
  InstallExtension(app4.get());

  // Sort with the name alphabetical order then commit the order.
  ChromeAppListModelUpdater* model_updater = GetChromeModelUpdater();
  model_updater->RequestAppListSort(ash::AppListSortOrder::kNameAlphabetical);
  Commit();

  // Sort with the name reverse alphabetical order without committing. The
  // permanent sort order and the permanent item positions should not change.
  model_updater->RequestAppListSort(
      ash::AppListSortOrder::kNameReverseAlphabetical);
  EXPECT_EQ(GetOrderedNamesFromSyncableService(),
            std::vector<std::string>({"A", "B", "C", "D"}));
  EXPECT_EQ(GetOrderedNamesFromModelUpdater(),
            std::vector<std::string>({"D", "C", "B", "A"}));
  EXPECT_EQ(ash::AppListSortOrder::kNameAlphabetical, GetSortOrderFromPrefs());

  // Trigger item removal from the local device by app list syncable service or
  // the model updater depending on the test param.
  if (uninstall_through_service_) {
    app_list_syncable_service()->RemoveItem(kItemId4, /*is_uninstall=*/true);
  } else {
    // Default apps uninstallation could bypass app list syncable service.
    // Therefore remove the item through `model_updater` to verify this
    // scenario.
    model_updater->RemoveItem(kItemId4, /*is_uninstall=*/true);
  }

  // Verify that the temporary order is committed.
  if (uninstall_through_service_) {
    EXPECT_EQ(GetOrderedNamesFromSyncableService(),
              std::vector<std::string>({"C", "B", "A"}));
  }
  EXPECT_EQ(GetOrderedNamesFromModelUpdater(),
            std::vector<std::string>({"C", "B", "A"}));
  EXPECT_EQ(ash::AppListSortOrder::kNameReverseAlphabetical,
            GetSortOrderFromPrefs());
}

}  // namespace app_list
