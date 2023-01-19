// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYNC_SYNC_APPSYNC_SERVICE_H_
#define CHROME_BROWSER_ASH_SYNC_SYNC_APPSYNC_SERVICE_H_

#include <memory>

#include "components/keyed_service/core/keyed_service.h"

namespace syncer {
class SyncService;
}  // namespace syncer

namespace user_manager {
class UserManager;
}  // namespace user_manager

namespace ash {

class SyncAppsyncOptinClient;

/**
 * SyncAppsyncService instatiates a client that listens for changes to that
 * profile's opt-in to AppsSync, and propagates those changes to a file in the
 * user's daemon-store.
 */
class SyncAppsyncService : public KeyedService {
 public:
  // |sync_service| must not be null. |this| should depend on |sync_service| and
  // be shut down before it.
  explicit SyncAppsyncService(syncer::SyncService* sync_service,
                              user_manager::UserManager* user_manager);
  SyncAppsyncService(const SyncAppsyncService& other) = delete;
  SyncAppsyncService& operator=(const SyncAppsyncService& other) = delete;
  ~SyncAppsyncService() override;

  // KeyedService
  void Shutdown() override;

 private:
  // Members below destroyed after Shutdown().
  std::unique_ptr<SyncAppsyncOptinClient> appsync_optin_client_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SYNC_SYNC_APPSYNC_SERVICE_H_
