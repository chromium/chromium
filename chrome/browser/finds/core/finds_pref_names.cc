// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/finds/core/finds_pref_names.h"

namespace finds::prefs {

// The timestamp of the last successful model execution for the Finds service.
const char kFindsModelExecutionLastTimestamp[] =
    "finds.model_execution.last_timestamp";

// A dictionary storing the last time a user marked a theme as "not
// interested".
const char kFindsNotInterestedThemesLastTimestamp[] =
    "finds.themes.not_interested_last_timestamp";

// LINT.IfChange(FindsOptInPromoInteractionPrefs)
// A boolean of whether the finds opt in promo was interacted with by the user.
const char kFindsOptInPromoUserInteracted[] =
    "finds.opt_in_promo.user_interacted";
// Deprecated. Do not remove.
// The number of times the finds opt in promo was interacted with by the user.
const char kFindsOptInPromoInteractedCount[] =
    "finds.opt_in_promo.interacted_count";
// Deprecated. Do not remove.
// The timestamp of the last time the finds opt in promo was interacted with.
const char kFindsOptInPromoLastInteractedTimestamp[] =
    "finds.opt_in_promo.last_interacted_timestamp";
// The number of times the finds opt in promo was shown to the user.
const char kFindsOptInPromoShownCount[] = "finds.opt_in_promo.shown_count";
// The timestamp of the last time the finds opt in promo was shown to the user.
const char kFindsOptInPromoLastShownTimestamp[] =
    "finds.opt_in_promo.last_shown_timestamp";
// LINT.ThenChange(//chrome/browser/finds/android/java/src/org/chromium/chrome/browser/finds/FindsUtils.java:FindsOptInPromoInteractionPrefs)

}  // namespace finds::prefs
