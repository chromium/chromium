// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/settings/shutdown_policy_handler.h"

#include "base/bind.h"
#include "base/callback.h"
#include "chromeos/settings/cros_settings_names.h"
#include "chromeos/settings/cros_settings_provider.h"

namespace chromeos {

ShutdownPolicyHandler::ShutdownPolicyHandler(CrosSettings* cros_settings,
                                             Delegate* delegate)
    : cros_settings_(cros_settings), delegate_(delegate) {
  DCHECK(cros_settings_);
  DCHECK(delegate);
  shutdown_policy_subscription_ = cros_settings_->AddSettingsObserver(
      kRebootOnShutdown,
      base::Bind(&ShutdownPolicyHandler::NotifyDelegateWithShutdownPolicy,
                 weak_factory_.GetWeakPtr()));
}

ShutdownPolicyHandler::~ShutdownPolicyHandler() {}

void ShutdownPolicyHandler::NotifyDelegateWithShutdownPolicy() {
  CrosSettingsProvider::TrustedStatus status =
      cros_settings_->PrepareTrustedValues(
          base::Bind(&ShutdownPolicyHandler::NotifyDelegateWithShutdownPolicy,
                     weak_factory_.GetWeakPtr()));
  if (status != CrosSettingsProvider::TRUSTED)
    return;

  // Get the updated policy.
  bool reboot_on_shutdown = false;
  cros_settings_->GetBoolean(kRebootOnShutdown, &reboot_on_shutdown);
  delegate_->OnShutdownPolicyChanged(reboot_on_shutdown);
}

}  // namespace chromeos
