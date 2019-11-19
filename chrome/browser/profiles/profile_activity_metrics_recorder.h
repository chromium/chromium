// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_PROFILE_ACTIVITY_METRICS_RECORDER_H_
#define CHROME_BROWSER_PROFILES_PROFILE_ACTIVITY_METRICS_RECORDER_H_

#include <stddef.h>

#include "base/macros.h"
#include "base/metrics/user_metrics.h"
#include "base/time/time.h"
#include "chrome/browser/metrics/desktop_session_duration/desktop_session_duration_tracker.h"
#include "chrome/browser/ui/browser_list_observer.h"

class Browser;
class Profile;

class ProfileActivityMetricsRecorder
    : public BrowserListObserver,
      public metrics::DesktopSessionDurationTracker::Observer {
 public:
  // Initializes a |ProfileActivityMetricsRecorder| object and starts
  // tracking/recording.
  static void Initialize();

  // Cleans up any global state for testing.
  static void CleanupForTesting();

  // BrowserListObserver overrides:
  void OnBrowserSetLastActive(Browser* browser) override;

  // metrics::DesktopSessionDurationTracker::Observer overrides:
  void OnSessionEnded(base::TimeDelta session_length,
                      base::TimeTicks session_end) override;

 private:
  ProfileActivityMetricsRecorder();
  ~ProfileActivityMetricsRecorder() override;

  void OnUserAction(const std::string& action);

  Profile* last_active_profile_ = nullptr;
  base::TimeTicks profile_session_start_;

  base::ActionCallback action_callback_;

  DISALLOW_COPY_AND_ASSIGN(ProfileActivityMetricsRecorder);
};

#endif  // CHROME_BROWSER_PROFILES_PROFILE_ACTIVITY_METRICS_RECORDER_H_
