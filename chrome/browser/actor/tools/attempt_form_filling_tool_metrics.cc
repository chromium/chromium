// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/attempt_form_filling_tool_metrics.h"

#include "base/metrics/histogram_functions.h"

namespace actor::form_fill_metrics {

void RecordOnInvokeMetrics() {
  base::UmaHistogramEnumeration("Autofill.Actor.AttemptFormFillingToolEvent",
                                AttemptFormFillingToolEvent::kInvoked);
}

void RecordOnSuggestionsRetrievedMetrics(int suggestions_count) {
  base::UmaHistogramEnumeration("Autofill.Actor.AttemptFormFillingToolEvent",
                                AttemptFormFillingToolEvent::kServiceResponded);
  if (suggestions_count > 0) {
    base::UmaHistogramEnumeration(
        "Autofill.Actor.AttemptFormFillingToolEvent",
        AttemptFormFillingToolEvent::kSuggestionsRetrieved);
    base::UmaHistogramCounts100("Autofill.Actor.AutofillSuggestionsPerDialog",
                                suggestions_count);
  }
}

void RecordOnSuggestionPresentedMetrics(
    bool is_first,
    AttemptFormFillingToolRequest::RequestedData requested_data) {
  base::UmaHistogramEnumeration(
      "Autofill.Actor.AutofillSuggestionPresented.RecordType", requested_data);
  // Only record `kAttentionDialogPresented` for the first form of
  // a card, as it's a card granularity metric.
  if (is_first) {
    base::UmaHistogramEnumeration(
        "Autofill.Actor.AttemptFormFillingToolEvent",
        AttemptFormFillingToolEvent::kAttentionDialogPresented);
  }
}

void RecordOnSuggestionConfirmedMetrics(
    bool is_last,
    AttemptFormFillingToolRequest::RequestedData requested_data) {
  base::UmaHistogramEnumeration(
      "Autofill.Actor.AutofillSuggestionAccepted.RecordType", requested_data);
  // Only record `kAttentionDialogAccepted` for the last form of
  // a card, as it's a card granularity metric.
  if (is_last) {
    base::UmaHistogramEnumeration(
        "Autofill.Actor.AttemptFormFillingToolEvent",
        AttemptFormFillingToolEvent::kAttentionDialogAccepted);
  }
}

}  // namespace actor::form_fill_metrics
