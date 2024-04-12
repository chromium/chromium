// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/webapk/webapk_restore_manager.h"

#include "base/types/pass_key.h"
#include "chrome/browser/android/webapk/webapk_restore_web_contents_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ssl/security_state_tab_helper.h"
#include "components/sync/protocol/web_app_specifics.pb.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"

namespace webapk {

WebApkRestoreManager::WebApkRestoreManager(Profile* profile)
    : profile_(profile),
      web_contents_manager_(
          std::make_unique<WebApkRestoreWebContentsManager>(profile)),
      sequenced_task_runner_(base::SequencedTaskRunner::GetCurrentDefault()) {}

WebApkRestoreManager::~WebApkRestoreManager() = default;

// static
WebApkRestoreManager::PassKey WebApkRestoreManager::PassKeyForTesting() {
  return PassKey();
}

void WebApkRestoreManager::ScheduleTask(
    const sync_pb::WebApkSpecifics& webapk_specifics) {
  tasks_.push_back(CreateNewTask(webapk_specifics));

  MaybeStartNextTask();
}

std::unique_ptr<WebApkRestoreTask> WebApkRestoreManager::CreateNewTask(
    const sync_pb::WebApkSpecifics& webapk_specifics) {
  return std::make_unique<WebApkRestoreTask>(PassKey(), profile_,
                                             webapk_specifics);
}

void WebApkRestoreManager::MaybeStartNextTask() {
  if (is_running_) {
    return;
  }

  if (tasks_.empty()) {
    // No tasks to run, clear the shared web contents and stop.
    web_contents_manager_->ClearSharedWebContents();
    return;
  }

  is_running_ = true;
  web_contents_manager_->EnsureWebContentsCreated(PassKey());
  tasks_.front()->Start(web_contents_manager_.get(),
                        base::BindOnce(&WebApkRestoreManager::OnTaskFinished,
                                       base::Unretained(this)));
}

void WebApkRestoreManager::OnTaskFinished(const GURL& manifest_id,
                                          webapps::WebApkInstallResult result) {
  tasks_.pop_front();
  is_running_ = false;

  sequenced_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&WebApkRestoreManager::MaybeStartNextTask,
                                weak_factory_.GetWeakPtr()));
}

}  // namespace webapk
