// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/site_data_size_collector.h"

#include <utility>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/task/post_task.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_usage_info.h"
#include "content/public/common/content_constants.h"

namespace {

int64_t GetFileSizeBlocking(const base::FilePath& file_path) {
  int64_t size = 0;
  bool success = base::GetFileSize(file_path, &size);
  return success ? size : -1;
}

}  // namespace

SiteDataSizeCollector::SiteDataSizeCollector(
    const base::FilePath& default_storage_partition_path,
    BrowsingDataCookieHelper* cookie_helper,
    BrowsingDataDatabaseHelper* database_helper,
    BrowsingDataLocalStorageHelper* local_storage_helper,
    BrowsingDataAppCacheHelper* appcache_helper,
    BrowsingDataIndexedDBHelper* indexed_db_helper,
    BrowsingDataFileSystemHelper* file_system_helper,
    BrowsingDataServiceWorkerHelper* service_worker_helper,
    BrowsingDataCacheStorageHelper* cache_storage_helper,
    BrowsingDataFlashLSOHelper* flash_lso_helper)
    : default_storage_partition_path_(default_storage_partition_path),
      appcache_helper_(appcache_helper),
      cookie_helper_(cookie_helper),
      database_helper_(database_helper),
      local_storage_helper_(local_storage_helper),
      indexed_db_helper_(indexed_db_helper),
      file_system_helper_(file_system_helper),
      service_worker_helper_(service_worker_helper),
      cache_storage_helper_(cache_storage_helper),
      flash_lso_helper_(flash_lso_helper),
      in_flight_operations_(0),
      total_bytes_(0) {}

SiteDataSizeCollector::~SiteDataSizeCollector() {
}

void SiteDataSizeCollector::Fetch(FetchCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!callback.is_null());

  fetch_callback_ = std::move(callback);
  total_bytes_ = 0;
  in_flight_operations_ = 0;

  if (appcache_helper_.get()) {
    appcache_helper_->StartFetching(
        base::BindOnce(&SiteDataSizeCollector::OnAppCacheModelInfoLoaded,
                       weak_ptr_factory_.GetWeakPtr()));
    in_flight_operations_++;
  }
  if (cookie_helper_.get()) {
    cookie_helper_->StartFetching(
        base::BindOnce(&SiteDataSizeCollector::OnCookiesModelInfoLoaded,
                       weak_ptr_factory_.GetWeakPtr()));
    in_flight_operations_++;
  }
  if (database_helper_.get()) {
    database_helper_->StartFetching(
        base::BindOnce(&SiteDataSizeCollector::OnDatabaseModelInfoLoaded,
                       weak_ptr_factory_.GetWeakPtr()));
    in_flight_operations_++;
  }
  if (local_storage_helper_.get()) {
    local_storage_helper_->StartFetching(
        base::BindOnce(&SiteDataSizeCollector::OnLocalStorageModelInfoLoaded,
                       weak_ptr_factory_.GetWeakPtr()));
    in_flight_operations_++;
  }
  if (indexed_db_helper_.get()) {
    indexed_db_helper_->StartFetching(
        base::BindOnce(&SiteDataSizeCollector::OnIndexedDBModelInfoLoaded,
                       weak_ptr_factory_.GetWeakPtr()));
    in_flight_operations_++;
  }
  if (file_system_helper_.get()) {
    file_system_helper_->StartFetching(
        base::BindOnce(&SiteDataSizeCollector::OnFileSystemModelInfoLoaded,
                       weak_ptr_factory_.GetWeakPtr()));
    in_flight_operations_++;
  }
  if (service_worker_helper_.get()) {
    service_worker_helper_->StartFetching(
        base::BindOnce(&SiteDataSizeCollector::OnServiceWorkerModelInfoLoaded,
                       weak_ptr_factory_.GetWeakPtr()));
    in_flight_operations_++;
  }
  if (cache_storage_helper_.get()) {
    cache_storage_helper_->StartFetching(
        base::BindOnce(&SiteDataSizeCollector::OnCacheStorageModelInfoLoaded,
                       weak_ptr_factory_.GetWeakPtr()));
    in_flight_operations_++;
  }
  if (flash_lso_helper_.get()) {
    flash_lso_helper_->StartFetching(
        base::BindOnce(&SiteDataSizeCollector::OnFlashLSOInfoLoaded,
                       weak_ptr_factory_.GetWeakPtr()));
    in_flight_operations_++;
  }
  // TODO(fukino): SITE_USAGE_DATA and WEB_APP_DATA should be counted too.
  // All data types included in REMOVE_SITE_USAGE_DATA should be counted.
}

