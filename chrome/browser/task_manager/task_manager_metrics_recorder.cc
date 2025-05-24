// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/task_manager_metrics_recorder.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/stringprintf.h"

namespace task_manager {

// Records a new entry for the "TaskManager.Opened" UMA metric. If StartAction
// is kOther, no event is recorded.
void RecordNewOpenEvent(StartAction type) {
  if (type == StartAction::kOther) {
    return;
  }
  UMA_HISTOGRAM_ENUMERATION(kStartActionHistogram, type);
}

void RecordCloseEvent(const base::TimeTicks& start_time,
                      const base::TimeTicks& end_time) {
  UMA_HISTOGRAM_LONG_TIMES_100(kClosedElapsedTimeHistogram,
                               end_time - start_time);
}

void RecordTabSwitchEvent(CategoryRecord record,
                          const base::TimeDelta& elapsed_time) {
  static const std::array<std::string, 5> kCategoryToString = {
      "Other", "TabsAndExtensions", "Browser", "System", "All"};

  // Out of bounds check.
  CHECK(static_cast<size_t>(record) < kCategoryToString.size());

  const auto histogram_name = base::StringPrintf(
      kClosedTabElapsedTimeHistogram,
      kCategoryToString[static_cast<size_t>(record)].c_str());

  base::UmaHistogramLongTimes100(histogram_name, elapsed_time);
}

void RecordEndProcessEvent(const base::TimeTicks& start_time,
                           const base::TimeTicks& end_time,
                           size_t end_process_count) {
  constexpr static std::array<std::string, 5> kEndProcessCountToString = {
      "First", "Second", "Third", "Fourth", "Fifth"};

  // Only record the first five end process events per task manager session.
  if (end_process_count > 0 &&
      end_process_count <= kEndProcessCountToString.size()) {
    // Build the histogram name.
    const auto histogram_name = base::StringPrintf(
        kTimeToEndProcessHistogram,
        kEndProcessCountToString[end_process_count - 1].c_str());

    // Record the event.
    base::UmaHistogramLongTimes100(histogram_name, end_time - start_time);
  }
}

}  // namespace task_manager
