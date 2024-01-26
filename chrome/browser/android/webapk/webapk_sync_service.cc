// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/webapk/webapk_sync_service.h"

#include "base/functional/callback_helpers.h"
#include "chrome/browser/android/webapk/webapk_sync_service_factory.h"

namespace webapk {

// static
WebApkSyncService* WebApkSyncService::GetForProfile(Profile* profile) {
  return WebApkSyncServiceFactory::GetForProfile(profile);
}

WebApkSyncService::WebApkSyncService(Profile* profile) {
  database_factory_ = std::make_unique<WebApkDatabaseFactory>(profile);
  sync_bridge_ = std::make_unique<WebApkSyncBridge>(database_factory_.get(),
                                                    base::DoNothing());
}

WebApkSyncService::~WebApkSyncService() = default;

void WebApkSyncService::RegisterDoneInitializingCallback(
    base::OnceCallback<void(bool)> init_done_callback) {
  sync_bridge_->RegisterDoneInitializingCallback(std::move(init_done_callback));
}

void WebApkSyncService::MergeSyncDataForTesting(
    std::vector<std::vector<std::string>> app_vector,
    std::vector<int> last_used_days_vector) {
  sync_bridge_->MergeSyncDataForTesting(
      std::move(app_vector), std::move(last_used_days_vector));  // IN-TEST
}

}  // namespace webapk
