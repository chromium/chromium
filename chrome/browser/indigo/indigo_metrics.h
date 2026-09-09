// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INDIGO_INDIGO_METRICS_H_
#define CHROME_BROWSER_INDIGO_INDIGO_METRICS_H_

#include <optional>

#include "base/time/time.h"
#include "chrome/browser/indigo/indigo_page_action_controller.h"
#include "google_apis/common/api_error_codes.h"

namespace page_actions {
enum class PageActionPriorityCategory;
}  // namespace page_actions

namespace indigo {

inline constexpr char kShownEntryPointHistogram[] =
    "Indigo.PageAction.ShownEntryPoint";
inline constexpr char kClickedEntryPointHistogram[] =
    "Indigo.PageAction.ClickedEntryPoint";
inline constexpr char kMaybeInvokeGlicResultHistogramName[] =
    "Indigo.Glic.MaybeInvokeResult";
inline constexpr char kPanelActuallyShowingOnCallbackHistogramName[] =
    "Indigo.Glic.PanelActuallyShowingOnCallback";

inline constexpr char kSuggestionChipShowAction[] =
    "Indigo.PageAction.SuggestionChip.Show";
inline constexpr char kProactiveAnchoredMessageShowAction[] =
    "Indigo.PageAction.AnchoredMessage.Proactive.Show";
inline constexpr char kReactiveAnchoredMessageShowAction[] =
    "Indigo.PageAction.AnchoredMessage.Reactive.Show";

inline constexpr char kSuggestionChipClickAction[] =
    "Indigo.PageAction.SuggestionChip.Click";
inline constexpr char kAnchoredMessageClickAction[] =
    "Indigo.PageAction.AnchoredMessage.Click";
inline constexpr char kErrorToastRetryClickAction[] =
    "Indigo.ErrorToast.Retry.Click";

enum class IndigoApiEndpoint {
  kGenerate,
  kGetStatus,
  kDelete,
};

// LINT.IfChange(IndigoApiStatus)
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class IndigoApiStatus {
  kSuccess = 0,
  kHttpError = 1,
  kParseError = 2,
  kTooLarge = 3,
  kNoSignedInUser = 4,
  kInvalidUrl = 5,
  kApiError = 6,
  kNetworkError = 7,
  kInvalidResponse = 8,
  kCancelled = 9,
  kOtherError = 10,
  kMaxValue = kOtherError,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/indigo/enums.xml:IndigoApiStatus)

// Results of Indigo's MaybeInvokeGlic attempt.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(IndigoMaybeInvokeGlicResult)
enum class IndigoMaybeInvokeGlicResult {
  kInvoked = 0,
  kFeatureDisabled = 1,
  kAlreadyShowing = 2,
  kNoGlicKeyedService = 3,
  kExistingConversation = 4,
  kPromptEmpty = 5,
  kInvokeRejected = 6,
  kNoWebContents = 7,
  kMaxValue = kNoWebContents,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/indigo/enums.xml:IndigoMaybeInvokeGlicResult)

IndigoApiStatus MapApiErrorCodeToStatus(google_apis::ApiErrorCode code);

// Records UMA histogram and UserAction for shown entry points.
void RecordShownEntryPoint(IndigoPageActionEntryPoint entry_point);

// Records UMA histogram and UserAction for clicked entry points.
void RecordClickedEntryPoint(
    EntryPoint entry_point,
    std::optional<page_actions::PageActionPriorityCategory>
        last_anchored_message_priority);

// Records UMA histogram for the raw HTTP response code.
void RecordApiHttpResponse(IndigoApiEndpoint endpoint,
                           google_apis::ApiErrorCode code);

// Records UMA histogram for the mapped API status and latency.
void RecordApiStatusAndLatency(IndigoApiEndpoint endpoint,
                               IndigoApiStatus status,
                               base::TimeDelta latency);

}  // namespace indigo

#endif  // CHROME_BROWSER_INDIGO_INDIGO_METRICS_H_
