// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_STORAGE_SETTINGS_SYNC_PROCESSOR_H_
#define CHROME_BROWSER_EXTENSIONS_API_STORAGE_SETTINGS_SYNC_PROCESSOR_H_

#include <optional>
#include <set>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "components/sync/base/data_type.h"
#include "components/value_store/value_store_change.h"
#include "extensions/common/extension_id.h"

namespace syncer {
class ModelError;
class SyncChangeProcessor;
}  // namespace syncer

namespace extensions {

// A wrapper for a SyncChangeProcessor that deals specifically with the syncing
// of a single extension's settings. Handles:
//  - translating SettingChanges into calls into the Sync API.
//  - deciding whether to ADD/REMOVE/SET depending on the current state of
//    settings.
//  - rate limiting (inherently per-extension, which is what we want).
class SettingsSyncProcessor {
 public:
  SettingsSyncProcessor(const ExtensionId& extension_id,
                        syncer::DataType type,
                        syncer::SyncChangeProcessor* sync_processor);

  SettingsSyncProcessor(const SettingsSyncProcessor&) = delete;
  SettingsSyncProcessor& operator=(const SettingsSyncProcessor&) = delete;

  ~SettingsSyncProcessor();

  // Initializes this with the initial state of sync.
  void Init(const base::Value::Dict& initial_state);

  // Sends |changes| to sync.
  std::optional<syncer::ModelError> SendChanges(
      const value_store::ValueStoreChangeList& changes);

  // Informs this that |changes| have been receieved from sync. No action will
  // be taken, but this must be notified for internal bookkeeping.
  void NotifyChanges(const value_store::ValueStoreChangeList& changes);

  syncer::DataType type() { return type_; }

 private:
  // ID of the extension the changes are for.
  const ExtensionId extension_id_;

  // Sync data type. Either EXTENSION_SETTING or APP_SETTING.
  const syncer::DataType type_;

  // The sync processor used to send changes to sync.
  const raw_ptr<syncer::SyncChangeProcessor, DanglingUntriaged> sync_processor_;

  // Whether Init() has been called.
  bool initialized_;

  // Keys of the settings that are currently being synced. Used to decide what
  // kind of action (ADD, UPDATE, REMOVE) to send to sync.
  std::set<std::string> synced_keys_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_STORAGE_SETTINGS_SYNC_PROCESSOR_H_
