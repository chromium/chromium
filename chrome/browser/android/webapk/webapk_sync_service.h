// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_WEBAPK_WEBAPK_SYNC_SERVICE_H_
#define CHROME_BROWSER_ANDROID_WEBAPK_WEBAPK_SYNC_SERVICE_H_

#include "base/time/clock.h"
#include "chrome/browser/android/webapk/webapk_database_factory.h"
#include "chrome/browser/android/webapk/webapk_restore_manager.h"
#include "chrome/browser/android/webapk/webapk_sync_bridge.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/sync/protocol/web_apk_specifics.pb.h"

class Profile;

namespace webapk {

class WebApkSyncService : public KeyedService {
 public:
  static WebApkSyncService* GetForProfile(Profile* profile);

  explicit WebApkSyncService(Profile* profile);
  WebApkSyncService(const WebApkSyncService&) = delete;
  WebApkSyncService& operator=(const WebApkSyncService&) = delete;
  ~WebApkSyncService() override;

  void RegisterDoneInitializingCallback(
      base::OnceCallback<void(bool)> init_done_callback);
  void MergeSyncDataForTesting(std::vector<std::vector<std::string>> app_vector,
                               std::vector<int> last_used_days_vector);

  void SetClockForTesting(std::unique_ptr<base::Clock> clock);

  const Registry& GetRegistryForTesting() const;

  base::WeakPtr<syncer::ModelTypeControllerDelegate>
  GetModelTypeControllerDelegate();

  void OnWebApkUsed(std::unique_ptr<sync_pb::WebApkSpecifics> app_specifics,
                    bool is_install);
  void OnWebApkUninstalled(const std::string& manifest_id);
  void RemoveOldWebAPKsFromSync(int64_t current_time_ms_since_unix_epoch);

  std::vector<std::vector<std::string>> GetRestorableAppsInfo() const;

  void RestoreAppList(std::vector<std::string> app_ids_to_restore);

 private:
  std::unique_ptr<AbstractWebApkDatabaseFactory> database_factory_;
  std::unique_ptr<WebApkSyncBridge> sync_bridge_;
  std::unique_ptr<WebApkRestoreManager> restore_manager_;
};

}  // namespace webapk

#endif  // CHROME_BROWSER_ANDROID_WEBAPK_WEBAPK_SYNC_SERVICE_H_
