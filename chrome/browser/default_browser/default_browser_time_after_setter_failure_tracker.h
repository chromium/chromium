// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEFAULT_BROWSER_DEFAULT_BROWSER_TIME_AFTER_SETTER_FAILURE_TRACKER_H_
#define CHROME_BROWSER_DEFAULT_BROWSER_DEFAULT_BROWSER_TIME_AFTER_SETTER_FAILURE_TRACKER_H_

#include "base/time/time.h"
#include "chrome/browser/default_browser/default_browser_controller.h"
#include "chrome/browser/default_browser/default_browser_setter.h"

namespace default_browser {

// TimeAfterSetterFailureTracker measures the conversion rate and
// time-to-set-default for asynchronous user settings journeys after the
// primary setter has failed (e.g. timed out).
class TimeAfterSetterFailureTracker {
 public:
  TimeAfterSetterFailureTracker(DefaultBrowserEntrypointType entrypoint,
                                DefaultBrowserSetterType setter);

  TimeAfterSetterFailureTracker(const TimeAfterSetterFailureTracker&) = delete;
  TimeAfterSetterFailureTracker& operator=(
      const TimeAfterSetterFailureTracker&) = delete;

  ~TimeAfterSetterFailureTracker();

  void OnDefaultBrowserSet();

 private:
  void RecordDefaultSet(bool default_set);

  const DefaultBrowserEntrypointType entrypoint_;
  const DefaultBrowserSetterType setter_;
  const base::TimeTicks start_time_;
  bool recorded_ = false;
};

}  // namespace default_browser

#endif  // CHROME_BROWSER_DEFAULT_BROWSER_DEFAULT_BROWSER_TIME_AFTER_SETTER_FAILURE_TRACKER_H_
