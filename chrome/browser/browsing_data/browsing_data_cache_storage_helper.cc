// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/browsing_data_cache_storage_helper.h"

#include <vector>

#include "base/bind.h"
#include "base/location.h"
#include "base/task/post_task.h"
#include "chrome/browser/browsing_data/browsing_data_helper.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/cache_storage_context.h"

using content::BrowserThread;
using content::CacheStorageContext;
using content::CacheStorageUsageInfo;

namespace {

void GetAllOriginsInfoForCacheStorageCallback(
    BrowsingDataCacheStorageHelper::FetchCallback callback,
    const std::vector<CacheStorageUsageInfo>& origins) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(!callback.is_null());

  std::list<content::CacheStorageUsageInfo> result;
  for (const CacheStorageUsageInfo& origin : origins) {
    if (!BrowsingDataHelper::HasWebScheme(origin.origin))
      continue;  // Non-websafe state is not considered browsing data.
    result.push_back(origin);
  }

  base::PostTaskWithTraits(FROM_HERE, {BrowserThread::UI},
                           base::BindOnce(std::move(callback), result));
}

}  // namespace

BrowsingDataCacheStorageHelper::BrowsingDataCacheStorageHelper(
    CacheStorageContext* cache_storage_context)
    : cache_storage_context_(cache_storage_context) {
  DCHECK(cache_storage_context_);
}

BrowsingDataCacheStorageHelper::~BrowsingDataCacheStorageHelper() {}

void BrowsingDataCacheStorageHelper::StartFetching(FetchCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!callback.is_null());
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(
          &BrowsingDataCacheStorageHelper::FetchCacheStorageUsageInfoOnIOThread,
          this, std::move(callback)));
}

void BrowsingDataCacheStorageHelper::DeleteCacheStorage(const GURL& origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(
          &BrowsingDataCacheStorageHelper::DeleteCacheStorageOnIOThread, this,
          origin));
}

void BrowsingDataCacheStorageHelper::FetchCacheStorageUsageInfoOnIOThread(
    FetchCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(!callback.is_null());
  cache_storage_context_->GetAllOriginsInfo(base::BindOnce(
      &GetAllOriginsInfoForCacheStorageCallback, std::move(callback)));
}

void BrowsingDataCacheStorageHelper::DeleteCacheStorageOnIOThread(
    const GURL& origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  cache_storage_context_->DeleteForOrigin(origin);
}

CannedBrowsingDataCacheStorageHelper::PendingCacheStorageUsageInfo::
    PendingCacheStorageUsageInfo(const GURL& origin,
                                 int64_t total_size_bytes,
                                 const base::Time& last_modified)
    : origin(origin),
      total_size_bytes(total_size_bytes),
      last_modified(last_modified) {}

CannedBrowsingDataCacheStorageHelper::PendingCacheStorageUsageInfo::
    ~PendingCacheStorageUsageInfo() {}

bool CannedBrowsingDataCacheStorageHelper::PendingCacheStorageUsageInfo::
operator<(const PendingCacheStorageUsageInfo& other) const {
  return origin < other.origin;
}

CannedBrowsingDataCacheStorageHelper::CannedBrowsingDataCacheStorageHelper(
    content::CacheStorageContext* context)
    : BrowsingDataCacheStorageHelper(context) {}

CannedBrowsingDataCacheStorageHelper::~CannedBrowsingDataCacheStorageHelper() {}

void CannedBrowsingDataCacheStorageHelper::AddCacheStorage(const GURL& origin) {
  if (!BrowsingDataHelper::HasWebScheme(origin))
    return;  // Non-websafe state is not considered browsing data.

  pending_cache_storage_info_.insert(
      PendingCacheStorageUsageInfo(origin, 0, base::Time()));
}

void CannedBrowsingDataCacheStorageHelper::Reset() {
  pending_cache_storage_info_.clear();
}

bool CannedBrowsingDataCacheStorageHelper::empty() const {
  return pending_cache_storage_info_.empty();
}

size_t CannedBrowsingDataCacheStorageHelper::GetCacheStorageCount() const {
  return pending_cache_storage_info_.size();
}

const std::set<
    CannedBrowsingDataCacheStorageHelper::PendingCacheStorageUsageInfo>&
CannedBrowsingDataCacheStorageHelper::GetCacheStorageUsageInfo() const {
  return pending_cache_storage_info_;
}

void CannedBrowsingDataCacheStorageHelper::StartFetching(
    FetchCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!callback.is_null());

  std::list<CacheStorageUsageInfo> result;
  for (const PendingCacheStorageUsageInfo& pending_info :
       pending_cache_storage_info_) {
    CacheStorageUsageInfo info(pending_info.origin,
                               pending_info.total_size_bytes,
                               pending_info.last_modified);
    result.push_back(info);
  }

  base::PostTaskWithTraits(FROM_HERE, {BrowserThread::UI},
                           base::BindOnce(std::move(callback), result));
}

void CannedBrowsingDataCacheStorageHelper::DeleteCacheStorage(
    const GURL& origin) {
  for (auto it = pending_cache_storage_info_.begin();
       it != pending_cache_storage_info_.end();) {
    if (it->origin == origin)
      pending_cache_storage_info_.erase(it++);
    else
      ++it;
  }
  BrowsingDataCacheStorageHelper::DeleteCacheStorage(origin);
}
