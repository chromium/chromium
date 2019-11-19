// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/local_data_container.h"

#include <utility>

#include "base/bind.h"
#include "chrome/browser/browsing_data/browsing_data_flash_lso_helper.h"
#include "chrome/browser/browsing_data/cookies_tree_model.h"
#include "content/public/browser/storage_usage_info.h"
#include "net/cookies/canonical_cookie.h"

///////////////////////////////////////////////////////////////////////////////
// LocalDataContainer, public:

LocalDataContainer::LocalDataContainer(
    scoped_refptr<BrowsingDataCookieHelper> cookie_helper,
    scoped_refptr<BrowsingDataDatabaseHelper> database_helper,
    scoped_refptr<BrowsingDataLocalStorageHelper> local_storage_helper,
    scoped_refptr<BrowsingDataLocalStorageHelper> session_storage_helper,
    scoped_refptr<BrowsingDataAppCacheHelper> appcache_helper,
    scoped_refptr<BrowsingDataIndexedDBHelper> indexed_db_helper,
    scoped_refptr<BrowsingDataFileSystemHelper> file_system_helper,
    scoped_refptr<BrowsingDataQuotaHelper> quota_helper,
    scoped_refptr<BrowsingDataServiceWorkerHelper> service_worker_helper,
    scoped_refptr<BrowsingDataSharedWorkerHelper> shared_worker_helper,
    scoped_refptr<BrowsingDataCacheStorageHelper> cache_storage_helper,
    scoped_refptr<BrowsingDataFlashLSOHelper> flash_lso_helper,
    scoped_refptr<BrowsingDataMediaLicenseHelper> media_license_helper)
    : appcache_helper_(std::move(appcache_helper)),
      cookie_helper_(std::move(cookie_helper)),
      database_helper_(std::move(database_helper)),
      local_storage_helper_(std::move(local_storage_helper)),
      session_storage_helper_(std::move(session_storage_helper)),
      indexed_db_helper_(std::move(indexed_db_helper)),
      file_system_helper_(std::move(file_system_helper)),
      quota_helper_(std::move(quota_helper)),
      service_worker_helper_(std::move(service_worker_helper)),
      shared_worker_helper_(std::move(shared_worker_helper)),
      cache_storage_helper_(std::move(cache_storage_helper)),
      flash_lso_helper_(std::move(flash_lso_helper)),
      media_license_helper_(std::move(media_license_helper)) {}

LocalDataContainer::~LocalDataContainer() {}

void LocalDataContainer::Init(CookiesTreeModel* model) {
  DCHECK(!model_);
  model_ = model;

  batches_started_ = 0;
  if (cookie_helper_.get()) {
    batches_started_++;
    cookie_helper_->StartFetching(
        base::BindOnce(&LocalDataContainer::OnCookiesModelInfoLoaded,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  if (database_helper_.get()) {
    batches_started_++;
    database_helper_->StartFetching(
        base::BindOnce(&LocalDataContainer::OnDatabaseModelInfoLoaded,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  if (local_storage_helper_.get()) {
    batches_started_++;
    local_storage_helper_->StartFetching(
        base::BindOnce(&LocalDataContainer::OnLocalStorageModelInfoLoaded,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  if (session_storage_helper_.get()) {
    batches_started_++;
    session_storage_helper_->StartFetching(
        base::BindOnce(&LocalDataContainer::OnSessionStorageModelInfoLoaded,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  // TODO(michaeln): When all of the UI implementations have been updated, make
  // this a required parameter.
  if (appcache_helper_.get()) {
    batches_started_++;
    appcache_helper_->StartFetching(
        base::BindOnce(&LocalDataContainer::OnAppCacheModelInfoLoaded,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  if (indexed_db_helper_.get()) {
    batches_started_++;
    indexed_db_helper_->StartFetching(
        base::BindOnce(&LocalDataContainer::OnIndexedDBModelInfoLoaded,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  if (file_system_helper_.get()) {
    batches_started_++;
    file_system_helper_->StartFetching(
        base::BindOnce(&LocalDataContainer::OnFileSystemModelInfoLoaded,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  if (quota_helper_.get()) {
    batches_started_++;
    quota_helper_->StartFetching(
        base::BindOnce(&LocalDataContainer::OnQuotaModelInfoLoaded,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  if (service_worker_helper_.get()) {
    batches_started_++;
    service_worker_helper_->StartFetching(
        base::BindOnce(&LocalDataContainer::OnServiceWorkerModelInfoLoaded,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  if (shared_worker_helper_.get()) {
    batches_started_++;
    shared_worker_helper_->StartFetching(
        base::BindOnce(&LocalDataContainer::OnSharedWorkerInfoLoaded,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  if (cache_storage_helper_.get()) {
    batches_started_++;
    cache_storage_helper_->StartFetching(
        base::BindOnce(&LocalDataContainer::OnCacheStorageModelInfoLoaded,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  if (flash_lso_helper_.get()) {
    batches_started_++;
    flash_lso_helper_->StartFetching(
        base::BindOnce(&LocalDataContainer::OnFlashLSOInfoLoaded,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  if (media_license_helper_.get()) {
    batches_started_++;
    media_license_helper_->StartFetching(
        base::BindOnce(&LocalDataContainer::OnMediaLicenseInfoLoaded,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  model_->SetBatchExpectation(batches_started_, true);
}

void LocalDataContainer::OnAppCacheModelInfoLoaded(
    const AppCacheInfoList& info_list) {
  appcache_info_list_ = info_list;
  DCHECK(model_);
  model_->PopulateAppCacheInfo(this);
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

void LocalDataContainer::OnFlashLSOInfoLoaded(
    const FlashLSODomainList& domains) {
  flash_lso_domain_list_ = domains;
  DCHECK(model_);
  model_->PopulateFlashLSOInfo(this);
}

void LocalDataContainer::OnMediaLicenseInfoLoaded(
    const MediaLicenseInfoList& media_license_info) {
  media_license_info_list_ = media_license_info;
  DCHECK(model_);
  model_->PopulateMediaLicenseInfo(this);
}
