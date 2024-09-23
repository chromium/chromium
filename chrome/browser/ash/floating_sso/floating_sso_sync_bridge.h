// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FLOATING_SSO_FLOATING_SSO_SYNC_BRIDGE_H_
#define CHROME_BROWSER_ASH_FLOATING_SSO_FLOATING_SSO_SYNC_BRIDGE_H_

#include <map>
#include <memory>
#include <optional>
#include <string>

#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "components/sync/model/conflict_resolution.h"
#include "components/sync/model/data_type_store.h"
#include "components/sync/model/data_type_store_base.h"
#include "components/sync/model/data_type_store_with_in_memory_cache.h"
#include "components/sync/model/data_type_sync_bridge.h"
#include "components/sync/protocol/cookie_specifics.pb.h"

namespace syncer {
class DataTypeLocalChangeProcessor;
struct EntityData;
class MetadataBatch;
class MetadataChangeList;
class ModelError;
}  // namespace syncer

namespace net {
class CanonicalCookie;
}  // namespace net

namespace ash::floating_sso {

class FloatingSsoSyncBridge : public syncer::DataTypeSyncBridge {
 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnCookiesAddedOrUpdatedRemotely(
        const std::vector<net::CanonicalCookie>& cookies) = 0;
    virtual void OnCookiesRemovedRemotely(
        const std::vector<net::CanonicalCookie>& cookies) = 0;
  };

  using CookieSpecificsEntries =
      std::map<std::string, sync_pb::CookieSpecifics>;

  explicit FloatingSsoSyncBridge(
      std::unique_ptr<syncer::DataTypeLocalChangeProcessor> change_processor,
      syncer::OnceDataTypeStoreFactory create_store_callback);
  ~FloatingSsoSyncBridge() override;

  // syncer::DataTypeSyncBridge:
  std::unique_ptr<syncer::MetadataChangeList> CreateMetadataChangeList()
      override;
  std::optional<syncer::ModelError> MergeFullSyncData(
      std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
      syncer::EntityChangeList remote_entities) override;
  std::optional<syncer::ModelError> ApplyIncrementalSyncChanges(
      std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
      syncer::EntityChangeList entity_changes) override;
  std::string GetStorageKey(const syncer::EntityData& entity_data) override;
  std::string GetClientTag(const syncer::EntityData& entity_data) override;
  std::unique_ptr<syncer::DataBatch> GetDataForCommit(
      StorageKeyList storage_keys) override;
  std::unique_ptr<syncer::DataBatch> GetAllDataForDebugging() override;
  syncer::ConflictResolution ResolveConflict(
      const std::string& storage_key,
      const syncer::EntityData& remote_data) const override;

  // Methods to notify about local changes. Made virtual for mocking in tests.
  virtual void AddOrUpdateCookie(const sync_pb::CookieSpecifics& specifics);
  virtual void DeleteCookie(const std::string& storage_key);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Assumes that the `store_` is initialized.
  const CookieSpecificsEntries& CookieSpecificsInStore() const;
  bool IsInitialDataReadFinishedForTest() const;
  void SetOnStoreCommitCallbackForTest(base::RepeatingClosure callback);

 private:
  using StoreWithCache =
      syncer::DataTypeStoreWithInMemoryCache<sync_pb::CookieSpecifics>;

  void OnStoreCreated(const std::optional<syncer::ModelError>& error,
                      std::unique_ptr<StoreWithCache> store,
                      std::unique_ptr<syncer::MetadataBatch> metadata_batch);
  void ProcessQueuedCookies();
  void OnStoreCommit(const std::optional<syncer::ModelError>& error);
  void CommitToStore(std::unique_ptr<StoreWithCache::WriteBatch> batch);
  bool IsCookieInStore(const std::string& storage_key) const;

  // Whether we finished reading data and metadata from disk on initial bridge
  // creation.
  bool is_initial_data_read_finished_ = false;

  base::RepeatingClosure on_store_commit_callback_for_test_;

  // Reads and writes data from/to disk, maintains an in-memory copy of the
  // data.
  std::unique_ptr<StoreWithCache> store_;

  // Used to store cookies to be added/deleted while the store or change
  // processor are not ready.
  std::map<std::string, sync_pb::CookieSpecifics> deferred_cookie_additions_;
  std::set<std::string> deferred_cookie_deletions_;

  // Observers which are notified about all incoming remote changes.
  base::ObserverList<Observer> observers_;

  base::WeakPtrFactory<FloatingSsoSyncBridge> weak_ptr_factory_{this};
};

}  // namespace ash::floating_sso

#endif  // CHROME_BROWSER_ASH_FLOATING_SSO_FLOATING_SSO_SYNC_BRIDGE_H_
