// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_WEBAPK_WEBAPK_RESTORE_MANAGER_H_
#define CHROME_BROWSER_ANDROID_WEBAPK_WEBAPK_RESTORE_MANAGER_H_

#include <deque>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/types/pass_key.h"
#include "chrome/browser/android/webapk/webapk_restore_task.h"
#include "components/sync/protocol/web_apk_specifics.pb.h"

class Profile;

namespace webapps {
enum class WebApkInstallResult;
}

namespace webapk {

// This class is responsible for managing tasks related to restore WebAPKs
// (install previously synced WebAPK on new devices).
class WebApkRestoreManager {
 public:
  using PassKey = base::PassKey<WebApkRestoreManager>;
  static PassKey PassKeyForTesting();

  explicit WebApkRestoreManager(Profile* profile);
  WebApkRestoreManager(const WebApkRestoreManager&) = delete;
  WebApkRestoreManager& operator=(const WebApkRestoreManager&) = delete;
  virtual ~WebApkRestoreManager();

  void ScheduleTask(const sync_pb::WebApkSpecifics& webapk_specifics);

  uint32_t GetTasksCountForTesting() const { return tasks_.size(); }

 protected:
  virtual std::unique_ptr<WebApkRestoreTask> CreateNewTask(
      const sync_pb::WebApkSpecifics& webapk_specifics);
  virtual void OnTaskFinished(const GURL& manifest_id,
                              webapps::WebApkInstallResult result);

  Profile* profile() const { return profile_; }

 private:
  void MaybeStartNextTask();

  raw_ptr<Profile> profile_;
  std::unique_ptr<WebApkRestoreWebContentsManager> web_contents_manager_;

  // The list of webapk infos to be restored.
  std::deque<std::unique_ptr<WebApkRestoreTask>> tasks_;
  bool is_running_ = false;

  scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner_;

  base::WeakPtrFactory<WebApkRestoreManager> weak_factory_{this};
};

}  // namespace webapk

#endif  // CHROME_BROWSER_ANDROID_WEBAPK_WEBAPK_RESTORE_MANAGER_H_
