// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/mock_local_change_processor.h"

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/sync_file_system/file_change.h"
#include "chrome/browser/sync_file_system/sync_file_metadata.h"
#include "storage/browser/file_system/file_system_url.h"

using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;

namespace sync_file_system {

MockLocalChangeProcessor::MockLocalChangeProcessor() {
  ON_CALL(*this, ApplyLocalChange(_, _, _, _, _))
      .WillByDefault(Invoke(this,
                            &MockLocalChangeProcessor::ApplyLocalChangeStub));
}

MockLocalChangeProcessor::~MockLocalChangeProcessor() {
}

void MockLocalChangeProcessor::ApplyLocalChangeStub(
    const FileChange& change,
    const base::FilePath& local_file_path,
    const SyncFileMetadata& local_file_metadata,
    const storage::FileSystemURL& url,
    SyncStatusCallback callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), SYNC_STATUS_OK));
}

}  // namespace sync_file_system
