// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_ATTEMPT_FORM_FILLING_TOOL_METRICS_H_
#define CHROME_BROWSER_ACTOR_TOOLS_ATTEMPT_FORM_FILLING_TOOL_METRICS_H_

#include "chrome/browser/actor/tools/attempt_form_filling_tool_request.h"

namespace actor::form_fill_metrics {

// LINT.IfChange(AttemptFormFillingToolEvent)

// Events used to reason about the funnel of the `AttemptFormFillingTool`
// lifetime. Each should be logged at most once per `AttemptFormFillingTool`
// lifetime.
enum class AttemptFormFillingToolEvent {
  // The tool was invoked by the actor.
  kInvoked = 0,
  // The tool received some (potentially empty) response from the
  // `GetSuggestions` method of the service.
  kServiceResponded = 1,
  // A tool received non-empty autofill suggestions from the `GetSuggestions`
  // method of the service.
  kSuggestionsRetrieved = 2,
  // An attention dialog was presented to the user.
  // If there are multiple cards in a dialog, it is logged only for the first
  // one.
  kAttentionDialogPresented = 3,
  // A dialog was accepted by the user.
  // If there are multiple cards in a dialog, it is logged only for the last
  // one.
  kAttentionDialogAccepted = 4,
  kMaxValue = kAttentionDialogAccepted
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/autofill/enums.xml:AttemptFormFillingToolEvent)

// Records metrics when the tool gets invoked.
void RecordOnInvokeMetrics();

// Records metrics when the tool receives autofill service `GetSuggestions`
// response.
void RecordOnSuggestionsRetrievedMetrics(bool has_results);

// Records metrics when an autofill suggestion form is presented in the dialog.
// Parameters:
//   is_first: Whether this is the first suggestion card in the dialog.
//   requested_data: The data that was requested for suggestion.
void RecordOnSuggestionPresentedMetrics(
    bool is_first,
    AttemptFormFillingToolRequest::RequestedData requested_data);

// Records metrics when an autofill suggestion form is confirmed in the dialog.
// Parameters:
//   is_last: Whether this is the last suggestion card in the dialog.
//   requested_data: The data that was requested for suggestion.
void RecordOnSuggestionConfirmedMetrics(
    bool is_last,
    AttemptFormFillingToolRequest::RequestedData requested_data);

}  // namespace actor::form_fill_metrics

#endif  // CHROME_BROWSER_ACTOR_TOOLS_ATTEMPT_FORM_FILLING_TOOL_METRICS_H_
