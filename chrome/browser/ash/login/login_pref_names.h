// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_LOGIN_PREF_NAMES_H_
#define CHROME_BROWSER_ASH_LOGIN_LOGIN_PREF_NAMES_H_

namespace ash {
namespace prefs {

extern const char kLastLoginInputMethod[];
extern const char kOobeComplete[];
extern const char kOobeOnboardingTime[];
extern const char kOobeMarketingOptInScreenFinished[];
extern const char kOobeMarketingOptInChoice[];
// TODO(https://crbug.com/1322394): deprecate this pref once update from
// CloudReady won't be available anymore.
extern const char kOobeRevenUpdatedToFlex[];
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
extern const char kOobeGuestMetricsEnabled[];
extern const char kOobeGuestAcceptedTos[];
extern const char kOobeLocaleChangedOnWelcomeScreen[];
// TODO(https://crbug.com/1322394): deprecate this pref once update from
// CloudReady won't be available anymore.
extern const char kRevenOobeConsolidatedConsentAccepted[];
extern const char kUrlParameterToAutofillSAMLUsername[];

}  // namespace prefs
}  // namespace ash

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace chromeos {
namespace prefs {
using ::ash::prefs::kLastLoginInputMethod;
using ::ash::prefs::kSamlInSessionPasswordChangeEnabled;
}  // namespace prefs
}  // namespace chromeos

#endif  // CHROME_BROWSER_ASH_LOGIN_LOGIN_PREF_NAMES_H_
