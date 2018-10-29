// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSING_DATA_MOCK_BROWSING_DATA_CACHE_STORAGE_HELPER_H_
#define CHROME_BROWSER_BROWSING_DATA_MOCK_BROWSING_DATA_CACHE_STORAGE_HELPER_H_

#include <list>
#include <map>

#include "base/callback.h"
#include "base/macros.h"
#include "chrome/browser/browsing_data/browsing_data_cache_storage_helper.h"

class Profile;

// Mock for BrowsingDataCacheStorageHelper.
// Use AddCacheStorageSamples() or add directly to response_ list, then
// call Notify().
class MockBrowsingDataCacheStorageHelper
    : public BrowsingDataCacheStorageHelper {
 public:
  explicit MockBrowsingDataCacheStorageHelper(Profile* profile);

  // Adds some CacheStorageUsageInfo samples.
  void AddCacheStorageSamples();

  // Notifies the callback.
  void Notify();

  // Marks all cache storage files as existing.
  void Reset();

  // Returns true if all cache storage files were deleted since the last
  // Reset() invokation.
  bool AllDeleted();

  // BrowsingDataCacheStorageHelper.
  void StartFetching(FetchCallback callback) override;
  void DeleteCacheStorage(const GURL& origin) override;

 private:
  ~MockBrowsingDataCacheStorageHelper() override;

  FetchCallback callback_;
  bool fetched_ = false;
  std::map<GURL, bool> origins_;
  std::list<content::CacheStorageUsageInfo> response_;

  DISALLOW_COPY_AND_ASSIGN(MockBrowsingDataCacheStorageHelper);
};

#endif  // CHROME_BROWSER_BROWSING_DATA_MOCK_BROWSING_DATA_CACHE_STORAGE_HELPER_H_
