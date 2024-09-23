// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/webapk/webapk_sync_service.h"

#include "base/functional/callback_helpers.h"

namespace webapk {

WebApkSyncService::WebApkSyncService(
    syncer::DataTypeStoreService* data_type_store_service,
    std::unique_ptr<WebApkRestoreManager> restore_manager) {
  sync_bridge_ = std::make_unique<WebApkSyncBridge>(data_type_store_service,
                                                    base::DoNothing());
  restore_manager_ = std::move(restore_manager);
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

void WebApkSyncService::SetClockForTesting(std::unique_ptr<base::Clock> clock) {
  sync_bridge_->SetClockForTesting(std::move(clock));  // IN-TEST
}

const Registry& WebApkSyncService::GetRegistryForTesting() const {
  return sync_bridge_->GetRegistryForTesting();  // IN-TEST
}

base::WeakPtr<syncer::DataTypeControllerDelegate>
WebApkSyncService::GetDataTypeControllerDelegate() {
  return sync_bridge_->GetDataTypeControllerDelegate();
}

void WebApkSyncService::OnWebApkUsed(
    std::unique_ptr<sync_pb::WebApkSpecifics> app_specifics,
    bool is_install) {
  sync_bridge_->OnWebApkUsed(std::move(app_specifics), is_install);
}

void WebApkSyncService::OnWebApkUninstalled(const std::string& manifest_id) {
  sync_bridge_->OnWebApkUninstalled(manifest_id);
}

void WebApkSyncService::RemoveOldWebAPKsFromSync(
    int64_t current_time_ms_since_unix_epoch) {
  sync_bridge_->RemoveOldWebAPKsFromSync(current_time_ms_since_unix_epoch);
}

void WebApkSyncService::PrepareRestorableAppsInfo(
    WebApkRestoreManager::RestorableAppsCallback result_callback) const {
  restore_manager_->PrepareRestorableApps(
      sync_bridge_->GetRestorableAppsShortcutInfo(),
      std::move(result_callback));
}

WebApkRestoreManager* WebApkSyncService::GetWebApkRestoreManager() const {
  return restore_manager_.get();
}

}  // namespace webapk
