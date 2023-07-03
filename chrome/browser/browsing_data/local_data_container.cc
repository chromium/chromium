// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/local_data_container.h"

#include <utility>

#include "base/functional/bind.h"
#include "chrome/browser/browsing_data/browsing_data_file_system_util.h"
#include "chrome/browser/browsing_data/cookies_tree_model.h"
#include "components/browsing_data/core/features.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/storage_usage_info.h"
#include "net/cookies/canonical_cookie.h"

///////////////////////////////////////////////////////////////////////////////
// LocalDataContainer, public:

// static
std::unique_ptr<LocalDataContainer>
LocalDataContainer::CreateFromLocalSharedObjectsContainer(
    const browsing_data::LocalSharedObjectsContainer& shared_objects) {
  return std::make_unique<LocalDataContainer>(
      shared_objects.cookies(), shared_objects.databases(),
      shared_objects.local_storages(), shared_objects.session_storages(),
      shared_objects.indexed_dbs(), shared_objects.file_systems(),
      /*quota_helper=*/nullptr, shared_objects.service_workers(),
      shared_objects.shared_workers(), shared_objects.cache_storages());
}

// static
std::unique_ptr<LocalDataContainer>
LocalDataContainer::CreateFromStoragePartition(
    content::StoragePartition* storage_partition,
    browsing_data::CookieHelper::IsDeletionDisabledCallback
        is_cookie_deletion_disabled_callback) {
  // Migrating storage handling to the `BrowsingDataModel` excludes all related
  // helpers that are handled by the model from the `LocalDataContainer` .
  // This works independently whether partitioned storage is enabled or not.
  if (base::FeatureList::IsEnabled(
          browsing_data::features::kMigrateStorageToBDM)) {
    return std::make_unique<LocalDataContainer>(
        base::MakeRefCounted<browsing_data::CookieHelper>(
            storage_partition, is_cookie_deletion_disabled_callback),
        /*database_helper=*/nullptr,
        /*local_storage_helper=*/nullptr,
        /*session_storage_helper=*/nullptr,
        /*indexed_db_helper=*/nullptr,
        /*file_system_helper=*/nullptr,
        /*quota_helper=*/nullptr,
        /*service_worker_helper=*/nullptr,
        base::MakeRefCounted<browsing_data::SharedWorkerHelper>(
            storage_partition),
        /*cache_storage_helper=*/nullptr);
  }

  // If partitioned storage is enabled, the quota node is used to represent all
  // types of quota managed storage. If not, the quota node type is excluded as
  // it is represented by other types.
  if (blink::StorageKey::IsThirdPartyStoragePartitioningEnabled()) {
    return std::make_unique<LocalDataContainer>(
        base::MakeRefCounted<browsing_data::CookieHelper>(
            storage_partition, is_cookie_deletion_disabled_callback),
        /*database_helper=*/nullptr,
        base::MakeRefCounted<browsing_data::LocalStorageHelper>(
            storage_partition),
        /*session_storage_helper=*/nullptr,
        /*indexed_db_helper=*/nullptr,
        /*file_system_helper=*/nullptr,
        /*quota_helper=*/BrowsingDataQuotaHelper::Create(storage_partition),
        /*service_worker_helper=*/nullptr,
        base::MakeRefCounted<browsing_data::SharedWorkerHelper>(
            storage_partition),
        /*cache_storage_helper=*/nullptr);
  }

  return std::make_unique<LocalDataContainer>(
      base::MakeRefCounted<browsing_data::CookieHelper>(
          storage_partition, is_cookie_deletion_disabled_callback),
      base::MakeRefCounted<browsing_data::DatabaseHelper>(storage_partition),
      base::MakeRefCounted<browsing_data::LocalStorageHelper>(
          storage_partition),
      /*session_storage_helper=*/nullptr,
      base::MakeRefCounted<browsing_data::IndexedDBHelper>(storage_partition),
      base::MakeRefCounted<browsing_data::FileSystemHelper>(
          storage_partition->GetFileSystemContext(),
          browsing_data_file_system_util::GetAdditionalFileSystemTypes()),
      /*quota_helper=*/nullptr,
      base::MakeRefCounted<browsing_data::ServiceWorkerHelper>(
          storage_partition->GetServiceWorkerContext()),
      base::MakeRefCounted<browsing_data::SharedWorkerHelper>(
          storage_partition),
      base::MakeRefCounted<browsing_data::CacheStorageHelper>(
          storage_partition));
}

LocalDataContainer::LocalDataContainer(
    scoped_refptr<browsing_data::CookieHelper> cookie_helper,
    scoped_refptr<browsing_data::DatabaseHelper> database_helper,
    scoped_refptr<browsing_data::LocalStorageHelper> local_storage_helper,
    scoped_refptr<browsing_data::LocalStorageHelper> session_storage_helper,
    scoped_refptr<browsing_data::IndexedDBHelper> indexed_db_helper,
    scoped_refptr<browsing_data::FileSystemHelper> file_system_helper,
    scoped_refptr<BrowsingDataQuotaHelper> quota_helper,
    scoped_refptr<browsing_data::ServiceWorkerHelper> service_worker_helper,
    scoped_refptr<browsing_data::SharedWorkerHelper> shared_worker_helper,
    scoped_refptr<browsing_data::CacheStorageHelper> cache_storage_helper)
    : cookie_helper_(std::move(cookie_helper)),
      database_helper_(std::move(database_helper)),
      local_storage_helper_(std::move(local_storage_helper)),
      session_storage_helper_(std::move(session_storage_helper)),
      indexed_db_helper_(std::move(indexed_db_helper)),
      file_system_helper_(std::move(file_system_helper)),
      quota_helper_(std::move(quota_helper)),
      service_worker_helper_(std::move(service_worker_helper)),
      shared_worker_helper_(std::move(shared_worker_helper)),
      cache_storage_helper_(std::move(cache_storage_helper)) {}

