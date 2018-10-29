// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/browsing_data_appcache_helper.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/task/post_task.h"
#include "chrome/browser/browsing_data/browsing_data_helper.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "net/base/completion_callback.h"

using content::BrowserContext;
using content::BrowserThread;

namespace {

void OnAppCacheInfoFetchComplete(
    const BrowsingDataAppCacheHelper::FetchCallback& callback,
    scoped_refptr<content::AppCacheInfoCollection> info_collection,
    int /*rv*/) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(!callback.is_null());

  // Filter out appcache info entries for non-websafe schemes. Extension state
  // and DevTools, for example, are not considered browsing data.
  auto& origin_map = info_collection->infos_by_origin;
  for (auto it = origin_map.begin(); it != origin_map.end();) {
    if (!BrowsingDataHelper::IsWebScheme(it->first.scheme()))
      origin_map.erase(it++);
    else
      ++it;
  }

  base::PostTaskWithTraits(FROM_HERE, {BrowserThread::UI},
                           base::BindOnce(callback, info_collection));
}

}  // namespace

BrowsingDataAppCacheHelper::BrowsingDataAppCacheHelper(
    BrowserContext* browser_context)
    : appcache_service_(
          BrowserContext::GetDefaultStoragePartition(browser_context)
              ->GetAppCacheService()) {}

void BrowsingDataAppCacheHelper::StartFetching(const FetchCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!callback.is_null());

  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&BrowsingDataAppCacheHelper::StartFetchingOnIOThread, this,
                     callback));
}

void BrowsingDataAppCacheHelper::DeleteAppCacheGroup(const GURL& manifest_url) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&BrowsingDataAppCacheHelper::DeleteAppCacheGroupOnIOThread,
                     this, manifest_url));
}

BrowsingDataAppCacheHelper::~BrowsingDataAppCacheHelper() {}

void BrowsingDataAppCacheHelper::StartFetchingOnIOThread(
    const FetchCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(!callback.is_null());

  scoped_refptr<content::AppCacheInfoCollection> info_collection =
      new content::AppCacheInfoCollection();

  appcache_service_->GetAllAppCacheInfo(
      info_collection.get(),
      base::Bind(&OnAppCacheInfoFetchComplete, callback, info_collection));
}

void BrowsingDataAppCacheHelper::DeleteAppCacheGroupOnIOThread(
    const GURL& manifest_url) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  appcache_service_->DeleteAppCacheGroup(manifest_url,
                                         net::CompletionCallback());
}

CannedBrowsingDataAppCacheHelper::CannedBrowsingDataAppCacheHelper(
    BrowserContext* browser_context)
    : BrowsingDataAppCacheHelper(browser_context) {
  info_collection_ = new content::AppCacheInfoCollection;
}

void CannedBrowsingDataAppCacheHelper::AddAppCache(const GURL& manifest_url) {
  if (!BrowsingDataHelper::HasWebScheme(manifest_url))
    return;  // Ignore non-websafe schemes.

  OriginAppCacheInfoMap& origin_map = info_collection_->infos_by_origin;
  content::AppCacheInfoVector& appcache_infos =
      origin_map[url::Origin::Create(manifest_url)];

  for (const auto& appcache : appcache_infos) {
    if (appcache.manifest_url == manifest_url)
      return;
  }

  content::AppCacheInfo info;
  info.manifest_url = manifest_url;
  appcache_infos.push_back(info);
}

void CannedBrowsingDataAppCacheHelper::Reset() {
  info_collection_->infos_by_origin.clear();
}

bool CannedBrowsingDataAppCacheHelper::empty() const {
  return info_collection_->infos_by_origin.empty();
}

size_t CannedBrowsingDataAppCacheHelper::GetAppCacheCount() const {
  size_t count = 0;
  const OriginAppCacheInfoMap& map = info_collection_->infos_by_origin;
  for (const auto& pair : map)
    count += pair.second.size();
  return count;
}

const BrowsingDataAppCacheHelper::OriginAppCacheInfoMap&
CannedBrowsingDataAppCacheHelper::GetOriginAppCacheInfoMap() const {
  return info_collection_->infos_by_origin;
}

void CannedBrowsingDataAppCacheHelper::StartFetching(
    const FetchCallback& completion_callback) {
  completion_callback.Run(info_collection_);
}

void CannedBrowsingDataAppCacheHelper::DeleteAppCacheGroup(
    const GURL& manifest_url) {
  info_collection_->infos_by_origin.erase(url::Origin::Create(manifest_url));
  BrowsingDataAppCacheHelper::DeleteAppCacheGroup(manifest_url);
}

CannedBrowsingDataAppCacheHelper::~CannedBrowsingDataAppCacheHelper() {}
