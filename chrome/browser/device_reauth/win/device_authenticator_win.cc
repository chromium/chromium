// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/device_reauth/win/device_authenticator_win.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/browser_process.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"

namespace {

void SaveAvailability(BiometricAuthenticationStatusWin availability) {
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

DeviceAuthenticatorWin::DeviceAuthenticatorWin(
    std::unique_ptr<AuthenticatorWinInterface> authenticator)
    : authenticator_(std::move(authenticator)) {}

DeviceAuthenticatorWin::~DeviceAuthenticatorWin() = default;

// static
scoped_refptr<DeviceAuthenticatorWin> DeviceAuthenticatorWin::CreateForTesting(
    std::unique_ptr<AuthenticatorWinInterface> authenticator) {
  return base::WrapRefCounted(
      new DeviceAuthenticatorWin(std::move(authenticator)));
}

bool DeviceAuthenticatorWin::CanAuthenticateWithBiometrics() {
  // Setting that pref happens once when the ChromeDeviceAuthenticatorFactory
  // is created and it is async so it can technically happen that this pref
  // doesn't have the latest value when you check it.
  return g_browser_process->local_state()->GetBoolean(
      password_manager::prefs::kIsBiometricAvailable);
}

bool DeviceAuthenticatorWin::CanAuthenticateWithBiometricOrScreenLock() {
  return CanAuthenticateWithBiometrics() ||
         authenticator_->CanAuthenticateWithScreenLock();
}

void DeviceAuthenticatorWin::Authenticate(
    device_reauth::DeviceAuthRequester requester,
    AuthenticateCallback callback,
    bool use_last_valid_auth) {
  NOTIMPLEMENTED();
}

void DeviceAuthenticatorWin::AuthenticateWithMessage(
    const std::u16string& message,
    AuthenticateCallback callback) {
  if (!NeedsToAuthenticate()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), /*success=*/true));
    return;
  }

  authenticator_->AuthenticateUser(
      message,
      base::BindOnce(&DeviceAuthenticatorWin::OnAuthenticationCompleted,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void DeviceAuthenticatorWin::Cancel(
    device_reauth::DeviceAuthRequester requester) {
  // TODO(crbug.com/1354552): Add implementation of the Cancel method.
  NOTIMPLEMENTED();
}

void DeviceAuthenticatorWin::CacheIfBiometricsAvailable() {
  authenticator_->CheckIfBiometricsAvailable(base::BindOnce(&SaveAvailability));
}

void DeviceAuthenticatorWin::OnAuthenticationCompleted(
    base::OnceCallback<void(bool)> callback,
    bool success) {
  RecordAuthenticationTimeIfSuccessful(success);
  std::move(callback).Run(success);
}
