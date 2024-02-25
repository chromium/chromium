// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_MODEL_APP_LIST_TEST_MODEL_H_
#define ASH_APP_LIST_MODEL_APP_LIST_TEST_MODEL_H_

#include <memory>
#include <string>

#include "ash/app_list/model/app_list_folder_item.h"
#include "ash/app_list/model/app_list_item.h"
#include "ash/app_list/model/app_list_model.h"
#include "ash/public/cpp/app_list/app_list_model_delegate.h"
#include "base/memory/raw_ptr.h"

namespace ui {
class SimpleMenuModel;
}  // namespace ui

namespace ash {

namespace test {

// Extends AppListModel with helper functions for use in tests. This class also
// overrides `AppListModelDelegate` in order to emulate the process of handling
// the requests to update app list items. In the production code, these requests
// are handled in the browser side.
class AppListTestModel : public AppListModel, public AppListModelDelegate {
 public:
  class AppListTestItem : public AppListItem {
   public:
    AppListTestItem(const std::string& id, AppListTestModel* model);

    AppListTestItem(const AppListTestItem&) = delete;
    AppListTestItem& operator=(const AppListTestItem&) = delete;

    ~AppListTestItem() override;
    void Activate(int event_flags);
    std::unique_ptr<ui::SimpleMenuModel> CreateContextMenuModel();
    const char* GetItemType() const override;

    void SetPosition(const syncer::StringOrdinal& new_position);

   private:
    const raw_ptr<AppListTestModel> model_;
  };

  static const char kItemType[];

  AppListTestModel();

  AppListTestModel(const AppListTestModel&) = delete;
  AppListTestModel& operator=(const AppListTestModel&) = delete;
  ~AppListTestModel() override;

  // AppListModelDelegate:
  void RequestPositionUpdate(std::string id,
                             const syncer::StringOrdinal& new_position,
                             RequestPositionUpdateReason reason) override;
  void RequestMoveItemToFolder(std::string id,
                               const std::string& folder_id) override;
  void RequestMoveItemToRoot(std::string id,
                             syncer::StringOrdinal target_position) override;
  std::string RequestFolderCreation(std::string merge_target_id,
                                    std::string item_to_merge_id) override;
  void RequestFolderRename(std::string id,
                           const std::string& new_name) override;
  void RequestAppListSort(AppListSortOrder order) override;
  void RequestAppListSortRevert() override;
  void RequestCommitTemporarySortOrder() override;

  // Raw pointer version convenience versions of AppListModel methods.
  AppListItem* AddItem(AppListItem* item);
  AppListItem* AddItemToFolder(AppListItem* item, const std::string& folder_id);
  void MoveItemToFolder(AppListItem* item, const std::string& folder_id);

  // Generates a name based on |id|. Note that the returned name is sometimes
  // also used as a string id.
  static std::string GetItemName(int id);

  // Populate the model with |n| items titled "Item #".
  void PopulateApps(int n);

  // Creates and populate a folder with |n| test apps in it.
  AppListFolderItem* CreateAndPopulateFolderWithApps(int n);

  AppListFolderItem* CreateAndAddOemFolder();

  AppListFolderItem* CreateSingleItemFolder(const std::string& folder_id,
                                            const std::string& item_id);

  AppListFolderItem* CreateSingleWebAppShortcutItemFolder(
      const std::string& folder_id,
      const std::string& item_id);

  // Populate the model with an item titled "Item |id|".
  void PopulateAppWithId(int id);

  // Get a string of all apps in |model| joined with ','.
  std::string GetModelContent();

  // Creates an item with id |id|. Caller owns the result.
  AppListTestItem* CreateItem(const std::string& id);

  // Creates a web app shortcut item with id `id`. Caller owns the result.
  AppListTestItem* CreateWebAppShortcutItem(const std::string& id);

  // Creates and adds an item with id |id| to the model. Returns an unowned
  // pointer to the created item.
  AppListTestItem* CreateAndAddItem(const std::string& id);

  // Creates and adds a promise app item with id |id| to the model (i.e. item
  // will be created with status as AppStatus::kPending). Returns an unowned
  // pointer to the created item.
  AppListTestItem* CreateAndAddPromiseItem(const std::string& id);

  // Creates and adds a web app shortcut item with id `id` to the model. Returns
  // an unowned pointer to the created item.
  AppListTestItem* CreateAndAddWebAppShortcutItemWithHostBadge(
      const std::string& id);

  int activate_count() { return activate_count_; }
  AppListItem* last_activated() { return last_activated_; }

  AppListSortOrder requested_sort_order() const {
    return requested_sort_order_.value_or(AppListSortOrder::kCustom);
  }

 private:
  void ItemActivated(AppListTestItem* item);

  syncer::StringOrdinal CalculatePosition();

  int activate_count_ = 0;
  raw_ptr<AppListItem> last_activated_ = nullptr;
  int naming_index_ = 0;

  // The last sort order requested using `RequestAppListSort()`.
  std::optional<AppListSortOrder> requested_sort_order_;
};

}  // namespace test
}  // namespace ash

#endif  // ASH_APP_LIST_MODEL_APP_LIST_TEST_MODEL_H_
