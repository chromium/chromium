// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system/kernel_feature_manager.h"

#include <stdint.h>

#include <string>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string_split.h"
#include "chromeos/dbus/debug_daemon/debug_daemon_client.h"
#include "third_party/cros_system_api/dbus/debugd/dbus-constants.h"

namespace ash {

KernelFeatureManager::KernelFeatureManager(
    DebugDaemonClient* debug_daemon_client)
    : debug_daemon_client_(debug_daemon_client) {
  debug_daemon_client_->WaitForServiceToBeAvailable(
      base::BindOnce(&KernelFeatureManager::OnDebugDaemonReady,
                     weak_ptr_factory_.GetWeakPtr()));
}

KernelFeatureManager::~KernelFeatureManager() = default;

void KernelFeatureManager::OnDebugDaemonReady(bool service_is_ready) {
  if (!service_is_ready) {
    LOG(ERROR) << "debugd service not ready";
    return;
  }

  // Initialize the system.
  debug_daemon_ready_ = true;
  GetKernelFeatureList();
}

void KernelFeatureManager::GetKernelFeatureList() {
  debug_daemon_client_->GetKernelFeatureList(
      base::BindOnce(&KernelFeatureManager::OnKernelFeatureList,
                     weak_ptr_factory_.GetWeakPtr()));
}

void KernelFeatureManager::OnKernelFeatureList(bool result,
                                               const std::string& out) {
  if (result) {
    // Split the CSV into a feature list
    kernel_feature_list_ = base::SplitString(out, ",", base::TRIM_WHITESPACE,
                                             base::SPLIT_WANT_NONEMPTY);
    if (!kernel_feature_list_.empty()) {
      // Find out which of these features requested in Finch
      EnableKernelFeatures();
      return;
    }
  }
  LOG(ERROR) << "Failed to get or parse kernel feature list from debugd.";
}

void KernelFeatureManager::EnableKernelFeatures() {
  base::FeatureList* feature_list_ins = base::FeatureList::GetInstance();
  DCHECK(feature_list_ins);

  for (const auto& name : kernel_feature_list_) {
    VLOG(1) << "Enabling kernel feature via debugd: " << name << std::endl;

    base::FieldTrial* trial = base::FeatureList::GetInstance()
                                  ->GetAssociatedFieldTrialByFeatureName(name);

    // Skip enabling features that are not a part of the field trial.
    // Note: We don't support dynamic disabling of kernel features on the device
    // right now. Requests to disable result in *only* the disabled group getting
    // activated. The kernel default for the device is already disabled.
    if (!trial) {
      continue;
    }

    if (feature_list_ins->GetEnabledFieldTrialByFeatureName(name)) {
      // Features marked as enabled need to be enabled on the device.
      // The actual finch experiment will be activated in the dbus callback
      // later once the feature is enabled on the device.
      debug_daemon_client_->KernelFeatureEnable(
          name, base::BindOnce(&KernelFeatureManager::OnKernelFeatureEnable,
          weak_ptr_factory_.GetWeakPtr()));
    } else {
      // The device defaults to "disabled", so no need to send any dbus commands
      // to disable. Just enable the field trial experiment for disabled group.
      trial->group();
      VLOG(1) << "Kernel experiment " << name << "activated as disabled.";
    }
  }
}

void KernelFeatureManager::OnKernelFeatureEnable(bool result,
                                                 const std::string& out) {
  base::FieldTrial* trial = base::FeatureList::GetInstance()
                                ->GetAssociatedFieldTrialByFeatureName(out);

  DCHECK(!result || trial);

  if (result) {
    trial->group();
    VLOG(1) << "Kernel experiment " << out << "activated as enabled.";
    return;
  }
  VLOG(1) << "Kernel feature has not been activated: " << out;
}

}  // namespace ash
