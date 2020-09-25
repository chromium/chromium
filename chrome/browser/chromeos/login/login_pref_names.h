// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_LOGIN_PREF_NAMES_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_LOGIN_PREF_NAMES_H_

namespace chromeos {

namespace prefs {

extern const char kLastLoginInputMethod[];
extern const char kOobeOnboardingTime[];
extern const char kSAMLOfflineSigninTimeLimit[];
extern const char kSAMLLastGAIASignInTime[];
extern const char kSamlInSessionPasswordChangeEnabled[];
extern const char kSamlPasswordExpirationAdvanceWarningDays[];
extern const char kSamlLockScreenReauthenticationEnabled[];
extern const char kSamlPasswordSyncToken[];

}  // namespace prefs

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_LOGIN_PREF_NAMES_H_
