// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_APP_LIST_APP_LIST_CONTROLLER_H_
#define ASH_PUBLIC_CPP_APP_LIST_APP_LIST_CONTROLLER_H_

#include <memory>
#include <string>

#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/public/cpp/ash_public_export.h"
#include "base/callback_forward.h"
#include "base/containers/flat_map.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/aura/window.h"

namespace ash {

class AppListClient;
class AppListControllerObserver;

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

  virtual void AddObserver(AppListControllerObserver* observer) = 0;
  virtual void RemoveObserver(AppListControllerObserver* obsever) = 0;

  // Adds an item to AppListModel.
  virtual void AddItem(std::unique_ptr<AppListItemMetadata> app_item) = 0;

  // Adds an item into a certain folder in AppListModel.
  virtual void AddItemToFolder(std::unique_ptr<AppListItemMetadata> app_item,
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
  virtual void SetStatus(AppListModelStatus status) = 0;

  // Sets whether the search engine is Google or not.
  virtual void SetSearchEngineIsGoogle(bool is_google) = 0;

  // Sets the text for the search box's Textfield and the voice search flag.
  virtual void UpdateSearchBox(const std::u16string& text,
                               bool initiated_by_user) = 0;

  // Publishes search results to Ash to render them.
  virtual void PublishSearchResults(
      std::vector<std::unique_ptr<SearchResultMetadata>> results) = 0;

  // Updates an item's metadata (e.g. name, position, etc).
  virtual void SetItemMetadata(const std::string& id,
                               std::unique_ptr<AppListItemMetadata> data) = 0;

  virtual void SetItemIconVersion(const std::string& id, int icon_version) = 0;

  // Updates an item's icon.
  virtual void SetItemIcon(const std::string& id,
                           const gfx::ImageSkia& icon) = 0;

  virtual void SetItemNotificationBadgeColor(const std::string& id,
                                             const SkColor color) = 0;

  // Update the whole model, usually when profile changes happen in Chrome.
  virtual void SetModelData(
      int profile_id,
      std::vector<std::unique_ptr<AppListItemMetadata>> apps,
      bool is_search_engine_google) = 0;

  // Updates a search rresult's metadata.
  virtual void SetSearchResultMetadata(
      std::unique_ptr<SearchResultMetadata> metadata) = 0;

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
      base::OnceCallback<void(std::unique_ptr<AppListItemMetadata>)>;
  virtual void ResolveOemFolderPosition(
      const syncer::StringOrdinal& preferred_oem_position,
      ResolveOemFolderPositionCallback callback) = 0;

  // Notifies sync service has finished processing sync changes.
  virtual void NotifyProcessSyncChangesFinished() = 0;

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

  // Returns whether the AppList is visible on the provided display.
  // If |display_id| is null, returns whether an app list is visible on any
  // display.
  virtual bool IsVisible(const absl::optional<int64_t>& display_id) = 0;

  // Returns whether the AppList is visible on any display.
  virtual bool IsVisible() = 0;

 protected:
  AppListController();
  virtual ~AppListController();
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_APP_LIST_APP_LIST_CONTROLLER_H_
