// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/device_reauth/chromeos/device_auth_availability_checker_chromeos.h"

#include "base/metrics/histogram_functions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/device_reauth/chromeos/authenticator_chromeos.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"

DeviceAuthAvailabilityCheckerChromeOS::DeviceAuthAvailabilityCheckerChromeOS()
    : authenticator_(std::make_unique<AuthenticatorChromeOS>()) {}

DeviceAuthAvailabilityCheckerChromeOS::DeviceAuthAvailabilityCheckerChromeOS(
    std::unique_ptr<AuthenticatorChromeOSInterface> authenticator)
    : authenticator_(std::move(authenticator)) {}

DeviceAuthAvailabilityCheckerChromeOS::
    ~DeviceAuthAvailabilityCheckerChromeOS() = default;

bool DeviceAuthAvailabilityCheckerChromeOS::CanAuthenticateWithBiometrics() {
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

bool DeviceAuthAvailabilityCheckerChromeOS::
    CanAuthenticateWithBiometricOrScreenLock() {
  CHECK(g_browser_process);
  CHECK(g_browser_process->local_state());
  // Check for biometrics availability.
  bool has_biometrics = CanAuthenticateWithBiometrics();
  // Read the cached value for PIN availability.
  bool has_pin = g_browser_process->local_state()->GetBoolean(
      password_manager::prefs::kPinAuthenticationAvailableOnChromeOS);
  return has_biometrics || has_pin;
}
