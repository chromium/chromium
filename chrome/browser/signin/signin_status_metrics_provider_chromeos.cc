// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_status_metrics_provider_chromeos.h"

#include "chromeos/login/login_state/login_state.h"
#include "components/user_manager/user_manager.h"

SigninStatusMetricsProviderChromeOS::SigninStatusMetricsProviderChromeOS() {
  SetCurrentSigninStatus();
}

SigninStatusMetricsProviderChromeOS::~SigninStatusMetricsProviderChromeOS() {
}

void SigninStatusMetricsProviderChromeOS::ProvideCurrentSessionData(
    metrics::ChromeUserMetricsExtension* uma_proto) {
  // Compare the current logged-in status with the recorded one and compute
  // sign-in status that should be reported.
  RecordSigninStatusHistogram(ComputeSigninStatusToUpload(
      signin_status(), IsSignedInNonGuest()));
  // After reporting the sign-in status for previous UMA session, start fresh
  // again regardless what was the status before.
  ResetSigninStatus();
  SetCurrentSigninStatus();
}

void SigninStatusMetricsProviderChromeOS::SetCurrentSigninStatus() {
  SigninStatus status = IsSignedInNonGuest() ? ALL_PROFILES_SIGNED_IN
                                             : ALL_PROFILES_NOT_SIGNED_IN;
  UpdateSigninStatus(status);
}

SigninStatusMetricsProviderBase::SigninStatus
SigninStatusMetricsProviderChromeOS::ComputeSigninStatusToUpload(
    SigninStatusMetricsProviderBase::SigninStatus recorded_status,
    bool logged_in_now) {
  if ((recorded_status == ALL_PROFILES_SIGNED_IN && logged_in_now) ||
      (recorded_status == ALL_PROFILES_NOT_SIGNED_IN && !logged_in_now)) {
    // If the status hasn't changed since we last recorded, report as it is.
    return recorded_status;
  } else if (recorded_status == ALL_PROFILES_NOT_SIGNED_IN && logged_in_now) {
    // It possible that the browser goes from not signed-in to signed-in (i.e.
    // user performed a login action through the login manager.)
    return MIXED_SIGNIN_STATUS;
  } else {
    // There should not be other possibilities, for example the browser on
    // ChromeOS can not go from signed-in to not signed-in. Record it as an
    // error.
    return ERROR_GETTING_SIGNIN_STATUS;
  }
}

bool SigninStatusMetricsProviderChromeOS::IsSignedInNonGuest() {
  if (guest_for_testing_.has_value())
    return !guest_for_testing_.value();

  return user_manager::UserManager::IsInitialized() &&
         user_manager::UserManager::Get()->IsUserLoggedIn() &&
         chromeos::LoginState::Get() &&
         !chromeos::LoginState::Get()->IsGuestSessionUser();
}

void SigninStatusMetricsProviderChromeOS::SetGuestForTesting(bool is_guest) {
  guest_for_testing_ = is_guest;
}

absl::optional<bool> SigninStatusMetricsProviderChromeOS::guest_for_testing_;
