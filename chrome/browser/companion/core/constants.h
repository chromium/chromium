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
const char kExpsPromoShownCountPref[] = "Companion.Promo.Exps.Shown.Count";
const char kPcoPromoShownCountPref[] = "Companion.Promo.PCO.Shown.Count";
const char kPcoPromoDeclinedCountPref[] = "Companion.Promo.PCO.Declined.Count";

// Pref name for storing experience opt-in status.
const char kExpsOptInStatusGrantedPref[] =
    "Companion.Exps.OptIn.Status.Granted";

// Pref name used for tracking whether the user has ever successfully navigated
// to exps registration success page.
const char kHasNavigatedToExpsSuccessPage[] =
    "Companion.HasNavigatedToExpsSuccessPage";

}  // namespace companion

#endif  // CHROME_BROWSER_COMPANION_CORE_CONSTANTS_H_
