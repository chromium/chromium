// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/selection/metrics_logger.h"

#include <string_view>

#include "base/metrics/histogram_functions.h"
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
