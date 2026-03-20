// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/finds/core/finds_utils.h"

#include "base/metrics/field_trial_params.h"
#include "base/notreached.h"
#include "chrome/browser/finds/core/finds_pref_names.h"

namespace finds {

namespace {

using SuggestionTheme =
    optimization_guide::proto::FindsSuggestionResponse::SuggestionTheme;

// Constants for the theme keys correlated with the DictPref for each theme's
// not interested last timestamp.
const char kThemeEventsAndActivities[] = "EventsAndActivities";
const char kThemeFoodAndDining[] = "FoodAndDining";
const char kThemeEntertainment[] = "Entertainment";
const char kThemeShopping[] = "Shopping";
const char kThemeTravel[] = "Travel";

}  // namespace

std::string ThemeTypeEnumToString(SuggestionTheme::ThemeType theme_type) {
  switch (theme_type) {
    case SuggestionTheme::EVENTS_AND_ACTIVITIES:
      return kThemeEventsAndActivities;
    case SuggestionTheme::FOOD_AND_DINING:
      return kThemeFoodAndDining;
    case SuggestionTheme::ENTERTAINMENT:
      return kThemeEntertainment;
    case SuggestionTheme::SHOPPING:
      return kThemeShopping;
    case SuggestionTheme::TRAVEL:
      return kThemeTravel;
    case SuggestionTheme::UNKNOWN:
      // Fall-through to default case.
    default:
      return "";
  }
}

}  // namespace finds
