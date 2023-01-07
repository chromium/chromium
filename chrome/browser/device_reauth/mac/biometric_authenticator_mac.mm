// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/device_reauth/mac/biometric_authenticator_mac.h"

#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "chrome/browser/browser_process.h"
#include "components/device_reauth/biometric_authenticator.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "device/fido/mac/touch_id_context.h"

BiometricAuthenticatorMac::BiometricAuthenticatorMac() = default;

BiometricAuthenticatorMac::~BiometricAuthenticatorMac() = default;

bool BiometricAuthenticatorMac::CanAuthenticate(
    device_reauth::BiometricAuthRequester requester) {
  base::scoped_nsobject<LAContext> context([[LAContext alloc] init]);
  bool is_available =
      [context canEvaluatePolicy:LAPolicyDeviceOwnerAuthenticationWithBiometrics
                           error:nil];
  base::UmaHistogramBoolean("PasswordManager.CanUseBiometricsMac",
                            is_available);
  if (is_available) {
    g_browser_process->local_state()->SetBoolean(
        password_manager::prefs::kHadBiometricsAvailable, is_available);
  }
  return is_available;
}

void BiometricAuthenticatorMac::Authenticate(
    device_reauth::BiometricAuthRequester requester,
    AuthenticateCallback callback,
    bool use_last_valid_auth) {
  NOTIMPLEMENTED();
}

void BiometricAuthenticatorMac::Cancel(device_reauth::BiometricAuthRequester) {
  touch_id_auth_context_ = nullptr;
  if (callback_) {
    // No code should be run after the callback as the callback could already be
    // destroying "this".
    std::move(callback_).Run(/*success=*/false);
  }
}

void BiometricAuthenticatorMac::AuthenticateWithMessage(
    device_reauth::BiometricAuthRequester requester,
    const std::u16string& message,
    AuthenticateCallback callback) {
  // Callers must ensure that previous authentication is canceled.
  DCHECK(!callback_);
  if (!NeedsToAuthenticate()) {
    // No code should be run after the callback as the callback could already be
    // destroying "this".
    std::move(callback).Run(/*success=*/true);
    return;
  }

  touch_id_auth_context_ = device::fido::mac::TouchIdContext::Create();
  callback_ = std::move(callback);

  touch_id_auth_context_->PromptTouchId(
      message,
      base::BindOnce(&BiometricAuthenticatorMac::OnAuthenticationCompleted,
                     weak_ptr_factory_.GetWeakPtr()));
}

void BiometricAuthenticatorMac::OnAuthenticationCompleted(bool success) {
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
