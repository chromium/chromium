// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/selection/metrics_logger.h"

#include <string_view>

#include "base/metrics/histogram_functions.h"

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
  }
}

}  // namespace selection
