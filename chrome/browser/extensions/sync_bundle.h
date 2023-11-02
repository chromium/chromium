// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_SYNC_BUNDLE_H_
#define CHROME_BROWSER_EXTENSIONS_SYNC_BUNDLE_H_

#include <map>
#include <memory>
#include <set>

#include "components/sync/model/sync_change.h"
#include "components/sync/model/sync_change_processor.h"
#include "components/sync/model/sync_data.h"
#include "extensions/common/extension_id.h"

namespace extensions {

class ExtensionSyncData;

class SyncBundle {
 public:
  SyncBundle();

  SyncBundle(const SyncBundle&) = delete;
  SyncBundle& operator=(const SyncBundle&) = delete;

  ~SyncBundle();

  void StartSyncing(
      std::unique_ptr<syncer::SyncChangeProcessor> sync_processor);

  // Resets this class back to its default values, which will disable all
  // syncing until StartSyncing is called again.
  void Reset();

  // Has this bundle started syncing yet?
  // Returns true if StartSyncing has been called, false otherwise.
  bool IsSyncing() const;

  // Handles the given local SyncDatas, one per extension ID. This updates the
  // set of synced extensions as appropriate, and then pushes the corresponding
  // SyncChanges to the server.
  void PushSyncDataMap(
      const std::map<ExtensionId, syncer::SyncData>& sync_data_per_extension);

  // Updates the set of synced extensions as appropriate, and then pushes a
  // SyncChange to the server.
  void PushSyncDeletion(const ExtensionId& extension_id,
                        const syncer::SyncData& sync_data);

  // Pushes any sync changes to an extension to the server and, if necessary,
  // updates the set of synced extension. This also clears any pending data for
  // the extension.
  void PushSyncAddOrUpdate(const ExtensionId& extension_id,
                           const syncer::SyncData& sync_data);

  // Applies the given sync change coming in from the server. This just updates
  // the list of synced extensions.
  void ApplySyncData(const ExtensionSyncData& extension_sync_data);

  // Checks if there is pending sync data for the extension with the given |id|,
  // i.e. data to be sent to the sync server until the extension is installed
  // locally.
  bool HasPendingExtensionData(const ExtensionId& id) const;

  // Adds pending data for the given extension.
  void AddPendingExtensionData(const ExtensionSyncData& extension_sync_data);

  // Returns a vector of all the pending extension data.
  std::vector<ExtensionSyncData> GetPendingExtensionData() const;

 private:
  // Creates a SyncChange to add or update an extension.
  syncer::SyncChange CreateSyncChange(const ExtensionId& extension_id,
                                      const syncer::SyncData& sync_data) const;

  // Pushes the given list of SyncChanges to the server.
  void PushSyncChanges(const syncer::SyncChangeList& sync_change_list);

  void AddSyncedExtension(const ExtensionId& id);
  void RemoveSyncedExtension(const ExtensionId& id);
  bool HasSyncedExtension(const ExtensionId& id) const;

  std::unique_ptr<syncer::SyncChangeProcessor> sync_processor_;

  // Stores the set of extensions we know about. Used to decide if a sync change
  // should be ACTION_ADD or ACTION_UPDATE.
  std::set<ExtensionId> synced_extensions_;

  // This stores pending installs we got from sync. We'll send this back to the
  // server until we've installed the extension locally, to prevent the sync
  // state from flipping back and forth until all clients are up to date.
  std::map<ExtensionId, ExtensionSyncData> pending_sync_data_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_SYNC_BUNDLE_H_
