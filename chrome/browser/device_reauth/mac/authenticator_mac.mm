// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/device_reauth/mac/authenticator_mac.h"

#import <LocalAuthentication/LAContext.h>

#include "base/mac/scoped_nsobject.h"

AuthenticatorMac::AuthenticatorMac() = default;

AuthenticatorMac::~AuthenticatorMac() = default;

bool AuthenticatorMac::CheckIfBiometricsAvailable() {
  base::scoped_nsobject<LAContext> context([[LAContext alloc] init]);
  return
      [context canEvaluatePolicy:LAPolicyDeviceOwnerAuthenticationWithBiometrics
                           error:nil];
}