void SiteDataSizeCollector::OnAppCacheModelInfoLoaded(
    const std::list<content::StorageUsageInfo>& info_list) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  int64_t total_size = 0;
  for (const auto& info : info_list)
    total_size += info.total_size_bytes;
  OnStorageSizeFetched(total_size);
}

void SiteDataSizeCollector::OnCookiesModelInfoLoaded(
    const net::CookieList& cookie_list) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (cookie_list.empty()) {
    OnStorageSizeFetched(0);
    return;
  }
  base::FilePath cookie_file_path = default_storage_partition_path_
      .Append(chrome::kCookieFilename);
  base::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::ThreadPool(), base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&GetFileSizeBlocking, cookie_file_path),
      base::BindOnce(&SiteDataSizeCollector::OnStorageSizeFetched,
                     weak_ptr_factory_.GetWeakPtr()));
}

void SiteDataSizeCollector::OnDatabaseModelInfoLoaded(
    const DatabaseInfoList& database_info_list) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  int64_t total_size = 0;
  for (const auto& database_info : database_info_list)
    total_size += database_info.total_size_bytes;
  OnStorageSizeFetched(total_size);
}

void SiteDataSizeCollector::OnLocalStorageModelInfoLoaded(
      const LocalStorageInfoList& local_storage_info_list) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  int64_t total_size = 0;
  for (const auto& local_storage_info : local_storage_info_list)
    total_size += local_storage_info.total_size_bytes;
  OnStorageSizeFetched(total_size);
}

void SiteDataSizeCollector::OnIndexedDBModelInfoLoaded(
    const std::list<content::StorageUsageInfo>& indexed_db_info_list) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  int64_t total_size = 0;
  for (const auto& indexed_db_info : indexed_db_info_list)
    total_size += indexed_db_info.total_size_bytes;
  OnStorageSizeFetched(total_size);
}

void SiteDataSizeCollector::OnFileSystemModelInfoLoaded(
    const FileSystemInfoList& file_system_info_list) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  int64_t total_size = 0;
  for (const auto& file_system_info : file_system_info_list) {
    for (const auto& usage : file_system_info.usage_map)
      total_size += usage.second;
  }
  OnStorageSizeFetched(total_size);
}

void SiteDataSizeCollector::OnServiceWorkerModelInfoLoaded(
    const ServiceWorkerUsageInfoList& service_worker_info_list) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  int64_t total_size = 0;
  for (const auto& service_worker_info : service_worker_info_list)
    total_size += service_worker_info.total_size_bytes;
  OnStorageSizeFetched(total_size);
}

void SiteDataSizeCollector::OnCacheStorageModelInfoLoaded(
      const CacheStorageUsageInfoList& cache_storage_info_list) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  int64_t total_size = 0;
  for (const auto& cache_storage_info : cache_storage_info_list)
    total_size += cache_storage_info.total_size_bytes;
  OnStorageSizeFetched(total_size);
}

void SiteDataSizeCollector::OnFlashLSOInfoLoaded(
    const FlashLSODomainList& domains) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // TODO(fukino): Flash is not the only plugin. We should check all types of
  // plugin data.
  if (domains.empty()) {
    OnStorageSizeFetched(0);
    return;
  }
  base::FilePath pepper_data_dir_path = default_storage_partition_path_
      .Append(content::kPepperDataDirname);
  base::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::ThreadPool(), base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&base::ComputeDirectorySize, pepper_data_dir_path),
      base::BindOnce(&SiteDataSizeCollector::OnStorageSizeFetched,
                     weak_ptr_factory_.GetWeakPtr()));
}

void SiteDataSizeCollector::OnStorageSizeFetched(int64_t size) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (size > 0)
    total_bytes_ += size;
  if (--in_flight_operations_ == 0)
    std::move(fetch_callback_).Run(total_bytes_);
}
