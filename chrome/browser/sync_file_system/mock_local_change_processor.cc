// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/mock_local_change_processor.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
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
    const SyncStatusCallback& callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(callback, SYNC_STATUS_OK));
}

}  // namespace sync_file_system
