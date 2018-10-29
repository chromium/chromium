// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_CACHE_STORAGE_HELPER_H_
#define CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_CACHE_STORAGE_HELPER_H_

#include <stddef.h>
#include <stdint.h>

#include <list>
#include <set>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "content/public/browser/cache_storage_context.h"
#include "content/public/browser/cache_storage_usage_info.h"
#include "url/gurl.h"

// BrowsingDataCacheStorageHelper is an interface for classes dealing with
// aggregating and deleting browsing data stored for Cache Storage.
// A client of this class need to call StartFetching from the UI thread to
// initiate the flow, and it'll be notified by the callback in its UI thread at
// some later point.
class BrowsingDataCacheStorageHelper
    : public base::RefCountedThreadSafe<BrowsingDataCacheStorageHelper> {
 public:
  using FetchCallback = base::OnceCallback<void(
      const std::list<content::CacheStorageUsageInfo>&)>;

  // Create a BrowsingDataCacheStorageHelper instance for the Cache Storage
  // stored in |context|'s associated profile's user data directory.
  explicit BrowsingDataCacheStorageHelper(
      content::CacheStorageContext* context);

  // Starts the fetching process, which will notify its completion via
  // |callback|. This must be called only in the UI thread.
  virtual void StartFetching(FetchCallback callback);
  // Requests the Cache Storage data for an origin be deleted.
  virtual void DeleteCacheStorage(const GURL& origin);

 protected:
  virtual ~BrowsingDataCacheStorageHelper();

  // Owned by the profile.
  content::CacheStorageContext* cache_storage_context_;

 private:
  friend class base::RefCountedThreadSafe<BrowsingDataCacheStorageHelper>;

  // Deletes Cache Storages for an origin the IO thread.
  void DeleteCacheStorageOnIOThread(const GURL& origin);

  // Enumerates all Cache Storage instances on the IO thread.
  void FetchCacheStorageUsageInfoOnIOThread(FetchCallback callback);

  DISALLOW_COPY_AND_ASSIGN(BrowsingDataCacheStorageHelper);
};

// This class is an implementation of BrowsingDataCacheStorageHelper that does
// not fetch its information from the Cache Storage context, but is passed the
// info as a parameter.
class CannedBrowsingDataCacheStorageHelper
    : public BrowsingDataCacheStorageHelper {
 public:
  // Contains information about a Cache Storage.
  struct PendingCacheStorageUsageInfo {
    PendingCacheStorageUsageInfo(const GURL& origin,
                                 int64_t total_size_bytes,
                                 const base::Time& last_modified);
    ~PendingCacheStorageUsageInfo();

    bool operator<(const PendingCacheStorageUsageInfo& other) const;

    GURL origin;
    int64_t total_size_bytes;
    base::Time last_modified;
  };

  explicit CannedBrowsingDataCacheStorageHelper(
      content::CacheStorageContext* context);

  // Add a Cache Storage to the set of canned Cache Storages that is
  // returned by this helper.
  void AddCacheStorage(const GURL& origin);

  // Clear the list of canned Cache Storages.
  void Reset();

  // True if no Cache Storages are currently stored.
  bool empty() const;

  // Returns the number of currently stored Cache Storages.
  size_t GetCacheStorageCount() const;

  // Returns the current list of Cache Storages.
  const std::set<
      CannedBrowsingDataCacheStorageHelper::PendingCacheStorageUsageInfo>&
  GetCacheStorageUsageInfo() const;

  // BrowsingDataCacheStorageHelper methods.
  void StartFetching(FetchCallback callback) override;
  void DeleteCacheStorage(const GURL& origin) override;

 private:
  ~CannedBrowsingDataCacheStorageHelper() override;

  std::set<PendingCacheStorageUsageInfo> pending_cache_storage_info_;

  DISALLOW_COPY_AND_ASSIGN(CannedBrowsingDataCacheStorageHelper);
};

#endif  // CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_CACHE_STORAGE_HELPER_H_
