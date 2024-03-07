// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_LOCAL_LOCAL_FILE_SYNC_STATUS_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_LOCAL_LOCAL_FILE_SYNC_STATUS_H_

#include <stdint.h>

#include <map>
#include <set>
#include <utility>

#include "base/gtest_prod_util.h"
#include "base/observer_list.h"
#include "storage/browser/file_system/file_system_url.h"

namespace storage {
class FileSystemURL;
}

namespace sync_file_system {

// Represents local file sync status.
// This class is supposed to run only on IO thread.
//
// This class manages two important synchronization flags: writing (counter)
// and syncing (flag).  Writing counter keeps track of which URL is in
// writing and syncing flag indicates which URL is in syncing.
//
// An entry can have multiple writers but sync is exclusive and cannot overwrap
// with any writes or syncs.
class LocalFileSyncStatus {
 public:
  using OriginAndType = std::pair<GURL, storage::FileSystemType>;

  class Observer {
   public:
    Observer() {}

    Observer(const Observer&) = delete;
    Observer& operator=(const Observer&) = delete;

    virtual ~Observer() {}
    virtual void OnSyncEnabled(const storage::FileSystemURL& url) = 0;
    virtual void OnWriteEnabled(const storage::FileSystemURL& url) = 0;
  };

  LocalFileSyncStatus();

  LocalFileSyncStatus(const LocalFileSyncStatus&) = delete;
  LocalFileSyncStatus& operator=(const LocalFileSyncStatus&) = delete;

  ~LocalFileSyncStatus();

  // Increment writing counter for |url|.
  // This should not be called if the |url| is not writable.
  void StartWriting(const storage::FileSystemURL& url);

  // Decrement writing counter for |url|.
  void EndWriting(const storage::FileSystemURL& url);

  // Start syncing for |url| and disable writing.
  // This should not be called if |url| is in syncing or in writing.
  void StartSyncing(const storage::FileSystemURL& url);

  // Clears the syncing flag for |url| and enable writing.
  void EndSyncing(const storage::FileSystemURL& url);

  // Returns true if the |url| or its parent or child is in writing.
  bool IsWriting(const storage::FileSystemURL& url) const;

  // Returns true if the |url| is enabled for writing (i.e. not in syncing).
  bool IsWritable(const storage::FileSystemURL& url) const;

  // Returns true if the |url| is enabled for syncing (i.e. neither in
  // syncing nor writing).
  bool IsSyncable(const storage::FileSystemURL& url) const;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  FRIEND_TEST_ALL_PREFIXES(LocalFileSyncStatusTest, WritingOnPathsWithPeriod);
  FRIEND_TEST_ALL_PREFIXES(LocalFileSyncStatusTest, SyncingOnPathsWithPeriod);

  using PathSet = std::set<base::FilePath>;
  using URLSet = std::map<OriginAndType, PathSet>;

  using PathBucket = std::map<base::FilePath, int64_t>;
  using URLBucket = std::map<OriginAndType, PathBucket>;

  bool IsChildOrParentWriting(const storage::FileSystemURL& url) const;
  bool IsChildOrParentSyncing(const storage::FileSystemURL& url) const;

  // If this count is non-zero, then there are ongoing write operations.
  URLBucket writing_;

  // If this flag is set sync process is running on the file.
  URLSet syncing_;

  base::ObserverList<Observer>::UncheckedAndDanglingUntriaged observer_list_;
};

}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_LOCAL_LOCAL_FILE_SYNC_STATUS_H_
