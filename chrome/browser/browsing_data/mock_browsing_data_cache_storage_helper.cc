// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/mock_browsing_data_cache_storage_helper.h"

#include "base/callback.h"
#include "base/logging.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "testing/gtest/include/gtest/gtest.h"

MockBrowsingDataCacheStorageHelper::MockBrowsingDataCacheStorageHelper(
    Profile* profile)
    : BrowsingDataCacheStorageHelper(
          content::BrowserContext::GetDefaultStoragePartition(profile)
              ->GetCacheStorageContext()) {}

MockBrowsingDataCacheStorageHelper::~MockBrowsingDataCacheStorageHelper() {}

void MockBrowsingDataCacheStorageHelper::StartFetching(FetchCallback callback) {
  ASSERT_FALSE(callback.is_null());
  ASSERT_TRUE(callback_.is_null());
  callback_ = std::move(callback);
  fetched_ = true;
}

void MockBrowsingDataCacheStorageHelper::DeleteCacheStorage(
    const GURL& origin) {
  ASSERT_TRUE(fetched_);
  ASSERT_TRUE(origins_.find(origin) != origins_.end());
  origins_[origin] = false;
}

void MockBrowsingDataCacheStorageHelper::AddCacheStorageSamples() {
  const GURL kOrigin1("https://cshost1:1/");
  const GURL kOrigin2("https://cshost2:2/");
  content::CacheStorageUsageInfo info1(kOrigin1, 1, base::Time());
  response_.push_back(info1);
  origins_[kOrigin1] = true;
  content::CacheStorageUsageInfo info2(kOrigin2, 2, base::Time());
  response_.push_back(info2);
  origins_[kOrigin2] = true;
}

void MockBrowsingDataCacheStorageHelper::Notify() {
  ASSERT_FALSE(callback_.is_null());
  std::move(callback_).Run(response_);
}

void MockBrowsingDataCacheStorageHelper::Reset() {
  for (auto& pair : origins_)
    pair.second = true;
}

bool MockBrowsingDataCacheStorageHelper::AllDeleted() {
  for (const auto& pair : origins_) {
    if (pair.second)
      return false;
  }
  return true;
}
