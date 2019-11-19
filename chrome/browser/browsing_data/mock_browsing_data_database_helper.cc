// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/mock_browsing_data_database_helper.h"

#include <utility>

#include "base/callback.h"
#include "base/stl_util.h"
#include "testing/gtest/include/gtest/gtest.h"

MockBrowsingDataDatabaseHelper::MockBrowsingDataDatabaseHelper(
    Profile* profile)
    : BrowsingDataDatabaseHelper(profile) {
}

MockBrowsingDataDatabaseHelper::~MockBrowsingDataDatabaseHelper() {
}

void MockBrowsingDataDatabaseHelper::StartFetching(FetchCallback callback) {
  callback_ = std::move(callback);
}

void MockBrowsingDataDatabaseHelper::DeleteDatabase(const url::Origin& origin) {
  const std::string identifier = storage::GetIdentifierFromOrigin(origin);
  ASSERT_TRUE(base::Contains(databases_, identifier));
  last_deleted_origin_ = identifier;
  databases_[identifier] = false;
}

void MockBrowsingDataDatabaseHelper::AddDatabaseSamples() {
  response_.push_back(content::StorageUsageInfo(
      url::Origin::Create(GURL("http://gdbhost1:1")), 1, base::Time()));
  databases_["http_gdbhost1_1"] = true;
  response_.push_back(content::StorageUsageInfo(
      url::Origin::Create(GURL("http://gdbhost2:2")), 2, base::Time()));
  databases_["http_gdbhost2_2"] = true;
}

void MockBrowsingDataDatabaseHelper::Notify() {
  std::move(callback_).Run(response_);
}

void MockBrowsingDataDatabaseHelper::Reset() {
  for (auto& pair : databases_)
    pair.second = true;
}

bool MockBrowsingDataDatabaseHelper::AllDeleted() {
  for (const auto& pair : databases_) {
    if (pair.second)
      return false;
  }
  return true;
}
