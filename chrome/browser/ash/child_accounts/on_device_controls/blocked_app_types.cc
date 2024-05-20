// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/child_accounts/on_device_controls/blocked_app_types.h"

#include "base/logging.h"
#include "base/time/time.h"

namespace ash::on_device_controls {

BlockedAppDetails::BlockedAppDetails() : BlockedAppDetails(base::Time::Now()) {}

BlockedAppDetails::BlockedAppDetails(base::Time block_timestamp)
    : block_timestamp_(block_timestamp) {}

BlockedAppDetails::BlockedAppDetails(base::Time block_timestamp,
                                     base::Time uninstall_timestamp)
    : block_timestamp_(block_timestamp),
      uninstall_timestamp_(uninstall_timestamp) {
  if (uninstall_timestamp_ && block_timestamp_ > uninstall_timestamp_) {
    LOG(WARNING) << "app-controls: block timestamp after uninstall timestamp";
  }
}

BlockedAppDetails::~BlockedAppDetails() = default;

bool BlockedAppDetails::IsInstalled() const {
  return !uninstall_timestamp_.has_value();
}

void BlockedAppDetails::MarkInstalled() {
  if (IsInstalled()) {
    LOG(WARNING) << "app-controls: installed app marked installed again";
  }
  uninstall_timestamp_.reset();
}

void BlockedAppDetails::SetUninstallTimestamp(base::Time timestamp) {
  if (!IsInstalled()) {
    LOG(WARNING) << "app-controls: uninstalled timestamp updated";
  }
  uninstall_timestamp_ = timestamp;
}

void BlockedAppDetails::SetBlockTimestamp(base::Time timestamp) {
  if (uninstall_timestamp_ && block_timestamp_ > uninstall_timestamp_) {
    LOG(WARNING) << "app-controls: block timestamp after uninstall timestamp";
  }
  block_timestamp_ = timestamp;
}

}  // namespace ash::on_device_controls
