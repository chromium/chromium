// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/device_reauth/chromeos/device_authenticator_chromeos.h"

#include "base/metrics/histogram_functions.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/browser_process.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"

DeviceAuthenticatorChromeOS::DeviceAuthenticatorChromeOS(
    std::unique_ptr<AuthenticatorChromeOSInterface> authenticator,
    DeviceAuthenticatorProxy* proxy,
    const device_reauth::DeviceAuthParams& params,
    PrefService* local_state)
    : DeviceAuthenticatorCommon(proxy,
                                params.GetAuthenticationValidityPeriod(),
                                params.GetAuthResultHistogram()),
      authenticator_(std::move(authenticator)),
      source_(params.GetDeviceAuthSource()),
      local_state_(local_state) {}

DeviceAuthenticatorChromeOS::~DeviceAuthenticatorChromeOS() = default;

bool DeviceAuthenticatorChromeOS::CanAuthenticateWithBiometrics() {
  BiometricsStatusChromeOS status =
      authenticator_->CheckIfBiometricsAvailable();
  bool is_available = status == BiometricsStatusChromeOS::kAvailable;

  CHECK(local_state_);

  if (is_available) {
    // If biometrics is available, we should record that at one point in time
    // biometrics was available on this device. This will never be set to false
    // after setting to true here as we only record this when biometrics is
    // available.
    local_state_->SetBoolean(password_manager::prefs::kHadBiometricsAvailable,
                             /*value=*/true);
  }

  base::UmaHistogramEnumeration("PasswordManager.BiometricAvailabilityChromeOS",
                                status);
  return is_available;
}

bool DeviceAuthenticatorChromeOS::CanAuthenticateWithBiometricOrScreenLock() {
  // Check for biometrics availability.
  bool has_biometrics = CanAuthenticateWithBiometrics();
  // Read the cached value for PIN availability.
  bool has_pin = local_state_->GetBoolean(
      password_manager::prefs::kPinAuthenticationAvailableOnChromeOS);
  return has_biometrics || has_pin;
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
      message, source_,
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

// static
void DeviceAuthenticatorChromeOS::CacheIfPinIsAvailable(
    AuthenticatorChromeOSInterface* authenticator,
    PrefService* local_state) {
  authenticator->CheckIfPinIsAvailable(base::BindOnce(
      [](PrefService* local_state, bool has_pin) {
        local_state->SetBoolean(
            password_manager::prefs::kPinAuthenticationAvailableOnChromeOS,
            has_pin);
      },
      local_state));
}
