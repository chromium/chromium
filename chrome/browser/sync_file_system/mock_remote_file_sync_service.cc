// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/mock_remote_file_sync_service.h"

#include <string>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/task/single_thread_task_runner.h"
#include "base/values.h"
#include "storage/browser/file_system/file_system_url.h"
#include "url/gurl.h"

using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;

namespace sync_file_system {

MockRemoteFileSyncService::MockRemoteFileSyncService()
    : state_(REMOTE_SERVICE_OK) {
  typedef MockRemoteFileSyncService self;
  ON_CALL(*this, AddServiceObserver(_))
      .WillByDefault(Invoke(this, &self::AddServiceObserverStub));
  ON_CALL(*this, AddFileStatusObserver(_))
      .WillByDefault(Invoke(this, &self::AddFileStatusObserverStub));
  ON_CALL(*this, RegisterOrigin(_, _))
      .WillByDefault(Invoke(this, &self::RegisterOriginStub));
  ON_CALL(*this, UninstallOrigin(_, _, _))
      .WillByDefault(
          Invoke(this, &self::DeleteOriginDirectoryStub));
  ON_CALL(*this, ProcessRemoteChange(_))
      .WillByDefault(Invoke(this, &self::ProcessRemoteChangeStub));
  ON_CALL(*this, GetLocalChangeProcessor())
      .WillByDefault(Return(&mock_local_change_processor_));
  ON_CALL(*this, GetCurrentState())
      .WillByDefault(Invoke(this, &self::GetCurrentStateStub));
}

MockRemoteFileSyncService::~MockRemoteFileSyncService() {
}

void MockRemoteFileSyncService::DumpFiles(const GURL& origin,
                                          ListCallback callback) {
  std::move(callback).Run(base::Value::List());
}

void MockRemoteFileSyncService::DumpDatabase(ListCallback callback) {
  std::move(callback).Run(base::Value::List());
}

void MockRemoteFileSyncService::SetServiceState(RemoteServiceState state) {
  state_ = state;
}

void MockRemoteFileSyncService::NotifyRemoteChangeQueueUpdated(
    int64_t pending_changes) {
  for (auto& observer : service_observers_)
    observer.OnRemoteChangeQueueUpdated(pending_changes);
}

void MockRemoteFileSyncService::NotifyRemoteServiceStateUpdated(
    RemoteServiceState state,
    const std::string& description) {
  for (auto& observer : service_observers_)
    observer.OnRemoteServiceStateUpdated(state, description);
}

void MockRemoteFileSyncService::NotifyFileStatusChanged(
    const storage::FileSystemURL& url,
    SyncFileType file_type,
    SyncFileStatus sync_status,
    SyncAction action_taken,
    SyncDirection direction) {
  for (auto& observer : file_status_observers_) {
    observer.OnFileStatusChanged(url, file_type, sync_status, action_taken,
                                 direction);
  }
}

void MockRemoteFileSyncService::AddServiceObserverStub(Observer* observer) {
  service_observers_.AddObserver(observer);
}

void MockRemoteFileSyncService::AddFileStatusObserverStub(
    FileStatusObserver* observer) {
  file_status_observers_.AddObserver(observer);
}

void MockRemoteFileSyncService::RegisterOriginStub(
    const GURL& origin,
    SyncStatusCallback callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), SYNC_STATUS_OK));
}

void MockRemoteFileSyncService::DeleteOriginDirectoryStub(
    const GURL& origin,
    UninstallFlag flag,
    SyncStatusCallback callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), SYNC_STATUS_OK));
}

void MockRemoteFileSyncService::ProcessRemoteChangeStub(
    SyncFileCallback callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), SYNC_STATUS_NO_CHANGE_TO_SYNC,
                     storage::FileSystemURL()));
}

RemoteServiceState MockRemoteFileSyncService::GetCurrentStateStub() const {
  return state_;
}

}  // namespace sync_file_system
