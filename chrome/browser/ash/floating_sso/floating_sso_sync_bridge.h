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
#include "components/sync/model/conflict_resolution.h"
#include "components/sync/model/model_type_store.h"
#include "components/sync/model/model_type_store_base.h"
#include "components/sync/model/model_type_sync_bridge.h"
#include "components/sync/protocol/cookie_specifics.pb.h"

namespace syncer {
struct EntityData;
class MetadataBatch;
class MetadataChangeList;
class ModelError;
class ModelTypeChangeProcessor;
template <typename Entry>
class ModelTypeStoreWithInMemoryCache;
}  // namespace syncer

namespace ash::floating_sso {

class FloatingSsoSyncBridge : public syncer::ModelTypeSyncBridge {
 public:
  using CookieSpecificsEntries =
      std::map<std::string, sync_pb::CookieSpecifics>;

  explicit FloatingSsoSyncBridge(
      std::unique_ptr<syncer::ModelTypeChangeProcessor> change_processor,
      syncer::OnceModelTypeStoreFactory create_store_callback);
  ~FloatingSsoSyncBridge() override;

  // syncer::ModelTypeSyncBridge:
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

  // Assumes that the `store_` is initialized.
  const CookieSpecificsEntries& CookieSpecificsEntriesForTest() const;
  bool IsInitialDataReadFinishedForTest() const;

 private:
  using StoreWithCache =
      syncer::ModelTypeStoreWithInMemoryCache<sync_pb::CookieSpecifics>;

  void OnStoreCreated(const std::optional<syncer::ModelError>& error,
                      std::unique_ptr<StoreWithCache> store,
                      std::unique_ptr<syncer::MetadataBatch> metadata_batch);
  void OnStoreCommit(const std::optional<syncer::ModelError>& error);

  // Whether we finished reading data and metadata from disk on initial bridge
  // creation.
  bool is_initial_data_read_finished_ = false;

  // Reads and writes data from/to disk, maintains an in-memory copy of the
  // data.
  std::unique_ptr<StoreWithCache> store_;

  base::WeakPtrFactory<FloatingSsoSyncBridge> weak_ptr_factory_{this};
};

}  // namespace ash::floating_sso

#endif  // CHROME_BROWSER_ASH_FLOATING_SSO_FLOATING_SSO_SYNC_BRIDGE_H_
