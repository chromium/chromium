// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_backend/sync_engine_context.h"

#include <memory>
#include <utility>

#include "base/memory/ref_counted.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/sync_file_system/drive_backend/metadata_database.h"
#include "chrome/browser/sync_file_system/remote_change_processor.h"
#include "chrome/browser/sync_file_system/task_logger.h"
#include "components/drive/drive_uploader.h"
#include "components/drive/service/drive_service_interface.h"

namespace sync_file_system {
namespace drive_backend {

SyncEngineContext::SyncEngineContext(
    std::unique_ptr<drive::DriveServiceInterface> drive_service,
    std::unique_ptr<drive::DriveUploaderInterface> drive_uploader,
    TaskLogger* task_logger,
    const scoped_refptr<base::SingleThreadTaskRunner>& ui_task_runner,
    const scoped_refptr<base::SequencedTaskRunner>& worker_task_runner)
    : drive_service_(std::move(drive_service)),
      drive_uploader_(std::move(drive_uploader)),
      task_logger_(task_logger ? task_logger->AsWeakPtr()
                               : base::WeakPtr<TaskLogger>()),
      remote_change_processor_(nullptr),
      ui_task_runner_(ui_task_runner),
      worker_task_runner_(worker_task_runner) {
  sequence_checker_.DetachFromSequence();
}

SyncEngineContext::~SyncEngineContext() {
  DCHECK(sequence_checker_.CalledOnValidSequence());
}

drive::DriveServiceInterface* SyncEngineContext::GetDriveService() {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  return drive_service_.get();
}

drive::DriveUploaderInterface* SyncEngineContext::GetDriveUploader() {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  return drive_uploader_.get();
}

base::WeakPtr<TaskLogger> SyncEngineContext::GetTaskLogger() {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  return task_logger_;
}

MetadataDatabase* SyncEngineContext::GetMetadataDatabase() {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  return metadata_database_.get();
}

std::unique_ptr<MetadataDatabase> SyncEngineContext::PassMetadataDatabase() {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  return std::move(metadata_database_);
}

RemoteChangeProcessor* SyncEngineContext::GetRemoteChangeProcessor() {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  return remote_change_processor_;
}

base::SingleThreadTaskRunner* SyncEngineContext::GetUITaskRunner() {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  return ui_task_runner_.get();
}

base::SequencedTaskRunner* SyncEngineContext::GetWorkerTaskRunner() {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  return worker_task_runner_.get();
}

void SyncEngineContext::SetMetadataDatabase(
    std::unique_ptr<MetadataDatabase> metadata_database) {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  if (metadata_database)
    metadata_database_ = std::move(metadata_database);
}

void SyncEngineContext::SetRemoteChangeProcessor(
    RemoteChangeProcessor* remote_change_processor) {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  DCHECK(remote_change_processor);
  remote_change_processor_ = remote_change_processor;
}

void SyncEngineContext::DetachFromSequence() {
  sequence_checker_.DetachFromSequence();
}

}  // namespace drive_backend
}  // namespace sync_file_system
