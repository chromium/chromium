// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_CHROME_SYNC_CLIENT_H__
#define CHROME_BROWSER_SYNC_CHROME_SYNC_CLIENT_H__

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "chrome/browser/sync/glue/extensions_activity_monitor.h"
#include "components/sync/driver/sync_client.h"
#include "components/sync/model/model_type_store_service.h"

class Profile;

namespace autofill {
class AutofillWebDataService;
}

namespace password_manager {
class PasswordStore;
}

namespace syncer {
class DeviceInfoTracker;
class SyncApiComponentFactory;
class SyncService;
}

namespace browser_sync {

class ChromeSyncClient : public syncer::SyncClient {
 public:
  explicit ChromeSyncClient(Profile* profile);
  ~ChromeSyncClient() override;

  void Initialize();

  // SyncClient implementation.
  syncer::SyncService* GetSyncService() override;
  PrefService* GetPrefService() override;
  base::FilePath GetLocalSyncBackendFolder() override;
  syncer::ModelTypeStoreService* GetModelTypeStoreService() override;
  bookmarks::BookmarkModel* GetBookmarkModel() override;
  favicon::FaviconService* GetFaviconService() override;
  history::HistoryService* GetHistoryService() override;
  sync_sessions::SessionSyncService* GetSessionSyncService() override;
  bool HasPasswordStore() override;
  base::Closure GetPasswordStateChangedCallback() override;
  syncer::DataTypeController::TypeVector CreateDataTypeControllers(
      syncer::LocalDeviceInfoProvider* local_device_info_provider) override;
  autofill::PersonalDataManager* GetPersonalDataManager() override;
  invalidation::InvalidationService* GetInvalidationService() override;
  BookmarkUndoService* GetBookmarkUndoServiceIfExists() override;
  scoped_refptr<syncer::ExtensionsActivity> GetExtensionsActivity() override;
  base::WeakPtr<syncer::SyncableService> GetSyncableServiceForType(
      syncer::ModelType type) override;
  base::WeakPtr<syncer::ModelTypeControllerDelegate>
  GetControllerDelegateForModelType(syncer::ModelType type) override;
  scoped_refptr<syncer::ModelSafeWorker> CreateModelWorkerForGroup(
      syncer::ModelSafeGroup group) override;
  syncer::SyncApiComponentFactory* GetSyncApiComponentFactory() override;

  // Helpers for overriding getters in tests.
  void SetSyncApiComponentFactoryForTesting(
      std::unique_ptr<syncer::SyncApiComponentFactory> component_factory);

  // Iterates over all of the profiles that have been loaded so far, and
  // extracts their tracker if present. If some profiles don't have trackers, no
  // indication is given in the passed vector.
  static void GetDeviceInfoTrackers(
      std::vector<const syncer::DeviceInfoTracker*>* trackers);

 private:
  Profile* const profile_;

  // The sync api component factory in use by this client.
  std::unique_ptr<syncer::SyncApiComponentFactory> component_factory_;

  // Members that must be fetched on the UI thread but accessed on their
  // respective backend threads.
  scoped_refptr<autofill::AutofillWebDataService> profile_web_data_service_;
  scoped_refptr<autofill::AutofillWebDataService> account_web_data_service_;
  scoped_refptr<password_manager::PasswordStore> password_store_;

  // The task runner for the |web_data_service_|, if any.
  scoped_refptr<base::SingleThreadTaskRunner> web_data_service_thread_;

  // Generates and monitors the ExtensionsActivity object used by sync.
  ExtensionsActivityMonitor extensions_activity_monitor_;

  DISALLOW_COPY_AND_ASSIGN(ChromeSyncClient);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_CHROME_SYNC_CLIENT_H__
