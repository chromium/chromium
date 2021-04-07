// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_STORAGE_SETTINGS_SYNC_PROCESSOR_H_
#define CHROME_BROWSER_EXTENSIONS_API_STORAGE_SETTINGS_SYNC_PROCESSOR_H_

#include <set>
#include <string>

#include "base/macros.h"
#include "components/sync/base/model_type.h"
#include "extensions/browser/value_store/value_store_change.h"

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
  SettingsSyncProcessor(const std::string& extension_id,
                        syncer::ModelType type,
                        syncer::SyncChangeProcessor* sync_processor);
  ~SettingsSyncProcessor();

  // Initializes this with the initial state of sync.
  void Init(const base::DictionaryValue& initial_state);

  // Sends |changes| to sync.
  base::Optional<syncer::ModelError> SendChanges(
      const ValueStoreChangeList& changes);

  // Informs this that |changes| have been receieved from sync. No action will
  // be taken, but this must be notified for internal bookkeeping.
  void NotifyChanges(const ValueStoreChangeList& changes);

  syncer::ModelType type() { return type_; }

 private:
  // ID of the extension the changes are for.
  const std::string extension_id_;

  // Sync model type. Either EXTENSION_SETTING or APP_SETTING.
  const syncer::ModelType type_;

  // The sync processor used to send changes to sync.
  syncer::SyncChangeProcessor* const sync_processor_;

  // Whether Init() has been called.
  bool initialized_;

  // Keys of the settings that are currently being synced. Used to decide what
  // kind of action (ADD, UPDATE, REMOVE) to send to sync.
  std::set<std::string> synced_keys_;

  DISALLOW_COPY_AND_ASSIGN(SettingsSyncProcessor);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_STORAGE_SETTINGS_SYNC_PROCESSOR_H_
