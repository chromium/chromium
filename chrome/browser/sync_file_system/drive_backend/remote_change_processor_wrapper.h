// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_REMOTE_CHANGE_PROCESSOR_WRAPPER_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_REMOTE_CHANGE_PROCESSOR_WRAPPER_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "chrome/browser/sync_file_system/remote_change_processor.h"

namespace sync_file_system {
namespace drive_backend {

// This class wraps a part of RemoteChangeProcessor class to support weak
// pointer.  Each method wraps corresponding name method of
// RemoteChangeProcessor.  See comments in remote_change_processor.h
// for details.
class RemoteChangeProcessorWrapper
    : public base::SupportsWeakPtr<RemoteChangeProcessorWrapper> {
 public:
  explicit RemoteChangeProcessorWrapper(
      RemoteChangeProcessor* remote_change_processor);

  void PrepareForProcessRemoteChange(
      const storage::FileSystemURL& url,
      RemoteChangeProcessor::PrepareChangeCallback callback);

  void ApplyRemoteChange(const FileChange& change,
                         const base::FilePath& local_path,
                         const storage::FileSystemURL& url,
                         SyncStatusCallback callback);

  void FinalizeRemoteSync(const storage::FileSystemURL& url,
                          bool clear_local_changes,
                          base::OnceClosure completion_callback);

  void RecordFakeLocalChange(const storage::FileSystemURL& url,
                             const FileChange& change,
                             SyncStatusCallback callback);

 private:
  RemoteChangeProcessor* remote_change_processor_;
  base::SequenceChecker sequence_checker_;

  DISALLOW_COPY_AND_ASSIGN(RemoteChangeProcessorWrapper);
};

}  // namespace drive_backend
}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_REMOTE_CHANGE_PROCESSOR_WRAPPER_H_
