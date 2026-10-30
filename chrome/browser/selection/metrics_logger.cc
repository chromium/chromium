// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/selection/metrics_logger.h"

#include <array>
#include <cstddef>
#include <memory>
#include <string_view>
#include <utility>

#include "base/containers/span.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "chrome/browser/selection/suggestion.h"
#include "components/optimization_guide/proto/features/smart_selection_suggestions.pb.h"

namespace selection {

namespace {

DocumentMimeType GetDocumentMimeType(std::string_view mime_type) {
  if (mime_type == "application/pdf") {
    return DocumentMimeType::kPdf;
  } else if (mime_type == "text/html") {
    return DocumentMimeType::kHtml;
  } else if (mime_type == "text/plain") {
    return DocumentMimeType::kPlainText;
  } else if (mime_type.starts_with("image/")) {
    return DocumentMimeType::kImage;
  } else if (mime_type.starts_with("video/")) {
    return DocumentMimeType::kVideo;
  } else if (mime_type.starts_with("audio/")) {
    return DocumentMimeType::kAudio;
  } else if (mime_type == "application/json") {
    return DocumentMimeType::kJson;
  }
  return DocumentMimeType::kUnknown;
}

std::string_view GetToolHistogramName(
    optimization_guide::proto::SmartSelectionToolId tool_id) {
  switch (tool_id) {
    case optimization_guide::proto::SMART_SELECTION_TOOL_GEMINI_IN_CHROME:
      return "GeminiInChrome";
    case optimization_guide::proto::SMART_SELECTION_TOOL_GOOGLE_LENS:
      return "GoogleLens";
    case optimization_guide::proto::SMART_SELECTION_TOOL_GOOGLE_SEARCH:
      return "GoogleSearch";
    case optimization_guide::proto::SMART_SELECTION_TOOL_QUICK_ANSWERS:
      return "QuickAnswers";
    case optimization_guide::proto::SMART_SELECTION_TOOL_UNSPECIFIED:
    case optimization_guide::proto::
        SmartSelectionToolId_INT_MIN_SENTINEL_DO_NOT_USE_:
    case optimization_guide::proto::
        SmartSelectionToolId_INT_MAX_SENTINEL_DO_NOT_USE_:
      return "Unknown";
  }
  return "Unknown";
}

}  // namespace

MetricsLogger::MetricsLogger(EntryPoint entry_point,
                             std::string_view mime_type,
                             bool has_preselected_region)
    : has_preselected_region_(has_preselected_region) {
  base::UmaHistogramEnumeration("SmartSelection.Invocation.ByEntryPoint",
                                entry_point);
  base::UmaHistogramEnumeration("SmartSelection.Invocation.ByDocumentMimeType",
                                GetDocumentMimeType(mime_type));
  if (has_preselected_region_) {
    base::UmaHistogramEnumeration("SmartSelection.Selection.Outcome",
                                  SelectionOutcome::kPreselectedText);
  }
}

MetricsLogger::~MetricsLogger() {
  if (!session_end_recorded_) {
    RecordSessionEnd(/*cancelled_by_user=*/false);
  }
}

void MetricsLogger::RecordSpatialSelectionCompleted() {
  if (has_preselected_region_ || spatial_selection_completed_) {
    return;
  }
  spatial_selection_completed_ = true;
  base::UmaHistogramEnumeration("SmartSelection.Selection.Outcome",
                                SelectionOutcome::kSpatialSelectionCompleted);
}

void MetricsLogger::RecordSuggestionsShown(
    base::span<const std::unique_ptr<Suggestion>> suggestions) {
  std::array<int, optimization_guide::proto::SmartSelectionToolId_ARRAYSIZE>
      counts = {};
  int total = 0;

  for (const std::unique_ptr<Suggestion>& suggestion : suggestions) {
    if (!suggestion) {
      continue;
    }
    ++total;
    const Suggestion::ToolId tool_id = suggestion->GetToolId();
    const size_t index =
        optimization_guide::proto::SmartSelectionToolId_IsValid(tool_id)
            ? static_cast<size_t>(std::to_underlying(tool_id))
            : static_cast<size_t>(std::to_underlying(
                  optimization_guide::proto::SMART_SELECTION_TOOL_UNSPECIFIED));
    ++counts[index];
  }

  base::UmaHistogramExactLinear("SmartSelection.Suggestions.TotalShownCount",
                                total, /*exclusive_max=*/10);
  for (size_t i = 0; i < counts.size(); ++i) {
    const auto tool_id = static_cast<Suggestion::ToolId>(i);
    base::UmaHistogramExactLinear(
        base::StrCat({"SmartSelection.Suggestions.",
                      GetToolHistogramName(tool_id), "ShownCount"}),
        counts[i], /*exclusive_max=*/10);
  }
}

void MetricsLogger::RecordSuggestionAccepted(const Suggestion& suggestion) {
  ++suggestion_acceptances_;
  base::UmaHistogramEnumeration(
      "SmartSelection.Suggestions.AcceptedSuggestionType",
      suggestion.GetToolId(),
      static_cast<Suggestion::ToolId>(
          optimization_guide::proto::SmartSelectionToolId_ARRAYSIZE));
}

void MetricsLogger::RecordPromptSubmitted() {
  prompt_submitted_ = true;
}

void MetricsLogger::RecordSessionEnd(bool cancelled_by_user) {
  if (session_end_recorded_) {
    return;
  }
  session_end_recorded_ = true;

  if (!has_preselected_region_ && !spatial_selection_completed_) {
    base::UmaHistogramEnumeration("SmartSelection.Selection.Outcome",
                                  cancelled_by_user
                                      ? SelectionOutcome::kCancelledByUser
                                      : SelectionOutcome::kAbandonedOrError);
    return;
  }

  SubmissionOutcome outcome;
  if (prompt_submitted_) {
    outcome = suggestion_acceptances_ > 0 ? SubmissionOutcome::kMultiple
                                          : SubmissionOutcome::kPromptSubmitted;
  } else if (suggestion_acceptances_ > 1) {
    outcome = SubmissionOutcome::kMultiple;
  } else if (suggestion_acceptances_ == 1) {
    outcome = SubmissionOutcome::kSuggestionAccepted;
  } else if (cancelled_by_user) {
    outcome = SubmissionOutcome::kCancelledByUser;
  } else {
    outcome = SubmissionOutcome::kAbandonedOrError;
  }
  base::UmaHistogramEnumeration("SmartSelection.Submission.Outcome", outcome);
}

}  // namespace selection
