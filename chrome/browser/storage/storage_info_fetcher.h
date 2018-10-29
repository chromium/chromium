// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_STORAGE_STORAGE_INFO_FETCHER_H_
#define CHROME_BROWSER_STORAGE_STORAGE_INFO_FETCHER_H_

#include "base/memory/ref_counted.h"
#include "storage/browser/quota/quota_callbacks.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom.h"

namespace storage {
class QuotaManager;
}

class Profile;

// Asynchronously fetches the amount of storage used by websites.
class StorageInfoFetcher :
    public base::RefCountedThreadSafe<StorageInfoFetcher> {
 public:
  using FetchCallback =
      base::Callback<void(const storage::UsageInfoEntries&)>;
  using ClearCallback =
      base::Callback<void(blink::mojom::QuotaStatusCode code)>;

  explicit StorageInfoFetcher(Profile* profile);

  // Asynchronously fetches the StorageInfo.
  void FetchStorageInfo(const FetchCallback& fetch_callback);

  // Asynchronously clears storage for the given host.
  void ClearStorage(const std::string& host,
                    blink::mojom::StorageType type,
                    const ClearCallback& clear_callback);

 private:
  virtual ~StorageInfoFetcher();

  friend class base::RefCountedThreadSafe<StorageInfoFetcher>;

  // Fetches the usage information.
  void GetUsageInfo(storage::GetUsageInfoCallback callback);

  // Called when usage information is available.
  void OnGetUsageInfoInternal(storage::UsageInfoEntries entries);

  // Reports back to all observers that information is available.
  void OnFetchCompleted();

  // Called when usage has been cleared.
  void OnUsageClearedInternal(blink::mojom::QuotaStatusCode code);

  // Reports back to all observers that storage has been deleted.
  void OnClearCompleted(blink::mojom::QuotaStatusCode code);

  // The quota manager to use to calculate the storage usage.
  storage::QuotaManager* quota_manager_;

  // Hosts and their usage.
  storage::UsageInfoEntries entries_;

  // The storage type to delete.
  blink::mojom::StorageType type_to_delete_;

  // The callback to use when fetching is complete.
  FetchCallback fetch_callback_;

  // The callback to use when storage has been cleared.
  ClearCallback clear_callback_;

  DISALLOW_COPY_AND_ASSIGN(StorageInfoFetcher);
};

#endif  // CHROME_BROWSER_STORAGE_STORAGE_INFO_FETCHER_H_
