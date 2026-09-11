// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INDIGO_INDIGO_METRICS_H_
#define CHROME_BROWSER_INDIGO_INDIGO_METRICS_H_

#include <optional>

#include "base/time/time.h"
#include "google_apis/common/api_error_codes.h"

namespace page_actions {
enum class PageActionPriorityCategory;
}  // namespace page_actions

namespace indigo {

struct CombinedEligibility;

// LINT.IfChange(IndigoTransformationResult)

// Results of Indigo action invocation.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class IndigoTransformationResult {
  kUnknown = 0,
  kSuccess = 1,
  kNotSignedIn = 2,
  kMissingCapabilities = 3,
  kDisabledByPolicy = 4,
  kMissingScript = 5,
  kRemoteStatusMissing = 6,
  kServiceNotSupported = 7,
  kMissingUserImage = 8,
  kNotOnboarded = 9,
  kGenerateImageError = 10,
  kRefreshTokenInPersistentErrorState = 11,
  kManagedDomain = 12,
  kGlicDisabledForProfile = 13,
  kEnterpriseDisallowed = 14,
  kPrimaryImageDisconnected = 15,
  kEmptyPrimaryImageSize = 16,
  kPrimaryImageTooSmall = 17,
  kNoPrimaryImageFound = 18,
  kPrimaryImageReplacementCreationFailed = 19,
  kMaxValue = kPrimaryImageReplacementCreationFailed,
};

// LINT.ThenChange(//tools/metrics/histograms/metadata/indigo/enums.xml:IndigoTransformationResult)

// Trigger sources for the Indigo page action.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(IndigoTriggerSource)
enum class IndigoTriggerSource {
  kUnknown = 0,
  kForced = 1,
  kOptimizationGuide = 2,
  kLocalProductKeywordHeuristic = 3,
  kMaxValue = kLocalProductKeywordHeuristic,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/indigo/enums.xml:IndigoTriggerSource)

// Entry points / presentation styles for the Indigo page action metrics.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(IndigoPageActionEntryPoint)
enum class IndigoPageActionEntryPoint {
  kSuggestionChip = 0,
  kProactiveAnchoredMessage = 1,
  kReactiveAnchoredMessage = 2,
  kErrorToast = 3,
  kMaxValue = kErrorToast,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/indigo/enums.xml:IndigoPageActionEntryPoint)

// Represents the UI surface that triggered an Indigo transformation.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(IndigoTransformationTriggerSource)
enum class IndigoTransformationTriggerSource {
  kPageAction = 0,
  kErrorToastRetry = 1,
  kRegenerate = 2,
  kReplacePhoto = 3,
  kMaxValue = kReplacePhoto,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/indigo/enums.xml:IndigoTransformationTriggerSource)

enum class EntryPoint {
  kSuggestionChip = 0,
  kAnchoredMessage = 1,
  kErrorToast = 2,
};

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

inline constexpr char kTransformationTriggerAction[] =
    "Indigo.Transformation.Trigger";
inline constexpr char kTransformationSuccessAction[] =
    "Indigo.Transformation.Success";
inline constexpr char kTransformationFailureAction[] =
    "Indigo.Transformation.Failure";
inline constexpr char kTransformationResultHistogram[] =
    "Indigo.Transformation.Result";
inline constexpr char kTransformationTriggerSourceHistogram[] =
    "Indigo.Transformation.TriggerSource";

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

// Records UMA histogram and UserAction for transformation triggers.
void RecordTransformationTrigger(IndigoTransformationTriggerSource source);

// Records UMA histogram and UserAction for transformation results.
void RecordTransformationResult(IndigoTransformationResult result);

// Translates CombinedEligibility into IndigoTransformationResult and records
// histogram.
void RecordTransformationResultCannotGenerateImage(
    const CombinedEligibility& eligibility);

}  // namespace indigo

#endif  // CHROME_BROWSER_INDIGO_INDIGO_METRICS_H_
