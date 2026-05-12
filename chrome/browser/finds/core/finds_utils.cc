// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/finds/core/finds_utils.h"

#include "base/metrics/field_trial_params.h"
#include "base/notreached.h"
#include "chrome/browser/finds/core/finds_features.h"
#include "chrome/browser/finds/core/finds_pref_names.h"
#include "components/optimization_guide/core/feature_registry/feature_registration.h"
#include "components/optimization_guide/core/model_execution/model_execution_prefs.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"
#include "components/unified_consent/pref_names.h"

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

void MarkModelExecutionLastTimestamp(PrefService* pref_service) {
  // Update model execution cooldown timestamp.
  pref_service->SetInt64(prefs::kFindsModelExecutionLastTimestamp,
                         base::Time::Now().InMillisecondsSinceUnixEpoch());
}

void MarkThemeAsNotInterested(PrefService* pref_service,
                              SuggestionTheme::ThemeType theme_type) {
  const std::string theme_pref_string = ThemeTypeEnumToString(theme_type);
  if (theme_pref_string.empty()) {
    // Do not set a pref if the theme type is unknown.
    return;
  }
  // Store as a double since base::DictValue only supports storing doubles, but
  // the value is essentially an int64_t timestamp.
  ScopedDictPrefUpdate update(pref_service,
                              prefs::kFindsNotInterestedThemesLastTimestamp);
  update->Set(theme_pref_string, base::Time::Now().InSecondsFSinceUnixEpoch());
}

base::TimeDelta GetModelExecutionCooldownDurationTimeDelta() {
  return base::Days(features::kModelExecutionCooldownDurationInDays.Get());
}

bool IsAllowedByEnterprisePolicy(PrefService* pref_service) {
  if (!pref_service) {
    return false;
  }
  return pref_service->GetInteger(
             optimization_guide::prefs::kFindsEnterprisePolicyAllowed) !=
         static_cast<int>(optimization_guide::model_execution::prefs::
                              ModelExecutionEnterprisePolicyValue::kDisable);
}

bool IsHistorySyncAndMsbbEnabled(syncer::SyncService* sync_service,
                                 PrefService* pref_service) {
  if (!sync_service || !pref_service) {
    return false;
  }
  const syncer::SyncUserSettings* user_settings =
      sync_service->GetUserSettings();
  return user_settings &&
         user_settings->GetSelectedTypes().Has(
             syncer::UserSelectableType::kHistory) &&
         pref_service->GetBoolean(
             unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled);
}

}  // namespace finds
