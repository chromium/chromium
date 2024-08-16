// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PRINTING_OAUTH2_PROFILE_AUTH_SERVERS_SYNC_BRIDGE_H_
#define CHROME_BROWSER_ASH_PRINTING_OAUTH2_PROFILE_AUTH_SERVERS_SYNC_BRIDGE_H_

#include <memory>
#include <optional>
#include <set>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/sync/model/data_type_store.h"
#include "components/sync/model/data_type_sync_bridge.h"
#include "components/sync/model/entity_change.h"
#include "components/sync/model/model_error.h"
#include "url/gurl.h"

namespace syncer {
class DataTypeLocalChangeProcessor;
struct EntityData;
class MetadataChangeList;
}  // namespace syncer

namespace ash::printing::oauth2 {

// This class is the bridge responsible for the synchronization of the list of
// trusted Authorization Servers between the user's profile and this client.
class ProfileAuthServersSyncBridge : public syncer::DataTypeSyncBridge {
 public:
  class Observer {
   public:
    // This method is called when the sync bridge is ready to process calls to
    // AddAuthorizationServer(). This method is called only once.
    virtual void OnProfileAuthorizationServersInitialized() = 0;

    // This method is called when new data from the user's profile is loaded
    // (either from the local store or from the sync server). This includes
    // loading of the initial list of servers as well as any changes that occur
    // on other clients. Each change reported by this method is represented by
    // two disjoint sets of URIs:
    //  * `added` - the set of URIs added to the list of trusted servers; and
    //  * `deleted` - the set of URIs removed from the list.
    virtual void OnProfileAuthorizationServersUpdate(
        std::set<GURL> added,
        std::set<GURL> deleted) = 0;

   protected:
    virtual ~Observer() = default;
  };

  // Factory function. |observer| must not be nullptr.
  static std::unique_ptr<ProfileAuthServersSyncBridge> Create(
      Observer* observer,
      syncer::OnceDataTypeStoreFactory store_factory);

  // Factory function for testing. |observer| and |change_processor| must not be
  // nullptr.
  static std::unique_ptr<ProfileAuthServersSyncBridge> CreateForTesting(
      Observer* observer,
      std::unique_ptr<syncer::DataTypeLocalChangeProcessor> change_processor,
      syncer::OnceDataTypeStoreFactory store_factory);

  ProfileAuthServersSyncBridge(const ProfileAuthServersSyncBridge&) = delete;
  ProfileAuthServersSyncBridge& operator=(const ProfileAuthServersSyncBridge&) =
      delete;

  ~ProfileAuthServersSyncBridge() override;

  // Returns true <=> the callback OnProfileAuthorizationServersInitialized()
  // was called. It means that the bridge can accept calls to
  // AddAuthorizationServer().
  bool IsInitialized() const { return initialization_completed_; }

  // This method must be called when a new Authorization Server is added to the
  // list of trusted Authorization Servers on this client. The new record will
  // be saved in the user's profile. This method DOES NOT trigger a call to the
  // Observer's method OnProfileAuthorizationServersUpdate() on this client and
  // the added record WILL NOT be included in the changes reported by any call
  // to OnProfileAuthorizationServersUpdate() on this client. This method MUST
  // NOT be called before the Observer receives the call to
  // OnProfileAuthorizationServersInitialized().
  void AddAuthorizationServer(const GURL& server);

  // Implementation of DataTypeSyncBridge interface. For internal use only.
  std::unique_ptr<syncer::MetadataChangeList> CreateMetadataChangeList()
      override;
  std::optional<syncer::ModelError> MergeFullSyncData(
      std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
      syncer::EntityChangeList entity_data) override;
  std::optional<syncer::ModelError> ApplyIncrementalSyncChanges(
      std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
      syncer::EntityChangeList entity_changes) override;
  std::unique_ptr<syncer::DataBatch> GetDataForCommit(
      StorageKeyList storage_keys) override;
  std::unique_ptr<syncer::DataBatch> GetAllDataForDebugging() override;
  std::string GetClientTag(const syncer::EntityData& entity_data) override;
  std::string GetStorageKey(const syncer::EntityData& entity_data) override;

 private:
  ProfileAuthServersSyncBridge(
      std::unique_ptr<syncer::DataTypeLocalChangeProcessor> change_processor,
      syncer::OnceDataTypeStoreFactory store_factory,
      Observer* observer);

  // Callback for DataTypeStore initialization.
  void OnStoreCreated(const std::optional<syncer::ModelError>& error,
                      std::unique_ptr<syncer::DataTypeStore> store);

  // Callback from the store when all data are loaded.
  void OnReadAllData(
      const std::optional<syncer::ModelError>& error,
      std::unique_ptr<syncer::DataTypeStore::RecordList> record_list);

  // Callback from the store when all metadata are loaded.
  void OnReadAllMetadata(const std::optional<syncer::ModelError>& error,
                         std::unique_ptr<syncer::MetadataBatch> metadata_batch);

  // Callback to handle commit errors.
  void OnCommit(const std::optional<syncer::ModelError>& error);

  // Notifies the observer about changes.
  void NotifyObserver(const std::set<std::string>& added,
                      const std::set<std::string>& deleted);

  // This is set to true when the object is ready to use. Use the method
  // IsInitialized() or the callback from the Observer to check this.
  bool initialization_completed_ = false;

  // The current trusted list of Authorization Servers URIs.
  std::set<std::string> servers_uris_;

  // The local store.
  std::unique_ptr<syncer::DataTypeStore> store_;

  raw_ptr<Observer> const observer_;
  base::WeakPtrFactory<ProfileAuthServersSyncBridge> weak_ptr_factory_{this};
};

}  // namespace ash::printing::oauth2

#endif  // CHROME_BROWSER_ASH_PRINTING_OAUTH2_PROFILE_AUTH_SERVERS_SYNC_BRIDGE_H_
