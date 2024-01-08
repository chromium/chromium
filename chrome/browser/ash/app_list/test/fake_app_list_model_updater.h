// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_TEST_FAKE_APP_LIST_MODEL_UPDATER_H_
#define CHROME_BROWSER_ASH_APP_LIST_TEST_FAKE_APP_LIST_MODEL_UPDATER_H_

#include <memory>
#include <string>

#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/ash/app_list/app_list_model_updater.h"
#include "chrome/browser/ash/app_list/search/chrome_search_result.h"

class ChromeAppListItem;

class FakeAppListModelUpdater : public AppListModelUpdater {
 public:
  FakeAppListModelUpdater(
      Profile* profile,
      app_list::reorder::AppListReorderDelegate* order_delegate);
  FakeAppListModelUpdater(const FakeAppListModelUpdater&) = delete;
  FakeAppListModelUpdater& operator=(const FakeAppListModelUpdater&) = delete;
  ~FakeAppListModelUpdater() override;

  // For AppListModel:
  void AddItem(std::unique_ptr<ChromeAppListItem> item) override;
  void AddAppItemToFolder(std::unique_ptr<ChromeAppListItem> item,
                          const std::string& folder_id,
                          bool add_from_local) override;
  void UpdateAppItemFromSyncItem(
      app_list::AppListSyncableService::SyncItem* sync_item,
      bool update_name,
      bool update_folder) override;
  void RemoveItem(const std::string& id, bool is_uninstall) override;
  void SetItemIconAndColor(const std::string& id,
                           const gfx::ImageSkia& icon,
                           const ash::IconColor& icon_color,
                           bool is_placeholder_icon) override;
  void SetItemFolderId(const std::string& id,
                       const std::string& folder_id) override;
  void SetItemPosition(const std::string& id,
                       const syncer::StringOrdinal& new_position) override;
  void SetItemName(const std::string& id, const std::string& new_name) override;
  // For SearchModel:
  void SetSearchEngineIsGoogle(bool is_google) override;
  void PublishSearchResults(
      const std::vector<raw_ptr<ChromeSearchResult, VectorExperimental>>&
          results,
      const std::vector<ash::AppListSearchResultCategory>& categories) override;
  void ClearSearchResults() override;

  void ActivateChromeItem(const std::string& id, int event_flags) override;
  void LoadAppIcon(const std::string& id) override;

  // For AppListModel:
  ChromeAppListItem* FindItem(const std::string& id) override;
  std::vector<const ChromeAppListItem*> GetItems() const override;
  std::set<std::string> GetTopLevelItemIds() const override;
  size_t ItemCount() override;
  std::vector<ChromeAppListItem*> GetTopLevelItems() const override;
  ChromeAppListItem* ItemAtForTest(size_t index) override;
  ChromeAppListItem* FindFolderItem(const std::string& folder_id) override;
  bool FindItemIndexForTest(const std::string& id, size_t* index) override;
  syncer::StringOrdinal GetPositionBeforeFirstItem() const override;
  void GetContextMenuModel(const std::string& id,
                           ash::AppListItemContext item_context,
                           GetMenuModelCallback callback) override;
  size_t BadgedItemCount() override;

  // For SearchModel:
  bool SearchEngineIsGoogle() override;
  void RecalculateWouldTriggerLauncherSearchIph() override;
  const std::vector<raw_ptr<ChromeSearchResult, VectorExperimental>>&
  search_results() const {
    return search_results_;
  }

  void OnAppListHidden() override {}

  void AddObserver(AppListModelUpdaterObserver* observer) override;
  void RemoveObserver(AppListModelUpdaterObserver* observer) override;

  void WaitForIconUpdates(size_t expected_updates);

  size_t update_image_count() const { return update_image_count_; }

 private:
  raw_ptr<Profile> profile_;

  bool search_engine_is_google_ = false;
  std::vector<std::unique_ptr<ChromeAppListItem>> items_;
  std::vector<raw_ptr<ChromeSearchResult, VectorExperimental>> search_results_;
  base::ObserverList<AppListModelUpdaterObserver> observers_;

  size_t update_image_count_ = 0;
  size_t expected_update_image_count_ = 0;
  base::OnceClosure icon_updated_callback_;

  void FindOrCreateOemFolder(
      const std::string& oem_folder_name,
      const syncer::StringOrdinal& preferred_oem_position);
  syncer::StringOrdinal GetOemFolderPos();
};

#endif  // CHROME_BROWSER_ASH_APP_LIST_TEST_FAKE_APP_LIST_MODEL_UPDATER_H_
