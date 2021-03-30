// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_backend/fake_sync_worker.h"

#include <utility>

#include "base/values.h"
#include "chrome/browser/sync_file_system/drive_backend/drive_backend_constants.h"
#include "chrome/browser/sync_file_system/drive_backend/sync_engine_context.h"
#include "chrome/browser/sync_file_system/sync_status_code.h"

namespace sync_file_system {
namespace drive_backend {

FakeSyncWorker::FakeSyncWorker()
    : sync_enabled_(true) {
  sequence_checker_.DetachFromSequence();
}

FakeSyncWorker::~FakeSyncWorker() {
  observers_.Clear();
}

void FakeSyncWorker::Initialize(
    std::unique_ptr<SyncEngineContext> sync_engine_context) {
  DCHECK(sequence_checker_.CalledOnValidSequence());

  sync_engine_context_ = std::move(sync_engine_context);
  status_map_.clear();
  // TODO(peria): Set |status_map_| as a fake metadata database.
}

void FakeSyncWorker::RegisterOrigin(const GURL& origin,
                                    SyncStatusCallback callback) {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  // TODO(peria): Check how it should act on installing installed app?
  status_map_[origin] = REGISTERED;
  std::move(callback).Run(SYNC_STATUS_OK);
}

void FakeSyncWorker::EnableOrigin(const GURL& origin,
                                  SyncStatusCallback callback) {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  // TODO(peria): Check how it should act on enabling non-installed app?
  status_map_[origin] = ENABLED;
  std::move(callback).Run(SYNC_STATUS_OK);
}

void FakeSyncWorker::DisableOrigin(const GURL& origin,
                                   SyncStatusCallback callback) {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  // TODO(peria): Check how it should act on disabling non-installed app?
  status_map_[origin] = DISABLED;
  std::move(callback).Run(SYNC_STATUS_OK);
}

void FakeSyncWorker::UninstallOrigin(const GURL& origin,
                                     RemoteFileSyncService::UninstallFlag flag,
                                     SyncStatusCallback callback) {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  // TODO(peria): Check how it should act on uninstalling non-installed app?
  status_map_[origin] = UNINSTALLED;
  std::move(callback).Run(SYNC_STATUS_OK);
}

void FakeSyncWorker::ProcessRemoteChange(SyncFileCallback callback) {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  std::move(callback).Run(SYNC_STATUS_OK, storage::FileSystemURL());
}

void FakeSyncWorker::SetRemoteChangeProcessor(
    RemoteChangeProcessorOnWorker* remote_change_processor_on_worker) {
  DCHECK(sequence_checker_.CalledOnValidSequence());
}

RemoteServiceState FakeSyncWorker::GetCurrentState() const {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  return REMOTE_SERVICE_OK;
}

void FakeSyncWorker::GetOriginStatusMap(
    RemoteFileSyncService::StatusMapCallback callback) {
  DCHECK(sequence_checker_.CalledOnValidSequence());

  std::unique_ptr<RemoteFileSyncService::OriginStatusMap> status_map(
      new RemoteFileSyncService::OriginStatusMap);
  for (StatusMap::const_iterator itr = status_map_.begin();
       itr != status_map_.end(); ++itr) {
    switch (itr->second) {
    case REGISTERED:
      (*status_map)[itr->first] = "Registered";
      break;
    case ENABLED:
      (*status_map)[itr->first] = "Enabled";
      break;
    case DISABLED:
      (*status_map)[itr->first] = "Disabled";
      break;
    case UNINSTALLED:
      (*status_map)[itr->first] = "Uninstalled";
      break;
    default:
      (*status_map)[itr->first] = "Unknown";
      break;
    }
  }
  std::move(callback).Run(std::move(status_map));
}

std::unique_ptr<base::ListValue> FakeSyncWorker::DumpFiles(const GURL& origin) {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  return nullptr;
}

std::unique_ptr<base::ListValue> FakeSyncWorker::DumpDatabase() {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  return nullptr;
}

void FakeSyncWorker::SetSyncEnabled(bool enabled) {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  sync_enabled_ = enabled;

  if (enabled)
    UpdateServiceState(REMOTE_SERVICE_OK, "Set FakeSyncWorker enabled.");
  else
    UpdateServiceState(REMOTE_SERVICE_DISABLED, "Disabled FakeSyncWorker.");
}

void FakeSyncWorker::PromoteDemotedChanges(base::OnceClosure callback) {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  for (auto& observer : observers_)
    observer.OnPendingFileListUpdated(10);
  std::move(callback).Run();
}

void FakeSyncWorker::ApplyLocalChange(const FileChange& local_change,
                                      const base::FilePath& local_path,
                                      const SyncFileMetadata& local_metadata,
                                      const storage::FileSystemURL& url,
                                      SyncStatusCallback callback) {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  std::move(callback).Run(SYNC_STATUS_OK);
}

void FakeSyncWorker::ActivateService(RemoteServiceState service_state,
                                     const std::string& description) {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  UpdateServiceState(service_state, description);
}

void FakeSyncWorker::DeactivateService(const std::string& description) {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  UpdateServiceState(REMOTE_SERVICE_TEMPORARY_UNAVAILABLE, description);
}

void FakeSyncWorker::DetachFromSequence() {
  sequence_checker_.DetachFromSequence();
}

void FakeSyncWorker::AddObserver(Observer* observer) {
  // This method is called on UI thread.
  observers_.AddObserver(observer);
}

void FakeSyncWorker::UpdateServiceState(RemoteServiceState state,
                                        const std::string& description) {
  DCHECK(sequence_checker_.CalledOnValidSequence());

  for (auto& observer : observers_)
    observer.UpdateServiceState(state, description);
}

}  // namespace drive_backend
}  // namespace sync_file_system
