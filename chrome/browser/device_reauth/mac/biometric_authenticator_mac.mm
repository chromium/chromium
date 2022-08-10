// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/device_reauth/mac/biometric_authenticator_mac.h"

#include "base/bind.h"
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
    DCHECK(callback_.is_null());
    std::move(callback).Run(/*success=*/true);
    return;
  }

  if (callback_) {
    std::move(callback_).Run(/*success=*/false);
  }

  touch_id_auth_context_ = device::fido::mac::TouchIdContext::Create();
  callback_ = std::move(callback);

  touch_id_auth_context_->PromptTouchId(
      message,
      base::BindOnce(&BiometricAuthenticatorMac::OnAuthenticationCompleted,
                     weak_ptr_factory_.GetWeakPtr()));
}

void BiometricAuthenticatorMac::OnAuthenticationCompleted(bool result) {
  if (callback_.is_null()) {
    return;
  }

  std::move(callback_).Run(RecordAuthenticationResult(result));
  touch_id_auth_context_ = nullptr;
}
