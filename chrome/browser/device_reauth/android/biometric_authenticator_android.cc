// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/device_reauth/android/biometric_authenticator_android.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/feature_list.h"
#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "chrome/browser/device_reauth/android/biometric_authenticator_bridge_impl.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/device_reauth/biometric_authenticator.h"
#include "components/password_manager/core/browser/origin_credential_store.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/android/view_android.h"

using content::WebContents;
using device_reauth::BiometricAuthFinalResult;
using device_reauth::BiometricAuthUIResult;
using device_reauth::BiometricsAvailability;
using password_manager::UiCredential;

namespace {

constexpr unsigned int kAuthValidSeconds = 60;

bool IsSuccessfulResult(BiometricAuthUIResult result) {
  return result == BiometricAuthUIResult::kSuccessWithUnknownMethod ||
         result == BiometricAuthUIResult::kSuccessWithBiometrics ||
         result == BiometricAuthUIResult::kSuccessWithDeviceLock;
}

BiometricAuthFinalResult MapUIResultToFinal(BiometricAuthUIResult result) {
  switch (result) {
    case BiometricAuthUIResult::kSuccessWithUnknownMethod:
      return BiometricAuthFinalResult::kSuccessWithUnknownMethod;
    case BiometricAuthUIResult::kSuccessWithBiometrics:
      return BiometricAuthFinalResult::kSuccessWithBiometrics;
    case BiometricAuthUIResult::kSuccessWithDeviceLock:
      return BiometricAuthFinalResult::kSuccessWithDeviceLock;
    case BiometricAuthUIResult::kCanceledByUser:
      return BiometricAuthFinalResult::kCanceledByUser;
    case BiometricAuthUIResult::kFailed:
      return BiometricAuthFinalResult::kFailed;
  }
}

bool isPasswordManagerRequester(
    const device_reauth::BiometricAuthRequester& requester) {
  switch (requester) {
    case device_reauth::BiometricAuthRequester::kTouchToFill:
    case device_reauth::BiometricAuthRequester::kAutofillSuggestion:
    case device_reauth::BiometricAuthRequester::kFallbackSheet:
    case device_reauth::BiometricAuthRequester::kAllPasswordsList:
    case device_reauth::BiometricAuthRequester::kAccountChooserDialog:
    case device_reauth::BiometricAuthRequester::kPasswordCheckAutoPwdChange:
      return true;
    case device_reauth::BiometricAuthRequester::kIncognitoReauthPage:
      return false;
  }
}

void LogAuthResult(const device_reauth::BiometricAuthRequester& requester,
                   const BiometricAuthFinalResult& result) {
  if (isPasswordManagerRequester(requester)) {
    base::UmaHistogramEnumeration(
        "PasswordManager.BiometricAuthPwdFill.AuthResult", result);
  }
}

void LogAuthRequester(const device_reauth::BiometricAuthRequester& requester) {
  // TODO(crbug.com/1263397): The
  // "PasswordManager.BiometricAuthPwdFill.AuthRequester" should be removed once
  // the "Android.BiometricAuth.AuthRequester" is saturated and adopted.
  if (isPasswordManagerRequester(requester)) {
    base::UmaHistogramEnumeration(
        "PasswordManager.BiometricAuthPwdFill.AuthRequester", requester);
  }
  base::UmaHistogramEnumeration("Android.BiometricAuth.AuthRequester",
                                requester);
}

void LogCanAuthenticate(const device_reauth::BiometricAuthRequester& requester,
                        const BiometricsAvailability& availability) {
  if (isPasswordManagerRequester(requester)) {
    base::UmaHistogramEnumeration(
        "PasswordManager.BiometricAuthPwdFill.CanAuthenticate", availability);
  }
}

}  // namespace

BiometricAuthenticatorAndroid::BiometricAuthenticatorAndroid(
    std::unique_ptr<BiometricAuthenticatorBridge> bridge)
    : bridge_(std::move(bridge)) {}

BiometricAuthenticatorAndroid::~BiometricAuthenticatorAndroid() {}

BiometricsAvailability BiometricAuthenticatorAndroid::CanAuthenticate(
    device_reauth::BiometricAuthRequester requester) {
  BiometricsAvailability availability = bridge_->CanAuthenticate();
  LogCanAuthenticate(requester, availability);
  return availability;
}

void BiometricAuthenticatorAndroid::Authenticate(
    device_reauth::BiometricAuthRequester requester,
    AuthenticateCallback callback) {
  // Previous authentication is not yet completed, so return.
  if (callback_ || requester_.has_value())
    return;

  callback_ = std::move(callback);
  requester_ = requester;

  LogAuthRequester(requester);

  if (last_good_auth_timestamp_.has_value() &&
      base::TimeTicks::Now() - last_good_auth_timestamp_.value() <
          base::Seconds(kAuthValidSeconds)) {
    LogAuthResult(requester, BiometricAuthFinalResult::kAuthStillValid);
    std::move(callback_).Run(/*success=*/true);
    requester_ = absl::nullopt;
    return;
  }
  // `this` owns the bridge so it's safe to use base::Unretained.
  bridge_->Authenticate(
      base::BindOnce(&BiometricAuthenticatorAndroid::OnAuthenticationCompleted,
                     base::Unretained(this)));
}

void BiometricAuthenticatorAndroid::Cancel(
    device_reauth::BiometricAuthRequester requester) {
  // The object cancelling the auth is not the same as the one to which
  // the ongoing auth corresponds.
  if (!requester_.has_value() || requester != requester_.value())
    return;

  LogAuthResult(requester, BiometricAuthFinalResult::kCanceledByChrome);

  callback_.Reset();
  requester_ = absl::nullopt;
  bridge_->Cancel();
}

// static
scoped_refptr<BiometricAuthenticatorAndroid>
BiometricAuthenticatorAndroid::CreateForTesting(
    std::unique_ptr<BiometricAuthenticatorBridge> bridge) {
  return base::WrapRefCounted(
      new BiometricAuthenticatorAndroid(std::move(bridge)));
}

void BiometricAuthenticatorAndroid::OnAuthenticationCompleted(
    BiometricAuthUIResult ui_result) {
  bool success = IsSuccessfulResult(ui_result);
  // OnAuthenticationCompleted is called aysnchronously and by the time it's
  // invoked Chrome can cancel the authentication via
  // BiometricAuthenticatorAndroid::Cancel which resets the callback_.
  if (callback_.is_null()) {
    return;
  }

  if (success) {
    last_good_auth_timestamp_ = base::TimeTicks::Now();
  }

  LogAuthResult(requester_.value(), MapUIResultToFinal(ui_result));
  std::move(callback_).Run(success);
  requester_ = absl::nullopt;
}
