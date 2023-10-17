// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/device_reauth/chrome_device_authenticator_common.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/device_reauth/chrome_device_authenticator_factory.h"
#include "components/device_reauth/device_reauth_metrics_util.h"

using device_reauth::ReauthResult;

ChromeDeviceAuthenticatorCommon::ChromeDeviceAuthenticatorCommon(
    DeviceAuthenticatorProxy* proxy,
    base::TimeDelta auth_validity_period,
    const std::string& auth_result_histogram)
    : device_authenticator_proxy_(proxy->GetWeakPtr()),
      auth_validity_period_(auth_validity_period),
      auth_result_histogram_(std::move(auth_result_histogram)) {}
ChromeDeviceAuthenticatorCommon::~ChromeDeviceAuthenticatorCommon() = default;

void ChromeDeviceAuthenticatorCommon::RecordAuthenticationTimeIfSuccessful(
    bool success) {
  if (!auth_result_histogram_.empty()) {
    base::UmaHistogramEnumeration(
        auth_result_histogram_,
        success ? ReauthResult::kSuccess : ReauthResult::kFailure);
  }
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

void ChromeDeviceAuthenticatorCommon::RecordAuthResultSkipped() {
  if (!auth_result_histogram_.empty()) {
    base::UmaHistogramEnumeration(auth_result_histogram_,
                                  ReauthResult::kSkipped);
  }
}
