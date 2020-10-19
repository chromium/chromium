// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_NTP_FEATURES_H_
#define CHROME_BROWSER_SEARCH_NTP_FEATURES_H_

#include "base/feature_list.h"

namespace base {
class Time;
}  // namespace base

namespace ntp_features {

// The features should be documented alongside the definition of their values in
// the .cc file.

extern const base::Feature kConfirmSuggestionRemovals;
extern const base::Feature kDismissPromos;
extern const base::Feature kIframeOneGoogleBar;
extern const base::Feature kNtpRepeatableQueries;
extern const base::Feature kOneGoogleBarModalOverlays;
extern const base::Feature kRealboxMatchOmniboxTheme;
extern const base::Feature kRealboxUseGoogleGIcon;
extern const base::Feature kWebUI;
extern const base::Feature kWebUIThemeModeDoodles;
extern const base::Feature kModules;
extern const base::Feature kNtpRecipeTasksModule;
extern const base::Feature kNtpShoppingTasksModule;

extern const base::Feature kSearchSuggestChips;
extern const base::Feature kDisableSearchSuggestChips;

// Parameter name determining the age threshold in days for local history
// repeatable queries.
// The value of this parameter should be parsable as an unsigned integer.
extern const char kNtpRepeatableQueriesAgeThresholdDaysParam[];
// Parameter name determining the number of seconds until the recency component
// of the frecency score for local history repeatable queries decays to half.
// The value of this parameter should be parsable as an unsigned integer.
extern const char kNtpRepeatableQueriesRecencyHalfLifeSecondsParam[];
// Parameter name determining the factor by which the frequency component of the
// frecency score for local history repeatable queries is exponentiated.
// The value of this parameter should be parsable as a double.
extern const char kNtpRepeatableQueriesFrequencyExponentParam[];

// Returns the age threshold for local history repeatable queries.
base::Time GetLocalHistoryRepeatableQueriesAgeThreshold();
// Returns the number of seconds until the recency component of the frecency
// score for local history repeatable queries decays to half.
int GetLocalHistoryRepeatableQueriesRecencyHalfLifeSeconds();
// Returns the factor by which the frequency component of the frecency score for
// local history repeatable queries is exponentiated.
double GetLocalHistoryRepeatableQueriesFrequencyExponent();

}  // namespace ntp_features

#endif  // CHROME_BROWSER_SEARCH_NTP_FEATURES_H_
