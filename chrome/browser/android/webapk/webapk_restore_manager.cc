// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/webapk/webapk_restore_manager.h"

#include "base/types/pass_key.h"
#include "chrome/browser/profiles/profile.h"
#include "components/sync/protocol/web_app_specifics.pb.h"

namespace webapk {

WebApkRestoreManager::WebApkRestoreManager(Profile* profile)
    : profile_(profile->GetWeakPtr()),
      sequenced_task_runner_(base::SequencedTaskRunner::GetCurrentDefault()) {}

WebApkRestoreManager::~WebApkRestoreManager() = default;

void WebApkRestoreManager::ScheduleTask(
    const sync_pb::WebApkSpecifics& webapk_specifics) {
  tasks_.push_back(std::make_unique<WebApkRestoreTask>(
      base::PassKey<WebApkRestoreManager>(), webapk_specifics,
      base::BindOnce(&WebApkRestoreManager::OnTaskFinished,
                     base::Unretained(this))));

  StartNextTaskIfNotAlreadyRunning();
}

void WebApkRestoreManager::StartNextTaskIfNotAlreadyRunning() {
  if (!is_running_ && !tasks_.empty()) {
    is_running_ = true;
    tasks_.front()->Start();
  }
}

void WebApkRestoreManager::OnTaskFinished(const GURL& manifest_id) {
  tasks_.pop_front();
  is_running_ = false;

  sequenced_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&WebApkRestoreManager::StartNextTaskIfNotAlreadyRunning,
                     weak_factory_.GetWeakPtr()));
}

}  // namespace webapk
