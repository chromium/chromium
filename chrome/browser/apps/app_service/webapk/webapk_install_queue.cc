// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/webapk/webapk_install_queue.h"

#include <utility>

#include "ash/components/arc/mojom/webapk.mojom.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/apps/app_service/webapk/webapk_install_task.h"
#include "chrome/browser/profiles/profile.h"

namespace apps {

// Queue of WebApks to be installed or updated.
WebApkInstallQueue::WebApkInstallQueue(Profile* profile) : profile_(profile) {
  arc::ArcServiceManager* arc_service_manager = arc::ArcServiceManager::Get();
  DCHECK(arc_service_manager);
  arc_service_manager->arc_bridge_service()->webapk()->AddObserver(this);
}

WebApkInstallQueue::~WebApkInstallQueue() {
  arc::ArcServiceManager* arc_service_manager = arc::ArcServiceManager::Get();
  if (arc_service_manager) {
    arc_service_manager->arc_bridge_service()->webapk()->RemoveObserver(this);
  }
}

void WebApkInstallQueue::InstallOrUpdate(const std::string& app_id) {
  pending_installs_.push_back(
      std::make_unique<WebApkInstallTask>(profile_, app_id));
  PostMaybeStartNext();
}

void WebApkInstallQueue::PostMaybeStartNext() {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&WebApkInstallQueue::MaybeStartNext,
                                weak_ptr_factory_.GetWeakPtr()));
}

void WebApkInstallQueue::MaybeStartNext() {
  if (pending_installs_.empty() || current_install_ || !connection_ready_) {
    return;
  }

  current_install_ = std::move(pending_installs_.front());
  pending_installs_.pop_front();

  current_install_->Start(base::BindOnce(
      &WebApkInstallQueue::OnInstallCompleted, weak_ptr_factory_.GetWeakPtr()));
}

void WebApkInstallQueue::OnInstallCompleted(bool success) {
  current_install_.reset();
  PostMaybeStartNext();
}

void WebApkInstallQueue::OnConnectionReady() {
  // Only start installing when WebApkInstance is ready, since installs cannot
  // complete without it.
  connection_ready_ = true;
  PostMaybeStartNext();
}

void WebApkInstallQueue::OnConnectionClosed() {
  connection_ready_ = false;
}

std::unique_ptr<WebApkInstallTask> WebApkInstallQueue::PopTaskForTest() {
  DCHECK(!current_install_);
  std::unique_ptr<WebApkInstallTask> task;
  if (!pending_installs_.empty()) {
    task = std::move(pending_installs_.front());
    pending_installs_.pop_front();
  }

  return task;
}

}  // namespace apps
