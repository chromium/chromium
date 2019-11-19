// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSING_DATA_MOCK_BROWSING_DATA_LOCAL_STORAGE_HELPER_H_
#define CHROME_BROWSER_BROWSING_DATA_MOCK_BROWSING_DATA_LOCAL_STORAGE_HELPER_H_

#include <list>
#include <map>

#include "base/callback.h"
#include "base/macros.h"
#include "chrome/browser/browsing_data/browsing_data_local_storage_helper.h"

// Mock for BrowsingDataLocalStorageHelper.
// Use AddLocalStorageSamples() or add directly to response_ list, then
// call Notify().
class MockBrowsingDataLocalStorageHelper
    : public BrowsingDataLocalStorageHelper {
 public:
  explicit MockBrowsingDataLocalStorageHelper(Profile* profile);

  // BrowsingDataLocalStorageHelper implementation.
  void StartFetching(FetchCallback callback) override;
  void DeleteOrigin(const url::Origin& origin,
                    base::OnceClosure callback) override;

  // Adds some LocalStorageInfo samples.
  void AddLocalStorageSamples();

  // Add a LocalStorageInfo entry for a single origin.
  void AddLocalStorageForOrigin(const url::Origin& origin, int64_t size);

  // Notifies the callback.
  void Notify();

  // Marks all local storage files as existing.
  void Reset();

  // Returns true if all local storage files were deleted since the last Reset()
  // invocation.
  bool AllDeleted();

  url::Origin last_deleted_origin_;

 private:
  ~MockBrowsingDataLocalStorageHelper() override;

  FetchCallback callback_;

  std::map<const url::Origin, bool> origins_;

  std::list<content::StorageUsageInfo> response_;

  DISALLOW_COPY_AND_ASSIGN(MockBrowsingDataLocalStorageHelper);
};

#endif  // CHROME_BROWSER_BROWSING_DATA_MOCK_BROWSING_DATA_LOCAL_STORAGE_HELPER_H_
