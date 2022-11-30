// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/device_reauth/mac/biometric_authenticator_mac.h"

#include "base/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "components/device_reauth/biometric_authenticator.h"
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
  return is_available;
}

void BiometricAuthenticatorMac::Authenticate(
    device_reauth::BiometricAuthRequester requester,
    AuthenticateCallback callback,
    bool use_last_valid_auth) {
  NOTIMPLEMENTED();
}

void BiometricAuthenticatorMac::Cancel(device_reauth::BiometricAuthRequester) {
  if (callback_) {
    std::move(callback_).Run(/*success=*/false);
  }
  touch_id_auth_context_ = nullptr;
}

void BiometricAuthenticatorMac::AuthenticateWithMessage(
    device_reauth::BiometricAuthRequester requester,
    const std::u16string& message,
    AuthenticateCallback callback) {
  if (!NeedsToAuthenticate()) {
    DCHECK(callback_.is_null());
    std::move(callback).Run(/*success=*/true);
    return;
  }

  // Cancel old authentication if a new one comes in.
  Cancel(requester);

  touch_id_auth_context_ = device::fido::mac::TouchIdContext::Create();
  callback_ = std::move(callback);

  touch_id_auth_context_->PromptTouchId(
      message,
      base::BindOnce(&BiometricAuthenticatorMac::OnAuthenticationCompleted,
                     weak_ptr_factory_.GetWeakPtr()));
}

void BiometricAuthenticatorMac::OnAuthenticationCompleted(bool result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (callback_.is_null()) {
    return;
  }

  std::move(callback_).Run(RecordAuthenticationResult(result));
  touch_id_auth_context_ = nullptr;
}
