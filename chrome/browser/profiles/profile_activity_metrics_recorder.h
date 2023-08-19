// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_PROFILE_ACTIVITY_METRICS_RECORDER_H_
#define CHROME_BROWSER_PROFILES_PROFILE_ACTIVITY_METRICS_RECORDER_H_

#include <stddef.h>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/metrics/user_metrics.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "chrome/browser/metrics/desktop_session_duration/desktop_session_duration_tracker.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "chrome/browser/ui/browser_list_observer.h"

class Browser;

class ProfileActivityMetricsRecorder
    : public BrowserListObserver,
      public metrics::DesktopSessionDurationTracker::Observer,
      public ProfileObserver {
 public:
  ProfileActivityMetricsRecorder(const ProfileActivityMetricsRecorder&) =
      delete;
  ProfileActivityMetricsRecorder& operator=(
      const ProfileActivityMetricsRecorder&) = delete;
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

  // ProfileObserver:
  void OnProfileWillBeDestroyed(Profile* profile) override;

 private:
  ProfileActivityMetricsRecorder();
  ~ProfileActivityMetricsRecorder() override;

  void OnUserAction(const std::string& action, base::TimeTicks action_time);

  // The profile of the last active window.
  raw_ptr<Profile, AcrossTasksDanglingUntriaged> last_active_profile_ = nullptr;

  // Profile of the currently running session, if there is any. Reset after
  // inactivity.
  raw_ptr<Profile, AcrossTasksDanglingUntriaged> running_session_profile_ =
      nullptr;
  base::TimeTicks running_session_start_;
  base::TimeTicks last_session_end_;

  base::ActionCallback action_callback_;

  base::ScopedObservation<Profile, ProfileObserver> profile_observation_{this};
};

#endif  // CHROME_BROWSER_PROFILES_PROFILE_ACTIVITY_METRICS_RECORDER_H_
