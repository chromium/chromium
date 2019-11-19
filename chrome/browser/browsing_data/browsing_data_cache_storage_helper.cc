// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/browsing_data_cache_storage_helper.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/location.h"
#include "base/task/post_task.h"
#include "chrome/browser/browsing_data/browsing_data_helper.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/cache_storage_context.h"
#include "content/public/browser/storage_usage_info.h"

using content::BrowserThread;
using content::CacheStorageContext;
using content::StorageUsageInfo;

namespace {

void GetAllOriginsInfoForCacheStorageCallback(
    BrowsingDataCacheStorageHelper::FetchCallback callback,
    const std::vector<StorageUsageInfo>& origins) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!callback.is_null());

  std::list<content::StorageUsageInfo> result;
  for (const StorageUsageInfo& origin : origins) {
    if (!BrowsingDataHelper::HasWebScheme(origin.origin.GetURL()))
      continue;  // Non-websafe state is not considered browsing data.
    result.push_back(origin);
  }

  std::move(callback).Run(result);
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
  cache_storage_context_->GetAllOriginsInfo(base::BindOnce(
      &GetAllOriginsInfoForCacheStorageCallback, std::move(callback)));
}

void BrowsingDataCacheStorageHelper::DeleteCacheStorage(const GURL& origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  cache_storage_context_->DeleteForOrigin(origin);
}

CannedBrowsingDataCacheStorageHelper::CannedBrowsingDataCacheStorageHelper(
    content::CacheStorageContext* context)
    : BrowsingDataCacheStorageHelper(context) {}

CannedBrowsingDataCacheStorageHelper::~CannedBrowsingDataCacheStorageHelper() {}

void CannedBrowsingDataCacheStorageHelper::Add(const url::Origin& origin) {
  if (!BrowsingDataHelper::HasWebScheme(origin.GetURL()))
    return;  // Non-websafe state is not considered browsing data.

  pending_origins_.insert(origin);
}

void CannedBrowsingDataCacheStorageHelper::Reset() {
  pending_origins_.clear();
}

bool CannedBrowsingDataCacheStorageHelper::empty() const {
  return pending_origins_.empty();
}

size_t CannedBrowsingDataCacheStorageHelper::GetCount() const {
  return pending_origins_.size();
}

const std::set<url::Origin>& CannedBrowsingDataCacheStorageHelper::GetOrigins()
    const {
  return pending_origins_;
}

void CannedBrowsingDataCacheStorageHelper::StartFetching(
    FetchCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!callback.is_null());

  std::list<StorageUsageInfo> result;
  for (const auto& origin : pending_origins_)
    result.emplace_back(origin, 0, base::Time());

  base::PostTask(FROM_HERE, {BrowserThread::UI},
                 base::BindOnce(std::move(callback), result));
}

void CannedBrowsingDataCacheStorageHelper::DeleteCacheStorage(
    const GURL& origin) {
  pending_origins_.erase(url::Origin::Create(origin));
  BrowsingDataCacheStorageHelper::DeleteCacheStorage(origin);
}
