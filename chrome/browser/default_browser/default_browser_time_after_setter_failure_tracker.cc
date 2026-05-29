// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/default_browser/default_browser_time_after_setter_failure_tracker.h"

#include <string>

#include "base/metrics/histogram_functions.h"
#include "base/strings/string_util.h"

namespace default_browser {

TimeAfterSetterFailureTracker::TimeAfterSetterFailureTracker(
    DefaultBrowserEntrypointType entrypoint,
    DefaultBrowserSetterType setter)
    : entrypoint_(entrypoint),
      setter_(setter),
      start_time_(base::TimeTicks::Now()) {}

TimeAfterSetterFailureTracker::~TimeAfterSetterFailureTracker() {
  if (!recorded_) {
    RecordDefaultSet(/*default_set=*/false);
  }
}

void TimeAfterSetterFailureTracker::RecordDefaultSet(bool default_set) {
  if (recorded_) {
    return;
  }

  recorded_ = true;

  std::string entrypoint_str = UiEntrypointTypeToString(entrypoint_);
  std::string setter_str = SetterTypeToString(setter_);

  std::string default_set_histogram_name =
      base::JoinString({"DefaultBrowser", entrypoint_str, setter_str,
                        "DefaultSetAfterSetterFailure"},
                       ".");
  base::UmaHistogramBoolean(default_set_histogram_name, default_set);

  if (default_set) {
    std::string duration_histogram_name =
        base::JoinString({"DefaultBrowser", entrypoint_str, setter_str,
                          "TimeAfterSetterFailure"},
                         ".");
    base::UmaHistogramLongTimes(duration_histogram_name,
                                base::TimeTicks::Now() - start_time_);
  }
}

void TimeAfterSetterFailureTracker::OnDefaultBrowserSet() {
  RecordDefaultSet(/*default_set=*/true);
}

}  // namespace default_browser
