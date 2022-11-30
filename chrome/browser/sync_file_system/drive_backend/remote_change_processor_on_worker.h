// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_REMOTE_CHANGE_PROCESSOR_ON_WORKER_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_REMOTE_CHANGE_PROCESSOR_ON_WORKER_H_

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "chrome/browser/sync_file_system/remote_change_processor.h"

namespace base {
class SingleThreadTaskRunner;
class SequencedTaskRunner;
}

namespace sync_file_system {
namespace drive_backend {

class RemoteChangeProcessorWrapper;

// This class wraps a part of RemoteChangeProcessor class to post actual
// tasks to RemoteChangeProcessorWrapper which lives in another thread.
// Each method wraps corresponding name method of RemoteChangeProcessor.
// See comments in remote_change_processor.h for details.
class RemoteChangeProcessorOnWorker : public RemoteChangeProcessor {
 public:
  RemoteChangeProcessorOnWorker(
      const base::WeakPtr<RemoteChangeProcessorWrapper>& wrapper,
      base::SingleThreadTaskRunner* ui_task_runner,
      base::SequencedTaskRunner* worker_task_runner);

  RemoteChangeProcessorOnWorker(const RemoteChangeProcessorOnWorker&) = delete;
  RemoteChangeProcessorOnWorker& operator=(
      const RemoteChangeProcessorOnWorker&) = delete;

  ~RemoteChangeProcessorOnWorker() override;

  void PrepareForProcessRemoteChange(const storage::FileSystemURL& url,
                                     PrepareChangeCallback callback) override;
  void ApplyRemoteChange(const FileChange& change,
                         const base::FilePath& local_path,
                         const storage::FileSystemURL& url,
                         SyncStatusCallback callback) override;
  void FinalizeRemoteSync(const storage::FileSystemURL& url,
                          bool clear_local_changes,
                          base::OnceClosure completion_callback) override;
  void RecordFakeLocalChange(const storage::FileSystemURL& url,
                             const FileChange& change,
                             SyncStatusCallback callback) override;

  void DetachFromSequence();

 private:
  base::WeakPtr<RemoteChangeProcessorWrapper> wrapper_;
  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner_;
  scoped_refptr<base::SequencedTaskRunner> worker_task_runner_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace drive_backend
}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_REMOTE_CHANGE_PROCESSOR_ON_WORKER_H_
