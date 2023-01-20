// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYNC_SYNC_APPSYNC_OPTIN_CLIENT_H_
#define CHROME_BROWSER_ASH_SYNC_SYNC_APPSYNC_OPTIN_CLIENT_H_

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "components/sync/driver/sync_service_observer.h"

namespace syncer {
class SyncService;
}  // namespace syncer

namespace user_manager {
class UserManager;
}  // namespace user_manager

namespace ash {

/**
 * SyncAppsyncOptinClient listens for changes to a profile's opt-in to AppsSync,
 * and propagates those changes to a file in the user's daemon-store.
 */
class SyncAppsyncOptinClient : public syncer::SyncServiceObserver {
 public:
  // |sync_service| must not be null. |this| must be destroyed before
  // |sync_service| shutdown.
  // |user_manager| must not be null. |this| must be destroyed before
  // |user_manager| shutdown.
  explicit SyncAppsyncOptinClient(syncer::SyncService* sync_service,
                                  user_manager::UserManager* user_manager);
  // If the daemon-store location needs to be specified (e.g. for test)
  // this must be provided at instantiation time, since the constructor
  // will attempt to write out the opt-in file.
  explicit SyncAppsyncOptinClient(syncer::SyncService* sync_service,
                                  user_manager::UserManager* user_manager,
                                  const base::FilePath& daemon_store_location);
  SyncAppsyncOptinClient(const SyncAppsyncOptinClient& other) = delete;
  SyncAppsyncOptinClient& operator=(const SyncAppsyncOptinClient& other) =
      delete;
  ~SyncAppsyncOptinClient() override;

  // syncer::SyncServiceObserver
  void OnStateChanged(syncer::SyncService* sync_service) override;

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class AppsSyncOptinFileWrite {
    kAttempt = 0,
    kFailure = 1,
    kMaxValue = kFailure,
  };

 private:
  // Issues a write to the opt-in file, to reflect Profile state.
  void UpdateOptinFile(bool opted_in, const syncer::SyncService* sync_service);

  const base::raw_ptr<syncer::SyncService> sync_service_;
  const base::raw_ptr<user_manager::UserManager> user_manager_;

  bool is_apps_sync_enabled_;

  // Location of daemon-store - can be changed for testing.
  base::FilePath daemon_store_filepath_;
};
}  // namespace ash

#endif  //  CHROME_BROWSER_ASH_SYNC_SYNC_APPSYNC_OPTIN_CLIENT_H_
