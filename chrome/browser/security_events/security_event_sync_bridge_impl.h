// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SECURITY_EVENTS_SECURITY_EVENT_SYNC_BRIDGE_IMPL_H_
#define CHROME_BROWSER_SECURITY_EVENTS_SECURITY_EVENT_SYNC_BRIDGE_IMPL_H_

#include <memory>
#include <optional>
#include <string>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/security_events/security_event_sync_bridge.h"
#include "components/sync/model/data_type_local_change_processor.h"
#include "components/sync/model/data_type_store_with_in_memory_cache.h"
#include "components/sync/model/data_type_sync_bridge.h"

class SecurityEventSyncBridgeImpl : public SecurityEventSyncBridge,
                                    public syncer::DataTypeSyncBridge {
 public:
  SecurityEventSyncBridgeImpl(
      syncer::OnceDataTypeStoreFactory store_factory,
      std::unique_ptr<syncer::DataTypeLocalChangeProcessor> change_processor);

  SecurityEventSyncBridgeImpl(const SecurityEventSyncBridgeImpl&) = delete;
  SecurityEventSyncBridgeImpl& operator=(const SecurityEventSyncBridgeImpl&) =
      delete;

  ~SecurityEventSyncBridgeImpl() override;

  void RecordSecurityEvent(sync_pb::SecurityEventSpecifics specifics) override;

  base::WeakPtr<syncer::DataTypeControllerDelegate> GetControllerDelegate()
      override;

  // DataTypeSyncBridge implementation.
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
  void ApplyDisableSyncChanges(std::unique_ptr<syncer::MetadataChangeList>
                                   delete_metadata_change_list) override;

 private:
  using StoreWithCache =
      syncer::DataTypeStoreWithInMemoryCache<sync_pb::SecurityEventSpecifics>;

  void OnStoreLoaded(const std::optional<syncer::ModelError>& error,
                     std::unique_ptr<StoreWithCache> store,
                     std::unique_ptr<syncer::MetadataBatch> metadata_batch);

  void OnStoreCommit(const std::optional<syncer::ModelError>& error);

  std::unique_ptr<StoreWithCache> store_;

  base::WeakPtrFactory<SecurityEventSyncBridgeImpl> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_SECURITY_EVENTS_SECURITY_EVENT_SYNC_BRIDGE_IMPL_H_
