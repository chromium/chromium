// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/device_reauth/chrome_device_authenticator_common.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "components/password_manager/core/browser/password_access_authenticator.h"

namespace {

using password_manager::PasswordAccessAuthenticator;

}  // namespace

ChromeDeviceAuthenticatorCommon::ChromeDeviceAuthenticatorCommon() = default;
ChromeDeviceAuthenticatorCommon::~ChromeDeviceAuthenticatorCommon() = default;

void ChromeDeviceAuthenticatorCommon::RecordAuthenticationTimeIfSuccessful(
    bool success) {
  if (!success) {
    return;
  }
  last_good_auth_timestamp_ = base::TimeTicks::Now();

  // Holds scoped_refptr for kAuthValidityPeriod seconds, preventing object
  // from being deleted.
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce([](scoped_refptr<ChromeDeviceAuthenticatorCommon> ptr) {},
                     base::WrapRefCounted(this)),
      PasswordAccessAuthenticator::kAuthValidityPeriod);
}

bool ChromeDeviceAuthenticatorCommon::NeedsToAuthenticate() const {
  return !last_good_auth_timestamp_.has_value() ||
         base::TimeTicks::Now() - last_good_auth_timestamp_.value() >=
             PasswordAccessAuthenticator::kAuthValidityPeriod;
}

base::WeakPtr<ChromeDeviceAuthenticatorCommon>
ChromeDeviceAuthenticatorCommon::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}
