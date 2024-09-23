// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_CHROME_APP_LIST_MODEL_UPDATER_H_
#define CHROME_BROWSER_ASH_APP_LIST_CHROME_APP_LIST_MODEL_UPDATER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "ash/app_list/model/app_list_model.h"
#include "ash/app_list/model/app_list_model_observer.h"
#include "ash/app_list/model/search/search_model.h"
#include "ash/app_list/quick_app_access_model.h"
#include "ash/public/cpp/app_list/app_list_model_delegate.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/ash/app_list/app_list_model_updater.h"

namespace app_list {
namespace reorder {
class AppListReorderDelegate;
struct ReorderParam;
}  // namespace reorder

class TemporaryAppListSortTest;
}  // namespace app_list

class ChromeAppListItem;
class ChromeAppListItemManager;

class ChromeAppListModelUpdater : public AppListModelUpdater,
                                  public ash::AppListModelDelegate,
                                  public ash::AppListModelObserver {
 public:
  ChromeAppListModelUpdater(
      Profile* profile,
      app_list::reorder::AppListReorderDelegate* order_delegate,
      app_list::AppListSyncModelSanitizer* sync_model_sanitizer);
  ChromeAppListModelUpdater(const ChromeAppListModelUpdater&) = delete;
  ChromeAppListModelUpdater& operator=(const ChromeAppListModelUpdater&) =
      delete;
  ~ChromeAppListModelUpdater() override;

  void SetActive(bool active) override;

  // AppListModelUpdater:
  void AddItem(std::unique_ptr<ChromeAppListItem> app_item) override;
  void AddAppItemToFolder(std::unique_ptr<ChromeAppListItem> app_item,
                          const std::string& folder_id,
                          bool add_from_local) override;
  void RemoveItem(const std::string& id, bool is_uninstall) override;
  void SetStatus(ash::AppListModelStatus status) override;
  void SetSearchEngineIsGoogle(bool is_google) override;
  void RecalculateWouldTriggerLauncherSearchIph() override;
  void PublishSearchResults(
      const std::vector<raw_ptr<ChromeSearchResult, VectorExperimental>>&
          results,
      const std::vector<ash::AppListSearchResultCategory>& categories) override;
  void ClearSearchResults() override;
  std::vector<raw_ptr<ChromeSearchResult, VectorExperimental>>
  GetPublishedSearchResultsForTest() override;
  void SetAccessibleName(const std::string& id,
                         const std::string& name) override;
  bool ModelHasBeenReorderedInThisSession() override;

  // Methods only used by ChromeAppListItem that talk to ash directly.
  void SetItemIconVersion(const std::string& id, int icon_version) override;
  void SetItemIconAndColor(const std::string& id,
                           const gfx::ImageSkia& icon,
                           const ash::IconColor& icon_color,
                           bool is_placeholder_icon) override;
  void SetItemBadgeIcon(const std::string& id,
                        const gfx::ImageSkia& badge_icon) override;
  void SetItemName(const std::string& id, const std::string& name) override;
  void SetAppStatus(const std::string& id, ash::AppStatus app_status) override;
  void SetItemPosition(const std::string& id,
                       const syncer::StringOrdinal& new_position) override;
  void SetItemIsSystemFolder(const std::string& id,
                             bool is_system_folder) override;
  void SetIsNewInstall(const std::string& id, bool is_new_install) override;
  void SetItemFolderId(const std::string& id,
                       const std::string& folder_id) override;
  void SetNotificationBadgeColor(const std::string& id,
                                 const SkColor color) override;
  void RequestDefaultPositionForModifiedOrder() override;

  // Methods only used by ChromeSearchResult that talk to ash directly.
  void SetSearchResultMetadata(
      const std::string& id,
      std::unique_ptr<ash::SearchResultMetadata> metadata) override;

  void ActivateChromeItem(const std::string& id, int event_flags) override;
  void LoadAppIcon(const std::string& id) override;
  void UpdateProgress(const std::string& id, float progress) override;

  // Methods for item querying.
  ChromeAppListItem* FindItem(const std::string& id) override;
  std::vector<const ChromeAppListItem*> GetItems() const override;
  std::set<std::string> GetTopLevelItemIds() const override;
  size_t ItemCount() override;
  std::vector<ChromeAppListItem*> GetTopLevelItems() const override;
  ChromeAppListItem* ItemAtForTest(size_t index) override;
  ChromeAppListItem* FindFolderItem(const std::string& folder_id) override;
  bool FindItemIndexForTest(const std::string& id, size_t* index) override;
  bool SearchEngineIsGoogle() override;
  size_t BadgedItemCount() override;
  void GetContextMenuModel(const std::string& id,
                           ash::AppListItemContext item_context,
                           GetMenuModelCallback callback) override;
  syncer::StringOrdinal GetPositionBeforeFirstItem() const override;

  // Methods for AppListSyncableService:
  void UpdateAppItemFromSyncItem(
      app_list::AppListSyncableService::SyncItem* sync_item,
      bool update_name,
      bool update_folder) override;

  void OnAppListHidden() override;

  void AddObserver(AppListModelUpdaterObserver* observer) override;
  void RemoveObserver(AppListModelUpdaterObserver* observer) override;

  // AppListModelObserver:
  void OnAppListItemAdded(ash::AppListItem* item) override;
  // NOTE: `OnAppListItemUpdated()` could reset the metadata of the chrome
  // app list item that shares the id of `item`. Therefore, do not access any
  // reference to the old metadata after calling this function.
  void OnAppListItemUpdated(ash::AppListItem* item) override;
  void OnAppListItemWillBeDeleted(ash::AppListItem* item) override;

  // AppListModelDelegate:
  void RequestPositionUpdate(std::string id,
                             const syncer::StringOrdinal& new_position,
                             ash::RequestPositionUpdateReason reason) override;
  void RequestMoveItemToFolder(std::string id,
                               const std::string& folder_id) override;
  void RequestMoveItemToRoot(std::string id,
                             syncer::StringOrdinal target_position) override;
  std::string RequestFolderCreation(std::string target_merge_id,
                                    std::string item_to_merge_id) override;
  void RequestFolderRename(std::string folder_id,
                           const std::string& new_name) override;
  void RequestAppListSort(ash::AppListSortOrder order) override;
  void RequestAppListSortRevert() override;
  void RequestCommitTemporarySortOrder() override;

  // Returns the temporary sort order.
  ash::AppListSortOrder GetTemporarySortOrderForTest() const;

  // Returns true if the app list is under temporary sort.
  bool is_under_temporary_sort() const { return !!temporary_sort_manager_; }

  ash::AppListModel* model_for_test() { return &model_; }

 private:
  friend class app_list::TemporaryAppListSortTest;

  class TemporarySortManager;

  enum class ItemChangeType {
    // An item is added.
    kAdd,

    // An item is updated.
    kUpdate,

    // An item will be deleted.
    kDelete
  };

  // Notifies observers of the change on `chrome_item` when temporary app list
  // sort is not active.
  void MaybeNotifyObserversOfItemChange(ChromeAppListItem* chrome_item,
                                        ItemChangeType type);

  // Lists the action that can be performed when app list exits the temporary
  // sort status.
  enum class EndAction {
    // Commit temporary positions and update the permanent order with the
    // temporary order.
    kCommit,

    // Revert temporary positions and the permanent order does not change.
    kRevert,

    // Commit temporary positions and clear the permanent order.
    kCommitAndClearSort
  };

  // Ends temporary sort status and performs the specified action.
  void EndTemporarySortAndTakeAction(EndAction action);

  // Commits item positions under the temporary sort.
  void CommitTemporaryPositions();

  // Calculates the reorder params for reverting the temporary order.
  std::vector<app_list::reorder::ReorderParam>
  CalculateReorderParamsForRevertOrder() const;

  // Updates the item positions in the ash side. `reorder_params` specifies
  // target positions.
  void UpdateItemPositionWithReorderParam(
      const std::vector<app_list::reorder::ReorderParam>& reorder_params);

  // Resets the pref sort order to be kCustom when the app list is not under
  // temporary sorting. `event` indicates the reason leading to reset.
  void ResetPrefSortOrderInNonTemporaryMode(ash::AppListOrderUpdateEvent event);

  // Updates the position carried by `data` based on the icon color if the app
  // list is sorted by color.
  void MaybeUpdatePositionWhenIconColorChange(ash::AppListItemMetadata* data);

  void OnFeatureEngagementTrackerInitialized(bool success);

  // Indicates the profile that the model updater is associated with.
  const raw_ptr<Profile> profile_ = nullptr;

  // Provides the access to the methods for ordering app list items.
  const raw_ptr<app_list::reorder::AppListReorderDelegate> order_delegate_;
  const raw_ptr<app_list::AppListSyncModelSanitizer> sync_model_sanitizer_;

  // A helper class to manage app list items. It never talks to ash.
  std::unique_ptr<ChromeAppListItemManager> item_manager_;

  ash::AppListModel model_;
  ash::SearchModel search_model_;
  ash::QuickAppAccessModel quick_app_access_model_;

  bool is_active_ = false;

  // The most recently list of search results.
  std::vector<raw_ptr<ChromeSearchResult, VectorExperimental>>
      published_results_;
  base::ObserverList<AppListModelUpdaterObserver> observers_;
  bool search_engine_is_google_ = false;

  // Whether the model has reordered the position of an item in the current
  // session.
  bool has_requested_move_item_position_ = false;

  // The id of the item whose icon update is in progress.
  std::optional<std::string> item_with_icon_update_;

  // Set when sort is triggered and reset when exiting the temporary sort
  // status.
  std::unique_ptr<TemporarySortManager> temporary_sort_manager_;

  base::WeakPtrFactory<ChromeAppListModelUpdater> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_ASH_APP_LIST_CHROME_APP_LIST_MODEL_UPDATER_H_
