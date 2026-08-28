// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/smart_tab_sharing_metrics.h"

#include "base/metrics/histogram_functions.h"

namespace contextual_tasks {

void LogMenuOptionClicked(SmartTabSharingToggleState state) {
  base::UmaHistogramEnumeration(
      "ContextualSearch.SmartTabSharing.MenuOptionClicked", state);
}

void LogPromoInteraction(SmartTabSharingPromoAction action) {
  base::UmaHistogramEnumeration(
      "ContextualSearch.SmartTabSharing.PromoInteraction", action);
}

void LogTabFilterReason(SmartTabSharingFilterReason reason) {
  base::UmaHistogramEnumeration(
      "ContextualSearch.SmartTabSharing.TabFilterReason", reason);
}

void LogThreadWithTabsSubmitted(bool submitted) {
  base::UmaHistogramBoolean(
      "ContextualSearch.SmartTabSharing.ThreadWithTabsSubmitted", submitted);
}

void LogOptOutMidThread(bool opted_out) {
  base::UmaHistogramBoolean("ContextualSearch.SmartTabSharing.OptOutMidThread",
                            opted_out);
}

}  // namespace contextual_tasks
