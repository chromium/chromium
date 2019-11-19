// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_STORAGE_SETTINGS_SYNC_UTIL_H_
#define CHROME_BROWSER_EXTENSIONS_API_STORAGE_SETTINGS_SYNC_UTIL_H_

#include <string>

#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "components/sync/model/sync_change.h"
#include "components/sync/model/sync_data.h"

namespace base {
class Value;
}

namespace content {
class BrowserContext;
}

namespace syncer {
class SyncableService;
}

namespace extensions {

namespace settings_sync_util {

// Creates a syncer::SyncData object for an extension or app setting.
syncer::SyncData CreateData(
    const std::string& extension_id,
    const std::string& key,
    const base::Value& value,
    syncer::ModelType type);

// Creates an "add" sync change for an extension or app setting.
syncer::SyncChange CreateAdd(
    const std::string& extension_id,
    const std::string& key,
    const base::Value& value,
    syncer::ModelType type);

// Creates an "update" sync change for an extension or app setting.
syncer::SyncChange CreateUpdate(
    const std::string& extension_id,
    const std::string& key,
    const base::Value& value,
    syncer::ModelType type);

// Creates a "delete" sync change for an extension or app setting.
syncer::SyncChange CreateDelete(
    const std::string& extension_id,
    const std::string& key,
    syncer::ModelType type);

// Returns a callback that provides a SyncableService. The function must be
// called on the UI thread and |type| must be either APP_SETTINGS or
// EXTENSION_SETTINGS. The returned callback must be called on the backend
// sequence.
base::OnceCallback<base::WeakPtr<syncer::SyncableService>()>
GetSyncableServiceProvider(content::BrowserContext* context,
                           syncer::ModelType type);

}  // namespace settings_sync_util

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_STORAGE_SETTINGS_SYNC_UTIL_H_
