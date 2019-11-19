// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_APP_LIST_APP_LIST_CONTROLLER_H_
#define ASH_PUBLIC_CPP_APP_LIST_APP_LIST_CONTROLLER_H_

#include <memory>

#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/public/cpp/ash_public_export.h"
#include "base/callback_forward.h"
#include "base/containers/flat_map.h"
#include "base/strings/string16.h"
#include "ui/aura/window.h"

namespace ash {

class AppListClient;

// An interface implemented in Ash to handle calls from Chrome.
// These include:
// - When app list data changes in Chrome, it should notifies the UI models and
//   views in Ash to get updated. This can happen while syncing, searching, etc.
// - When Chrome needs real-time UI information from Ash. This can happen while
//   calculating recommended search results based on the app list item order.
// - When app list states in Chrome change that require UI's response. This can
//   happen while installing/uninstalling apps and the app list gets toggled.
class ASH_PUBLIC_EXPORT AppListController {
 public:
  // Gets the instance.
  static AppListController* Get();

  // Sets a client to handle calls from Ash.
  virtual void SetClient(AppListClient* client) = 0;

  // Gets the client that handles calls from Ash.
  virtual AppListClient* GetClient() = 0;

  // Adds an item to AppListModel.
  virtual void AddItem(std::unique_ptr<ash::AppListItemMetadata> app_item) = 0;

  // Adds an item into a certain folder in AppListModel.
  virtual void AddItemToFolder(
      std::unique_ptr<ash::AppListItemMetadata> app_item,
      const std::string& folder_id) = 0;

  // Removes an item by its id from AppListModel.
  virtual void RemoveItem(const std::string& id) = 0;

  // Removes an item by its id, and also cleans up if its parent folder has a
  // single child left.
  virtual void RemoveUninstalledItem(const std::string& id) = 0;

  // Moves the item with |id| to the folder with |folder_id|.
  virtual void MoveItemToFolder(const std::string& id,
                                const std::string& folder_id) = 0;

  // Tells Ash what the current status of AppListModel should be,
  // e.g. the model is under synchronization or in normal status.
  virtual void SetStatus(ash::AppListModelStatus status) = 0;

  // Tells Ash what the current state of the app list should be,
  // e.g. the user is searching for something, or showing apps, etc.
  virtual void SetState(ash::AppListState state) = 0;

  // Highlights the given item in the app list. If not present and it is later
  // added, the item will be highlighted after being added.
  virtual void HighlightItemInstalledFromUI(const std::string& id) = 0;

  // Sets whether the search engine is Google or not.
  virtual void SetSearchEngineIsGoogle(bool is_google) = 0;

  // Sets the text for screen readers on the search box, and updates the
  // accessible names.
  virtual void SetSearchTabletAndClamshellAccessibleName(
      const base::string16& tablet_accessible_name,
      const base::string16& clamshell_accessible_name) = 0;

  // Sets the hint text to display when there is in input.
  virtual void SetSearchHintText(const base::string16& hint_text) = 0;

  // Sets the text for the search box's Textfield and the voice search flag.
  virtual void UpdateSearchBox(const base::string16& text,
                               bool initiated_by_user) = 0;

  // Publishes search results to Ash to render them.
  virtual void PublishSearchResults(
      std::vector<std::unique_ptr<ash::SearchResultMetadata>> results) = 0;

  // Updates an item's metadata (e.g. name, position, etc).
  virtual void SetItemMetadata(
      const std::string& id,
      std::unique_ptr<ash::AppListItemMetadata> data) = 0;

  // Updates an item's icon.
  virtual void SetItemIcon(const std::string& id,
                           const gfx::ImageSkia& icon) = 0;

  // Updates whether an item is installing.
  virtual void SetItemIsInstalling(const std::string& id,
                                   bool is_installing) = 0;

  // Updates the downloaded percentage of an item.
  virtual void SetItemPercentDownloaded(const std::string& id,
                                        int32_t percent_downloaded) = 0;

  // Update the whole model, usually when profile changes happen in Chrome.
  virtual void SetModelData(
      int profile_id,
      std::vector<std::unique_ptr<ash::AppListItemMetadata>> apps,
      bool is_search_engine_google) = 0;

  // Updates a search rresult's metadata.
  virtual void SetSearchResultMetadata(
      std::unique_ptr<ash::SearchResultMetadata> metadata) = 0;

  // Updates whether a search result is being installed.
  virtual void SetSearchResultIsInstalling(const std::string& id,
                                           bool is_installing) = 0;

  // Updates the download progress of a search result.
  virtual void SetSearchResultPercentDownloaded(const std::string& id,
                                                int32_t percent_downloaded) = 0;

  // Called when the app represented by a search result is installed.
  virtual void NotifySearchResultItemInstalled(const std::string& id) = 0;

  // Returns a map from each item's id to its shown index in the app list.
  using GetIdToAppListIndexMapCallback =
      base::OnceCallback<void(const base::flat_map<std::string, uint16_t>&)>;
  virtual void GetIdToAppListIndexMap(
      GetIdToAppListIndexMapCallback callback) = 0;

  // Finds the OEM folder or creates one if it doesn't exist.
  // |oem_folder_name|: the expected name of the OEM folder while creating.
  // |preferred_oem_position|: the preferred position of the OEM folder while
  //                           creating; if it's invalid then the final position
  //                           is determined in Ash.
  // |oem_folder|: the meta data of the existing/created OEM folder.
  using FindOrCreateOemFolderCallback = base::OnceClosure;
  virtual void FindOrCreateOemFolder(
      const std::string& oem_folder_name,
      const syncer::StringOrdinal& preferred_oem_position,
      FindOrCreateOemFolderCallback callback) = 0;

  // Resolves the position of the OEM folder.
  // |preferred_oem_position|: the preferred position of the OEM folder; if it's
  //                           invalid then the final position is determined in
  //                           Ash.
  // |oem_folder|: the meta data of the OEM folder, or null if it doesn't exist.
  using ResolveOemFolderPositionCallback =
      base::OnceCallback<void(std::unique_ptr<ash::AppListItemMetadata>)>;
  virtual void ResolveOemFolderPosition(
      const syncer::StringOrdinal& preferred_oem_position,
      ResolveOemFolderPositionCallback callback) = 0;

  // Dismisses the app list.
  virtual void DismissAppList() = 0;

  // Returns bounds of a rectangle to show an AppInfo dialog.
  using GetAppInfoDialogBoundsCallback =
      base::OnceCallback<void(const gfx::Rect&)>;
  virtual void GetAppInfoDialogBounds(
      GetAppInfoDialogBoundsCallback callback) = 0;

  // Shows the app list.
  virtual void ShowAppList() = 0;

  // Returns the app list window or nullptr if it is not visible.
  virtual aura::Window* GetWindow() = 0;

  // Returns whether the AppList is visible.
  virtual bool IsVisible() = 0;

 protected:
  AppListController();
  virtual ~AppListController();
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_APP_LIST_APP_LIST_CONTROLLER_H_
