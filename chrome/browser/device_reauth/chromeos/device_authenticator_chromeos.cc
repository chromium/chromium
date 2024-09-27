// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/device_reauth/chromeos/device_authenticator_chromeos.h"

#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions_internal_overloads.h"
#include "base/notreached.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"

DeviceAuthenticatorChromeOS::DeviceAuthenticatorChromeOS(
    std::unique_ptr<AuthenticatorChromeOSInterface> authenticator,
    DeviceAuthenticatorProxy* proxy,
    const device_reauth::DeviceAuthParams& params)
    : DeviceAuthenticatorCommon(proxy,
                                params.GetAuthenticationValidityPeriod(),
                                params.GetAuthResultHistogram()),
      authenticator_(std::move(authenticator)) {}

DeviceAuthenticatorChromeOS::~DeviceAuthenticatorChromeOS() = default;

bool DeviceAuthenticatorChromeOS::CanAuthenticateWithBiometrics() {
  BiometricsStatusChromeOS status =
      authenticator_->CheckIfBiometricsAvailable();
  bool is_available = status == BiometricsStatusChromeOS::kAvailable;

  CHECK(g_browser_process);
  CHECK(g_browser_process->local_state());

  if (is_available) {
    // If biometrics is available, we should record that at one point in time
    // biometrics was available on this device. This will never be set to false
    // after setting to true here as we only record this when biometrics is
    // available.
    g_browser_process->local_state()->SetBoolean(
        password_manager::prefs::kHadBiometricsAvailable, /*value=*/true);
  }

  base::UmaHistogramEnumeration("PasswordManager.BiometricAvailabilityChromeOS",
                                status);
  return is_available;
}

bool DeviceAuthenticatorChromeOS::CanAuthenticateWithBiometricOrScreenLock() {
  // We check if we can authenticate strictly with biometrics first as this
  // function has important side effects such as logging metrics related to how
  // often users have biometrics available, and setting a pref that denotes that
  // at one point biometrics was available on this device.
  return CanAuthenticateWithBiometrics();
}

void DeviceAuthenticatorChromeOS::AuthenticateWithMessage(
    const std::u16string& message,
    AuthenticateCallback callback) {
  if (!NeedsToAuthenticate()) {
    RecordAuthResultSkipped();

    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), /*success=*/true));
    return;
  }

  callback_ = std::move(callback);

  authenticator_->AuthenticateUser(
      message,
      base::BindOnce(&DeviceAuthenticatorChromeOS::OnAuthenticationCompleted,
                     weak_ptr_factory_.GetWeakPtr()));
}

void DeviceAuthenticatorChromeOS::Cancel() {
  // TODO(b/292097975): Cancel the in session auth dialog.
  if (callback_) {
    std::move(callback_).Run(false);
  }
}

void DeviceAuthenticatorChromeOS::OnAuthenticationCompleted(bool success) {
  if (!callback_) {
    return;
  }

  RecordAuthenticationTimeIfSuccessful(success);
  std::move(callback_).Run(success);
}
