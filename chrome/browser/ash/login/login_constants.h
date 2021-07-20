// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_LOGIN_CONSTANTS_H_
#define CHROME_BROWSER_ASH_LOGIN_LOGIN_CONSTANTS_H_

#include "base/time/time.h"

namespace chromeos {

namespace constants {

// This constant value comes from the policy definitions of the offline signin
// limiter policies. The value -1 means that online authentication will not be
// enforced by `OfflineSigninLimiter` so the user will be allowed to use offline
// authentication until a different reason than this policy enforces an online
// login.
constexpr int kOfflineSigninTimeLimitNotSet = -1;

// The value -2 means match the offline sign in time limit of the login screen,
// so policies GaiaLockScreenOfflineSigninTimeLimitDays would get the same value
// as GaiaOfflineSigninTimeLimitDays and
// SamlLockScreenOfflineSigninTimeLimitDays would get the same value as
// SAMLOfflineSigninTimeLimit
constexpr int kLockScreenOfflineSigninTimeLimitDaysMatchLogin = -2;

constexpr int kDefaultGaiaOfflineSigninTimeLimitDays =
    kOfflineSigninTimeLimitNotSet;
constexpr int kDefaultSAMLOfflineSigninTimeLimit =
    base::TimeDelta::FromDays(14).InSeconds();

constexpr int kDefaultGaiaLockScreenOfflineSigninTimeLimitDays =
    kLockScreenOfflineSigninTimeLimitDaysMatchLogin;
constexpr int kDefaultSamlLockScreenOfflineSigninTimeLimitDays =
    kLockScreenOfflineSigninTimeLimitDaysMatchLogin;

// In-session password-change feature (includes password expiry notifications).
const bool kDefaultSamlInSessionPasswordChangeEnabled = false;
const int kDefaultSamlPasswordExpirationAdvanceWarningDays = 14;

// Online reauthentication on the lock screen.
const bool kDefaultLockScreenReauthenticationEnabled = false;

}  // namespace constants

}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace ash {
namespace constants {
using ::chromeos::constants::kDefaultGaiaLockScreenOfflineSigninTimeLimitDays;
using ::chromeos::constants::kDefaultGaiaOfflineSigninTimeLimitDays;
using ::chromeos::constants::kDefaultLockScreenReauthenticationEnabled;
using ::chromeos::constants::kDefaultSAMLOfflineSigninTimeLimit;
using ::chromeos::constants::kDefaultSamlPasswordExpirationAdvanceWarningDays;
using ::chromeos::constants::kDefaultSamlInSessionPasswordChangeEnabled;
using ::chromeos::constants::kDefaultSamlLockScreenOfflineSigninTimeLimitDays;
using ::chromeos::constants::kLockScreenOfflineSigninTimeLimitDaysMatchLogin;
using ::chromeos::constants::kOfflineSigninTimeLimitNotSet;
}  // namespace constants
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_LOGIN_CONSTANTS_H_
