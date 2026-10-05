// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/device_reauth/mac/device_auth_availability_checker_mac.h"

#include "base/metrics/histogram_functions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/device_reauth/mac/authenticator_mac.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"

DeviceAuthAvailabilityCheckerMac::DeviceAuthAvailabilityCheckerMac()
    : authenticator_(std::make_unique<AuthenticatorMac>()) {}

DeviceAuthAvailabilityCheckerMac::DeviceAuthAvailabilityCheckerMac(
    std::unique_ptr<AuthenticatorMacInterface> authenticator)
    : authenticator_(std::move(authenticator)) {}

DeviceAuthAvailabilityCheckerMac::~DeviceAuthAvailabilityCheckerMac() = default;

bool DeviceAuthAvailabilityCheckerMac::CanAuthenticateWithBiometrics() {
  bool is_available = authenticator_->CheckIfBiometricsAvailable();
  base::UmaHistogramBoolean("PasswordManager.CanUseBiometricsMac",
                            is_available);
  if (is_available) {
    CHECK(g_browser_process);
    CHECK(g_browser_process->local_state());
    // If biometrics is available, we should record that at one point in time
    // biometrics was available on this device. This will never be set to false
    // after setting to true here as we only record this when biometrics is
    // available.
    g_browser_process->local_state()->SetBoolean(
        password_manager::prefs::kHadBiometricsAvailable, /*value=*/true);
  }
  return is_available;
}

bool DeviceAuthAvailabilityCheckerMac::
    CanAuthenticateWithBiometricOrScreenLock() {
  // We check if we can authenticate strictly with biometrics first as this
  // function has important side effects such as logging metrics related to how
  // often users have biometrics available, and setting a pref that denotes that
  // at one point biometrics was available on this device.
  if (CanAuthenticateWithBiometrics()) {
    return true;
  }

  // TODO(crbug.com/4555994): Add metrics logging for the only screen lock
  // available case.
  return authenticator_->CheckIfBiometricsOrScreenLockAvailable();
}
