// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/sync/sync_appsync_service.h"

#include "chrome/browser/ash/sync/sync_appsync_optin_client.h"

namespace ash {

SyncAppsyncService::SyncAppsyncService(
    syncer::SyncService* sync_service,
    user_manager::UserManager* user_manager) {
  appsync_optin_client_ =
      std::make_unique<SyncAppsyncOptinClient>(sync_service, user_manager);
}

SyncAppsyncService::~SyncAppsyncService() = default;

void SyncAppsyncService::Shutdown() {
  appsync_optin_client_ = nullptr;
}

}  // namespace ash
