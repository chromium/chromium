// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/device_reauth/mac/biometric_authenticator_mac.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/notreached.h"
#include "components/device_reauth/biometric_authenticator.h"
#include "device/fido/mac/touch_id_context.h"

BiometricAuthenticatorMac::BiometricAuthenticatorMac() = default;

BiometricAuthenticatorMac::~BiometricAuthenticatorMac() = default;

bool BiometricAuthenticatorMac::CanAuthenticate(
    device_reauth::BiometricAuthRequester requester) {
  NOTIMPLEMENTED();
  return false;
}

void BiometricAuthenticatorMac::Authenticate(
    device_reauth::BiometricAuthRequester requester,
    AuthenticateCallback callback,
    bool use_last_valid_auth) {
  NOTIMPLEMENTED();
}

void BiometricAuthenticatorMac::Cancel(
    device_reauth::BiometricAuthRequester requester) {
  NOTIMPLEMENTED();
}

void BiometricAuthenticatorMac::AuthenticateWithMessage(
    device_reauth::BiometricAuthRequester requester,
    const std::u16string message,
    AuthenticateCallback callback) {
  if (!NeedsToAuthenticate()) {
    std::move(callback).Run(/*success=*/true);
    return;
  }

  // TODO(crbug.com/1350994): Clean the touchIdContext object after
  // authentication is done.
  touch_id_auth_context_ = device::fido::mac::TouchIdContext::Create();
  base::OnceCallback<bool(bool)> record_authentication_result =
      base::BindOnce(&BiometricAuthenticatorMac::RecordAuthenticationResult,
                     base::Unretained(this));

  touch_id_auth_context_->PromptTouchId(
      message,
      std::move(record_authentication_result).Then(std::move(callback)));
}
