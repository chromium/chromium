// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/webapk/webapk_restore_manager.h"

#include "base/barrier_closure.h"
#include "base/strings/utf_string_conversions.h"
#include "base/types/pass_key.h"
#include "chrome/browser/android/webapk/webapk_restore_web_contents_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ssl/security_state_tab_helper.h"
#include "components/webapps/browser/web_contents/web_app_url_loader.h"
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
    RestorableAppsCallback apps_info_callback) {
  if (apps.empty()) {
    OnAllIconsDownloaded(std::move(apps_info_callback));
    return;
  }

  for (auto&& [app_id, shortcut_info] : apps) {
    restorable_tasks_.emplace(app_id, CreateNewTask(std::move(shortcut_info)));
  }

  // Prepare a web_contents and load |kAboutBlankURL| in the web contents to
  // download icons.
  web_contents_manager()->EnsureWebContentsCreated(PassKey());
  web_contents_manager()->LoadUrl(
      GURL(url::kAboutBlankURL),
      base::BindOnce(&WebApkRestoreManager::DownloadIcon,
                     weak_factory_.GetWeakPtr(),
                     std::move(apps_info_callback)));
}

void WebApkRestoreManager::DownloadIcon(
    RestorableAppsCallback apps_info_callback,
    webapps::WebAppUrlLoaderResult result) {
  auto barrier_closure = base::BarrierClosure(
      restorable_tasks_.size(),
      base::BindOnce(&WebApkRestoreManager::OnAllIconsDownloaded,
                     weak_factory_.GetWeakPtr(),
                     std::move(apps_info_callback)));

  for (auto&& [app_id, task] : restorable_tasks_) {
    task->DownloadIcon(barrier_closure);
  }
}

void WebApkRestoreManager::OnAllIconsDownloaded(
    RestorableAppsCallback apps_info_callback) {
  std::vector<std::vector<std::string>> results;
  for (auto&& [app_id, task] : restorable_tasks_) {
    results.push_back({app_id, base::UTF16ToUTF8(task->AppName())});
  }
  std::move(apps_info_callback).Run(results);
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
  return std::make_unique<WebApkRestoreTask>(
      PassKey(), profile_, web_contents_manager(), std::move(shortcut_info));
}

void WebApkRestoreManager::MaybeStartNextTask() {
  if (is_running_) {
    return;
  }

  if (tasks_.empty()) {
    ResetIfNotRunning();
    return;
  }

  is_running_ = true;
  web_contents_manager()->EnsureWebContentsCreated(PassKey());
  restorable_tasks_.at(tasks_.front())
      ->Start(base::BindOnce(&WebApkRestoreManager::OnTaskFinished,
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

void WebApkRestoreManager::ResetIfNotRunning() {
  if (is_running_ || !tasks_.empty()) {
    return;
  }

  restorable_tasks_.clear();
  web_contents_manager()->ClearSharedWebContents();
}

base::WeakPtr<WebApkRestoreManager> WebApkRestoreManager::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace webapk
