// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_APP_LIST_SYNCABLE_SERVICE_H_
#define CHROME_BROWSER_ASH_APP_LIST_APP_LIST_SYNCABLE_SERVICE_H_

#include <stddef.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "ash/public/cpp/app_list/app_list_types.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/one_shot_event.h"
#include "base/scoped_observation_traits.h"
#include "build/build_config.h"
#include "chrome/browser/ash/app_list/reorder/app_list_reorder_delegate.h"
#include "chrome/browser/sync/glue/sync_start_util.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/sync/model/string_ordinal.h"
#include "components/sync/model/sync_change.h"
#include "components/sync/model/sync_change_processor.h"
#include "components/sync/model/syncable_service.h"
#include "components/sync/protocol/app_list_specifics.pb.h"

class AppListModelUpdater;
class AppServiceAppModelBuilder;
class AppServicePromiseAppModelBuilder;
class AppServiceShortcutModelBuilder;
class ChromeAppListItem;
class Profile;

namespace extensions {
class ExtensionRegistry;
class ExtensionSystem;
}  // namespace extensions

namespace sync_pb {
class AppListSpecifics;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace app_list {
class AppListReorderDelegate;
class AppListSyncModelSanitizer;

// Keyed Service that owns, stores, and syncs an AppListModel for a profile.
class AppListSyncableService : public syncer::SyncableService,
                               public KeyedService,
                               public reorder::AppListReorderDelegate {
 public:
  struct SyncItem {
    SyncItem(const std::string& id,
             sync_pb::AppListSpecifics::AppListItemType type);
    SyncItem(const SyncItem&) = delete;
    SyncItem& operator=(const SyncItem&) = delete;
    ~SyncItem();
    const std::string item_id;
    sync_pb::AppListSpecifics::AppListItemType item_type;
    std::string item_name;
    std::string parent_id;
    syncer::StringOrdinal item_ordinal;
    syncer::StringOrdinal item_pin_ordinal;
    ash::IconColor item_color;

    // Indicates whether the item represents a system-created folder - i.e. a
    // folder that was not created explicitly by a user.
    // Unlike other properties, this value is not persisted to local state, nor
    // synced. It reflects the associated ChromeAppListItem state.
    bool is_system_folder = false;

    // Whether the `item_ordinal` should be fixed after initial sync data is
    // received during a user session.
    // This value is preserved in local prefs, but not synced. It helps identify
    // app items added to persistent storage to set default shelf pin ordinal,
    // which may happen before a default app gets installed.
    // If initial data is received before the app is first installed,
    // the item ordinal would be initialized without taking special cases like
    // default item ordinals, or sort order into account - see
    // https://crbug.com/1306913.
    bool empty_item_ordinal_fixable = true;

    // Indicates whether the sync item is ephemeral - i.e. an app or a folder
    // that does not persist across sessions. These items have a uniquely
    // generated ID per-session.
    // Sync items that are marked as ephemeral will not persist to local state,
    // nor be synced, in order to avoid growing the App List indefinitely with
    // IDs of obsolete ephemeral items.
    bool is_ephemeral = false;

    // Whether the app was pinned to shelf by the user or not.
    // The eventual consistency (a sufficient amount of time after rollout)
    // we're aspiring to reach here is for this field to be interleaved with the
    // pin ordinal: `item_pin_ordinal.IsValid() <=> is_user_pinned.has_value()`.
    // However, it's okay for this contract to be violated in the meantine.
    //
    //  * missing value indicates that either `item_pin_ordinal` is invalid or
    //    this field is new and hasn't yet been processed by sync.
    //  * `true` means that the app was pinned by the user.
    //    We are using this definition in a relaxed way -- for instance, default
    //    OS apps that are shown in the shelf (like Chrome itself) also have
    //    this set to true.
    //  * `false` means that the app was pinned by PinnedLauncherApps policy.
    //    Note that user pin has priority: if an app was first pinned by the
    //    user and then additionally specified in PinnedLauncherApps, this value
    //    will be set to true.
    absl::optional<bool> is_user_pinned;

    std::string ToString() const;
  };

  class Observer : public base::CheckedObserver {
   public:
    // Notifies that sync model was updated.
    virtual void OnSyncModelUpdated() = 0;

    // Notifies the addition or update from the sync items for testing.
    virtual void OnAddOrUpdateFromSyncItemForTest() {}

   protected:
    ~Observer() override;
  };

  // An app list model updater factory function used by tests.
  using ModelUpdaterFactoryCallback =
      base::RepeatingCallback<std::unique_ptr<AppListModelUpdater>(
          reorder::AppListReorderDelegate*)>;

  // Sets an app list model updater factory function for tests. Its lifetime is
  // bound to the lifetime of the returned unique_ptr<>.
  static std::unique_ptr<base::ScopedClosureRunner>
  SetScopedModelUpdaterFactoryForTest(ModelUpdaterFactoryCallback callback);

  using SyncItemMap = std::map<std::string, std::unique_ptr<SyncItem>>;

  // Populates the model when |profile|'s extension system is ready.
  explicit AppListSyncableService(Profile* profile);
  AppListSyncableService(const AppListSyncableService&) = delete;
  AppListSyncableService& operator=(const AppListSyncableService&) = delete;
  ~AppListSyncableService() override;

  // Registers prefs to support local storage.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Some sync behavior depends on whether or not an app was installed by
  // default (as opposed to e.g. installed via explicit user action). Some
  // tests want the AppListSyncableService to consider an app to be installed
  // by default, without going through the heavyweight process of completely
  // installing an app. These functions facilitate that.
  static bool AppIsDefaultForTest(Profile* profile, const std::string& id);
  static void SetAppIsDefaultForTest(Profile* profile, const std::string& id);

  // Adds |item| to |sync_items_| and |model_|. If a sync item already exists,
  // updates the existing sync item instead.
  void AddItem(std::unique_ptr<ChromeAppListItem> app_item);

  // Removes sync item matching |id|. |is_uninstall| indicates whether the item
  // was removed due to an app uninstall.
  void RemoveItem(const std::string& id, bool is_uninstall);

  // Returns the default position for the OEM folder.
  syncer::StringOrdinal GetDefaultOemFolderPosition() const;

  // Creates a string ordinal that would position an app list item as the last
  // item in the app list.
  syncer::StringOrdinal GetLastPosition() const;

  // Gets a string ordinal that would position an app after the item with the
  // provided `id`.
  syncer::StringOrdinal GetPositionAfterApp(const std::string& id) const;

  // Called when properties of an item may have changed, e.g. default/oem state.
  void UpdateItem(const ChromeAppListItem* app_item);

  // Returns the existing sync item matching |id| or NULL.
  virtual const SyncItem* GetSyncItem(const std::string& id) const;

  // Adds a page break item with the provided ID at the provided position.
  void AddPageBreakItem(const std::string& page_break_id,
                        const syncer::StringOrdinal& position);

  // Transfers app attributes, such as parent folder id, position in App
  // Launcher and pin position on the shelf from one app to another app. Target
  // app defined by |to_app_id| is not required to be present at call time. In
  // which case attributes would be applied once the target app appears on the
  // device. Note, pending attributes are not preserved between the user
  // sessions. This functionality is primarily used for migrating app in case
  // app id is changed but it is required to preserve position in App Launcher
  // and in shelf.
  // Returns true on success and false in case app defined by |from_app_id|
  // does not exist.
  bool TransferItemAttributes(const std::string& from_app_id,
                              const std::string& to_app_id);

  // Sets the name of the folder for OEM apps.
  void SetOemFolderName(const std::string& name);

  // Returns optional pin position for the app specified by |app_id|. If app is
  // not synced or does not have associated pin position then empty ordinal is
  // returned.
  virtual syncer::StringOrdinal GetPinPosition(const std::string& app_id);

  // Sets pin position and how it is pinned for the app specified by |app_id|.
  // |item_pin_ordinal| must be valid.
  // |pinned_by_policy| tells whether this item is pinned to the shelf by the
  // `PinnedLauncherApps` policy.
  virtual void SetPinPosition(const std::string& app_id,
                              const syncer::StringOrdinal& item_pin_ordinal,
                              bool pinned_by_policy);

  // Sets |is_user_pinned| to false for the given item specified by |item_id|.
  // Item must exist, |item_pin_ordinal| must be valid, and |is_user_pinned|
  // must be unset by the time of the call.
  void SetIsPolicyPinned(const std::string& app_id);

  // Removes pin position for the app specified by |app_id|.
  virtual void RemovePinPosition(const std::string& app_id);

  // Gets the app list model updater.
  AppListModelUpdater* GetModelUpdater();

  // Returns true if this service was initialized.
  // Virtual for testing.
  virtual bool IsInitialized() const;

  // Returns true if sync was started.
  bool IsSyncing() const;

  // Registers a `callback` to be run from a posted task on completion of the
  // first sync in the session. The `callback` is notified of whether the first
  // sync in the session was thought to be the first sync ever across all
  // ChromeOS devices and sessions for the associated user. This method is safe
  // to call even after completion of the first sync in the session, in which
  // case the `callback` will be run from a task posted immediately.
  // NOTE: Virtual for testing.
  virtual void OnFirstSync(
      base::OnceCallback<void(bool was_first_sync_ever)> callback);

  const std::string& GetOemFolderNameForTest() const {
    return oem_folder_name_;
  }

  void PopulateSyncItemsForTest(std::vector<std::unique_ptr<SyncItem>>&& items);

  virtual const SyncItemMap& sync_items() const;

  // syncer::SyncableService
  void WaitUntilReadyToSync(base::OnceClosure done) override;
  absl::optional<syncer::ModelError> MergeDataAndStartSyncing(
      syncer::ModelType type,
      const syncer::SyncDataList& initial_sync_data,
      std::unique_ptr<syncer::SyncChangeProcessor> sync_processor) override;
  void StopSyncing(syncer::ModelType type) override;
  syncer::SyncDataList GetAllSyncDataForTesting() const;
  absl::optional<syncer::ModelError> ProcessSyncChanges(
      const base::Location& from_here,
      const syncer::SyncChangeList& change_list) override;

  // KeyedService
  void Shutdown() override;

  // reorder::AppListReorderDelegate:
  void SetAppListPreferredOrder(ash::AppListSortOrder order) override;
  syncer::StringOrdinal CalculateGlobalFrontPosition() const override;
  bool CalculateItemPositionInPermanentSortOrder(
      const ash::AppListItemMetadata& metadata,
      syncer::StringOrdinal* target_position) const override;
  ash::AppListSortOrder GetPermanentSortingOrder() const override;

 private:
  friend class AppListSyncModelSanitizer;
  friend struct base::ScopedObservationTraits<AppListSyncableService,
                                              AppListSyncableService::Observer>;
  class ModelUpdaterObserver;

  // Registers new observers and makes sure that service is started.
  void AddObserverAndStart(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Builds the model once ExtensionService is ready.
  void BuildModel();

  // Returns true if sync has restarted, otherwise runs |flare_|.
  bool SyncStarted();

  // If |app_item| matches an existing sync item, returns it. Otherwise adds
  // |app_item| to |sync_items_| and returns the new item. If |app_item| is
  // invalid returns NULL.
  SyncItem* FindOrAddSyncItem(const ChromeAppListItem* app_item);

  // Creates a sync item for |app_item| and sends an ADD SyncChange event.
  SyncItem* CreateSyncItemFromAppItem(const ChromeAppListItem* app_item);

  // If a sync item for |app_item| already exists, update |app_item| from the
  // sync item, otherwise create a new sync item from |app_item|.
  void AddOrUpdateFromSyncItem(const ChromeAppListItem* app_item);

  // Either uninstalling a default app or remove the REMOVE_DEFAULT sync item.
  // Returns true if the app is removed. Otherwise deletes the existing sync
  // item and returns false.
  bool RemoveDefaultApp(const ChromeAppListItem* item, SyncItem* sync_item);

  // Returns whether the delete-sync-item request was for a default app. If
  // true, the |sync_item| is set to REMOVE_DEFAULT and bounced back to the
  // sync server. The caller should abort deleting the |sync_item|.
  bool InterceptDeleteDefaultApp(SyncItem* sync_item);

  // Deletes a sync item from |sync_items_| and sends a DELETE action.
  void DeleteSyncItem(const std::string& item_id);

  // Updates existing entry in |sync_items_| from |app_item|.
  void UpdateSyncItem(const ChromeAppListItem* app_item);

  // Removes sync item matching |id|.
  void RemoveSyncItem(const std::string& id);

  // Updates folder items that may get created during initial sync.
  void ResolveFolderPositions();

  // Removes any empty SyncItem folders and deletes them from sync. Called
  // after a sync item is removed (which may result in an empty folder).
  void PruneEmptySyncFolders();

  // Creates or updates a SyncItem from |specifics|. Returns true if a new item
  // was created.
  // TODO(crbug.com/1057577): Change return type to void.
  bool ProcessSyncItemSpecifics(const sync_pb::AppListSpecifics& specifics);

  // Handles a newly created sync item (e.g. creates a new AppItem and adds it
  // to the model or uninstalls a deleted default item.
  void ProcessNewSyncItem(SyncItem* sync_item);

  // Handles an existing sync item.
  void ProcessExistingSyncItem(SyncItem* sync_item);

  // Sends ADD or CHANGED for sync item.
  void SendSyncChange(SyncItem* sync_item,
                      syncer::SyncChange::SyncChangeType sync_change_type);

  // Returns an existing sync item corresponding to `item_id` or NULL.
  SyncItem* FindSyncItem(const std::string& item_id);

  // Creates a new sync item for |item_id|.
  SyncItem* CreateSyncItem(
      const std::string& item_id,
      sync_pb::AppListSpecifics::AppListItemType item_type);

  // Deletes a SyncItem matching |specifics|.
  void DeleteSyncItemSpecifics(const sync_pb::AppListSpecifics& specifics);

  // Returns true if an extension matching |id| exists and was installed by
  // an OEM (extension->was_installed_by_oem() is true).
  bool AppIsOem(const std::string& id);

  // Initializes sync items from the local storage while sync service is not
  // enabled.
  void InitFromLocalStorage();

  // Helper that notifies observers that sync model has been updated.
  void NotifyObserversSyncUpdated();

  // Handles model update start/finish.
  void HandleUpdateStarted();
  void HandleUpdateFinished(bool clean_up_after_init_sync);

  // Returns true if extension service is ready.
  bool IsExtensionServiceReady() const;

  // Returns a list of top level sync items sorted by item ordinal.
  std::vector<SyncItem*> GetSortedTopLevelSyncItems() const;

  // Remove leading, trailing and duplicate "page break" items in sorted top
  // level item list.
  void PruneRedundantPageBreakItems();

  // Applies sync changes to the local item.
  void UpdateSyncItemFromSync(const sync_pb::AppListSpecifics& specifics,
                              AppListSyncableService::SyncItem* item);

  // Applies changes from the local item to sync item.
  bool UpdateSyncItemFromAppItem(const ChromeAppListItem* app_item,
                                 AppListSyncableService::SyncItem* sync_item);

  // Initializes `new_item`'s position. This function should be called before
  // adding `new_item` to `model_updater_`.
  void InitNewItemPosition(ChromeAppListItem* new_item);

  // Sets position, folder id and pin position for the app |app_id|. Attributes
  // are taken from the sync item |attributes|. This generates sync update and
  // notifies app models and Chrome shelf controller that are automatically
  // refreshed.
  void ApplyAppAttributes(const std::string& app_id,
                          std::unique_ptr<SyncItem> attributes);

  // Creates a `ChromeAppListItem` and a sync item for OEM folder, if they don't
  // already exist.
  void EnsureOemFolderExists();

  // Creates or updates a GuestOS folder's sync data if the folder is
  // missing.
  void MaybeAddOrUpdateGuestOsFolderSyncData(const std::string& folder_id);

  // Creates a folder if the parent folder is missing before adding `app_item`.
  // Returns true if the folder already existed, or if it got created. Returns
  // false if the method failed to ensure the folder existence.
  bool MaybeCreateFolderBeforeAddingItem(ChromeAppListItem* app_item,
                                         const std::string& folder_id);

  raw_ptr<Profile> profile_;
  raw_ptr<extensions::ExtensionSystem> extension_system_;
  raw_ptr<extensions::ExtensionRegistry> extension_registry_;
  std::unique_ptr<AppListModelUpdater> model_updater_;
  std::unique_ptr<ModelUpdaterObserver> model_updater_observer_;
  std::unique_ptr<AppListSyncModelSanitizer> sync_model_sanitizer_;

  std::unique_ptr<AppServiceAppModelBuilder> app_service_apps_builder_;
  std::unique_ptr<AppServicePromiseAppModelBuilder>
      app_service_promise_apps_builder_;
  std::unique_ptr<AppServiceShortcutModelBuilder>
      app_service_shortcuts_builder_;
  std::unique_ptr<syncer::SyncChangeProcessor> sync_processor_;
  SyncItemMap sync_items_;
  // Map that keeps pending request to transfer attributes from one app to
  // another.
  SyncItemMap pending_transfer_map_;
  syncer::SyncableService::StartSyncFlare flare_;
  bool initial_sync_data_processed_ = false;
  bool first_app_list_sync_ = true;
  // Whether OEM folder position is set to a provisional value - the default OEM
  // folder position depends on whether sync data contains any non-default apps.
  // If an OEM app gets installed before initial app lists sync data is
  // processed, the OEM folder position may be incorrect due to unknown sync
  // data state, and has to be recalculated when initial sync gets processed -
  // this variable is used to detect this state.
  bool oem_folder_using_provisional_default_position_ = false;
  std::string oem_folder_name_;
  base::OnceClosure wait_until_ready_to_sync_cb_;

  // Whether the first sync in the session was thought to be the first sync ever
  // across all ChromeOS devices and sessions for the associated user. Note that
  // this value is absent until completion of the first sync in the session.
  absl::optional<bool> first_sync_was_first_sync_ever_;

  // List of observers.
  base::ObserverList<Observer> observer_list_;
  base::OneShotEvent on_first_sync_;

  base::WeakPtrFactory<AppListSyncableService> weak_ptr_factory_{this};
};

}  // namespace app_list

namespace base {

template <>
struct ScopedObservationTraits<app_list::AppListSyncableService,
                               app_list::AppListSyncableService::Observer> {
  static void AddObserver(
      app_list::AppListSyncableService* source,
      app_list::AppListSyncableService::Observer* observer) {
    source->AddObserverAndStart(observer);
  }
  static void RemoveObserver(
      app_list::AppListSyncableService* source,
      app_list::AppListSyncableService::Observer* observer) {
    source->RemoveObserver(observer);
  }
};

}  // namespace base

#endif  // CHROME_BROWSER_ASH_APP_LIST_APP_LIST_SYNCABLE_SERVICE_H_
