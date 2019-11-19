// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTENT_SETTINGS_LOCAL_SHARED_OBJECTS_CONTAINER_H_
#define CHROME_BROWSER_CONTENT_SETTINGS_LOCAL_SHARED_OBJECTS_CONTAINER_H_

#include <stddef.h>

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"

class CannedBrowsingDataAppCacheHelper;
class CannedBrowsingDataCookieHelper;
class CannedBrowsingDataDatabaseHelper;
class CannedBrowsingDataFileSystemHelper;
class CannedBrowsingDataIndexedDBHelper;
class CannedBrowsingDataLocalStorageHelper;
class CannedBrowsingDataServiceWorkerHelper;
class CannedBrowsingDataSharedWorkerHelper;
class CannedBrowsingDataCacheStorageHelper;
class CookiesTreeModel;
class GURL;
class Profile;

class LocalSharedObjectsContainer {
 public:
  explicit LocalSharedObjectsContainer(Profile* profile);
  ~LocalSharedObjectsContainer();

  // Returns the number of objects stored in the container.
  size_t GetObjectCount() const;

  // Returns the number of objects for the given |origin|.
  size_t GetObjectCountForDomain(const GURL& origin) const;

  // Get number of unique registrable domains in the container.
  size_t GetDomainCount() const;

  // Empties the container.
  void Reset();

  // Creates a new CookiesTreeModel for all objects in the container,
  // copying each of them.
  std::unique_ptr<CookiesTreeModel> CreateCookiesTreeModel() const;

  CannedBrowsingDataAppCacheHelper* appcaches() const {
    return appcaches_.get();
  }
  CannedBrowsingDataCookieHelper* cookies() const { return cookies_.get(); }
  CannedBrowsingDataDatabaseHelper* databases() const {
    return databases_.get();
  }
  CannedBrowsingDataFileSystemHelper* file_systems() const {
    return file_systems_.get();
  }
  CannedBrowsingDataIndexedDBHelper* indexed_dbs() const {
    return indexed_dbs_.get();
  }
  CannedBrowsingDataLocalStorageHelper* local_storages() const {
    return local_storages_.get();
  }
  CannedBrowsingDataServiceWorkerHelper* service_workers() const {
    return service_workers_.get();
  }
  CannedBrowsingDataSharedWorkerHelper* shared_workers() const {
    return shared_workers_.get();
  }
  CannedBrowsingDataCacheStorageHelper* cache_storages() const {
    return cache_storages_.get();
  }
  CannedBrowsingDataLocalStorageHelper* session_storages() const {
    return session_storages_.get();
  }

 private:
  scoped_refptr<CannedBrowsingDataAppCacheHelper> appcaches_;
  scoped_refptr<CannedBrowsingDataCookieHelper> cookies_;
  scoped_refptr<CannedBrowsingDataDatabaseHelper> databases_;
  scoped_refptr<CannedBrowsingDataFileSystemHelper> file_systems_;
  scoped_refptr<CannedBrowsingDataIndexedDBHelper> indexed_dbs_;
  scoped_refptr<CannedBrowsingDataLocalStorageHelper> local_storages_;
  scoped_refptr<CannedBrowsingDataServiceWorkerHelper> service_workers_;
  scoped_refptr<CannedBrowsingDataSharedWorkerHelper> shared_workers_;
  scoped_refptr<CannedBrowsingDataCacheStorageHelper> cache_storages_;
  scoped_refptr<CannedBrowsingDataLocalStorageHelper> session_storages_;

  DISALLOW_COPY_AND_ASSIGN(LocalSharedObjectsContainer);
};

#endif  // CHROME_BROWSER_CONTENT_SETTINGS_LOCAL_SHARED_OBJECTS_CONTAINER_H_
