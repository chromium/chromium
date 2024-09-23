// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_CHROME_APP_LIST_ITEM_H_
#define CHROME_BROWSER_ASH_APP_LIST_CHROME_APP_LIST_ITEM_H_

#include <memory>
#include <string>
#include <utility>

#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/public/cpp/shelf_types.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/app_list/app_context_menu.h"
#include "chrome/browser/ash/app_list/app_list_syncable_service.h"
#include "ui/gfx/image/image_skia.h"

class AppListControllerDelegate;
class AppListModelUpdater;
class Profile;

namespace extensions {
class AppSorting;
}  // namespace extensions

namespace ui {
class SimpleMenuModel;
}  // namespace ui

// Base class of all chrome app list items.
class ChromeAppListItem {
 public:
  class TestApi {
   public:
    explicit TestApi(ChromeAppListItem* item);
    ~TestApi() = default;

    void SetFolderId(const std::string& folder_id);
    void SetPosition(const syncer::StringOrdinal& position);
    void SetName(const std::string& name);

   private:
    const raw_ptr<ChromeAppListItem> item_;
  };

  ChromeAppListItem(Profile* profile,
                    const std::string& app_id,
                    AppListModelUpdater* model_updater);
  ChromeAppListItem(const ChromeAppListItem&) = delete;
  ChromeAppListItem& operator=(const ChromeAppListItem&) = delete;
  virtual ~ChromeAppListItem();

  // AppListControllerDelegate is not properly implemented in tests. Use mock
  // |controller| for unit_tests.
  static void OverrideAppListControllerDelegateForTesting(
      AppListControllerDelegate* controller);

  static gfx::ImageSkia CreateDisabledIcon(const gfx::ImageSkia& icon);

  const std::string& id() const { return metadata_->id; }
  const std::string& promise_package_id() const {
    return metadata_->promise_package_id;
  }
  const std::string& folder_id() const { return metadata_->folder_id; }
  const syncer::StringOrdinal& position() const { return metadata_->position; }
  const std::string& name() const { return metadata_->name; }
  ash::AppStatus app_status() const { return metadata_->app_status; }
  bool is_folder() const { return metadata_->is_folder; }
  bool is_system_folder() const { return metadata_->is_system_folder; }
  const gfx::ImageSkia& icon() const { return metadata_->icon; }
  const ash::IconColor& icon_color() const { return metadata_->icon_color; }
  bool is_new_install() const { return metadata_->is_new_install; }
  bool is_ephemeral() const { return metadata_->is_ephemeral; }
  float progress() const { return metadata_->progress; }
  bool is_placeholder_icon() const { return metadata_->is_placeholder_icon; }
  const std::string accessible_name() const {
    return metadata_->accessible_name;
  }
  ash::AppCollection collection_id() const { return metadata_->collection_id; }

  void SetMetadata(std::unique_ptr<ash::AppListItemMetadata> metadata);
  std::unique_ptr<ash::AppListItemMetadata> CloneMetadata() const;
  const ash::AppListItemMetadata& metadata() const { return *metadata_; }

  // Loads the app icon and call SetIcon to update ash when finished.
  virtual void LoadIcon();

  // The following methods set Chrome side data here, and call model updater
  // interfaces that talk to ash directly.
  void IncrementIconVersion();
  void SetIcon(const gfx::ImageSkia& icon, bool is_place_holder_icon);
  void SetBadgeIcon(const gfx::ImageSkia& badge_icon);
  void SetAppStatus(ash::AppStatus app_status);
  void SetFolderId(const std::string& folder_id);
  void SetIsSystemFolder(bool is_system_folder);
  void SetIsNewInstall(bool is_new_install);

  // The following methods won't make changes to Ash and it should be called
  // by this item itself or the model updater.
  void SetChromeFolderId(const std::string& folder_id);
  void SetChromeIsFolder(bool is_folder);
  void SetChromeName(const std::string& name);
  void SetChromePosition(const syncer::StringOrdinal& position);
  void SetIsEphemeral(bool is_ephemeral);
  void SetCollectionId(ash::AppCollection collection);

  // Checks whether the item is for a promise app.
  bool IsPromiseApp() const;

  // Call |Activate()| and dismiss launcher if necessary.
  void PerformActivate(int event_flags);

  // Returns the default position if it exists; otherwise returns an empty
  // value.
  syncer::StringOrdinal CalculateDefaultPositionIfApplicable();
  syncer::StringOrdinal CalculateDefaultPositionForModifiedOrder();

  // Activates (opens) the item. Does nothing by default.
  virtual void Activate(int event_flags);

  // Returns a static const char* identifier for the subclass (defaults to "").
  // Pointers can be compared for quick type checking.
  virtual const char* GetItemType() const;

  // Returns the context menu model in |callback| for this item. NULL if there
  // is currently no menu for the item (e.g. during install). Note |callback|
  // takes the ownership of the returned menu model.
  using GetMenuModelCallback =
      base::OnceCallback<void(std::unique_ptr<ui::SimpleMenuModel>)>;
  virtual void GetContextMenuModel(ash::AppListItemContext item_context,
                                   GetMenuModelCallback callback);

  // Returns true iff this item was badged because it's an extension app that
  // has its Android analog installed.
  virtual bool IsBadged() const;

  virtual std::string GetPromisedItemId() const;

  bool CompareForTest(const ChromeAppListItem* other) const;

  std::string ToDebugString() const;

  syncer::StringOrdinal CalculateDefaultPositionForTest();

  AppListModelUpdater* model_updater() { return model_updater_; }

 protected:
  friend class ChromeAppListModelUpdater;

  ChromeAppListItem(Profile* profile, const std::string& app_id);

  Profile* profile() const { return profile_; }

  extensions::AppSorting* GetAppSorting();

  AppListControllerDelegate* GetController();

  void SetAccessibleName(const std::string& label);
  void SetName(const std::string& name);
  void SetPromisePackageId(const std::string& promise_package_id);
  void SetProgress(float progress);
  void SetPosition(const syncer::StringOrdinal& position);

  void set_model_updater(AppListModelUpdater* model_updater) {
    model_updater_ = model_updater;
  }

  // Initializes item position and name from `sync_item`. `sync_item` must be
  // valid.
  void InitFromSync(
      const app_list::AppListSyncableService::SyncItem* sync_item);

  // Get the context menu of a certain app. This could be different for
  // different kinds of items.
  virtual app_list::AppContextMenu* GetAppContextMenu();

  void MaybeDismissAppList();

 private:
  std::unique_ptr<ash::AppListItemMetadata> metadata_;
  raw_ptr<Profile> profile_;
  raw_ptr<AppListModelUpdater, DanglingUntriaged> model_updater_ = nullptr;
};

#endif  // CHROME_BROWSER_ASH_APP_LIST_CHROME_APP_LIST_ITEM_H_
