// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/easy_unlock/easy_unlock_user_login_flow.h"

#include "base/metrics/histogram_macros.h"
#include "chrome/browser/chromeos/login/easy_unlock/easy_unlock_service.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"

namespace chromeos {

EasyUnlockUserLoginFlow::EasyUnlockUserLoginFlow(const AccountId& account_id)
    : ExtendedUserFlow(account_id) {}

EasyUnlockUserLoginFlow::~EasyUnlockUserLoginFlow() {}

bool EasyUnlockUserLoginFlow::CanLockScreen() {
  return true;
}

bool EasyUnlockUserLoginFlow::CanStartArc() {
  return true;
}

bool EasyUnlockUserLoginFlow::ShouldLaunchBrowser() {
  return true;
}

bool EasyUnlockUserLoginFlow::ShouldSkipPostLoginScreens() {
  return false;
}

bool EasyUnlockUserLoginFlow::HandleLoginFailure(const AuthFailure& failure) {
  SmartLockMetricsRecorder::RecordAuthResultSignInFailure(
      SmartLockMetricsRecorder::SmartLockAuthResultFailureReason::
          kUserControllerSignInFailure);
  UMA_HISTOGRAM_ENUMERATION(
      "SmartLock.AuthResult.SignIn.Failure.UserControllerAuth",
      failure.reason(), AuthFailure::FailureReason::NUM_FAILURE_REASONS);
  Profile* profile = ProfileHelper::GetSigninProfile();
  EasyUnlockService* service = EasyUnlockService::Get(profile);
  if (!service)
    return false;
  service->HandleAuthFailure(account_id());
  service->RecordEasySignInOutcome(account_id(), false);
  UnregisterFlowSoon();
  return true;
}

void EasyUnlockUserLoginFlow::HandleLoginSuccess(const UserContext& context) {
  Profile* profile = ProfileHelper::GetSigninProfile();
  EasyUnlockService* service = EasyUnlockService::Get(profile);
  if (!service)
    return;
  service->RecordEasySignInOutcome(account_id(), true);
}

void EasyUnlockUserLoginFlow::HandleOAuthTokenStatusChange(
    user_manager::User::OAuthTokenStatus status) {}

void EasyUnlockUserLoginFlow::LaunchExtraSteps(Profile* profile) {}

bool EasyUnlockUserLoginFlow::SupportsEarlyRestartToApplyFlags() {
  return true;
}

}  // namespace chromeos
