// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shutdown_controller_impl.h"

#include <utility>

#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/shutdown_reason.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash {

ShutdownControllerImpl::ShutdownControllerImpl() = default;

ShutdownControllerImpl::~ShutdownControllerImpl() = default;

void ShutdownControllerImpl::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void ShutdownControllerImpl::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void ShutdownControllerImpl::SetRebootOnShutdown(bool reboot_on_shutdown) {
  if (reboot_on_shutdown_ == reboot_on_shutdown)
    return;
  reboot_on_shutdown_ = reboot_on_shutdown;
  for (auto& observer : observers_)
    observer.OnShutdownPolicyChanged(reboot_on_shutdown_);
}

void ShutdownControllerImpl::ShutDownOrReboot(ShutdownReason reason) {
  // For developers on Linux desktop just exit the app.
  if (!base::SysInfo::IsRunningOnChromeOS()) {
    Shell::Get()->session_controller()->RequestSignOut();
    return;
  }

  if (reason == ShutdownReason::POWER_BUTTON)
    base::RecordAction(base::UserMetricsAction("Accel_ShutDown_PowerButton"));

  // On real Chrome OS hardware the power manager handles shutdown.
  std::string description = base::StringPrintf("UI request from ash: %s",
                                               ShutdownReasonToString(reason));
  if (reboot_on_shutdown_) {
    chromeos::PowerManagerClient::Get()->RequestRestart(
        power_manager::REQUEST_RESTART_FOR_USER, description);
  } else {
    chromeos::PowerManagerClient::Get()->RequestShutdown(
        power_manager::REQUEST_SHUTDOWN_FOR_USER, description);
  }
}

}  // namespace ash
