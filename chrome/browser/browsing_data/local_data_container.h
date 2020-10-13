// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSING_DATA_LOCAL_DATA_CONTAINER_H_
#define CHROME_BROWSER_BROWSING_DATA_LOCAL_DATA_CONTAINER_H_

#include <list>
#include <map>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "chrome/browser/browsing_data/browsing_data_media_license_helper.h"
#include "chrome/browser/browsing_data/browsing_data_quota_helper.h"
#include "components/browsing_data/content/appcache_helper.h"
#include "components/browsing_data/content/cache_storage_helper.h"
#include "components/browsing_data/content/cookie_helper.h"
#include "components/browsing_data/content/database_helper.h"
#include "components/browsing_data/content/file_system_helper.h"
#include "components/browsing_data/content/indexed_db_helper.h"
#include "components/browsing_data/content/local_storage_helper.h"
#include "components/browsing_data/content/service_worker_helper.h"
#include "components/browsing_data/content/shared_worker_helper.h"

class BrowsingDataFlashLSOHelper;
class CookiesTreeModel;
class LocalDataContainer;

namespace content {
struct StorageUsageInfo;
}

namespace net {
class CanonicalCookie;
}

// LocalDataContainer ---------------------------------------------------------
// This class is a wrapper around all the BrowsingData*Helper classes. Because
// isolated applications have separate storage, we need different helper
// instances. As such, this class contains the app name and id, along with the
// helpers for all of the data types we need. The browser-wide "app id" will be
// the empty string, as no app can have an empty id.
class LocalDataContainer {
 public:
  // Friendly typedefs for the multiple types of lists used in the model.
  using CookieList = std::list<net::CanonicalCookie>;
  using DatabaseInfoList = std::list<content::StorageUsageInfo>;
  using LocalStorageInfoList = std::list<content::StorageUsageInfo>;
  using SessionStorageInfoList = std::list<content::StorageUsageInfo>;
  using IndexedDBInfoList = std::list<content::StorageUsageInfo>;
  using FileSystemInfoList =
      std::list<browsing_data::FileSystemHelper::FileSystemInfo>;
  using QuotaInfoList = std::list<BrowsingDataQuotaHelper::QuotaInfo>;
  using ServiceWorkerUsageInfoList = std::list<content::StorageUsageInfo>;
  using SharedWorkerInfoList =
      std::list<browsing_data::SharedWorkerHelper::SharedWorkerInfo>;
  using CacheStorageUsageInfoList = std::list<content::StorageUsageInfo>;
  using AppCacheInfoList = std::list<content::StorageUsageInfo>;
  using FlashLSODomainList = std::vector<std::string>;
  using MediaLicenseInfoList =
      std::list<BrowsingDataMediaLicenseHelper::MediaLicenseInfo>;

  LocalDataContainer(
      scoped_refptr<browsing_data::CookieHelper> cookie_helper,
      scoped_refptr<browsing_data::DatabaseHelper> database_helper,
      scoped_refptr<browsing_data::LocalStorageHelper> local_storage_helper,
      scoped_refptr<browsing_data::LocalStorageHelper> session_storage_helper,
      scoped_refptr<browsing_data::AppCacheHelper> appcache_helper,
      scoped_refptr<browsing_data::IndexedDBHelper> indexed_db_helper,
      scoped_refptr<browsing_data::FileSystemHelper> file_system_helper,
      scoped_refptr<BrowsingDataQuotaHelper> quota_helper,
      scoped_refptr<browsing_data::ServiceWorkerHelper> service_worker_helper,
      scoped_refptr<browsing_data::SharedWorkerHelper> shared_worker_helper,
      scoped_refptr<browsing_data::CacheStorageHelper> cache_storage_helper,
      scoped_refptr<BrowsingDataFlashLSOHelper> flash_data_helper,
      scoped_refptr<BrowsingDataMediaLicenseHelper> media_license_helper);
  virtual ~LocalDataContainer();

  // This method must be called to start the process of fetching the resources.
  // The delegate passed in is called back to deliver the updates.
  void Init(CookiesTreeModel* delegate);

