// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FINDS_CORE_FINDS_PREF_NAMES_H_
#define CHROME_BROWSER_FINDS_CORE_FINDS_PREF_NAMES_H_

namespace finds::prefs {

// The timestamp of the last successful model execution for the Finds service.
// Stored as an integer (base::Time's internal value).
extern const char kFindsModelExecutionLastTimestamp[];

// A dictionary storing the last time a user marked a theme as "not
// interested". The keys are the theme strings (e.g., kThemeEventsAndActivities)
// and the values are timestamps (as doubles).
extern const char kFindsNotInterestedThemesLastTimestamp[];

// A boolean of whether the finds opt in promo was interacted with by the user,
// indicating that we should not prompt them again.
extern const char kFindsOptInPromoUserInteracted[];

// The number of times the finds opt in promo was shown to the user.
extern const char kFindsOptInPromoShownCount[];

// The timestamp of the last time the finds opt in promo was shown to the user.
extern const char kFindsOptInPromoLastShownTimestamp[];

}  // namespace finds::prefs

#endif  // CHROME_BROWSER_FINDS_CORE_FINDS_PREF_NAMES_H_
