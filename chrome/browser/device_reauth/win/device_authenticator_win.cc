// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/device_reauth/win/device_authenticator_win.h"

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/browser_process.h"
#include "components/password_manager/core/browser/features/password_features.h"
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
  // Callers must ensure that previous authentication is finished or canceled.
  CHECK(!callback_);
  if (!NeedsToAuthenticate()) {
    RecordAuthResultSkipped();

    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), /*success=*/true));
    return;
  }

  if (!base::FeatureList::IsEnabled(
          password_manager::features::kDeviceAuthenticatorWinCancel)) {
    authenticator_->AuthenticateUser(
        message,
        base::BindOnce(&DeviceAuthenticatorWin::OnAuthenticationCompleted,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
    return;
  }

  callback_ = std::move(callback);
  authenticator_->AuthenticateUser(
      message,
      base::BindOnce(&DeviceAuthenticatorWin::OnAuthenticationCompleted,
                     weak_ptr_factory_.GetWeakPtr(), base::NullCallback()));
}

void DeviceAuthenticatorWin::Cancel() {
  if (callback_) {
    // Note that `kFinished` will still be recorded when the OS prompt
    // eventually closes.
    base::UmaHistogramEnumeration("PasswordManager.AuthenticationStateWin",
                                  AuthenticationStateWin::kCanceledByChrome);
    weak_ptr_factory_.InvalidateWeakPtrs();
    // No code should be run after the callback as the callback could already be
    // destroying "this".
    std::move(callback_).Run(/*success=*/false);
  }
}

// static
void DeviceAuthenticatorWin::CacheIfBiometricsAvailable(
    AuthenticatorWinInterface* authenticator) {
  authenticator->CheckIfBiometricsAvailable(base::BindOnce(&SaveAvailability));
}

void DeviceAuthenticatorWin::OnAuthenticationCompleted(
    AuthenticateCallback callback,
    bool success) {
  if (!base::FeatureList::IsEnabled(
          password_manager::features::kDeviceAuthenticatorWinCancel)) {
    RecordAuthenticationTimeIfSuccessful(success);
    CHECK(callback);
    std::move(callback).Run(success);
    return;
  }
  if (!callback_) {
    return;
  }
  RecordAuthenticationTimeIfSuccessful(success);
  // No code should be run after the callback as the callback could already be
  // destroying "this".
  std::move(callback_).Run(success);
}
