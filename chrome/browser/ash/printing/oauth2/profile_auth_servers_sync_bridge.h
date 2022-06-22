// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PRINTING_OAUTH2_PROFILE_AUTH_SERVERS_SYNC_BRIDGE_H_
#define CHROME_BROWSER_ASH_PRINTING_OAUTH2_PROFILE_AUTH_SERVERS_SYNC_BRIDGE_H_

#include <memory>
#include <set>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/sync/model/entity_change.h"
#include "components/sync/model/model_error.h"
#include "components/sync/model/model_type_store.h"
#include "components/sync/model/model_type_sync_bridge.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace chromeos {
class Uri;
}

namespace syncer {
struct EntityData;
class ModelTypeChangeProcessor;
class MetadataChangeList;
}  // namespace syncer

namespace ash::printing::oauth2 {

// This class is the bridge responsible for the synchronization of the list of
// trusted Authorization Servers between the user's profile and local storage.
class ProfileAuthServersSyncBridge : public syncer::ModelTypeSyncBridge {
 public:
  class Observer {
   public:
    // This method is called when the sync bridge is ready to process calls to
    // AddAuthorizationServer(...). This method is called only once and it is
    // always the first method called on the observer by the sync bridge.
    virtual void OnProfileAuthorizationServersInitialized() = 0;

   protected:
    virtual ~Observer() = default;
  };

  // Factory function. |observer| must not be nullptr.
  static std::unique_ptr<ProfileAuthServersSyncBridge> Create(
      Observer* observer,
      syncer::OnceModelTypeStoreFactory store_factory);

  // Factory function for testing. |observer| and |change_processor| must not be
  // nullptr.
  static std::unique_ptr<ProfileAuthServersSyncBridge> CreateForTesting(
      Observer* observer,
      std::unique_ptr<syncer::ModelTypeChangeProcessor> change_processor,
      syncer::OnceModelTypeStoreFactory store_factory);

  ProfileAuthServersSyncBridge(const ProfileAuthServersSyncBridge&) = delete;
  ProfileAuthServersSyncBridge& operator=(const ProfileAuthServersSyncBridge&) =
      delete;

  ~ProfileAuthServersSyncBridge() override;

  // This method must be called when new Authorization Server is added to the
  // list of trusted Authorization Servers.
  void AddAuthorizationServer(const chromeos::Uri& server);

 private:
  ProfileAuthServersSyncBridge(
      std::unique_ptr<syncer::ModelTypeChangeProcessor> change_processor,
      syncer::OnceModelTypeStoreFactory store_factory,
      Observer* observer);

  // Callback for ModelTypeStore initialization.
  void OnStoreCreated(const absl::optional<syncer::ModelError>& error,
                      std::unique_ptr<syncer::ModelTypeStore> store);

  // Callback from the store when all data are loaded.
  void OnReadAllData(
      const absl::optional<syncer::ModelError>& error,
      std::unique_ptr<syncer::ModelTypeStore::RecordList> record_list);

  // Callback from the store when all metadata are loaded.
  void OnReadAllMetadata(const absl::optional<syncer::ModelError>& error,
                         std::unique_ptr<syncer::MetadataBatch> metadata_batch);

  // Implementation of ModelTypeSyncBridge interface.
  std::unique_ptr<syncer::MetadataChangeList> CreateMetadataChangeList()
      override;
  absl::optional<syncer::ModelError> MergeSyncData(
      std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
      syncer::EntityChangeList entity_data) override;
  absl::optional<syncer::ModelError> ApplySyncChanges(
      std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
      syncer::EntityChangeList entity_changes) override;
  void GetData(StorageKeyList storage_keys, DataCallback callback) override;
  void GetAllDataForDebugging(DataCallback callback) override;
  std::string GetClientTag(const syncer::EntityData& entity_data) override;
  std::string GetStorageKey(const syncer::EntityData& entity_data) override;

  // Callback to handle commit errors.
  void OnCommit(const absl::optional<syncer::ModelError>& error);

  // This is set to true when the object is ready to use. Use the callback
  // from the Observer to check this. This field is used only for internal
  // validation.
  bool initialization_completed_ = false;

  // The current trusted list of Authorization Servers URIs.
  std::set<std::string> servers_uris_;

  // The local storage.
  std::unique_ptr<syncer::ModelTypeStore> store_;

  raw_ptr<Observer> const observer_;
  base::WeakPtrFactory<ProfileAuthServersSyncBridge> weak_ptr_factory_{this};
};

}  // namespace ash::printing::oauth2

#endif  // CHROME_BROWSER_ASH_PRINTING_OAUTH2_PROFILE_AUTH_SERVERS_SYNC_BRIDGE_H_
