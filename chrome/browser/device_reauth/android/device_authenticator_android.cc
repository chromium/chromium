// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/device_reauth/android/device_authenticator_android.h"

#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
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
#include "third_party/abseil-cpp/absl/types/optional.h"
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
      return DeviceAuthFinalResult::kFailed;
  }
}

// Checks whether authentication request was made by the password manager on
// Android.
bool isAndroidPasswordManagerRequester(
    const device_reauth::DeviceAuthRequester& requester) {
  switch (requester) {
    case device_reauth::DeviceAuthRequester::kTouchToFill:
    case device_reauth::DeviceAuthRequester::kAutofillSuggestion:
    case device_reauth::DeviceAuthRequester::kFallbackSheet:
    case device_reauth::DeviceAuthRequester::kAllPasswordsList:
    case device_reauth::DeviceAuthRequester::kAccountChooserDialog:
    case device_reauth::DeviceAuthRequester::kPasswordCheckAutoPwdChange:
      return true;
    case device_reauth::DeviceAuthRequester::kIncognitoReauthPage:
    // kPasswordsInSettings flag is used only for desktop.
    case device_reauth::DeviceAuthRequester::kPasswordsInSettings:
    case device_reauth::DeviceAuthRequester::kLocalCardAutofill:
    case device_reauth::DeviceAuthRequester::kDeviceLockPage:
    case device_reauth::DeviceAuthRequester::kPaymentMethodsReauthInSettings:
    case device_reauth::DeviceAuthRequester::kVirtualCardAutofill:
    case device_reauth::DeviceAuthRequester::kPaymentsAutofillOptIn:
      return false;
  }
}

void LogAuthResult(const device_reauth::DeviceAuthRequester& requester,
                   const DeviceAuthFinalResult& result) {
  if (isAndroidPasswordManagerRequester(requester)) {
    base::UmaHistogramEnumeration(
        "PasswordManager.BiometricAuthPwdFill.AuthResult", result);
  } else if (device_reauth::DeviceAuthRequester::kIncognitoReauthPage ==
             requester) {
    base::UmaHistogramEnumeration("Android.IncognitoReauth.AuthResult", result);
  }
}

void LogAuthRequester(const device_reauth::DeviceAuthRequester& requester) {
  base::UmaHistogramEnumeration("Android.BiometricAuth.AuthRequester",
                                requester);
}

void LogCanAuthenticate(const BiometricsAvailability& availability) {
  base::UmaHistogramEnumeration(
      "PasswordManager.BiometricAuthPwdFill.CanAuthenticate", availability);
}

}  // namespace

DeviceAuthenticatorAndroid::DeviceAuthenticatorAndroid(
    std::unique_ptr<DeviceAuthenticatorBridge> bridge)
    : bridge_(std::move(bridge)) {}

DeviceAuthenticatorAndroid::~DeviceAuthenticatorAndroid() = default;

bool DeviceAuthenticatorAndroid::CanAuthenticateWithBiometrics() {
  BiometricsAvailability availability = bridge_->CanAuthenticateWithBiometric();
  LogCanAuthenticate(availability);
  return availability == BiometricsAvailability::kAvailable;
}

bool DeviceAuthenticatorAndroid::CanAuthenticateWithBiometricOrScreenLock() {
  return bridge_->CanAuthenticateWithBiometricOrScreenLock();
}

void DeviceAuthenticatorAndroid::Authenticate(
    device_reauth::DeviceAuthRequester requester,
    AuthenticateCallback callback,
    bool use_last_valid_auth) {
  // Previous authentication is not yet completed, so return.
  if (callback_ || requester_.has_value()) {
    return;
  }

  callback_ = std::move(callback);
  requester_ = requester;

  LogAuthRequester(requester);

  if (use_last_valid_auth && !NeedsToAuthenticate()) {
    LogAuthResult(requester, DeviceAuthFinalResult::kAuthStillValid);
    std::move(callback_).Run(/*success=*/true);
    requester_ = absl::nullopt;
    return;
  }
  // `this` owns the bridge so it's safe to use base::Unretained.
  bridge_->Authenticate(
      base::BindOnce(&DeviceAuthenticatorAndroid::OnAuthenticationCompleted,
                     base::Unretained(this)));
}

void DeviceAuthenticatorAndroid::AuthenticateWithMessage(
    const std::u16string& message,
    AuthenticateCallback callback) {
  NOTIMPLEMENTED();
}

void DeviceAuthenticatorAndroid::Cancel(
    device_reauth::DeviceAuthRequester requester) {
  // The object cancelling the auth is not the same as the one to which
  // the ongoing auth corresponds.
  if (!requester_.has_value() || requester != requester_.value()) {
    return;
  }

  LogAuthResult(requester, DeviceAuthFinalResult::kCanceledByChrome);

  callback_.Reset();
  requester_ = absl::nullopt;
  bridge_->Cancel();
}

// static
scoped_refptr<DeviceAuthenticatorAndroid>
DeviceAuthenticatorAndroid::CreateForTesting(
    std::unique_ptr<DeviceAuthenticatorBridge> bridge) {
  return base::WrapRefCounted(
      new DeviceAuthenticatorAndroid(std::move(bridge)));
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

  LogAuthResult(requester_.value(), MapUIResultToFinal(ui_result));
  std::move(callback_).Run(success);
  requester_ = absl::nullopt;
}
