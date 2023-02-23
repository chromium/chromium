// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/sync_bundle.h"

#include <utility>
#include <vector>

#include "base/location.h"
#include "chrome/browser/extensions/extension_sync_data.h"
#include "chrome/browser/extensions/extension_sync_service.h"
#include "chrome/browser/extensions/extension_util.h"
#include "extensions/common/extension.h"

namespace extensions {

SyncBundle::SyncBundle() {}
SyncBundle::~SyncBundle() {}

void SyncBundle::StartSyncing(
    std::unique_ptr<syncer::SyncChangeProcessor> sync_processor) {
  sync_processor_ = std::move(sync_processor);
}

void SyncBundle::Reset() {
  sync_processor_.reset();
  synced_extensions_.clear();
  pending_sync_data_.clear();
}

bool SyncBundle::IsSyncing() const {
  return sync_processor_ != nullptr;
}

void SyncBundle::PushSyncDataMap(
    const std::map<ExtensionId, syncer::SyncData>& sync_data_per_extension) {
  syncer::SyncChangeList sync_change_list;
  for (const auto& extension_id_and_sync_data : sync_data_per_extension) {
    sync_change_list.push_back(CreateSyncChange(
        extension_id_and_sync_data.first, extension_id_and_sync_data.second));

    AddSyncedExtension(extension_id_and_sync_data.first);
  }

  PushSyncChanges(sync_change_list);
}

void SyncBundle::PushSyncDeletion(const ExtensionId& extension_id,
                                  const syncer::SyncData& sync_data) {
  if (!HasSyncedExtension(extension_id))
    return;

  RemoveSyncedExtension(extension_id);
  PushSyncChanges(syncer::SyncChangeList(1,
      syncer::SyncChange(FROM_HERE,
                         syncer::SyncChange::ACTION_DELETE,
                         sync_data)));
}

void SyncBundle::PushSyncAddOrUpdate(const ExtensionId& extension_id,
                                     const syncer::SyncData& sync_data) {
  PushSyncChanges(syncer::SyncChangeList(
      1, CreateSyncChange(extension_id, sync_data)));
  AddSyncedExtension(extension_id);
  // Now sync and local state agree. If we had a pending install from sync,
  // clear it now.
  pending_sync_data_.erase(extension_id);
}

void SyncBundle::ApplySyncData(const ExtensionSyncData& extension_sync_data) {
  if (extension_sync_data.uninstalled())
    RemoveSyncedExtension(extension_sync_data.id());
  else
    AddSyncedExtension(extension_sync_data.id());
}

bool SyncBundle::HasPendingExtensionData(const ExtensionId& id) const {
  return pending_sync_data_.find(id) != pending_sync_data_.end();
}

void SyncBundle::AddPendingExtensionData(
    const ExtensionSyncData& extension_sync_data) {
  pending_sync_data_.insert(std::make_pair(extension_sync_data.id(),
                                           extension_sync_data));
}

std::vector<ExtensionSyncData> SyncBundle::GetPendingExtensionData() const {
  std::vector<ExtensionSyncData> pending_extensions;
  for (const auto& data : pending_sync_data_)
    pending_extensions.push_back(data.second);

  return pending_extensions;
}

syncer::SyncChange SyncBundle::CreateSyncChange(
    const ExtensionId& extension_id,
    const syncer::SyncData& sync_data) const {
  return syncer::SyncChange(
      FROM_HERE,
      HasSyncedExtension(extension_id) ? syncer::SyncChange::ACTION_UPDATE
                                       : syncer::SyncChange::ACTION_ADD,
      sync_data);
}

void SyncBundle::PushSyncChanges(
    const syncer::SyncChangeList& sync_change_list) {
  sync_processor_->ProcessSyncChanges(FROM_HERE, sync_change_list);
}

void SyncBundle::AddSyncedExtension(const ExtensionId& id) {
  synced_extensions_.insert(id);
}

void SyncBundle::RemoveSyncedExtension(const ExtensionId& id) {
  synced_extensions_.erase(id);
}

bool SyncBundle::HasSyncedExtension(const ExtensionId& id) const {
  return synced_extensions_.find(id) != synced_extensions_.end();
}

}  // namespace extensions
