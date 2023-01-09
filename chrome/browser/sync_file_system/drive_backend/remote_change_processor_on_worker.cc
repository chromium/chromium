// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_backend/remote_change_processor_on_worker.h"

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/sync_file_system/drive_backend/callback_helper.h"
#include "chrome/browser/sync_file_system/drive_backend/remote_change_processor_wrapper.h"
#include "chrome/browser/sync_file_system/file_change.h"
#include "chrome/browser/sync_file_system/sync_file_metadata.h"

namespace sync_file_system {
namespace drive_backend {

RemoteChangeProcessorOnWorker::RemoteChangeProcessorOnWorker(
      const base::WeakPtr<RemoteChangeProcessorWrapper>& wrapper,
      base::SingleThreadTaskRunner* ui_task_runner,
      base::SequencedTaskRunner* worker_task_runner)
    : wrapper_(wrapper),
      ui_task_runner_(ui_task_runner),
      worker_task_runner_(worker_task_runner) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

RemoteChangeProcessorOnWorker::~RemoteChangeProcessorOnWorker() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void RemoteChangeProcessorOnWorker::PrepareForProcessRemoteChange(
    const storage::FileSystemURL& url,
    PrepareChangeCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  ui_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &RemoteChangeProcessorWrapper::PrepareForProcessRemoteChange,
          wrapper_, url,
          RelayCallbackToTaskRunner(worker_task_runner_.get(), FROM_HERE,
                                    std::move(callback))));
}

void RemoteChangeProcessorOnWorker::ApplyRemoteChange(
    const FileChange& change,
    const base::FilePath& local_path,
    const storage::FileSystemURL& url,
    SyncStatusCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  ui_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &RemoteChangeProcessorWrapper::ApplyRemoteChange, wrapper_, change,
          local_path, url,
          RelayCallbackToTaskRunner(worker_task_runner_.get(), FROM_HERE,
                                    std::move(callback))));
}

void RemoteChangeProcessorOnWorker::FinalizeRemoteSync(
    const storage::FileSystemURL& url,
    bool clear_local_changes,
    base::OnceClosure completion_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  ui_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &RemoteChangeProcessorWrapper::FinalizeRemoteSync, wrapper_, url,
          clear_local_changes,
          RelayCallbackToTaskRunner(worker_task_runner_.get(), FROM_HERE,
                                    std::move(completion_callback))));
}

void RemoteChangeProcessorOnWorker::RecordFakeLocalChange(
    const storage::FileSystemURL& url,
    const FileChange& change,
    SyncStatusCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  ui_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &RemoteChangeProcessorWrapper::RecordFakeLocalChange, wrapper_, url,
          change,
          RelayCallbackToTaskRunner(worker_task_runner_.get(), FROM_HERE,
                                    std::move(callback))));
}

void RemoteChangeProcessorOnWorker::DetachFromSequence() {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

}  // namespace drive_backend
}  // namespace sync_file_system
