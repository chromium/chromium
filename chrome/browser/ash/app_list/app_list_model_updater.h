// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_APP_LIST_MODEL_UPDATER_H_
#define CHROME_BROWSER_ASH_APP_LIST_APP_LIST_MODEL_UPDATER_H_

#include <memory>
#include <string>
#include <vector>

#include "ash/public/cpp/app_list/app_list_types.h"
#include "base/functional/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/app_list/app_list_model_updater_observer.h"
#include "chrome/browser/ash/app_list/app_list_syncable_service.h"
#include "chrome/browser/ash/app_list/chrome_app_list_item.h"

class ChromeSearchResult;

namespace ui {
class SimpleMenuModel;
}  // namespace ui

// An interface to wrap AppListModel access in browser.
class AppListModelUpdater {
 public:
  class TestApi {
   public:
    explicit TestApi(AppListModelUpdater* model_updater)
        : model_updater_(model_updater) {}
    ~TestApi() = default;

    void SetItemPosition(const std::string& id,
                         const syncer::StringOrdinal& new_position) {
      model_updater_->SetItemPosition(id, new_position);
    }

   private:
    const raw_ptr<AppListModelUpdater, DanglingUntriaged> model_updater_;
  };

  virtual ~AppListModelUpdater();

  int model_id() const { return model_id_; }

  // Returns the first available position in app list.
  syncer::StringOrdinal GetFirstAvailablePosition() const;

  // Set whether this model updater is active.
  // When we have multiple user profiles, only the active one has access to the
  // model. All others profile can only cache model changes in Chrome.
  virtual void SetActive(bool active) {}

  // For AppListModel:
  virtual void AddItem(std::unique_ptr<ChromeAppListItem> item) {}
  virtual void AddAppItemToFolder(std::unique_ptr<ChromeAppListItem> app_item,
                                  const std::string& folder_id,
                                  bool add_from_local) {}
  virtual void RemoveItem(const std::string& id, bool is_uninstall) {}
  virtual void SetStatus(ash::AppListModelStatus status) {}
  virtual void RequestDefaultPositionForModifiedOrder() {}
  virtual bool ModelHasBeenReorderedInThisSession();

  // For SearchModel:
  virtual void SetSearchEngineIsGoogle(bool is_google) {}
  virtual void PublishSearchResults(
      const std::vector<raw_ptr<ChromeSearchResult, VectorExperimental>>&
          results,
      const std::vector<ash::AppListSearchResultCategory>& categories) {}
  virtual void ClearSearchResults() {}
  virtual std::vector<raw_ptr<ChromeSearchResult, VectorExperimental>>
  GetPublishedSearchResultsForTest();

  // Item field setters only used by ChromeAppListItem and its derived classes.
  virtual void SetItemIconVersion(const std::string& id, int icon_version) {}
  virtual void SetItemIconAndColor(const std::string& id,
                                   const gfx::ImageSkia& icon,
                                   const ash::IconColor& icon_color,
                                   bool is_placeholder_icon) {}
  virtual void SetItemBadgeIcon(const std::string& id,
                                const gfx::ImageSkia& badge_icon) {}
  virtual void SetItemName(const std::string& id, const std::string& name) {}
  virtual void SetAppStatus(const std::string& id, ash::AppStatus app_status) {}
  virtual void SetItemPosition(const std::string& id,
                               const syncer::StringOrdinal& new_position) {}
  virtual void SetItemIsSystemFolder(const std::string& id,
                                     bool is_system_folder) {}
  virtual void SetIsNewInstall(const std::string& id, bool is_new_install) {}
  virtual void SetItemFolderId(const std::string& id,
                               const std::string& folder_id) = 0;
  virtual void SetNotificationBadgeColor(const std::string& id,
                                         const SkColor color) {}
  virtual void SetAccessibleName(const std::string& id,
                                 const std::string& name) {}

  virtual void SetSearchResultMetadata(
      const std::string& id,
      std::unique_ptr<ash::SearchResultMetadata> metadata) {}
  virtual void SetSearchResultIcon(const std::string& id,
                                   const gfx::ImageSkia& icon) {}
  virtual void SetSearchResultBadgeIcon(const std::string& id,
                                        const gfx::ImageSkia& badge_icon) {}
  virtual void ActivateChromeItem(const std::string& id, int event_flags) {}
  virtual void LoadAppIcon(const std::string& id) {}
  virtual void UpdateProgress(const std::string& id, float progress) {}

  // For AppListModel:
  virtual ChromeAppListItem* FindItem(const std::string& id) = 0;
  virtual std::vector<const ChromeAppListItem*> GetItems() const = 0;
  virtual std::set<std::string> GetTopLevelItemIds() const = 0;
  virtual size_t ItemCount() = 0;
  virtual std::vector<ChromeAppListItem*> GetTopLevelItems() const = 0;
  virtual ChromeAppListItem* ItemAtForTest(size_t index) = 0;
  virtual ChromeAppListItem* FindFolderItem(const std::string& folder_id) = 0;
  virtual bool FindItemIndexForTest(const std::string& id, size_t* index) = 0;
  // Returns a position which is before the first item in the item list.
  virtual syncer::StringOrdinal GetPositionBeforeFirstItem() const = 0;

  // Methods for AppListSyncableService:
  virtual void UpdateAppItemFromSyncItem(
      app_list::AppListSyncableService::SyncItem* sync_item,
      bool update_name,
      bool update_folder) {}
  virtual void NotifyProcessSyncChangesFinished() {}

  using GetMenuModelCallback =
      base::OnceCallback<void(std::unique_ptr<ui::SimpleMenuModel>)>;
  virtual void GetContextMenuModel(const std::string& id,
                                   ash::AppListItemContext item_context,
                                   GetMenuModelCallback callback) = 0;
  virtual size_t BadgedItemCount() = 0;
  // For SearchModel:
  virtual bool SearchEngineIsGoogle() = 0;
  virtual void RecalculateWouldTriggerLauncherSearchIph() = 0;

  // Notifies when the app list gets hidden.
  virtual void OnAppListHidden() = 0;

  virtual void AddObserver(AppListModelUpdaterObserver* observer) = 0;
  virtual void RemoveObserver(AppListModelUpdaterObserver* observer) = 0;

 protected:
  FRIEND_TEST_ALL_PREFIXES(AppListSyncableServiceTest, FirstAvailablePosition);
  FRIEND_TEST_ALL_PREFIXES(AppListSyncableServiceTest,
                           FirstAvailablePositionNotExist);

  AppListModelUpdater();

  // Returns a position which is before the first item in the app list. If
  // |top_level_items| is empty, creates an initial position instead.
  static syncer::StringOrdinal GetPositionBeforeFirstItemInternal(
      const std::vector<ChromeAppListItem*>& top_level_items);

 private:
  const int model_id_;
};

#endif  // CHROME_BROWSER_ASH_APP_LIST_APP_LIST_MODEL_UPDATER_H_
