// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/browsing_data_appcache_helper.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/task/post_task.h"
#include "base/time/time.h"
#include "chrome/browser/browsing_data/browsing_data_helper.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/storage_usage_info.h"
#include "net/base/completion_once_callback.h"
#include "third_party/blink/public/mojom/appcache/appcache_info.mojom.h"

using content::AppCacheService;
using content::BrowserThread;

namespace {

void OnAppCacheInfoFetchComplete(
    BrowsingDataAppCacheHelper::FetchCallback callback,
    scoped_refptr<content::AppCacheInfoCollection> info_collection,
    int /*rv*/) {
  DCHECK(!callback.is_null());

  std::list<content::StorageUsageInfo> result;
  for (const auto& origin_info : info_collection->infos_by_origin) {
    const url::Origin& origin = origin_info.first;
    // Filter out appcache info entries for non-websafe schemes. Extension state
    // and DevTools, for example, are not considered browsing data.
    if (!BrowsingDataHelper::IsWebScheme(origin.scheme()))
      continue;
    DCHECK(!origin_info.second.empty());

    base::Time last_modified;
    int64_t total_size = 0;
    for (const auto& info : origin_info.second) {
      last_modified = std::max(last_modified, info.last_update_time);
      // The sizes only cover the on-disk response sizes. They do not include
      // the padding sizes added by the Quota system to cross-origin resources.
      //
      // We count the actual disk usage because this number is only reported in
      // UI (not in any API accessible to the site). This decision may need to
      // be revisited if users are confused by the Quota system's decisions.
      total_size += info.response_sizes;
    }
    result.emplace_back(origin, total_size, last_modified);
  }

  std::move(callback).Run(std::move(result));
}

}  // namespace

BrowsingDataAppCacheHelper::BrowsingDataAppCacheHelper(
    AppCacheService* appcache_service)
    : appcache_service_(appcache_service) {}

void BrowsingDataAppCacheHelper::StartFetching(FetchCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!callback.is_null());

  scoped_refptr<content::AppCacheInfoCollection> info_collection =
      new content::AppCacheInfoCollection();

  appcache_service_->GetAllAppCacheInfo(
      info_collection.get(),
      base::BindOnce(&OnAppCacheInfoFetchComplete, std::move(callback),
                     info_collection));
}

void BrowsingDataAppCacheHelper::DeleteAppCaches(const url::Origin& origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  appcache_service_->DeleteAppCachesForOrigin(origin,
                                              net::CompletionOnceCallback());
}

BrowsingDataAppCacheHelper::~BrowsingDataAppCacheHelper() {}

CannedBrowsingDataAppCacheHelper::CannedBrowsingDataAppCacheHelper(
    AppCacheService* appcache_service)
    : BrowsingDataAppCacheHelper(appcache_service) {}

void CannedBrowsingDataAppCacheHelper::Add(const url::Origin& origin) {
  if (!BrowsingDataHelper::HasWebScheme(origin.GetURL()))
    return;  // Ignore non-websafe schemes.

  pending_origins_.insert(origin);
}

void CannedBrowsingDataAppCacheHelper::Reset() {
  pending_origins_.clear();
}

bool CannedBrowsingDataAppCacheHelper::empty() const {
  return pending_origins_.empty();
}

size_t CannedBrowsingDataAppCacheHelper::GetCount() const {
  return pending_origins_.size();
}

const std::set<url::Origin>& CannedBrowsingDataAppCacheHelper::GetOrigins()
    const {
  return pending_origins_;
}

void CannedBrowsingDataAppCacheHelper::StartFetching(FetchCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!callback.is_null());

  std::list<content::StorageUsageInfo> result;
  for (const auto& origin : pending_origins_)
    result.emplace_back(origin, 0, base::Time());

  base::PostTask(FROM_HERE, {BrowserThread::UI},
                 base::BindOnce(std::move(callback), result));
}

void CannedBrowsingDataAppCacheHelper::DeleteAppCaches(
    const url::Origin& origin) {
  pending_origins_.erase(origin);
  BrowsingDataAppCacheHelper::DeleteAppCaches(origin);
}

CannedBrowsingDataAppCacheHelper::~CannedBrowsingDataAppCacheHelper() {}
