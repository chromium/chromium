// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_LOGIN_PREF_NAMES_H_
#define CHROME_BROWSER_ASH_LOGIN_LOGIN_PREF_NAMES_H_

namespace chromeos {

namespace prefs {

extern const char kLastLoginInputMethod[];
extern const char kOobeComplete[];
extern const char kOobeOnboardingTime[];
extern const char kOobeMarketingOptInScreenFinished[];
extern const char kOobeMarketingOptInChoice[];
extern const char kOobeScreenPending[];
extern const char kGaiaOfflineSigninTimeLimitDays[];
extern const char kGaiaLastOnlineSignInTime[];
extern const char kSAMLOfflineSigninTimeLimit[];
extern const char kSAMLLastGAIASignInTime[];
extern const char kGaiaLockScreenOfflineSigninTimeLimitDays[];
extern const char kSamlLockScreenOfflineSigninTimeLimitDays[];
extern const char kSamlInSessionPasswordChangeEnabled[];
extern const char kSamlPasswordExpirationAdvanceWarningDays[];
extern const char kLockScreenReauthenticationEnabled[];
extern const char kSamlPasswordSyncToken[];
extern const char kActivityTimeAfterOnboarding[];

}  // namespace prefs

}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace ash {
namespace prefs {
using ::chromeos::prefs::kGaiaLastOnlineSignInTime;
using ::chromeos::prefs::kGaiaLockScreenOfflineSigninTimeLimitDays;
using ::chromeos::prefs::kGaiaOfflineSigninTimeLimitDays;
using ::chromeos::prefs::kLastLoginInputMethod;
using ::chromeos::prefs::kLockScreenReauthenticationEnabled;
using ::chromeos::prefs::kOobeMarketingOptInChoice;
using ::chromeos::prefs::kOobeMarketingOptInScreenFinished;
using ::chromeos::prefs::kOobeOnboardingTime;
using ::chromeos::prefs::kOobeScreenPending;
using ::chromeos::prefs::kSamlInSessionPasswordChangeEnabled;
using ::chromeos::prefs::kSAMLLastGAIASignInTime;
using ::chromeos::prefs::kSamlLockScreenOfflineSigninTimeLimitDays;
using ::chromeos::prefs::kSAMLOfflineSigninTimeLimit;
using ::chromeos::prefs::kSamlPasswordExpirationAdvanceWarningDays;
using ::chromeos::prefs::kSamlPasswordSyncToken;
}
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_LOGIN_PREF_NAMES_H_
