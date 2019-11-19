// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/mock_browsing_data_service_worker_helper.h"

#include <vector>

#include "base/callback.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/storage_usage_info.h"
#include "testing/gtest/include/gtest/gtest.h"

MockBrowsingDataServiceWorkerHelper::MockBrowsingDataServiceWorkerHelper(
    Profile* profile)
    : BrowsingDataServiceWorkerHelper(
        content::BrowserContext::GetDefaultStoragePartition(profile)->
            GetServiceWorkerContext()) {
}

MockBrowsingDataServiceWorkerHelper::~MockBrowsingDataServiceWorkerHelper() {
}

void MockBrowsingDataServiceWorkerHelper::StartFetching(
    FetchCallback callback) {
  ASSERT_FALSE(callback.is_null());
  ASSERT_TRUE(callback_.is_null());
  callback_ = std::move(callback);
}

void MockBrowsingDataServiceWorkerHelper::DeleteServiceWorkers(
    const GURL& origin) {
  ASSERT_TRUE(base::Contains(origins_, origin));
  origins_[origin] = false;
}

void MockBrowsingDataServiceWorkerHelper::AddServiceWorkerSamples() {
  const GURL kOrigin1("https://swhost1:1/");
  const GURL kOrigin2("https://swhost2:2/");

  response_.emplace_back(url::Origin::Create(kOrigin1), 1, base::Time());
  origins_[kOrigin1] = true;

  response_.emplace_back(url::Origin::Create(kOrigin2), 2, base::Time());
  origins_[kOrigin2] = true;
}

void MockBrowsingDataServiceWorkerHelper::Notify() {
  std::move(callback_).Run(response_);
}

void MockBrowsingDataServiceWorkerHelper::Reset() {
  for (auto& pair : origins_)
    pair.second = true;
}

bool MockBrowsingDataServiceWorkerHelper::AllDeleted() {
  for (const auto& pair : origins_) {
    if (pair.second)
      return false;
  }
  return true;
}
