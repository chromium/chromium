// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_STORAGE_SYNC_VALUE_STORE_CACHE_H_
#define CHROME_BROWSER_EXTENSIONS_API_STORAGE_SYNC_VALUE_STORE_CACHE_H_

#include <memory>
#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "components/sync/model/syncable_service.h"
#include "extensions/browser/api/storage/settings_observer.h"
#include "extensions/browser/api/storage/value_store_cache.h"
#include "extensions/common/extension_id.h"

namespace base {
class FilePath;
}

namespace syncer {
class SyncableService;
}

namespace value_store {
class ValueStoreFactory;
}

namespace extensions {

class SyncStorageBackend;

// ValueStoreCache for the SYNC namespace. It owns a backend for apps and
// another for extensions. Each backend takes care of persistence and syncing.
class SyncValueStoreCache : public ValueStoreCache {
 public:
  SyncValueStoreCache(scoped_refptr<value_store::ValueStoreFactory> factory,
                      SettingsChangedCallback observer,
                      const base::FilePath& profile_path);

  SyncValueStoreCache(const SyncValueStoreCache&) = delete;
  SyncValueStoreCache& operator=(const SyncValueStoreCache&) = delete;

  ~SyncValueStoreCache() override;

  base::WeakPtr<SyncValueStoreCache> AsWeakPtr();
  syncer::SyncableService* GetSyncableService(syncer::DataType type);

  // ValueStoreCache implementation:
  void RunWithValueStoreForExtension(
      StorageCallback callback,
      scoped_refptr<const Extension> extension) override;
  void DeleteStorageSoon(const ExtensionId& extension_id) override;

 private:
  void InitOnBackend(scoped_refptr<value_store::ValueStoreFactory> factory,
                     SequenceBoundSettingsChangedCallback observer,
                     const base::FilePath& profile_path);

  bool initialized_;
  std::unique_ptr<SyncStorageBackend> app_backend_;
  std::unique_ptr<SyncStorageBackend> extension_backend_;
  base::WeakPtrFactory<SyncValueStoreCache> weak_ptr_factory_{this};
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_STORAGE_SYNC_VALUE_STORE_CACHE_H_
