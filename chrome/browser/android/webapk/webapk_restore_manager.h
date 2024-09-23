// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_WEBAPK_WEBAPK_RESTORE_MANAGER_H_
#define CHROME_BROWSER_ANDROID_WEBAPK_WEBAPK_RESTORE_MANAGER_H_

#include <deque>

#include "base/android/scoped_java_ref.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/types/pass_key.h"
#include "chrome/browser/android/webapk/webapk_restore_task.h"
#include "chrome/browser/android/webapk/webapk_restore_web_contents_manager.h"
#include "components/webapps/common/web_app_id.h"

class WebApkInstallService;

namespace webapps {
struct ShortcutInfo;
enum class WebApkInstallResult;
}

namespace webapk {

// This class is responsible for managing tasks related to restore WebAPKs
// (install previously synced WebAPK on new devices).
class WebApkRestoreManager {
 public:
  using PassKey = base::PassKey<WebApkRestoreManager>;
  static PassKey PassKeyForTesting();

  WebApkRestoreManager(
      WebApkInstallService* web_apk_install_service,
      std::unique_ptr<WebApkRestoreWebContentsManager> web_contents_manager);
  WebApkRestoreManager(const WebApkRestoreManager&) = delete;
  WebApkRestoreManager& operator=(const WebApkRestoreManager&) = delete;
  virtual ~WebApkRestoreManager();

  using RestorableAppsCallback =
      base::OnceCallback<void(const std::vector<std::string>& app_ids,
                              const std::vector<std::u16string>& names,
                              const std::vector<int>& last_used_in_days,
                              const std::vector<SkBitmap>& icons)>;

  void PrepareRestorableApps(std::vector<WebApkRestoreData>&& apps,
                             RestorableAppsCallback result_callback);
  void ScheduleRestoreTasks(
      const std::vector<webapps::AppId>& app_ids_to_restore);

  // If no restore task is running, clear all tasks and reset states.
  void ResetIfNotRunning();

  base::WeakPtr<WebApkRestoreManager> GetWeakPtr();

  uint32_t GetTasksCountForTesting() const { return tasks_.size(); }

 protected:
  virtual std::unique_ptr<WebApkRestoreTask> CreateNewTask(
      std::unique_ptr<webapps::ShortcutInfo> restore_info,
      base::Time last_used_time);
  virtual void OnTaskFinished(const GURL& manifest_id,
                              webapps::WebApkInstallResult result);

  WebApkRestoreWebContentsManager* web_contents_manager() const {
    return web_contents_manager_.get();
  }

 private:
  void MaybeStartNextTask();

  void DownloadIcon(RestorableAppsCallback apps_info_callback,
                    webapps::WebAppUrlLoaderResult result);
  void OnAllIconsDownloaded(RestorableAppsCallback result_callback);

  raw_ptr<WebApkInstallService> web_apk_install_service_;
  std::unique_ptr<WebApkRestoreWebContentsManager> web_contents_manager_;

  // All restorable WebAPKs
  std::map<webapps::AppId, std::unique_ptr<WebApkRestoreTask>>
      restorable_tasks_;

  // The list of AppId for WebAPKs to be restored.
  std::deque<webapps::AppId> tasks_;
  bool is_running_ = false;

  scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner_;

  base::WeakPtrFactory<WebApkRestoreManager> weak_factory_{this};
};

}  // namespace webapk

#endif  // CHROME_BROWSER_ANDROID_WEBAPK_WEBAPK_RESTORE_MANAGER_H_
