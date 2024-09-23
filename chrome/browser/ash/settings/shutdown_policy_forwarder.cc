// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/settings/shutdown_policy_forwarder.h"

#include "ash/public/cpp/shutdown_controller.h"
#include "chromeos/ash/components/settings/cros_settings.h"

namespace ash {

ShutdownPolicyForwarder::ShutdownPolicyForwarder()
    : shutdown_policy_handler_(CrosSettings::Get(), this) {
  // Request the initial setting.
  shutdown_policy_handler_.NotifyDelegateWithShutdownPolicy();
}

ShutdownPolicyForwarder::~ShutdownPolicyForwarder() = default;

void ShutdownPolicyForwarder::OnShutdownPolicyChanged(bool reboot_on_shutdown) {
  // Forward the setting to ShutdownController.
  ShutdownController::Get()->SetRebootOnShutdown(reboot_on_shutdown);
}

}  // namespace ash
