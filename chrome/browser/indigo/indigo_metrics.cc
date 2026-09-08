// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/indigo/indigo_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/strcat.h"
#include "chrome/browser/ui/page_action/page_action_controller.h"

namespace indigo {

namespace {
const char* GetEndpointString(IndigoApiEndpoint endpoint) {
  switch (endpoint) {
    case IndigoApiEndpoint::kGenerate:
      return "Generate";
    case IndigoApiEndpoint::kGetStatus:
      return "GetStatus";
    case IndigoApiEndpoint::kDelete:
      return "Delete";
  }
}

bool IsHttpCode(google_apis::ApiErrorCode code) {
  return code > 0 && code < 600;
}
}  // namespace

IndigoApiStatus MapApiErrorCodeToStatus(google_apis::ApiErrorCode code) {
  if (code == google_apis::HTTP_SUCCESS) {
    return IndigoApiStatus::kSuccess;
  }
  if (code == google_apis::CANCELLED) {
    return IndigoApiStatus::kCancelled;
  }
  if (code == google_apis::NO_CONNECTION) {
    return IndigoApiStatus::kNetworkError;
  }
  if (code == google_apis::PARSE_ERROR) {
    return IndigoApiStatus::kParseError;
  }
  if (IsHttpCode(code)) {
    return IndigoApiStatus::kHttpError;
  }
  return IndigoApiStatus::kOtherError;
}

void RecordShownEntryPoint(IndigoPageActionEntryPoint entry_point) {
  switch (entry_point) {
    case IndigoPageActionEntryPoint::kSuggestionChip:
      base::RecordAction(base::UserMetricsAction(kSuggestionChipShowAction));
      break;
    case IndigoPageActionEntryPoint::kProactiveAnchoredMessage:
      base::RecordAction(
          base::UserMetricsAction(kProactiveAnchoredMessageShowAction));
      break;
    case IndigoPageActionEntryPoint::kReactiveAnchoredMessage:
      base::RecordAction(
          base::UserMetricsAction(kReactiveAnchoredMessageShowAction));
      break;
    case IndigoPageActionEntryPoint::kErrorToast:
      break;
  }
  base::UmaHistogramEnumeration(kShownEntryPointHistogram, entry_point);
}

void RecordClickedEntryPoint(
    EntryPoint entry_point,
    std::optional<page_actions::PageActionPriorityCategory>
        last_anchored_message_priority) {
  switch (entry_point) {
    case EntryPoint::kSuggestionChip:
      base::RecordAction(base::UserMetricsAction(kSuggestionChipClickAction));
      base::UmaHistogramEnumeration(
          kClickedEntryPointHistogram,
          IndigoPageActionEntryPoint::kSuggestionChip);
      break;
    case EntryPoint::kAnchoredMessage:
      base::RecordAction(base::UserMetricsAction(kAnchoredMessageClickAction));
      if (last_anchored_message_priority ==
          page_actions::PageActionPriorityCategory::kContextualCue) {
        base::UmaHistogramEnumeration(
            kClickedEntryPointHistogram,
            IndigoPageActionEntryPoint::kProactiveAnchoredMessage);
      } else if (last_anchored_message_priority ==
                 page_actions::PageActionPriorityCategory::kUserInteraction) {
        base::UmaHistogramEnumeration(
            kClickedEntryPointHistogram,
            IndigoPageActionEntryPoint::kReactiveAnchoredMessage);
      }
      break;
    case EntryPoint::kErrorToast:
      base::RecordAction(base::UserMetricsAction(kErrorToastRetryClickAction));
      base::UmaHistogramEnumeration(kClickedEntryPointHistogram,
                                    IndigoPageActionEntryPoint::kErrorToast);
      break;
  }
}

void RecordApiHttpResponse(IndigoApiEndpoint endpoint,
                           google_apis::ApiErrorCode code) {
  if (!IsHttpCode(code)) {
    return;
  }
  base::UmaHistogramSparse(
      base::StrCat(
          {"Indigo.Api.", GetEndpointString(endpoint), ".HttpResponseCode"}),
      code);
}

void RecordApiStatusAndLatency(IndigoApiEndpoint endpoint,
                               IndigoApiStatus status,
                               base::TimeDelta latency) {
  const char* endpoint_str = GetEndpointString(endpoint);
  base::UmaHistogramEnumeration(
      base::StrCat({"Indigo.Api.", endpoint_str, ".Status"}), status);

  bool is_success = (status == IndigoApiStatus::kSuccess);
  base::UmaHistogramMediumTimes(
      base::StrCat({"Indigo.Api.", endpoint_str, ".Latency.",
                    is_success ? "Success" : "Failure"}),
      latency);
}

}  // namespace indigo
