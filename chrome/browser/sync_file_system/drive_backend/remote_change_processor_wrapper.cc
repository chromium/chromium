// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_backend/remote_change_processor_wrapper.h"

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/sync_file_system/remote_change_processor.h"

namespace sync_file_system {
namespace drive_backend {

RemoteChangeProcessorWrapper::RemoteChangeProcessorWrapper(
    RemoteChangeProcessor* remote_change_processor)
    : remote_change_processor_(remote_change_processor) {}

RemoteChangeProcessorWrapper::~RemoteChangeProcessorWrapper() = default;

void RemoteChangeProcessorWrapper::PrepareForProcessRemoteChange(
    const storage::FileSystemURL& url,
    RemoteChangeProcessor::PrepareChangeCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  remote_change_processor_->PrepareForProcessRemoteChange(url,
                                                          std::move(callback));
}

void RemoteChangeProcessorWrapper::ApplyRemoteChange(
    const FileChange& change,
    const base::FilePath& local_path,
    const storage::FileSystemURL& url,
    SyncStatusCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  remote_change_processor_->ApplyRemoteChange(change, local_path, url,
                                              std::move(callback));
}

void RemoteChangeProcessorWrapper::FinalizeRemoteSync(
    const storage::FileSystemURL& url,
    bool clear_local_changes,
    base::OnceClosure completion_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  remote_change_processor_->FinalizeRemoteSync(url, clear_local_changes,
                                               std::move(completion_callback));
}

void RemoteChangeProcessorWrapper::RecordFakeLocalChange(
    const storage::FileSystemURL& url,
    const FileChange& change,
    SyncStatusCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  remote_change_processor_->RecordFakeLocalChange(url, change,
                                                  std::move(callback));
}

}  // namespace drive_backend
}  // namespace sync_file_system
