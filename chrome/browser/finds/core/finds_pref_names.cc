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
// Deprecated. Do not remove.
const char kFindsOptInPromoUserInteracted[] =
    "finds.opt_in_promo.user_interacted";
// The number of times the finds opt in promo was interacted with by the user.
const char kFindsOptInPromoInteractedCount[] =
    "finds.opt_in_promo.interacted_count";
// The timestamp of the last time the finds opt in promo was interacted with.
const char kFindsOptInPromoLastInteractedTimestamp[] =
    "finds.opt_in_promo.last_interacted_timestamp";
// LINT.ThenChange(//chrome/android/java/src/org/chromium/chrome/browser/notifications/finds/ChromeFindsUtils.java:FindsOptInPromoInteractionPrefs)

}  // namespace finds::prefs
