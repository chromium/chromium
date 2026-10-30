// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SELECTION_METRICS_LOGGER_H_
#define CHROME_BROWSER_SELECTION_METRICS_LOGGER_H_

#include <string_view>

namespace selection {

// Entry point from which Smart Selection was invoked.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(EntryPoint)
enum class EntryPoint {
  kKeyboardShortcut = 0,
  kContextMenuText = 1,
  kContextMenuImage = 2,
  kContextMenuPage = 3,
  kInlineCue = 4,
  kCursorWiggle = 5,
  kToolbarButton = 6,
  kMaxValue = kToolbarButton,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/smart_selection/enums.xml:SmartSelectionEntryPoint)

// MIME type of the document on which Smart Selection was invoked.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(DocumentMimeType)
enum class DocumentMimeType {
  kUnknown = 0,
  kPdf = 1,
  kHtml = 2,
  kPlainText = 3,
  kImage = 4,
  kVideo = 5,
  kAudio = 6,
  kJson = 7,
  kMaxValue = kJson,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/smart_selection/enums.xml:SmartSelectionDocumentMimeType)

// Outcome of the initial selection stage in a Smart Selection UI session.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(SelectionOutcome)
enum class SelectionOutcome {
  // Text was preselected.
  kPreselectedText = 0,
  // The user completed a spatial selection.
  kSpatialSelectionCompleted = 1,
  // The user explicitly exited the selection mode (e.g., by pressing ESC).
  kCancelledByUser = 2,
  // The selection mode was exited for other reasons - for example tab or
  // browser close.
  kAbandonedOrError = 3,
  kMaxValue = kAbandonedOrError,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/smart_selection/enums.xml:SmartSelectionSelectionOutcome)

// Logs UMA metrics for a single Smart Selection UI session.
class MetricsLogger {
 public:
  MetricsLogger(EntryPoint entry_point,
                std::string_view mime_type,
                bool has_preselected_region);
  MetricsLogger(const MetricsLogger&) = delete;
  MetricsLogger& operator=(const MetricsLogger&) = delete;
  ~MetricsLogger();

  void RecordSpatialSelectionCompleted();
  void RecordSessionEnd(bool cancelled_by_user);

 private:
  const bool has_preselected_region_;
  bool spatial_selection_completed_ = false;
  bool session_end_recorded_ = false;
};

}  // namespace selection

#endif  // CHROME_BROWSER_SELECTION_METRICS_LOGGER_H_
