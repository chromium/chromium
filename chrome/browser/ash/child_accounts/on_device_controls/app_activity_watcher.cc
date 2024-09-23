// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/child_accounts/on_device_controls/app_activity_watcher.h"

#include <string>

#include "base/containers/contains.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/ash/child_accounts/on_device_controls/blocked_app_registry.h"

namespace ash::on_device_controls {

namespace {

bool IsAppBlocked(BlockedAppRegistry* blocked_app_registry,
                  const std::string& app_id) {
  return blocked_app_registry->GetAppState(app_id) == LocalAppState::kBlocked;
}

}  // namespace

AppActivityWatcher::AppActivityWatcher(BlockedAppRegistry* blocked_app_registry,
                                       apps::AppServiceProxy* app_service_proxy)
    : blocked_app_registry_(blocked_app_registry),
      app_service_proxy_(app_service_proxy) {
  instance_registry_observation_.Observe(
      &app_service_proxy_->InstanceRegistry());
}

AppActivityWatcher::~AppActivityWatcher() = default;

void AppActivityWatcher::OnInstanceUpdate(const apps::InstanceUpdate& update) {
  if (!update.StateChanged()) {
    return;
  }

  const bool is_destroyed = update.State() & apps::InstanceState::kDestroyed;
  if (is_destroyed) {
    blocking_instance_ids_.erase(update.InstanceId());
    return;
  }

  const bool is_active = update.State() & apps::InstanceState::kActive;
  if (!is_active) {
    return;
  }

  const std::string& app_id = update.AppId();
  const bool should_block = IsAppBlocked(blocked_app_registry_, app_id);
  if (!should_block) {
    return;
  }

  if (base::Contains(blocking_instance_ids_, update.InstanceId())) {
    return;
  }

  blocking_instance_ids_.insert(update.InstanceId());
  app_service_proxy_->BlockApps({app_id}, /*show_block_dialog=*/true);
  app_service_proxy_->StopApp(app_id);
}

void AppActivityWatcher::OnInstanceRegistryWillBeDestroyed(
    apps::InstanceRegistry* cache) {
  instance_registry_observation_.Reset();
}

}  // namespace ash::on_device_controls
