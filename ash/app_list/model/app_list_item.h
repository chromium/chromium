// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_MODEL_APP_LIST_ITEM_H_
#define ASH_APP_LIST_MODEL_APP_LIST_ITEM_H_

#include <stddef.h>

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "ash/app_list/model/app_list_model_export.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/public/cpp/shelf_types.h"
#include "base/observer_list.h"
#include "components/sync/model/string_ordinal.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/view.h"

namespace ash {
enum class AppListConfigType;
class AppListItemList;
class AppListItemListTest;
class AppListItemObserver;
class AppListModel;

// AppListItem provides icon and title to be shown in a AppListItemView
// and action to be executed when the AppListItemView is activated.
class APP_LIST_MODEL_EXPORT AppListItem {
 public:
  explicit AppListItem(const std::string& id);
  AppListItem(const AppListItem&) = delete;
  AppListItem& operator=(const AppListItem&) = delete;
  virtual ~AppListItem();

  void SetIcon(AppListConfigType config_type, const gfx::ImageSkia& icon);
  const gfx::ImageSkia& GetIcon(AppListConfigType config_type) const;

  // Setter and getter for the default app list item icon. Used as a base to
  // generate appropriate app list item icon for an app list config if an icon
  // for the config has not been set using `SetIcon()`. The icon color is
  // associated with the icon so set the icon color when the icon is set.
  void SetDefaultIconAndColor(const gfx::ImageSkia& icon,
                              const IconColor& color);
  const gfx::ImageSkia& GetDefaultIcon() const;

  // Returns the icon color associated with the default icon.
  const IconColor& GetDefaultIconColor() const;

  // Sets an number to represent the current icon version. It is used so that
  // the data provider side (AppService) only marks an icon change without
  // actually loading the icon. When AppLIteItem is added to UI, UI code
  // observes this icon version number and calls back into data provider to
  // perform the actual icon loading. When the icon is loaded, SetIcon is called
  // and UI would be updated since it also observe ItemIconChanged.
  void SetIconVersion(int icon_version);

  SkColor GetNotificationBadgeColor() const;
  void SetNotificationBadgeColor(const SkColor color);

  const std::string& GetDisplayName() const {
    return short_name_.empty() ? name() : short_name_;
  }

  const std::string& name() const { return metadata_->name; }
  // Should only be used in tests; otherwise use GetDisplayName().
  const std::string& short_name() const { return short_name_; }

  bool IsInFolder() const { return !folder_id().empty(); }

  const std::string& id() const { return metadata_->id; }
  const std::string& folder_id() const { return metadata_->folder_id; }
  const syncer::StringOrdinal& position() const { return metadata_->position; }

  void SetMetadata(std::unique_ptr<AppListItemMetadata> metadata) {
    metadata_ = std::move(metadata);
  }
  const AppListItemMetadata* GetMetadata() const { return metadata_.get(); }
  std::unique_ptr<AppListItemMetadata> CloneMetadata() const {
    return std::make_unique<AppListItemMetadata>(*metadata_);
  }

  void AddObserver(AppListItemObserver* observer);
  void RemoveObserver(AppListItemObserver* observer);

  // Returns a static const char* identifier for the subclass (defaults to "").
  // Pointers can be compared for quick type checking.
  virtual const char* GetItemType() const;

  // Returns the item matching |id| contained in this item (e.g. if the item is
  // a folder), or nullptr if the item was not found or this is not a container.
  virtual AppListItem* FindChildItem(const std::string& id);

  // Returns the child item at the provided index in the child item list.
  // Returns nullptr for non-folder items.
  virtual AppListItem* GetChildItemAt(size_t index);

  // Returns the number of child items if it has any (e.g. is a folder) or 0.
  virtual size_t ChildItemCount() const;

  // Request a folder item for an icon refresh. Method is no-op for app items.
  virtual void RequestFolderIconUpdate() {}

  // Returns whether the item is a folder with max allowed children.
  bool IsFolderFull() const;

  std::string ToDebugString() const;

  bool is_folder() const { return metadata_->is_folder; }

  bool has_notification_badge() const { return has_notification_badge_; }

  bool is_new_install() const { return metadata_->is_new_install; }

  // Sets the `is_new_install` metadata field and notifies observers.
  void SetIsNewInstall(bool is_new_install);

  AppStatus app_status() const { return metadata_->app_status; }

  void UpdateNotificationBadgeForTesting(bool has_badge) {
    UpdateNotificationBadge(has_badge);
  }

  void UpdateAppStatusForTesting(AppStatus app_status) {
    metadata_->app_status = app_status;
  }

 protected:
  // Subclasses also have mutable access to the metadata ptr.
  AppListItemMetadata* metadata() { return metadata_.get(); }

  friend class AppListBadgeController;
  friend class AppListItemList;
  friend class AppListItemListTest;
  friend class AppListItemViewPixelTest;
  friend class AppListItemViewTest;
  friend class AppListModel;

  // These should only be called by AppListModel or in tests so that name
  // changes trigger update notifications.

  // Sets the full name of the item. Clears any shortened name.
  void SetName(const std::string& name);

  // Updates whether the notification badge is shown on the view.
  void UpdateNotificationBadge(bool has_badge);

  void set_position(const syncer::StringOrdinal& new_position) {
    DCHECK(new_position.IsValid());
    metadata_->position = new_position;
  }

  void set_folder_id(const std::string& folder_id) {
    metadata_->folder_id = folder_id;
  }

  void set_is_folder(bool is_folder) { metadata_->is_folder = is_folder; }

 private:
  friend class AppListModelTest;

  std::unique_ptr<AppListItemMetadata> metadata_;

  // Contains icons for AppListConfigTypes different than kShared. For kShared
  // config type, the item will always use the icon provided by |metadata_|.
  // This is currently used for folder icons only (which are all generated in
  // ash).
  std::map<AppListConfigType, gfx::ImageSkia> per_config_icons_;

  // A shortened name for the item, used for display.
  std::string short_name_;

  // Whether this item currently has a notification badge that should be shown.
  bool has_notification_badge_ = false;

  base::ObserverList<AppListItemObserver> observers_;
};

}  // namespace ash

#endif  // ASH_APP_LIST_MODEL_APP_LIST_ITEM_H_
