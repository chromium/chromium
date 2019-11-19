// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/mock_browsing_data_local_storage_helper.h"

#include <utility>

#include "base/callback.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

MockBrowsingDataLocalStorageHelper::MockBrowsingDataLocalStorageHelper(
    Profile* profile)
    : BrowsingDataLocalStorageHelper(profile) {
}

MockBrowsingDataLocalStorageHelper::~MockBrowsingDataLocalStorageHelper() {
}

void MockBrowsingDataLocalStorageHelper::StartFetching(FetchCallback callback) {
  ASSERT_FALSE(callback.is_null());
  ASSERT_TRUE(callback_.is_null());
  callback_ = std::move(callback);
}

void MockBrowsingDataLocalStorageHelper::DeleteOrigin(
    const url::Origin& origin,
    base::OnceClosure callback) {
  ASSERT_TRUE(base::Contains(origins_, origin));
  last_deleted_origin_ = origin;
  origins_[origin] = false;
  std::move(callback).Run();
}

void MockBrowsingDataLocalStorageHelper::AddLocalStorageSamples() {
  const GURL kOrigin1("http://host1:1/");
  const GURL kOrigin2("http://host2:2/");
  AddLocalStorageForOrigin(url::Origin::Create(kOrigin1), 1);
  AddLocalStorageForOrigin(url::Origin::Create(kOrigin2), 2);
}

void MockBrowsingDataLocalStorageHelper::AddLocalStorageForOrigin(
    const url::Origin& origin,
    int64_t size) {
  response_.emplace_back(origin, size, base::Time());
  origins_[origin] = true;
}

void MockBrowsingDataLocalStorageHelper::Notify() {
  std::move(callback_).Run(response_);
}

void MockBrowsingDataLocalStorageHelper::Reset() {
  for (auto& pair : origins_)
    pair.second = true;
}

bool MockBrowsingDataLocalStorageHelper::AllDeleted() {
  for (const auto& pair : origins_) {
    if (pair.second)
      return false;
  }
  return true;
}
