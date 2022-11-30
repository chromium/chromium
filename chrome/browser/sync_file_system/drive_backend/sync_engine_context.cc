// Copyright 2014 The Chromium Authors
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
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

SyncEngineContext::~SyncEngineContext() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

drive::DriveServiceInterface* SyncEngineContext::GetDriveService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return drive_service_.get();
}

drive::DriveUploaderInterface* SyncEngineContext::GetDriveUploader() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return drive_uploader_.get();
}

base::WeakPtr<TaskLogger> SyncEngineContext::GetTaskLogger() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return task_logger_;
}

MetadataDatabase* SyncEngineContext::GetMetadataDatabase() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return metadata_database_.get();
}

std::unique_ptr<MetadataDatabase> SyncEngineContext::PassMetadataDatabase() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return std::move(metadata_database_);
}

RemoteChangeProcessor* SyncEngineContext::GetRemoteChangeProcessor() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return remote_change_processor_;
}

base::SingleThreadTaskRunner* SyncEngineContext::GetUITaskRunner() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return ui_task_runner_.get();
}

base::SequencedTaskRunner* SyncEngineContext::GetWorkerTaskRunner() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return worker_task_runner_.get();
}

void SyncEngineContext::SetMetadataDatabase(
    std::unique_ptr<MetadataDatabase> metadata_database) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (metadata_database)
    metadata_database_ = std::move(metadata_database);
}

void SyncEngineContext::SetRemoteChangeProcessor(
    RemoteChangeProcessor* remote_change_processor) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(remote_change_processor);
  remote_change_processor_ = remote_change_processor;
}

void SyncEngineContext::DetachFromSequence() {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

}  // namespace drive_backend
}  // namespace sync_file_system
