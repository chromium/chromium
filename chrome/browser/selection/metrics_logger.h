// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SELECTION_METRICS_LOGGER_H_
#define CHROME_BROWSER_SELECTION_METRICS_LOGGER_H_

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

// Logs UMA metrics for a single Smart Selection UI session.
class MetricsLogger {
 public:
  explicit MetricsLogger(EntryPoint entry_point);
  MetricsLogger(const MetricsLogger&) = delete;
  MetricsLogger& operator=(const MetricsLogger&) = delete;
  ~MetricsLogger();
};

}  // namespace selection

#endif  // CHROME_BROWSER_SELECTION_METRICS_LOGGER_H_
