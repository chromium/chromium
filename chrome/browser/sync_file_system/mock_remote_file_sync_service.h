// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_MOCK_REMOTE_FILE_SYNC_SERVICE_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_MOCK_REMOTE_FILE_SYNC_SERVICE_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/observer_list.h"
#include "chrome/browser/sync_file_system/file_status_observer.h"
#include "chrome/browser/sync_file_system/mock_local_change_processor.h"
#include "chrome/browser/sync_file_system/remote_change_processor.h"
#include "chrome/browser/sync_file_system/remote_file_sync_service.h"
#include "chrome/browser/sync_file_system/sync_callbacks.h"
#include "chrome/browser/sync_file_system/sync_direction.h"
#include "chrome/browser/sync_file_system/sync_file_metadata.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"

namespace sync_file_system {

class MockRemoteFileSyncService : public RemoteFileSyncService {
 public:
  MockRemoteFileSyncService();

  MockRemoteFileSyncService(const MockRemoteFileSyncService&) = delete;
  MockRemoteFileSyncService& operator=(const MockRemoteFileSyncService&) =
      delete;

  ~MockRemoteFileSyncService() override;

  // RemoteFileSyncService overrides.
  MOCK_METHOD1(AddServiceObserver,
               void(RemoteFileSyncService::Observer* observer));
  MOCK_METHOD1(AddFileStatusObserver,
               void(FileStatusObserver* observer));
  MOCK_METHOD2(RegisterOrigin,
               void(const GURL& origin, SyncStatusCallback callback));
  MOCK_METHOD2(EnableOrigin,
               void(const GURL& origin, SyncStatusCallback callback));
  MOCK_METHOD2(DisableOrigin,
               void(const GURL& origin, SyncStatusCallback callback));
  MOCK_METHOD3(UninstallOrigin,
               void(const GURL& origin,
                    UninstallFlag flag,
                    SyncStatusCallback callback));
  MOCK_METHOD1(ProcessRemoteChange, void(SyncFileCallback callback));
  MOCK_METHOD1(SetRemoteChangeProcessor,
               void(RemoteChangeProcessor* processor));
  MOCK_METHOD0(GetLocalChangeProcessor, LocalChangeProcessor*());
  MOCK_CONST_METHOD0(GetCurrentState,
                     RemoteServiceState());
  MOCK_METHOD1(GetOriginStatusMap, void(StatusMapCallback callback));
  MOCK_METHOD1(SetSyncEnabled, void(bool enabled));
  MOCK_METHOD1(PromoteDemotedChanges, void(base::OnceClosure callback));

  void DumpFiles(const GURL& origin, ListCallback callback) override;
  void DumpDatabase(ListCallback callback) override;

  void SetServiceState(RemoteServiceState state);

  // Send notifications to the observers.
  // Can be used in the mock implementation.
  void NotifyRemoteChangeQueueUpdated(int64_t pending_changes);
  void NotifyRemoteServiceStateUpdated(
      RemoteServiceState state,
      const std::string& description);
  void NotifyFileStatusChanged(const storage::FileSystemURL& url,
                               SyncFileType file_type,
                               SyncFileStatus sync_status,
                               SyncAction action_taken,
                               SyncDirection direction);

 private:
  void AddServiceObserverStub(Observer* observer);
  void AddFileStatusObserverStub(FileStatusObserver* observer);
  void RegisterOriginStub(const GURL& origin, SyncStatusCallback callback);
  void DeleteOriginDirectoryStub(const GURL& origin,
                                 UninstallFlag flag,
                                 SyncStatusCallback callback);
  void ProcessRemoteChangeStub(SyncFileCallback callback);
  RemoteServiceState GetCurrentStateStub() const;

  // For default implementation.
  ::testing::NiceMock<MockLocalChangeProcessor> mock_local_change_processor_;

  base::ObserverList<Observer>::UncheckedAndDanglingUntriaged
      service_observers_;
  base::ObserverList<FileStatusObserver>::UncheckedAndDanglingUntriaged
      file_status_observers_;

  RemoteServiceState state_;
};

}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_MOCK_REMOTE_FILE_SYNC_SERVICE_H_
