// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/webapk/webapk_restore_manager.h"

#include "base/strings/utf_string_conversions.h"
#include "base/types/pass_key.h"
#include "chrome/browser/android/webapk/webapk_restore_web_contents_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ssl/security_state_tab_helper.h"
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

void WebApkRestoreManager::PrepareRestorableApps(
    std::map<webapps::AppId, std::unique_ptr<webapps::ShortcutInfo>> apps,
    base::OnceCallback<void(std::vector<std::vector<std::string>>)>
        result_callback) {
  std::vector<std::vector<std::string>> results;
  for (auto&& [appId, shortcut_info] : apps) {
    results.push_back({appId, base::UTF16ToUTF8(shortcut_info->name)});
    restorable_tasks_.emplace(std::move(appId),
                              CreateNewTask(std::move(shortcut_info)));
  }
  std::move(result_callback).Run(results);
}

void WebApkRestoreManager::ScheduleRestoreTasks(
    const std::vector<webapps::AppId>& app_ids_to_restore) {
  for (auto appId : app_ids_to_restore) {
    tasks_.push_back(appId);
  }

  MaybeStartNextTask();
}

std::unique_ptr<WebApkRestoreTask> WebApkRestoreManager::CreateNewTask(
    std::unique_ptr<webapps::ShortcutInfo> shortcut_info) {
  return std::make_unique<WebApkRestoreTask>(PassKey(), profile_,
                                             std::move(shortcut_info));
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
  restorable_tasks_.at(tasks_.front())
      ->Start(web_contents_manager_.get(),
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
