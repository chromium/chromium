// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SECURITY_EVENTS_SECURITY_EVENT_SYNC_BRIDGE_IMPL_H_
#define CHROME_BROWSER_SECURITY_EVENTS_SECURITY_EVENT_SYNC_BRIDGE_IMPL_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "chrome/browser/security_events/security_event_sync_bridge.h"
#include "components/sync/model/model_type_change_processor.h"
#include "components/sync/model/model_type_store.h"
#include "components/sync/model/model_type_sync_bridge.h"
#include "components/sync/protocol/sync.pb.h"

class SecurityEventSyncBridgeImpl : public SecurityEventSyncBridge,
                                    public syncer::ModelTypeSyncBridge {
 public:
  SecurityEventSyncBridgeImpl(
      syncer::OnceModelTypeStoreFactory store_factory,
      std::unique_ptr<syncer::ModelTypeChangeProcessor> change_processor);
  ~SecurityEventSyncBridgeImpl() override;

  void RecordSecurityEvent(sync_pb::SecurityEventSpecifics specifics) override;

  base::WeakPtr<syncer::ModelTypeControllerDelegate> GetControllerDelegate()
      override;

  // ModelTypeSyncBridge implementation.
  std::unique_ptr<syncer::MetadataChangeList> CreateMetadataChangeList()
      override;
  base::Optional<syncer::ModelError> MergeSyncData(
      std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
      syncer::EntityChangeList entity_data) override;
  base::Optional<syncer::ModelError> ApplySyncChanges(
      std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
      syncer::EntityChangeList entity_changes) override;
  void GetData(StorageKeyList storage_keys, DataCallback callback) override;
  void GetAllDataForDebugging(DataCallback callback) override;
  std::string GetClientTag(const syncer::EntityData& entity_data) override;
  std::string GetStorageKey(const syncer::EntityData& entity_data) override;
  void ApplyStopSyncChanges(std::unique_ptr<syncer::MetadataChangeList>
                                delete_metadata_change_list) override;

 private:
  void OnStoreCreated(const base::Optional<syncer::ModelError>& error,
                      std::unique_ptr<syncer::ModelTypeStore> store);

  void OnReadData(
      DataCallback callback,
      const base::Optional<syncer::ModelError>& error,
      std::unique_ptr<syncer::ModelTypeStore::RecordList> data_records,
      std::unique_ptr<syncer::ModelTypeStore::IdList> missing_id_list);

  void OnReadAllData(
      DataCallback callback,
      const base::Optional<syncer::ModelError>& error,
      std::unique_ptr<syncer::ModelTypeStore::RecordList> data_records);

  void OnReadAllMetadata(const base::Optional<syncer::ModelError>& error,
                         std::unique_ptr<syncer::MetadataBatch> metadata_batch);

  void OnCommit(const base::Optional<syncer::ModelError>& error);

  std::unique_ptr<syncer::ModelTypeStore> store_;

  base::WeakPtrFactory<SecurityEventSyncBridgeImpl> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SecurityEventSyncBridgeImpl);
};

#endif  // CHROME_BROWSER_SECURITY_EVENTS_SECURITY_EVENT_SYNC_BRIDGE_IMPL_H_
