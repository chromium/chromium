// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_CHROME_SIGNIN_PREF_NAMES_H_
#define CHROME_BROWSER_SIGNIN_CHROME_SIGNIN_PREF_NAMES_H_

namespace prefs {

// Dictionary pref to store the number of times a user declined profile creation
// per account. The key is the hash of the email.
inline constexpr char kProfileCreationInterceptionDeclined[] =
    "signin.ProfileCreationInterceptionDeclinedPref";

// Integer pref to store the number of times the password bubble signin promo
// has been shown per profile while the user is signed out.
inline constexpr char kPasswordSignInPromoShownCountPerProfile[] =
    "signin.PasswordSignInPromoShownCount";

// Integer pref to store the number of times the address bubble signin promo
// has been shown per profile while the user is signed out.
inline constexpr char kAddressSignInPromoShownCountPerProfile[] =
    "signin.AddressSignInPromoShownCount";

// Integer pref to store the number of times the bookmark bubble signin promo
// has been shown per profile while the user is signed out.
inline constexpr char kBookmarkSignInPromoShownCountPerProfile[] =
    "signin.BookmarkSignInPromoShownCount";

// A timestamp of the last time the history sync promo was dismissed.
inline constexpr char
    kHistoryPageHistorySyncPromoLastDismissedTimestampPerProfile[] =
        "signin.HistoryPageHistorySyncPromoLastDismissedTimestampPerProfile";

// A boolean pref to store whether the history sync promo was shown one time
// after dismissal.
inline constexpr char
    kHistoryPageHistorySyncPromoShownAfterDismissalPerProfile[] =
        "signin.HistoryPageHistorySyncPromoShownAfterDismissalPerProfile";

// Integer pref to store the number of times the history sync promo has been
// shown on the history page per profile while the user is signed out.
inline constexpr char kHistoryPageHistorySyncPromoShownCountPerProfile[] =
    "signin.HistoryPageHistorySyncPromoShownCount";

// Integer pref to store the number of times any autofill bubble signin promo
// has been dismissed per profile while the user is signed out. This also
// includes the bookmark bubble after `UnoPhase2FollowUp` is enabled.
inline constexpr char kAutofillSignInPromoDismissCountPerProfile[] =
    "signin.AutofillSignInPromoDismissCount";

// Integer pref to store the number of times any address bubble signin promo
// has been dismissed per profile while the user is signed out.
inline constexpr char
    kAddressSignInPromoDismissCountPerProfileForLimitsExperiment[] =
        "signin.AddressSignInPromoDismissCountForLimitsExperiment";

// Integer pref to store the number of times the password bubble signin promo
// has been dismissed per profile while the user is signed out.
inline constexpr char
    kPasswordSignInPromoDismissCountPerProfileForLimitsExperiment[] =
        "signin.PasswordSignInPromoDismissCountForLimitsExperiment";

// Integer pref to store the number of times the bookmark bubble signin promo
// has been dismissed per profile while the user is signed out.
inline constexpr char
    kBookmarkSignInPromoDismissCountPerProfileForLimitsExperiment[] =
        "signin.BookmarkSignInPromoDismissCountForLimitsExperiment";

}  // namespace prefs

#endif  // CHROME_BROWSER_SIGNIN_CHROME_SIGNIN_PREF_NAMES_H_
