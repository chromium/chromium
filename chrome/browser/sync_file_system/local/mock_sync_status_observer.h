// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_LOCAL_MOCK_SYNC_STATUS_OBSERVER_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_LOCAL_MOCK_SYNC_STATUS_OBSERVER_H_

#include "chrome/browser/sync_file_system/local/local_file_sync_status.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace sync_file_system {

class MockSyncStatusObserver : public LocalFileSyncStatus::Observer {
 public:
  MockSyncStatusObserver();

  MockSyncStatusObserver(const MockSyncStatusObserver&) = delete;
  MockSyncStatusObserver& operator=(const MockSyncStatusObserver&) = delete;

  ~MockSyncStatusObserver() override;

  // LocalFileSyncStatus::Observer overrides.
  MOCK_METHOD1(OnSyncEnabled, void(const storage::FileSystemURL& url));
  MOCK_METHOD1(OnWriteEnabled, void(const storage::FileSystemURL& url));
};

}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_LOCAL_MOCK_SYNC_STATUS_OBSERVER_H_
