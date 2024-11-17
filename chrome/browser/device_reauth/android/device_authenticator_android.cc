// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/device_reauth/android/device_authenticator_android.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "chrome/browser/device_reauth/android/device_authenticator_bridge_impl.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/device_reauth/device_authenticator.h"
#include "components/password_manager/core/browser/origin_credential_store.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/view_android.h"

using content::WebContents;
using device_reauth::BiometricsAvailability;
using device_reauth::DeviceAuthUIResult;
using password_manager::UiCredential;

namespace {

bool IsSuccessfulResult(DeviceAuthUIResult result) {
  return result == DeviceAuthUIResult::kSuccessWithUnknownMethod ||
         result == DeviceAuthUIResult::kSuccessWithBiometrics ||
         result == DeviceAuthUIResult::kSuccessWithDeviceLock;
}

DeviceAuthFinalResult MapUIResultToFinal(DeviceAuthUIResult result) {
  switch (result) {
    case DeviceAuthUIResult::kSuccessWithUnknownMethod:
      return DeviceAuthFinalResult::kSuccessWithUnknownMethod;
    case DeviceAuthUIResult::kSuccessWithBiometrics:
      return DeviceAuthFinalResult::kSuccessWithBiometrics;
    case DeviceAuthUIResult::kSuccessWithDeviceLock:
      return DeviceAuthFinalResult::kSuccessWithDeviceLock;
    case DeviceAuthUIResult::kCanceledByUser:
      return DeviceAuthFinalResult::kCanceledByUser;
    case DeviceAuthUIResult::kFailed:
    case DeviceAuthUIResult::kLockout:
      return DeviceAuthFinalResult::kFailed;
  }
}

void LogAuthResult(device_reauth::DeviceAuthSource source,
                   DeviceAuthFinalResult result) {
  if (device_reauth::DeviceAuthSource::kPasswordManager == source) {
    base::UmaHistogramEnumeration(
        "PasswordManager.BiometricAuthPwdFill.AuthResult", result);
  } else if (device_reauth::DeviceAuthSource::kIncognito == source) {
    base::UmaHistogramEnumeration("Android.IncognitoReauth.AuthResult", result);
  }
}

void LogAuthSource(device_reauth::DeviceAuthSource source) {
  base::UmaHistogramEnumeration("Android.DeviceAuthenticator.AuthSource",
                                source);
}

void LogCanAuthenticate(BiometricsAvailability availability) {
  base::UmaHistogramEnumeration(
      "Android.DeviceAuthenticator.CanAuthenticateWithBiometrics",
      availability);
  // TODO (crbug.com/350658581): Remove this histogram in favor of the above
  // because its name is misleading. Keeping now to track
  // `DeviceAuthenticatorAndroidx` experiment.
  base::UmaHistogramEnumeration(
      "PasswordManager.BiometricAuthPwdFill.CanAuthenticate", availability);
}

}  // namespace

DeviceAuthenticatorAndroid::DeviceAuthenticatorAndroid(
    std::unique_ptr<DeviceAuthenticatorBridge> bridge,
    DeviceAuthenticatorProxy* proxy,
    const device_reauth::DeviceAuthParams& params)
    : DeviceAuthenticatorCommon(proxy,
                                params.GetAuthenticationValidityPeriod(),
                                params.GetAuthResultHistogram()),
      bridge_(std::move(bridge)),
      source_(params.GetDeviceAuthSource()) {}

DeviceAuthenticatorAndroid::~DeviceAuthenticatorAndroid() = default;

bool DeviceAuthenticatorAndroid::CanAuthenticateWithBiometrics() {
  BiometricsAvailability availability = bridge_->CanAuthenticateWithBiometric();
  LogCanAuthenticate(availability);
  return availability == BiometricsAvailability::kAvailable;
}

bool DeviceAuthenticatorAndroid::CanAuthenticateWithBiometricOrScreenLock() {
  return bridge_->CanAuthenticateWithBiometricOrScreenLock();
}

void DeviceAuthenticatorAndroid::AuthenticateWithMessage(
    const std::u16string& message,
    AuthenticateCallback callback) {
  CHECK(message.empty())
      << "Android doesn't support messages for authentication dialog";

  // Previous authentication is not yet completed, so return.
  if (callback_) {
    return;
  }

  callback_ = std::move(callback);

  LogAuthSource(source_);

  if (!NeedsToAuthenticate()) {
    LogAuthResult(source_, DeviceAuthFinalResult::kAuthStillValid);
    // No code should be run after the callback as the callback could already be
    // destroying "this".
    std::move(callback_).Run(/*success=*/true);
    return;
  }
  // `this` owns the bridge so it's safe to use base::Unretained.
  bridge_->Authenticate(
      base::BindOnce(&DeviceAuthenticatorAndroid::OnAuthenticationCompleted,
                     base::Unretained(this)));
}

device_reauth::BiometricStatus
DeviceAuthenticatorAndroid::GetBiometricAvailabilityStatus() {
  BiometricsAvailability availability = bridge_->CanAuthenticateWithBiometric();
  switch (availability) {
    case device_reauth::BiometricsAvailability::kRequired:
    case device_reauth::BiometricsAvailability::kRequiredButHasError:
      return device_reauth::BiometricStatus::kRequired;
    case device_reauth::BiometricsAvailability::kAvailable:
      return device_reauth::BiometricStatus::kBiometricsAvailable;
    // TODO (crbug.com/369057610): Probably return status `kAvailable` for
    // BiometricsAvailability::kAvailableNoFallback case.
    case device_reauth::BiometricsAvailability::kAvailableNoFallback:
    case device_reauth::BiometricsAvailability::kNoHardware:
    case device_reauth::BiometricsAvailability::kHwUnavailable:
    case device_reauth::BiometricsAvailability::kNotEnrolled:
    case device_reauth::BiometricsAvailability::kSecurityUpdateRequired:
    case device_reauth::BiometricsAvailability::kAndroidVersionNotSupported:
    case device_reauth::BiometricsAvailability::kOtherError:
      break;
  }
  // TODO (crbug.com/368586157): Call just hasScreenLockSetUp here.
  if (CanAuthenticateWithBiometricOrScreenLock()) {
    return device_reauth::BiometricStatus::kOnlyLskfAvailable;
  }
  return device_reauth::BiometricStatus::kUnavailable;
}

void DeviceAuthenticatorAndroid::Cancel() {
  // There is no ongoing reauth to cancel.
  if (!callback_) {
    return;
  }
  LogAuthResult(source_, DeviceAuthFinalResult::kCanceledByChrome);

  callback_.Reset();
  bridge_->Cancel();
}

void DeviceAuthenticatorAndroid::OnAuthenticationCompleted(
    DeviceAuthUIResult ui_result) {
  // OnAuthenticationCompleted is called aysnchronously and by the time it's
  // invoked Chrome can cancel the authentication via
  // DeviceAuthenticatorAndroid::Cancel which resets the callback_.
  if (callback_.is_null()) {
    return;
  }

  bool success = IsSuccessfulResult(ui_result);
  RecordAuthenticationTimeIfSuccessful(success);

  LogAuthResult(source_, MapUIResultToFinal(ui_result));
  // No code should be run after the callback as the callback could already be
  // destroying "this".
  std::move(callback_).Run(success);
}
