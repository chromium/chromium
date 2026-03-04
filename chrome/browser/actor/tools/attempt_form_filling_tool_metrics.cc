// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/attempt_form_filling_tool_metrics.h"

#include "base/metrics/histogram_functions.h"

namespace actor {

void RecordOnSuggestionPresentedMetrics(
    int form_index,
    AttemptFormFillingToolRequest::RequestedData requested_data) {
  base::UmaHistogramEnumeration(
      "Autofill.Actor.AutofillSuggestionPresented.Type", requested_data);
}

}  // namespace actor