 private:
  friend class CookiesTreeModel;
  friend class CookieTreeAppCacheNode;
  friend class CookieTreeMediaLicenseNode;
  friend class CookieTreeCookieNode;
  friend class CookieTreeDatabaseNode;
  friend class CookieTreeLocalStorageNode;
  friend class CookieTreeSessionStorageNode;
  friend class CookieTreeIndexedDBNode;
  friend class CookieTreeFileSystemNode;
  friend class CookieTreeQuotaNode;
  friend class CookieTreeServiceWorkerNode;
  friend class CookieTreeSharedWorkerNode;
  friend class CookieTreeCacheStorageNode;
  friend class CookieTreeFlashLSONode;

  // Callback methods to be invoked when fetching the data is complete.
  void OnAppCacheModelInfoLoaded(const AppCacheInfoList& appcache_info_list);
  void OnCookiesModelInfoLoaded(const net::CookieList& cookie_list);
  void OnDatabaseModelInfoLoaded(const DatabaseInfoList& database_info);
  void OnLocalStorageModelInfoLoaded(
      const LocalStorageInfoList& local_storage_info);
  void OnSessionStorageModelInfoLoaded(
      const LocalStorageInfoList& local_storage_info);
  void OnIndexedDBModelInfoLoaded(
      const IndexedDBInfoList& indexed_db_info);
  void OnFileSystemModelInfoLoaded(
      const FileSystemInfoList& file_system_info);
  void OnQuotaModelInfoLoaded(const QuotaInfoList& quota_info);
  void OnServiceWorkerModelInfoLoaded(
      const ServiceWorkerUsageInfoList& service_worker_info);
  void OnSharedWorkerInfoLoaded(const SharedWorkerInfoList& shared_worker_info);
  void OnCacheStorageModelInfoLoaded(
      const CacheStorageUsageInfoList& cache_storage_info);
  void OnFlashLSOInfoLoaded(const FlashLSODomainList& domains);
  void OnMediaLicenseInfoLoaded(const MediaLicenseInfoList& media_license_info);

  // Pointers to the helper objects, needed to retreive all the types of locally
  // stored data.
  scoped_refptr<browsing_data::AppCacheHelper> appcache_helper_;
  scoped_refptr<browsing_data::CookieHelper> cookie_helper_;
  scoped_refptr<browsing_data::DatabaseHelper> database_helper_;
  scoped_refptr<browsing_data::LocalStorageHelper> local_storage_helper_;
  scoped_refptr<browsing_data::LocalStorageHelper> session_storage_helper_;
  scoped_refptr<browsing_data::IndexedDBHelper> indexed_db_helper_;
  scoped_refptr<browsing_data::FileSystemHelper> file_system_helper_;
  scoped_refptr<BrowsingDataQuotaHelper> quota_helper_;
  scoped_refptr<browsing_data::ServiceWorkerHelper> service_worker_helper_;
  scoped_refptr<browsing_data::SharedWorkerHelper> shared_worker_helper_;
  scoped_refptr<browsing_data::CacheStorageHelper> cache_storage_helper_;
  scoped_refptr<BrowsingDataFlashLSOHelper> flash_lso_helper_;
  scoped_refptr<BrowsingDataMediaLicenseHelper> media_license_helper_;

  // Storage for all the data that was retrieved through the helper objects.
  // The collected data is used for (re)creating the CookiesTreeModel.
  AppCacheInfoList appcache_info_list_;
  CookieList cookie_list_;
  DatabaseInfoList database_info_list_;
  LocalStorageInfoList local_storage_info_list_;
  LocalStorageInfoList session_storage_info_list_;
  IndexedDBInfoList indexed_db_info_list_;
  FileSystemInfoList file_system_info_list_;
  QuotaInfoList quota_info_list_;
  ServiceWorkerUsageInfoList service_worker_info_list_;
  SharedWorkerInfoList shared_worker_info_list_;
  CacheStorageUsageInfoList cache_storage_info_list_;
  FlashLSODomainList flash_lso_domain_list_;
  MediaLicenseInfoList media_license_info_list_;

  // A delegate, which must outlive this object. The update callbacks use the
  // delegate to deliver the updated data to the CookieTreeModel.
  CookiesTreeModel* model_ = nullptr;

  // Keeps track of how many batches are expected to start.
  int batches_started_ = 0;

  base::WeakPtrFactory<LocalDataContainer> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(LocalDataContainer);
};

#endif  // CHROME_BROWSER_BROWSING_DATA_LOCAL_DATA_CONTAINER_H_
