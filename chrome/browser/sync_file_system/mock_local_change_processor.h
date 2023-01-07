// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_MOCK_LOCAL_CHANGE_PROCESSOR_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_MOCK_LOCAL_CHANGE_PROCESSOR_H_

#include "base/functional/callback.h"
#include "chrome/browser/sync_file_system/local_change_processor.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace sync_file_system {

class MockLocalChangeProcessor : public LocalChangeProcessor {
 public:
  MockLocalChangeProcessor();

  MockLocalChangeProcessor(const MockLocalChangeProcessor&) = delete;
  MockLocalChangeProcessor& operator=(const MockLocalChangeProcessor&) = delete;

  ~MockLocalChangeProcessor() override;

  // LocalChangeProcessor override.
  MOCK_METHOD5(ApplyLocalChange,
               void(const FileChange& change,
                    const base::FilePath& local_file_path,
                    const SyncFileMetadata& local_file_metadata,
                    const storage::FileSystemURL& url,
                    SyncStatusCallback callback));

 private:
  void ApplyLocalChangeStub(const FileChange& change,
                            const base::FilePath& local_file_path,
                            const SyncFileMetadata& local_file_metadata,
                            const storage::FileSystemURL& url,
                            SyncStatusCallback callback);
};

}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_MOCK_LOCAL_CHANGE_PROCESSOR_H_
