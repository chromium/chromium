// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/easy_unlock/easy_unlock_user_login_flow.h"

#include "base/metrics/histogram_macros.h"
#include "chrome/browser/ash/login/easy_unlock/easy_unlock_service.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chromeos/ash/components/login/auth/public/auth_failure.h"

namespace ash {

EasyUnlockUserLoginFlow::EasyUnlockUserLoginFlow(const AccountId& account_id)
    : ExtendedUserFlow(account_id) {}

EasyUnlockUserLoginFlow::~EasyUnlockUserLoginFlow() {}

bool EasyUnlockUserLoginFlow::HandleLoginFailure(const AuthFailure& failure) {
  Profile* profile = ProfileHelper::GetSigninProfile();
  EasyUnlockService* service = EasyUnlockService::Get(profile);
  if (!service)
    return false;
  service->HandleAuthFailure(account_id());
  service->RecordEasySignInOutcome(account_id(), false);

  SmartLockMetricsRecorder::RecordAuthResultSignInFailure(
      SmartLockMetricsRecorder::SmartLockAuthResultFailureReason::
          kUserControllerSignInFailure);
  UMA_HISTOGRAM_ENUMERATION(
      "SmartLock.AuthResult.SignIn.Failure.UserControllerAuth",
      failure.reason(), AuthFailure::FailureReason::NUM_FAILURE_REASONS);

  UnregisterFlowSoon();
  return true;
}

void EasyUnlockUserLoginFlow::HandleLoginSuccess(const UserContext& context) {
  Profile* profile = ProfileHelper::GetSigninProfile();
  EasyUnlockService* service = EasyUnlockService::Get(profile);
  if (!service)
    return;
  service->RecordEasySignInOutcome(account_id(), true);
  SmartLockMetricsRecorder::RecordAuthResultSignInSuccess();
}

}  // namespace ash
