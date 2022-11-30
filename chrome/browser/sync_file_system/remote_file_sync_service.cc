// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/remote_file_sync_service.h"

#include "chrome/browser/sync_file_system/drive_backend/sync_engine.h"

namespace sync_file_system {

std::unique_ptr<RemoteFileSyncService>
RemoteFileSyncService::CreateForBrowserContext(content::BrowserContext* context,
                                               TaskLogger* task_logger) {
  return drive_backend::SyncEngine::CreateForBrowserContext(context,
                                                            task_logger);
}

void RemoteFileSyncService::AppendDependsOnFactories(
    std::set<BrowserContextKeyedServiceFactory*>* factories) {
  drive_backend::SyncEngine::AppendDependsOnFactories(factories);
}

}  // namespace sync_file_system
