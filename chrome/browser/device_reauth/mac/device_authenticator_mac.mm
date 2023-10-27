// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/device_reauth/mac/device_authenticator_mac.h"

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/device_reauth/mac/authenticator_mac.h"
#include "chrome/browser/password_manager/password_manager_util_mac.h"
#include "chrome/grit/branded_strings.h"
#include "components/device_reauth/device_authenticator.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "device/fido/mac/touch_id_context.h"
#include "ui/base/l10n/l10n_util.h"

DeviceAuthenticatorMac::DeviceAuthenticatorMac(
    std::unique_ptr<AuthenticatorMacInterface> authenticator,
    DeviceAuthenticatorProxy* proxy,
    const device_reauth::DeviceAuthParams& params)
    : DeviceAuthenticatorCommon(proxy,
                                params.GetAuthenticationValidityPeriod(),
                                params.GetAuthResultHistogram()),
      authenticator_(std::move(authenticator)) {}

DeviceAuthenticatorMac::~DeviceAuthenticatorMac() = default;

bool DeviceAuthenticatorMac::CanAuthenticateWithBiometrics() {
  bool is_available = authenticator_->CheckIfBiometricsAvailable();
  base::UmaHistogramBoolean("PasswordManager.CanUseBiometricsMac",
                            is_available);
  if (is_available) {
    // If biometrics is available, we should record that at one point in time
    // biometrics was available on this device. This will never be set to false
    // after setting to true here as we only record this when biometrics is
    // available.
    g_browser_process->local_state()->SetBoolean(
        password_manager::prefs::kHadBiometricsAvailable, /*value=*/true);
  }
  return is_available;
}

bool DeviceAuthenticatorMac::CanAuthenticateWithBiometricOrScreenLock() {
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

void DeviceAuthenticatorMac::Cancel() {
  touch_id_auth_context_ = nullptr;
  if (callback_) {
    // No code should be run after the callback as the callback could already be
    // destroying "this".
    std::move(callback_).Run(/*success=*/false);
  }
}

void DeviceAuthenticatorMac::AuthenticateWithMessage(
    const std::u16string& message,
    AuthenticateCallback callback) {
  // Callers must ensure that previous authentication is canceled.
  DCHECK(!callback_);
  if (!NeedsToAuthenticate()) {
    RecordAuthResultSkipped();
    // No code should be run after the callback as the callback could already be
    // destroying "this".
    std::move(callback).Run(/*success=*/true);
    return;
  }
  callback_ = std::move(callback);
  // Always use CanAuthenticateWithBiometrics() before invoking the biometrics
  // API, and if it fails use password_manager_util_mac::AuthenticateUser()
  // instead, until crbug.com/1358442 is fixed.
  if (!CanAuthenticateWithBiometrics()) {
    OnAuthenticationCompleted(authenticator_->AuthenticateUserWithNonBiometrics(
        l10n_util::GetStringFUTF16(IDS_PASSWORDS_AUTHENTICATION_PROMPT_PREFIX,
                                   message)));
    return;
  }

  touch_id_auth_context_ = device::fido::mac::TouchIdContext::Create();
  touch_id_auth_context_->PromptTouchId(
      message,
      base::BindOnce(&DeviceAuthenticatorMac::OnAuthenticationCompleted,
                     weak_ptr_factory_.GetWeakPtr()));
}

void DeviceAuthenticatorMac::OnAuthenticationCompleted(bool success) {
  touch_id_auth_context_ = nullptr;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!callback_) {
    return;
  }
  RecordAuthenticationTimeIfSuccessful(success);
  // No code should be run after the callback as the callback could already be
  // destroying "this".
  std::move(callback_).Run(success);
}
