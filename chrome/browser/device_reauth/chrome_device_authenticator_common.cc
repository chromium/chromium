// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/device_reauth/chrome_device_authenticator_common.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/device_reauth/chrome_device_authenticator_factory.h"

ChromeDeviceAuthenticatorCommon::ChromeDeviceAuthenticatorCommon(
    DeviceAuthenticatorProxy* proxy,
    base::TimeDelta auth_validity_period)
    : device_authenticator_proxy_(proxy->GetWeakPtr()),
      auth_validity_period_(auth_validity_period) {}
ChromeDeviceAuthenticatorCommon::~ChromeDeviceAuthenticatorCommon() = default;

void ChromeDeviceAuthenticatorCommon::RecordAuthenticationTimeIfSuccessful(
    bool success) {
  if (!success) {
    return;
  }
  device_authenticator_proxy_->UpdateLastGoodAuthTimestamp();
}

bool ChromeDeviceAuthenticatorCommon::NeedsToAuthenticate() const {
  auto last_good_auth_timestamp =
      device_authenticator_proxy_->GetLastGoodAuthTimestamp();

  return !last_good_auth_timestamp.has_value() ||
         base::TimeTicks::Now() - last_good_auth_timestamp.value() >=
             auth_validity_period_;
}