LocalDataContainer::~LocalDataContainer() {}

void LocalDataContainer::Init(CookiesTreeModel* model) {
  DCHECK(!model_);
  model_ = model;

  int batches_started = 0;
  if (cookie_helper_.get()) {
    batches_started++;
    cookie_helper_->StartFetching(
        base::BindOnce(&LocalDataContainer::OnCookiesModelInfoLoaded,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  if (database_helper_.get()) {
    batches_started++;
    database_helper_->StartFetching(
        base::BindOnce(&LocalDataContainer::OnDatabaseModelInfoLoaded,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  if (local_storage_helper_.get()) {
    batches_started++;
    local_storage_helper_->StartFetching(
        base::BindOnce(&LocalDataContainer::OnLocalStorageModelInfoLoaded,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  if (session_storage_helper_.get()) {
    batches_started++;
    session_storage_helper_->StartFetching(
        base::BindOnce(&LocalDataContainer::OnSessionStorageModelInfoLoaded,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  if (indexed_db_helper_.get()) {
    batches_started++;
    indexed_db_helper_->StartFetching(
        base::BindOnce(&LocalDataContainer::OnIndexedDBModelInfoLoaded,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  if (file_system_helper_.get()) {
    batches_started++;
    file_system_helper_->StartFetching(
        base::BindOnce(&LocalDataContainer::OnFileSystemModelInfoLoaded,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  if (quota_helper_.get()) {
    batches_started++;
    quota_helper_->StartFetching(
        base::BindOnce(&LocalDataContainer::OnQuotaModelInfoLoaded,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  if (service_worker_helper_.get()) {
    batches_started++;
    service_worker_helper_->StartFetching(
        base::BindOnce(&LocalDataContainer::OnServiceWorkerModelInfoLoaded,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  if (shared_worker_helper_.get()) {
    batches_started++;
    shared_worker_helper_->StartFetching(
        base::BindOnce(&LocalDataContainer::OnSharedWorkerInfoLoaded,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  if (cache_storage_helper_.get()) {
    batches_started++;
    cache_storage_helper_->StartFetching(
        base::BindOnce(&LocalDataContainer::OnCacheStorageModelInfoLoaded,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  // Don't reset batches, as some completions may have been reported
  // synchronously. As this function is called during model construction, there
  // can't have been any batches started outside this function.
  model_->SetBatchExpectation(batches_started, /*reset=*/false);
}

void LocalDataContainer::OnCookiesModelInfoLoaded(
    const net::CookieList& cookie_list) {
  cookie_list_.insert(cookie_list_.begin(),
                      cookie_list.begin(),
                      cookie_list.end());
  DCHECK(model_);
  model_->PopulateCookieInfo(this);
}

void LocalDataContainer::OnDatabaseModelInfoLoaded(
    const DatabaseInfoList& database_info) {
  database_info_list_ = database_info;
  DCHECK(model_);
  model_->PopulateDatabaseInfo(this);
}

void LocalDataContainer::OnLocalStorageModelInfoLoaded(
    const LocalStorageInfoList& local_storage_info) {
  local_storage_info_list_ = local_storage_info;
  DCHECK(model_);
  model_->PopulateLocalStorageInfo(this);
}

void LocalDataContainer::OnSessionStorageModelInfoLoaded(
    const LocalStorageInfoList& session_storage_info) {
  session_storage_info_list_ = session_storage_info;
  DCHECK(model_);
  model_->PopulateSessionStorageInfo(this);
}

void LocalDataContainer::OnIndexedDBModelInfoLoaded(
    const IndexedDBInfoList& indexed_db_info) {
  indexed_db_info_list_ = indexed_db_info;
  DCHECK(model_);
  model_->PopulateIndexedDBInfo(this);
}

void LocalDataContainer::OnFileSystemModelInfoLoaded(
    const FileSystemInfoList& file_system_info) {
  file_system_info_list_ = file_system_info;
  DCHECK(model_);
  model_->PopulateFileSystemInfo(this);
}

void LocalDataContainer::OnQuotaModelInfoLoaded(
    const QuotaInfoList& quota_info) {
  quota_info_list_ = quota_info;
  DCHECK(model_);
  model_->PopulateQuotaInfo(this);
}

void LocalDataContainer::OnServiceWorkerModelInfoLoaded(
    const ServiceWorkerUsageInfoList& service_worker_info) {
  service_worker_info_list_ = service_worker_info;
  DCHECK(model_);
  model_->PopulateServiceWorkerUsageInfo(this);
}

void LocalDataContainer::OnSharedWorkerInfoLoaded(
    const SharedWorkerInfoList& shared_worker_info) {
  shared_worker_info_list_ = shared_worker_info;
  DCHECK(model_);
  model_->PopulateSharedWorkerInfo(this);
}

void LocalDataContainer::OnCacheStorageModelInfoLoaded(
    const CacheStorageUsageInfoList& cache_storage_info) {
  cache_storage_info_list_ = cache_storage_info;
  DCHECK(model_);
  model_->PopulateCacheStorageUsageInfo(this);
}
