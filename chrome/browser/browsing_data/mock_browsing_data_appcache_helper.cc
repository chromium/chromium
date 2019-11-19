// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/mock_browsing_data_appcache_helper.h"

#include <utility>
#include <vector>

#include "base/callback.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/storage_usage_info.h"
#include "testing/gtest/include/gtest/gtest.h"

MockBrowsingDataAppCacheHelper::MockBrowsingDataAppCacheHelper(Profile* profile)
    : BrowsingDataAppCacheHelper(
          content::BrowserContext::GetDefaultStoragePartition(profile)
              ->GetAppCacheService()) {}

MockBrowsingDataAppCacheHelper::~MockBrowsingDataAppCacheHelper() {
}

void MockBrowsingDataAppCacheHelper::StartFetching(
    FetchCallback completion_callback) {
  ASSERT_FALSE(completion_callback.is_null());
  ASSERT_TRUE(completion_callback_.is_null());
  completion_callback_ = std::move(completion_callback);
}

void MockBrowsingDataAppCacheHelper::DeleteAppCaches(
    const url::Origin& origin) {}

void MockBrowsingDataAppCacheHelper::AddAppCacheSamples() {
  response_.emplace_back(url::Origin::Create(GURL("http://hello/")), 6,
                         base::Time());
}

void MockBrowsingDataAppCacheHelper::Notify() {
  std::move(completion_callback_).Run(response_);
}
