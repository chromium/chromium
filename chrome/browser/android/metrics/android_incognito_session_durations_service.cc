// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/metrics/android_incognito_session_durations_service.h"

#include "base/metrics/histogram_functions.h"

AndroidIncognitoSessionDurationsService::
    AndroidIncognitoSessionDurationsService() {
  // The AndroidIncognitoSessionDurationsService object is created as soon as
  // the profile is initialized. On Android, the profile is initialized as part
  // of the native code initialization, which is done soon after the application
  // enters foreground and before any of the Chrome UI is shown. Let's start
  // tracking the session now.
  OnAppEnterForeground();
}

AndroidIncognitoSessionDurationsService::
    ~AndroidIncognitoSessionDurationsService() = default;

void AndroidIncognitoSessionDurationsService::Shutdown() {
  OnAppEnterBackground();
  session_start_ = base::Time();
  is_foreground_ = false;
}

void AndroidIncognitoSessionDurationsService::OnAppEnterForeground() {
  // When Chrome recovers from a crash while in Incognito, it creates the
  // Incognito profile first and then brings it to foreground. In this case we
  // should prevent double counting.
  if (is_foreground_)
    return;
  is_foreground_ = true;

  // |session_start_| is null prior to start of a new session. Therefore we only
  // need to record the current time.
  if (session_start_.is_null()) {
    session_start_ = base::Time::Now();
    return;
  }

  // Record the previously reported duration, so that subtracting this
  // histogram from 'Profile.Incognito.MovedToBackgroundAfterDuration' would
  // offset for the sessions that were recorded there, but were resumed
  // later.
  base::UmaHistogramCustomCounts(
      "Profile.Incognito.ResumedAfterReportedDuration",
      last_reported_duration_.InMinutes(), 1,
      base::TimeDelta::FromDays(28).InMinutes(), 50);
}

void AndroidIncognitoSessionDurationsService::OnAppEnterBackground() {
  // This function may be called when Chrome is already in background and a
  // proper shutdown of the service takes place.
  if (!is_foreground_)
    return;
  is_foreground_ = false;

  last_reported_duration_ = base::Time::Now() - session_start_;
  base::UmaHistogramCustomCounts(
      "Profile.Incognito.MovedToBackgroundAfterDuration",
      last_reported_duration_.InMinutes(), 1,
      base::TimeDelta::FromDays(28).InMinutes(), 50);
}
