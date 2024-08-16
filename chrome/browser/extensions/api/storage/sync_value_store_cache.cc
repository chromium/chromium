// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/storage/sync_value_store_cache.h"

#include <stddef.h>

#include <utility>

#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/extensions/api/storage/sync_storage_backend.h"
#include "chrome/browser/sync/glue/sync_start_util.h"
#include "components/value_store/value_store_factory.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/api/storage/backend_task_runner.h"
#include "extensions/common/api/storage.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_id.h"

using content::BrowserThread;

namespace extensions {

namespace {

// Returns the quota limit for sync storage, taken from the schema in
// extensions/common/api/storage.json.
SettingsStorageQuotaEnforcer::Limits GetSyncQuotaLimits() {
  SettingsStorageQuotaEnforcer::Limits limits = {
      static_cast<size_t>(api::storage::sync::QUOTA_BYTES),
      static_cast<size_t>(api::storage::sync::QUOTA_BYTES_PER_ITEM),
      static_cast<size_t>(api::storage::sync::MAX_ITEMS)};
  return limits;
}

}  // namespace

SyncValueStoreCache::SyncValueStoreCache(
    scoped_refptr<value_store::ValueStoreFactory> factory,
    SettingsChangedCallback observer,
    const base::FilePath& profile_path)
    : initialized_(false) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // This post is safe since the destructor can only be invoked from the
  // same message loop, and any potential post of a deletion task must come
  // after the constructor returns.
  GetBackendTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&SyncValueStoreCache::InitOnBackend,
                     base::Unretained(this), std::move(factory),
                     GetSequenceBoundSettingsChangedCallback(
                         base::SequencedTaskRunner::GetCurrentDefault(),
                         std::move(observer)),
                     profile_path));
}

SyncValueStoreCache::~SyncValueStoreCache() {
  DCHECK(IsOnBackendSequence());
}

base::WeakPtr<SyncValueStoreCache> SyncValueStoreCache::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

syncer::SyncableService* SyncValueStoreCache::GetSyncableService(
    syncer::DataType type) {
  DCHECK(IsOnBackendSequence());
  DCHECK(initialized_);

  switch (type) {
    case syncer::APP_SETTINGS:
      return app_backend_.get();
    case syncer::EXTENSION_SETTINGS:
      return extension_backend_.get();
    default:
      NOTREACHED_IN_MIGRATION();
      return nullptr;
  }
}

void SyncValueStoreCache::RunWithValueStoreForExtension(
    StorageCallback callback,
    scoped_refptr<const Extension> extension) {
  DCHECK(IsOnBackendSequence());
  DCHECK(initialized_);
  SyncStorageBackend* backend =
      extension->is_app() ? app_backend_.get() : extension_backend_.get();
  std::move(callback).Run(backend->GetStorage(extension->id()));
}

void SyncValueStoreCache::DeleteStorageSoon(const ExtensionId& extension_id) {
  DCHECK(IsOnBackendSequence());
  app_backend_->DeleteStorage(extension_id);
  extension_backend_->DeleteStorage(extension_id);
}

void SyncValueStoreCache::InitOnBackend(
    scoped_refptr<value_store::ValueStoreFactory> factory,
    SequenceBoundSettingsChangedCallback observer,
    const base::FilePath& profile_path) {
  DCHECK(IsOnBackendSequence());
  DCHECK(!initialized_);
  app_backend_ = std::make_unique<SyncStorageBackend>(
      factory, GetSyncQuotaLimits(), observer, syncer::APP_SETTINGS,
      sync_start_util::GetFlareForSyncableService(profile_path));
  extension_backend_ = std::make_unique<SyncStorageBackend>(
      std::move(factory), GetSyncQuotaLimits(), std::move(observer),
      syncer::EXTENSION_SETTINGS,
      sync_start_util::GetFlareForSyncableService(profile_path));
  initialized_ = true;
}

}  // namespace extensions
