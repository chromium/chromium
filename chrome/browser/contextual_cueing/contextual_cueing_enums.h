// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_CUEING_CONTEXTUAL_CUEING_ENUMS_H_
#define CHROME_BROWSER_CONTEXTUAL_CUEING_CONTEXTUAL_CUEING_ENUMS_H_

namespace contextual_cueing {

// LINT.IfChange(ContextualCueingDecision)
enum class ContextualCueingDecision {
  kUnspecified = 0,
  // Tab was not active when the page was classified.
  kNoLongerActiveTabAfterCategoryClassification = 1,
  // Tab was active but the page was not classified as a vertical we support.
  kFailedCategoryClassification = 2,
  // Model execution service is unavailable.
  kModelExecutionUnavailable = 3,
  // Model execution failed.
  kModelExecutionFailed = 4,
  // Model execution response failed to parse.
  kModelExecutionResponseFailedToParse = 5,
  // Contextual cue was shown to the user.
  kSuccess = 6,
  // The response didn't have both anchored_message_text and action_text.
  kMissingAnchoredMessageText = 7,
  // The response didn't match a known target feature.
  kUnknownFulfillmentSurface = 8,
  // The response was for a target feature that didn't register itself.
  kTargetFeatureNotRegistered = 9,
  // The feature reported that its cue shouldn't be shown.
  kTargetFeatureNotEligible = 10,
  // The cue couldn't be shown because the window had no active tab.
  kNoActiveTab = 11,
  // The cue couldn't be shown because the page actions framework wasn't
  // available.
  kNoPageActions = 12,
  // The cue couldn't be shown because the tab for the cue was no longer active.
  kNoLongerActiveTabAfterModelExecution = 13,
  // The cue couldn't be shown because there was a feature promo active.
  kFeaturePromoActive = 14,
  // The cue couldn't be generated/shown because history sync is off.
  kHistorySyncOff = 15,
  // The cue couldn't be shown because not enough page loads have occurred since
  // the last cue was shown.
  kNotEnoughPageLoadsSinceLastCue = 16,
  // The cue couldn't be shown because not enough time has passed since the
  // last cue was shown.
  kNotEnoughTimeSinceLastCue = 17,
  // The cue couldn't be shown because too many cues were shown to the user for
  // this specific origin recently.
  kTooManyCuesShownToTheUser = 18,
  // The cue couldn't be shown because too many cues were shown to the user for
  // this specific origin recently.
  kTooManyCuesShownToTheUserForOrigin = 19,
  // The cue couldn't be shown because the URL is ineligible.
  kUrlNotEligible = 20,
  // The cue couldn't be shown because not enough time has passed since the
  // last cue was dismissed.
  kNotEnoughTimeSinceLastDismissal = 21,
  // The cue couldn't be shown because the side panel is showing.
  kSidePanelShowing = 22,
  // The cue couldn't be shown because there are no eligible cue surfaces.
  kNoEligibleCueSurfaces = 23,
  // The cue couldn't be shown because an infobar is visible.
  kInfobarVisible = 24,
  // The cue couldn't be shown because the user has opted out.
  kUserOptedOut = 25,
  // The cue couldn't be shown because it is disabled by enterprise policy.
  kDisabledByEnterprisePolicy = 26,
  // The cue couldn't be shown because the user is subject to age restrictions.
  kAgeRestrictionEnforced = 27,
  // No cue could be shown because the model execution response contained no cue
  // data.
  kNoCues = 28,
  // The cue couldn't be shown because not enough time has passed since the
  // last cue was clicked.
  kNotEnoughTimeSinceLastClick = 29,
  // There is already another contextual cue or anchored message showing.
  kAnchoredMessageAlreadyShowing = 30,
  kMaxValue = kAnchoredMessageAlreadyShowing,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/contextual_cueing/enums.xml:ContextualCueingDecision)

// LINT.IfChange(ContextualCueingInteraction)
enum class ContextualCueingInteraction {
  kCueClicked = 0,
  kCueDismissed = 1,
  kCueEditPrompt = 2,
  kCueSuggestionsSettings = 3,
  kMaxValue = kCueSuggestionsSettings,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/contextual_cueing/enums.xml:ContextualCueingInteraction)

// LINT.IfChange(CueFormFactor)
enum class CueFormFactor {
  kIcon = 0,
  kChip = 1,
  kAnchoredMessage = 2,
  kMaxValue = kAnchoredMessage,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/contextual_cueing/enums.xml:CueFormFactor)

}  // namespace contextual_cueing

#endif  // CHROME_BROWSER_CONTEXTUAL_CUEING_CONTEXTUAL_CUEING_ENUMS_H_
