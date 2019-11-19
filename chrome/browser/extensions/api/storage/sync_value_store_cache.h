// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_STORAGE_SYNC_VALUE_STORE_CACHE_H_
#define CHROME_BROWSER_EXTENSIONS_API_STORAGE_SYNC_VALUE_STORE_CACHE_H_

#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "components/sync/model/syncable_service.h"
#include "extensions/browser/api/storage/settings_observer.h"
#include "extensions/browser/api/storage/value_store_cache.h"

namespace base {
class FilePath;
}

namespace syncer {
class SyncableService;
}

namespace extensions {

class SyncStorageBackend;
class ValueStoreFactory;

// ValueStoreCache for the SYNC namespace. It owns a backend for apps and
// another for extensions. Each backend takes care of persistence and syncing.
class SyncValueStoreCache : public ValueStoreCache {
 public:
  SyncValueStoreCache(scoped_refptr<ValueStoreFactory> factory,
                      scoped_refptr<SettingsObserverList> observers,
                      const base::FilePath& profile_path);
  ~SyncValueStoreCache() override;

  base::WeakPtr<SyncValueStoreCache> AsWeakPtr();
  syncer::SyncableService* GetSyncableService(syncer::ModelType type);

  // ValueStoreCache implementation:
  void RunWithValueStoreForExtension(
      const StorageCallback& callback,
      scoped_refptr<const Extension> extension) override;
  void DeleteStorageSoon(const std::string& extension_id) override;

 private:
  void InitOnBackend(scoped_refptr<ValueStoreFactory> factory,
                     scoped_refptr<SettingsObserverList> observers,
                     const base::FilePath& profile_path);

  bool initialized_;
  std::unique_ptr<SyncStorageBackend> app_backend_;
  std::unique_ptr<SyncStorageBackend> extension_backend_;
  base::WeakPtrFactory<SyncValueStoreCache> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SyncValueStoreCache);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_STORAGE_SYNC_VALUE_STORE_CACHE_H_
