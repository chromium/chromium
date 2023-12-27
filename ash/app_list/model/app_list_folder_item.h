// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_MODEL_APP_LIST_FOLDER_ITEM_H_
#define ASH_APP_LIST_MODEL_APP_LIST_FOLDER_ITEM_H_

#include <stddef.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "ash/app_list/model/app_list_item.h"
#include "ash/app_list/model/app_list_item_list_observer.h"
#include "ash/app_list/model/app_list_item_observer.h"
#include "ash/app_list/model/app_list_model_export.h"
#include "ash/app_list/model/folder_image.h"
#include "ash/public/cpp/app_list/app_list_config_provider.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"

namespace gfx {
class Rect;
}  // namespace gfx

namespace ash {

class AppListConfig;
class AppListItemList;
class AppListModelDelegate;

// AppListFolderItem implements the model/controller for folders. It observes
// all the items in its list to watch for property changes (e.g. whether a child
// item is a new install).
class APP_LIST_MODEL_EXPORT AppListFolderItem
    : public AppListItem,
      public FolderImageObserver,
      public AppListConfigProvider::Observer,
      public AppListItemListObserver,
      public AppListItemObserver {
 public:
  // The folder type affects folder behavior.
  enum FolderType {
    // Default folder type.
    FOLDER_TYPE_NORMAL,
    // Items can not be moved to/from OEM folders in the UI.
    FOLDER_TYPE_OEM
  };

  static const char kItemType[];

  AppListFolderItem(const std::string& id,
                    AppListModelDelegate* app_list_model_delegate);

  AppListFolderItem(const AppListFolderItem&) = delete;
  AppListFolderItem& operator=(const AppListFolderItem&) = delete;

  ~AppListFolderItem() override;

  // Returns the target icon bounds for |item| to fly back to its parent folder
  // icon in animation UI. If |item| is one of the top item icon, this will
  // match its corresponding top item icon in the folder icon. Otherwise,
  // the target icon bounds is centered at the |folder_icon_bounds| with
  // the same size of the top item icon.
  // The Rect returned is in the same coordinates of |folder_icon_bounds|.
  gfx::Rect GetTargetIconRectInFolderForItem(
      const AppListConfig& app_list_config,
      AppListItem* item,
      const gfx::Rect& folder_icon_bounds);

  AppListItemList* item_list() { return item_list_.get(); }
  const AppListItemList* item_list() const { return item_list_.get(); }

  FolderType folder_type() const { return folder_type_; }

  // AppListItem overrides:
  AppListFolderItem* AsFolderItem() override;
  const char* GetItemType() const override;
  AppListItem* FindChildItem(const std::string& id) override;
  AppListItem* GetChildItemAt(size_t index) override;
  size_t ChildItemCount() const override;
  void RequestFolderIconUpdate() override;

  // AppListConfigProvider::Observer override:
  void OnAppListConfigCreated(AppListConfigType config_type) override;

  // AppListItemListObserver:
  void OnListItemAdded(size_t index, AppListItem* item) override;
  void OnListItemRemoved(size_t index, AppListItem* item) override;

  // AppListItemObserver:
  void ItemBadgeVisibilityChanged() override;
  void ItemIsNewInstallChanged() override;

  // Whether this is a system created folder like the Linux apps folder or the
  // OEM folder.
  bool IsSystemFolder() const;
  void SetIsSystemFolder(bool is_system_folder);

  // Returns an id for a new folder.
  static std::string GenerateId();

  // FolderImageObserver overrides:
  void OnFolderImageUpdated(AppListConfigType config_type) override;

  // Informs the folder item of an item being dragged, that it may notify its
  // image.
  void NotifyOfDraggedItem(AppListItem* dragged_item);

  FolderImage* GetFolderImageForTesting(AppListConfigType type) const;

 private:
  // Creates FolderImages for config types in |config_types| that also exist in
  // AppListConfigProvider, and adds them to |folder_images_|.
  // |request_icon_update| - Whether FolderImage::UpdateIcon() should be called
  //     on the created icon images - this should be set if called outside app
  //     list model initialization (i.e. outside constructor).
  void EnsureIconsForAvailableConfigTypes(
      const std::vector<AppListConfigType>& config_types,
      bool request_icon_update);

  // Sets the "new install" property on this folder item if any of the items
  // inside the folder are new installs.
  void UpdateIsNewInstall();

  // Adds a notification badge on this folder item if any of the items inside
  // the folder are new installs.
  void UpdateNotificationBadge();

  // The type of folder; may affect behavior of folder views.
  const FolderType folder_type_;

  // List of items in the folder.
  std::unique_ptr<AppListItemList> item_list_;

  std::map<AppListConfigType, std::unique_ptr<FolderImage>> folder_images_;

  // Set when a folder item is being dragged.
  raw_ptr<AppListItem> dragged_item_ = nullptr;

  base::ScopedObservation<AppListConfigProvider,
                          AppListConfigProvider::Observer>
      config_provider_observation_{this};
};

}  // namespace ash

#endif  // ASH_APP_LIST_MODEL_APP_LIST_FOLDER_ITEM_H_
