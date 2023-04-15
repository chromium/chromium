// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPANION_CORE_CONSTANTS_H_
#define CHROME_BROWSER_COMPANION_CORE_CONSTANTS_H_

namespace companion {

// Pref names for storing various promo states.
const char kMsbbPromoDeclinedCountPref[] =
    "Companion.Promo.MSBB.Declined.Count";
const char kSigninPromoDeclinedCountPref[] =
    "Companion.Promo.Signin.Declined.Count";
const char kExpsPromoDeclinedCountPref[] =
    "Companion.Promo.Exps.Declined.Count";

// Pref name for storing experience opt-in status.
const char kExpsOptInStatusGrantedPref[] =
    "Companion.Exps.OptIn.Status.Granted";

}  // namespace companion

#endif  // CHROME_BROWSER_COMPANION_CORE_CONSTANTS_H_
