// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_CUEING_CONTEXTUAL_CUEING_ENUMS_H_
#define CHROME_BROWSER_CONTEXTUAL_CUEING_CONTEXTUAL_CUEING_ENUMS_H_

namespace contextual_cueing {

// LINT.IfChange(NudgeDecision)
enum class NudgeDecision {
  kUnknown = 0,
  // A nudge was available for the page.
  kSuccess = 1,
  // The server had no data for the page.
  kServerDataUnavailable = 2,
  // The server had data for the page, but it was malformed.
  kServerDataMalformed = 3,
  // The server had data for the page, but the client conditions did not
  // evaluate to true.
  kClientConditionsUnmet = 4,
  // The page was eligible for the nudge, but not enough page loads have been
  // navigated to since the last nudge shown to the user.
  kNotEnoughPageLoadsSinceLastNudge = 5,
  // The page was eligible for the nudge, but not enough time has elapsed since
  // the last nudge was dismissed. This is only relevant when a nudge has been
  // dismissed in the past.
  kNotEnoughTimeSinceLastNudgeDismissed = 6,
  // The page was eligible for the nudge, but too many nudges have been shown to
  // the user recently.
  kTooManyNudgesShownToTheUser = 7,
  // The page was eligible for the nudge, but too many nudges have been shown to
  // the user recently for the domain.
  kTooManyNudgesShownToTheUserForDomain = 8,
  // The page was eligible for the nudge, but the user was currently being
  // presented the IPH.
  kNudgeNotShownIPH = 9,
  // The page was eligible for the nudge, but the user already has the feature
  // window open.
  kNudgeNotShownWindowShowing = 10,
  // User closes the tab/window as a nudge decision is being computed.
  kNudgeDecisionInterrupted = 11,
  // The page was eligible for the nudge, but not enough time has elapsed since
  // the last nudge was shown to the user.
  kNotEnoughTimeSinceLastNudgeShown = 12,
  // User closes the tab/window as a nudge decision is being computed.
  kNudgeNotShownWindowCallToActionUI = 13,
  // New values above this line.
  kMaxValue = kNudgeNotShownWindowCallToActionUI,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/contextual_cueing/enums.xml:NudgeDecision)

// LINT.IfChange(NudgeInteraction)
enum class NudgeInteraction {
  kUnknown = 0,
  // The server sent data and we successfully showed the nudge.
  kShown = 1,
  // The server sent data, but for a different web context so we did not nudge.
  kNudgeNotShownWebContents = 2,
  // The user clicked on a shown nudge.
  kClicked = 3,
  // The user dismissed the nudge with the close button.
  kDismissed = 4,
  // The nudge was dismissed by the tab changing.
  kIgnoredTabChange = 5,
  //  The nudge was dismissed by the navigation changing.
  kIgnoredNavigation = 6,
  // The server sent data, but another call to action was present in the
  // browser.
  kNudgeNotShownWindowCallToActionUI = 7,
  // New values above this line.
  kMaxValue = kNudgeNotShownWindowCallToActionUI,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/contextual_cueing/enums.xml:NudgeInteraction)

}  // namespace contextual_cueing

// namespace contextual_cueing

#endif  // CHROME_BROWSER_CONTEXTUAL_CUEING_CONTEXTUAL_CUEING_ENUMS_H_
