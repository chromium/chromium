// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FINDS_CORE_FINDS_UTILS_H_
#define CHROME_BROWSER_FINDS_CORE_FINDS_UTILS_H_

#include <string>

#include "base/time/time.h"
#include "components/optimization_guide/proto/features/finds.pb.h"
#include "components/prefs/pref_service.h"

namespace syncer {
class SyncService;
}  // namespace syncer

namespace finds {

// Converts a FindsSuggestionResponse::SuggestionTheme::ThemeType proto enum to
// its corresponding string representation used in preference names. Returns
// an empty string if the theme type is unknown.
std::string ThemeTypeEnumToString(
    optimization_guide::proto::FindsSuggestionResponse::SuggestionTheme::
        ThemeType theme_type);

// Update the PrefService with the timestamp of the last model execution for
// cooldown tracking.
void MarkModelExecutionLastTimestamp(PrefService* pref_service);

// Mark theme as not interested in the PrefService. This is called when the user
// clicks the finds notification unhelpful button.
void MarkThemeAsNotInterested(
    PrefService* pref_service,
    optimization_guide::proto::FindsSuggestionResponse::SuggestionTheme::
        ThemeType theme_type);

// Returns the model execution cooldown duration as a base::TimeDelta.
base::TimeDelta GetModelExecutionCooldownDurationTimeDelta();

// Returns true if History Sync and MSBB are enabled.
bool IsHistorySyncAndMsbbEnabled(syncer::SyncService* sync_service,
                                 PrefService* pref_service);

// Returns true if the Chrome finds feature is allowed by enterprise policy.
bool IsAllowedByEnterprisePolicy(PrefService* pref_service);

}  // namespace finds

#endif  // CHROME_BROWSER_FINDS_CORE_FINDS_UTILS_H_
