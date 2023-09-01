// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/quickstart_controller.h"

#include "ash/constants/ash_features.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/target_device_connection_broker.h"
#include "chrome/browser/ash/login/ui/login_display_host.h"
#include "chrome/browser/ash/login/wizard_context.h"

namespace ash {

QuickStartController::QuickStartController() {
  if (features::IsOobeQuickStartEnabled()) {
    LoginDisplayHost::default_host()->GetWizardContext()->quick_start_enabled =
        true;
    bootstrap_controller_ =
        LoginDisplayHost::default_host()->GetQuickStartBootstrapController();
  }
}

QuickStartController::~QuickStartController() = default;

void QuickStartController::ForceEnableQuickStart() {
  if (bootstrap_controller_) {
    return;
  }

  LoginDisplayHost::default_host()->GetWizardContext()->quick_start_enabled =
      true;
  bootstrap_controller_ =
      LoginDisplayHost::default_host()->GetQuickStartBootstrapController();
}

void QuickStartController::IsSupported(
    EntryPointButtonVisibilityCallback callback) {
  // Bootstrap controller is only instantiated when the feature is enabled (also
  // via the keyboard shortcut. See |ForceEnableQuickStart|.)
  if (!bootstrap_controller_) {
    std::move(callback).Run(/*visible=*/false);
    return;
  }

  if (quickstart_supported_.has_value()) {
    std::move(callback).Run(quickstart_supported_.value());
    return;
  }

  bootstrap_controller_->GetFeatureSupportStatusAsync(
      base::BindOnce(&QuickStartController::OnGetQuickStartFeatureSupportStatus,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void QuickStartController::OnGetQuickStartFeatureSupportStatus(
    EntryPointButtonVisibilityCallback callback,
    quick_start::TargetDeviceConnectionBroker::FeatureSupportStatus status) {
  quickstart_supported_ = status == quick_start::TargetDeviceConnectionBroker::
                                        FeatureSupportStatus::kSupported;

  std::move(callback).Run(quickstart_supported_.value());
}

}  // namespace ash
