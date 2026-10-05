// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/device_reauth/win/device_auth_availability_checker_win.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/device_reauth/win/authenticator_win.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"

DeviceAuthAvailabilityCheckerWin::DeviceAuthAvailabilityCheckerWin()
    : authenticator_(std::make_unique<AuthenticatorWin>()) {}

DeviceAuthAvailabilityCheckerWin::DeviceAuthAvailabilityCheckerWin(
    std::unique_ptr<AuthenticatorWinInterface> authenticator)
    : authenticator_(std::move(authenticator)) {}

DeviceAuthAvailabilityCheckerWin::~DeviceAuthAvailabilityCheckerWin() = default;

bool DeviceAuthAvailabilityCheckerWin::CanAuthenticateWithBiometrics() {
  CHECK(g_browser_process);
  CHECK(g_browser_process->local_state());
  // Setting that pref happens once when the `ChromeDeviceAuthenticatorFactory`
  // is created and it is async so it can technically happen that this pref
  // doesn't have the latest value when you check it.
  return g_browser_process->local_state()->GetBoolean(
      password_manager::prefs::kIsBiometricAvailable);
}

bool DeviceAuthAvailabilityCheckerWin::
    CanAuthenticateWithBiometricOrScreenLock() {
  return CanAuthenticateWithBiometrics() ||
         authenticator_->CanAuthenticateWithScreenLock();
}
