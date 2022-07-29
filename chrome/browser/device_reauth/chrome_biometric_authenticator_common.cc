// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/device_reauth/chrome_biometric_authenticator_common.h"

#include "base/time/time.h"

namespace {
// For how long after the last successful authentication a user is considered
// authenticated without repeating the challenge and how long authenticator
// object is being preserved from being deleted.
constexpr base::TimeDelta kAuthValidityPeriod = base::Seconds(60);

}  // namespace

ChromeBiometricAuthenticatorCommon::ChromeBiometricAuthenticatorCommon() =
    default;
ChromeBiometricAuthenticatorCommon::~ChromeBiometricAuthenticatorCommon() =
    default;

bool ChromeBiometricAuthenticatorCommon::RecordAuthenticationResult(
    bool success) {
  if (success) {
    last_good_auth_timestamp_ = base::TimeTicks::Now();
  }
  return success;
}

bool ChromeBiometricAuthenticatorCommon::NeedsToAuthenticate() {
  return !last_good_auth_timestamp_.has_value() ||
         base::TimeTicks::Now() - last_good_auth_timestamp_.value() >=
             kAuthValidityPeriod;
}
