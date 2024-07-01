// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FLOATING_SSO_FLOATING_SSO_SYNC_BRIDGE_H_
#define CHROME_BROWSER_ASH_FLOATING_SSO_FLOATING_SSO_SYNC_BRIDGE_H_

#include <memory>
#include <optional>
#include <string>

#include "components/sync/model/model_type_sync_bridge.h"

namespace syncer {
struct EntityData;
class MetadataChangeList;
class ModelError;
class ModelTypeChangeProcessor;
}  // namespace syncer

namespace ash::floating_sso {

class FloatingSsoSyncBridge : public syncer::ModelTypeSyncBridge {
 public:
  explicit FloatingSsoSyncBridge(
      std::unique_ptr<syncer::ModelTypeChangeProcessor> change_processor);
  ~FloatingSsoSyncBridge() override;

  // syncer::ModelTypeSyncBridge:
  std::unique_ptr<syncer::MetadataChangeList> CreateMetadataChangeList()
      override;
  std::optional<syncer::ModelError> MergeFullSyncData(
      std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
      syncer::EntityChangeList entity_changes) override;
  std::optional<syncer::ModelError> ApplyIncrementalSyncChanges(
      std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
      syncer::EntityChangeList entity_changes) override;
  std::string GetStorageKey(const syncer::EntityData& entity_data) override;
  std::string GetClientTag(const syncer::EntityData& entity_data) override;
  std::unique_ptr<syncer::DataBatch> GetDataForCommit(
      StorageKeyList storage_keys) override;
  std::unique_ptr<syncer::DataBatch> GetAllDataForDebugging() override;
};

}  // namespace ash::floating_sso

#endif  // CHROME_BROWSER_ASH_FLOATING_SSO_FLOATING_SSO_SYNC_BRIDGE_H_
