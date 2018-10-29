// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_settings/local_shared_objects_container.h"

#include "chrome/browser/browsing_data/browsing_data_appcache_helper.h"
#include "chrome/browser/browsing_data/browsing_data_cache_storage_helper.h"
#include "chrome/browser/browsing_data/browsing_data_channel_id_helper.h"
#include "chrome/browser/browsing_data/browsing_data_cookie_helper.h"
#include "chrome/browser/browsing_data/browsing_data_database_helper.h"
#include "chrome/browser/browsing_data/browsing_data_file_system_helper.h"
#include "chrome/browser/browsing_data/browsing_data_flash_lso_helper.h"
#include "chrome/browser/browsing_data/browsing_data_indexed_db_helper.h"
#include "chrome/browser/browsing_data/browsing_data_local_storage_helper.h"
#include "chrome/browser/browsing_data/browsing_data_service_worker_helper.h"
#include "chrome/browser/browsing_data/browsing_data_shared_worker_helper.h"
#include "chrome/browser/browsing_data/canonical_cookie_hash.h"
#include "chrome/browser/browsing_data/cookies_tree_model.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/url_constants.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/cookies/canonical_cookie.h"
#include "url/gurl.h"

namespace {

bool SameDomainOrHost(const GURL& gurl1, const GURL& gurl2) {
  return net::registry_controlled_domains::SameDomainOrHost(
      gurl1,
      gurl2,
      net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
}

}  // namespace

LocalSharedObjectsContainer::LocalSharedObjectsContainer(Profile* profile)
    : appcaches_(new CannedBrowsingDataAppCacheHelper(profile)),
      channel_ids_(new CannedBrowsingDataChannelIDHelper()),
      cookies_(new CannedBrowsingDataCookieHelper(
          content::BrowserContext::GetDefaultStoragePartition(profile))),
      databases_(new CannedBrowsingDataDatabaseHelper(profile)),
      file_systems_(new CannedBrowsingDataFileSystemHelper(profile)),
      indexed_dbs_(new CannedBrowsingDataIndexedDBHelper(
          content::BrowserContext::GetDefaultStoragePartition(profile)
              ->GetIndexedDBContext())),
      local_storages_(new CannedBrowsingDataLocalStorageHelper(profile)),
      service_workers_(new CannedBrowsingDataServiceWorkerHelper(
          content::BrowserContext::GetDefaultStoragePartition(profile)
              ->GetServiceWorkerContext())),
      shared_workers_(new CannedBrowsingDataSharedWorkerHelper(
          content::BrowserContext::GetDefaultStoragePartition(profile),
          profile->GetResourceContext())),
      cache_storages_(new CannedBrowsingDataCacheStorageHelper(
          content::BrowserContext::GetDefaultStoragePartition(profile)
              ->GetCacheStorageContext())),
      session_storages_(new CannedBrowsingDataLocalStorageHelper(profile)) {}

LocalSharedObjectsContainer::~LocalSharedObjectsContainer() {
}

size_t LocalSharedObjectsContainer::GetObjectCount() const {
  size_t count = 0;
  count += appcaches()->GetAppCacheCount();
  count += channel_ids()->GetChannelIDCount();
  count += cookies()->GetCookieCount();
  count += databases()->GetDatabaseCount();
  count += file_systems()->GetFileSystemCount();
  count += indexed_dbs()->GetIndexedDBCount();
  count += local_storages()->GetLocalStorageCount();
  count += service_workers()->GetServiceWorkerCount();
  count += shared_workers()->GetSharedWorkerCount();
  count += cache_storages()->GetCacheStorageCount();
  count += session_storages()->GetLocalStorageCount();
  return count;
}

size_t LocalSharedObjectsContainer::GetObjectCountForDomain(
    const GURL& origin) const {
  size_t count = 0;

  // Count all cookies that have the same domain as the provided |origin|. This
  // means count all cookies that have been set by a host that is not considered
  // to be a third party regarding the domain of the provided |origin|. E.g. if
  // the origin is "http://foo.com" then all cookies with domain foo.com,
  // a.foo.com, b.a.foo.com or *.foo.com will be counted.
  typedef CannedBrowsingDataCookieHelper::OriginCookieSetMap OriginCookieSetMap;
  const OriginCookieSetMap& origin_cookies_set_map =
      cookies()->origin_cookie_set_map();
  for (auto it = origin_cookies_set_map.begin();
       it != origin_cookies_set_map.end(); ++it) {
    const canonical_cookie::CookieHashSet* cookie_list = it->second.get();
    for (const auto& cookie : *cookie_list) {
      // Strip leading '.'s.
      std::string cookie_domain = cookie.Domain();
      if (cookie_domain[0] == '.')
        cookie_domain = cookie_domain.substr(1);
      // The |domain_url| is only created in order to use the
      // SameDomainOrHost method below. It does not matter which scheme is
      // used as the scheme is ignored by the SameDomainOrHost method.
      GURL domain_url(std::string(url::kHttpScheme) +
                      url::kStandardSchemeSeparator + cookie_domain);

      if (origin.SchemeIsHTTPOrHTTPS() && SameDomainOrHost(origin, domain_url))
        ++count;
    }
  }

  // Count local storages for the domain of the given |origin|.
  const std::set<GURL> local_storage_info =
      local_storages()->GetLocalStorageInfo();
  for (auto it = local_storage_info.begin(); it != local_storage_info.end();
       ++it) {
    if (SameDomainOrHost(origin, *it))
      ++count;
  }

  // Count session storages for the domain of the given |origin|.
  const std::set<GURL> urls = session_storages()->GetLocalStorageInfo();
  for (auto it = urls.begin(); it != urls.end(); ++it) {
    if (SameDomainOrHost(origin, *it))
      ++count;
  }

  // Count indexed dbs for the domain of the given |origin|.
  typedef CannedBrowsingDataIndexedDBHelper::PendingIndexedDBInfo IndexedDBInfo;
  const std::set<IndexedDBInfo>& indexed_db_info =
      indexed_dbs()->GetIndexedDBInfo();
  for (auto it = indexed_db_info.begin(); it != indexed_db_info.end(); ++it) {
    if (SameDomainOrHost(origin, it->origin))
      ++count;
  }

  // Count service workers for the domain of the given |origin|.
  typedef CannedBrowsingDataServiceWorkerHelper::PendingServiceWorkerUsageInfo
      ServiceWorkerInfo;
  const std::set<ServiceWorkerInfo>& service_worker_info =
      service_workers()->GetServiceWorkerUsageInfo();
  for (auto it = service_worker_info.begin(); it != service_worker_info.end();
       ++it) {
    if (SameDomainOrHost(origin, it->origin))
      ++count;
  }

  // Count shared workers for the domain of the given |origin|.
  typedef BrowsingDataSharedWorkerHelper::SharedWorkerInfo SharedWorkerInfo;
  const std::set<SharedWorkerInfo>& shared_worker_info =
      shared_workers()->GetSharedWorkerInfo();
  for (const auto& it : shared_worker_info) {
    if (SameDomainOrHost(origin, it.worker))
      ++count;
  }

  // Count cache storages for the domain of the given |origin|.
  typedef CannedBrowsingDataCacheStorageHelper::PendingCacheStorageUsageInfo
      CacheStorageInfo;
  const std::set<CacheStorageInfo>& cache_storage_info =
      cache_storages()->GetCacheStorageUsageInfo();
  for (const CacheStorageInfo& it : cache_storage_info) {
    if (SameDomainOrHost(origin, it.origin))
      ++count;
  }

  // Count filesystems for the domain of the given |origin|.
  typedef BrowsingDataFileSystemHelper::FileSystemInfo FileSystemInfo;
  typedef std::list<FileSystemInfo> FileSystemInfoList;
  const FileSystemInfoList& file_system_info =
      file_systems()->GetFileSystemInfo();
  for (auto it = file_system_info.begin(); it != file_system_info.end(); ++it) {
    if (SameDomainOrHost(origin, it->origin))
      ++count;
  }

  // Count databases for the domain of the given |origin|.
  typedef CannedBrowsingDataDatabaseHelper::PendingDatabaseInfo DatabaseInfo;
  const std::set<DatabaseInfo>& database_list =
      databases()->GetPendingDatabaseInfo();
  for (auto it = database_list.begin(); it != database_list.end(); ++it) {
    if (SameDomainOrHost(origin, it->origin))
      ++count;
  }

  // Count the AppCache manifest files for the domain of the given |origin|.
  typedef BrowsingDataAppCacheHelper::OriginAppCacheInfoMap
      OriginAppCacheInfoMap;
  const OriginAppCacheInfoMap& map = appcaches()->GetOriginAppCacheInfoMap();
  for (auto it = map.begin(); it != map.end(); ++it) {
    const content::AppCacheInfoVector& info_vector = it->second;
    for (auto info = info_vector.begin(); info != info_vector.end(); ++info) {
      if (SameDomainOrHost(origin, info->manifest_url))
        ++count;
    }
  }

  return count;
}

void LocalSharedObjectsContainer::Reset() {
  appcaches_->Reset();
  channel_ids_->Reset();
  cookies_->Reset();
  databases_->Reset();
  file_systems_->Reset();
  indexed_dbs_->Reset();
  local_storages_->Reset();
  service_workers_->Reset();
  shared_workers_->Reset();
  cache_storages_->Reset();
  session_storages_->Reset();
}

std::unique_ptr<CookiesTreeModel>
LocalSharedObjectsContainer::CreateCookiesTreeModel() const {
  auto container = std::make_unique<LocalDataContainer>(
      cookies_, databases_, local_storages_, session_storages_, appcaches_,
      indexed_dbs_, file_systems_, nullptr, channel_ids_, service_workers_,
      shared_workers_, cache_storages_, nullptr, nullptr);

  return std::make_unique<CookiesTreeModel>(std::move(container), nullptr);
}
