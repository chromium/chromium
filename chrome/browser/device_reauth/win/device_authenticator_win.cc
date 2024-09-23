// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/device_reauth/win/device_authenticator_win.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/browser_process.h"
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

DeviceAuthenticatorWin::DeviceAuthenticatorWin(
    std::unique_ptr<AuthenticatorWinInterface> authenticator,
    DeviceAuthenticatorProxy* proxy,
    const device_reauth::DeviceAuthParams& params)
    : DeviceAuthenticatorCommon(proxy,
                                params.GetAuthenticationValidityPeriod(),
                                params.GetAuthResultHistogram()),
      authenticator_(std::move(authenticator)) {}

DeviceAuthenticatorWin::~DeviceAuthenticatorWin() = default;

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

void DeviceAuthenticatorWin::AuthenticateWithMessage(
    const std::u16string& message,
    AuthenticateCallback callback) {
  if (!NeedsToAuthenticate()) {
    RecordAuthResultSkipped();

    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), /*success=*/true));
    return;
  }

  authenticator_->AuthenticateUser(
      message,
      base::BindOnce(&DeviceAuthenticatorWin::OnAuthenticationCompleted,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void DeviceAuthenticatorWin::Cancel() {
  // TODO(crbug.com/40235610): Add implementation of the Cancel method.
  NOTIMPLEMENTED();
}

// static
void DeviceAuthenticatorWin::CacheIfBiometricsAvailable(
    AuthenticatorWinInterface* authenticator) {
  authenticator->CheckIfBiometricsAvailable(base::BindOnce(&SaveAvailability));
}

void DeviceAuthenticatorWin::OnAuthenticationCompleted(
    base::OnceCallback<void(bool)> callback,
    bool success) {
  RecordAuthenticationTimeIfSuccessful(success);
  std::move(callback).Run(success);
}
