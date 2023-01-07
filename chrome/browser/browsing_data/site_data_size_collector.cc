// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/site_data_size_collector.h"

#include <utility>

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/task/thread_pool.h"
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
    scoped_refptr<browsing_data::CookieHelper> cookie_helper,
    scoped_refptr<browsing_data::LocalStorageHelper> local_storage_helper,
    scoped_refptr<BrowsingDataQuotaHelper> quota_helper)
    : default_storage_partition_path_(default_storage_partition_path),
      cookie_helper_(std::move(cookie_helper)),
      local_storage_helper_(std::move(local_storage_helper)),
      quota_helper_(std::move(quota_helper)),
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

  if (cookie_helper_.get()) {
    cookie_helper_->StartFetching(
        base::BindOnce(&SiteDataSizeCollector::OnCookiesModelInfoLoaded,
                       weak_ptr_factory_.GetWeakPtr()));
    in_flight_operations_++;
  }
  if (local_storage_helper_.get()) {
    local_storage_helper_->StartFetching(
        base::BindOnce(&SiteDataSizeCollector::OnLocalStorageModelInfoLoaded,
                       weak_ptr_factory_.GetWeakPtr()));
    in_flight_operations_++;
  }
  if (quota_helper_.get()) {
    quota_helper_->StartFetching(
        base::BindOnce(&SiteDataSizeCollector::OnQuotaModelInfoLoaded,
                       weak_ptr_factory_.GetWeakPtr()));
    in_flight_operations_++;
  }
  // TODO(fukino): SITE_USAGE_DATA and WEB_APP_DATA should be counted too.
  // All data types included in REMOVE_SITE_USAGE_DATA should be counted.
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
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&GetFileSizeBlocking, cookie_file_path),
      base::BindOnce(&SiteDataSizeCollector::OnStorageSizeFetched,
                     weak_ptr_factory_.GetWeakPtr()));
}

void SiteDataSizeCollector::OnLocalStorageModelInfoLoaded(
      const LocalStorageInfoList& local_storage_info_list) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  int64_t total_size = 0;
  for (const auto& local_storage_info : local_storage_info_list)
    total_size += local_storage_info.total_size_bytes;
  OnStorageSizeFetched(total_size);
}

void SiteDataSizeCollector::OnQuotaModelInfoLoaded(
    const QuotaStorageUsageInfoList& quota_storage_info_list) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  int64_t total_size = 0;
  for (const auto& quota_info : quota_storage_info_list)
    total_size += quota_info.temporary_usage + quota_info.syncable_usage;
  OnStorageSizeFetched(total_size);
}

void SiteDataSizeCollector::OnStorageSizeFetched(int64_t size) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (size > 0)
    total_bytes_ += size;
  if (--in_flight_operations_ == 0)
    std::move(fetch_callback_).Run(total_bytes_);
}
