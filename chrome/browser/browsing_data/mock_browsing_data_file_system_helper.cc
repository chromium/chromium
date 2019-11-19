// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/mock_browsing_data_file_system_helper.h"

#include "base/callback.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/storage_partition.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

MockBrowsingDataFileSystemHelper::MockBrowsingDataFileSystemHelper(
    Profile* profile)
    : BrowsingDataFileSystemHelper(
          content::BrowserContext::GetDefaultStoragePartition(profile)
              ->GetFileSystemContext()) {}

MockBrowsingDataFileSystemHelper::~MockBrowsingDataFileSystemHelper() {
}

void MockBrowsingDataFileSystemHelper::StartFetching(FetchCallback callback) {
  ASSERT_FALSE(callback.is_null());
  ASSERT_TRUE(callback_.is_null());
  callback_ = std::move(callback);
}

void MockBrowsingDataFileSystemHelper::DeleteFileSystemOrigin(
    const url::Origin& origin) {
  std::string key = origin.Serialize();
  ASSERT_TRUE(base::Contains(file_systems_, key));
  last_deleted_origin_ = origin;
  file_systems_[key] = false;
}

void MockBrowsingDataFileSystemHelper::AddFileSystem(const url::Origin& origin,
                                                     bool has_persistent,
                                                     bool has_temporary,
                                                     bool has_syncable,
                                                     int64_t size_persistent,
                                                     int64_t size_temporary,
                                                     int64_t size_syncable) {
  BrowsingDataFileSystemHelper::FileSystemInfo info(origin);
  if (has_persistent)
    info.usage_map[storage::kFileSystemTypePersistent] = size_persistent;
  if (has_temporary)
    info.usage_map[storage::kFileSystemTypeTemporary] = size_temporary;
  if (has_syncable)
    info.usage_map[storage::kFileSystemTypeSyncable] = size_syncable;
  response_.push_back(info);
  file_systems_[origin.Serialize()] = true;
}

void MockBrowsingDataFileSystemHelper::AddFileSystemSamples() {
  AddFileSystem(url::Origin::Create(GURL("http://fshost1:1")), false, true,
                false, 0, 1, 0);
  AddFileSystem(url::Origin::Create(GURL("http://fshost2:2")), true, false,
                true, 2, 0, 2);
  AddFileSystem(url::Origin::Create(GURL("http://fshost3:3")), true, true, true,
                3, 3, 3);
}

void MockBrowsingDataFileSystemHelper::Notify() {
  std::move(callback_).Run(response_);
}

void MockBrowsingDataFileSystemHelper::Reset() {
  for (auto& pair : file_systems_)
    pair.second = true;
}

bool MockBrowsingDataFileSystemHelper::AllDeleted() {
  for (const auto& pair : file_systems_) {
    if (pair.second)
      return false;
  }
  return true;
}
