// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/attempt_form_filling_tool_metrics.h"

#include "base/metrics/histogram_functions.h"

namespace actor::actor_metrics {

void RecordOnSuggestionPresentedMetrics(
    int form_index,
    AttemptFormFillingToolRequest::RequestedData requested_data) {
  base::UmaHistogramEnumeration(
      "Autofill.Actor.AutofillSuggestionPresented.RecordType", requested_data);
  // Only record `AutofillAttentionCardEvent` for the first form of
  // a card, as it's a card granularity metric.
  if (form_index == 0) {
    base::UmaHistogramEnumeration("Autofill.Actor.AutofillAttentionCardEvent",
                                  AutofillAttentionCardEvent::kPresented);
  }
}

void RecordOnSuggestionConfirmedMetrics(
    int form_index,
    AttemptFormFillingToolRequest::RequestedData requested_data) {
  base::UmaHistogramEnumeration(
      "Autofill.Actor.AutofillSuggestionAccepted.RecordType", requested_data);
  // Only record `AutofillAttentionCardEvent` for the first form of
  // a card, as it's a card granularity metric.
  if (form_index == 0) {
    base::UmaHistogramEnumeration("Autofill.Actor.AutofillAttentionCardEvent",
                                  AutofillAttentionCardEvent::kAccepted);
  }
}

}  // namespace actor::actor_metrics
