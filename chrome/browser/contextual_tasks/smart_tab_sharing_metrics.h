// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_SMART_TAB_SHARING_METRICS_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_SMART_TAB_SHARING_METRICS_H_

namespace contextual_tasks {

// Enums matching those in enums.xml

enum class SmartTabSharingToggleState {
  kToggledOff = 0,
  kToggledOn = 1,
  kMaxValue = kToggledOn,
};

enum class SmartTabSharingPromoAction {
  kPromoShown = 0,
  kPromoAccepted = 1,
  kPromoDismissed = 2,
  kMaxValue = kPromoDismissed,
};

enum class SmartTabSharingFilterReason {
  kNotFiltered = 0,
  kSensitiveContent = 1,
  kDomainDenylisted = 2,
  kLowRelevance = 3,
  kMaxValue = kLowRelevance,
};

void LogMenuOptionClicked(SmartTabSharingToggleState state);
void LogPromoInteraction(SmartTabSharingPromoAction action);
void LogTabFilterReason(SmartTabSharingFilterReason reason);
void LogThreadWithTabsSubmitted(bool submitted);
void LogOptOutMidThread(bool opted_out);

}  // namespace contextual_tasks

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_SMART_TAB_SHARING_METRICS_H_
