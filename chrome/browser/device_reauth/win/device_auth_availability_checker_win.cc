// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/device_reauth/win/device_auth_availability_checker_win.h"

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/device_reauth/win/authenticator_win.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"

namespace {

void SaveAvailability(BiometricAuthenticationStatusWin availability) {
  CHECK(g_browser_process);
  CHECK(g_browser_process->local_state());

  bool is_available =
      availability == BiometricAuthenticationStatusWin::kAvailable;
  g_browser_process->local_state()->SetBoolean(
      password_manager::prefs::kIsBiometricAvailable, is_available);
  if (is_available) {
    g_browser_process->local_state()->SetBoolean(
        password_manager::prefs::kHadBiometricsAvailable, is_available);
  }
  base::UmaHistogramEnumeration("PasswordManager.BiometricAvailabilityWin",
                                availability);
}

}  // namespace

DeviceAuthAvailabilityCheckerWin::DeviceAuthAvailabilityCheckerWin()
    : DeviceAuthAvailabilityCheckerWin(std::make_unique<AuthenticatorWin>()) {}

DeviceAuthAvailabilityCheckerWin::DeviceAuthAvailabilityCheckerWin(
    std::unique_ptr<AuthenticatorWinInterface> authenticator)
    : authenticator_(std::move(authenticator)) {}

DeviceAuthAvailabilityCheckerWin::~DeviceAuthAvailabilityCheckerWin() = default;

// static
void DeviceAuthAvailabilityCheckerWin::CacheIfBiometricsAvailable(
    std::unique_ptr<AuthenticatorWinInterface> authenticator) {
  CHECK(authenticator);
  authenticator->CheckIfBiometricsAvailable(base::BindOnce(&SaveAvailability));
}

bool DeviceAuthAvailabilityCheckerWin::CanAuthenticateWithBiometrics() {
  CHECK(g_browser_process);
  CHECK(g_browser_process->local_state());
  return g_browser_process->local_state()->GetBoolean(
      password_manager::prefs::kIsBiometricAvailable);
}

bool DeviceAuthAvailabilityCheckerWin::
    CanAuthenticateWithBiometricOrScreenLock() {
  return CanAuthenticateWithBiometrics() ||
         authenticator_->CanAuthenticateWithScreenLock();
}
