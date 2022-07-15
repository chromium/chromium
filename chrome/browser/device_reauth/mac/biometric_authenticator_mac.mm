// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/device_reauth/mac/biometric_authenticator_mac.h"

#include "base/notreached.h"
#include "components/device_reauth/biometric_authenticator.h"
#include "device/fido/mac/touch_id_context.h"

BiometricAuthenticatorMac::BiometricAuthenticatorMac() {
  touchIdAuthObject_ = device::fido::mac::TouchIdContext::Create();
}

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
  this->touchIdAuthObject_->PromptTouchId(message, std::move(callback));
}