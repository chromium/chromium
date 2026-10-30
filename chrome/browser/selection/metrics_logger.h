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

// Logs UMA metrics for a single Smart Selection UI session.
class MetricsLogger {
 public:
  MetricsLogger(EntryPoint entry_point, std::string_view mime_type);
  MetricsLogger(const MetricsLogger&) = delete;
  MetricsLogger& operator=(const MetricsLogger&) = delete;
  ~MetricsLogger();
};

}  // namespace selection

#endif  // CHROME_BROWSER_SELECTION_METRICS_LOGGER_H_
