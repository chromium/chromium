// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/device_reauth/chrome_biometric_authenticator_common.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "components/password_manager/core/browser/password_access_authenticator.h"

namespace {

using password_manager::PasswordAccessAuthenticator;

}  // namespace

ChromeBiometricAuthenticatorCommon::ChromeBiometricAuthenticatorCommon() =
    default;
ChromeBiometricAuthenticatorCommon::~ChromeBiometricAuthenticatorCommon() =
    default;

void ChromeBiometricAuthenticatorCommon::RecordAuthenticationTimeIfSuccessful(
    bool success) {
  if (!success)
    return;
  last_good_auth_timestamp_ = base::TimeTicks::Now();

  // Holds scoped_refptr for kAuthValidityPeriod seconds, preventing object
  // from being deleted.
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          [](scoped_refptr<ChromeBiometricAuthenticatorCommon> ptr) {},
          base::WrapRefCounted(this)),
      PasswordAccessAuthenticator::kAuthValidityPeriod);
}

bool ChromeBiometricAuthenticatorCommon::NeedsToAuthenticate() const {
  return !last_good_auth_timestamp_.has_value() ||
         base::TimeTicks::Now() - last_good_auth_timestamp_.value() >=
             PasswordAccessAuthenticator::kAuthValidityPeriod;
}

base::WeakPtr<ChromeBiometricAuthenticatorCommon>
ChromeBiometricAuthenticatorCommon::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}
