// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/child_accounts/on_device_controls/blocked_app_registry.h"

#include "base/containers/contains.h"
#include "base/logging.h"
#include "base/time/time.h"

namespace ash::on_device_controls {

BlockedAppRegistry::AppDetails::AppDetails()
    : AppDetails(base::TimeTicks::Now()) {}

BlockedAppRegistry::AppDetails::AppDetails(base::TimeTicks block_timestamp)
    : block_timestamp(block_timestamp) {}

BlockedAppRegistry::AppDetails::~AppDetails() = default;

BlockedAppRegistry::BlockedAppRegistry() = default;

BlockedAppRegistry::~BlockedAppRegistry() = default;

void BlockedAppRegistry::AddApp(const std::string& app_id) {
  if (base::Contains(registry_, app_id)) {
    LOG(WARNING) << app_id << " already in blocked app registry";
    return;
  }
  registry_[app_id] = AppDetails();

  // TODO(b/338247185): Update pref state.
}

void BlockedAppRegistry::RemoveApp(const std::string& app_id) {
  if (!base::Contains(registry_, app_id)) {
    LOG(WARNING) << app_id << " not in blocked app registry";
    return;
  }
  registry_.erase(app_id);

  // TODO(b/338247185): Update pref state.
}

std::set<std::string> BlockedAppRegistry::GetBlockedApps() {
  std::set<std::string> blocked_app_ids_;
  for (const auto& app : registry_) {
    blocked_app_ids_.insert(app.first);
  }
  return blocked_app_ids_;
}

LocalAppState BlockedAppRegistry::GetAppState(const std::string& app_id) const {
  if (!base::Contains(registry_, app_id)) {
    return LocalAppState::kAvailable;
  }

  if (registry_.at(app_id).uninstall_timestamp.has_value()) {
    return LocalAppState::kBlockedUninstalled;
  }

  return LocalAppState::kBlocked;
}
}  // namespace ash::on_device_controls
