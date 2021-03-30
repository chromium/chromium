// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_METRICS_ANDROID_INCOGNITO_SESSION_DURATIONS_SERVICE_H_
#define CHROME_BROWSER_ANDROID_METRICS_ANDROID_INCOGNITO_SESSION_DURATIONS_SERVICE_H_

#include "base/time/time.h"
#include "components/keyed_service/core/keyed_service.h"

// Tracks the duration of Incognito sessions on Android. The session is defined
// as the time between the Incognito profile is created and when it is closed by
// the user or Chrome is closed.
// In cases that an Incognito profile exists and the application dies in the
// background, the time between the last time it went to background and the
// dying time is not covered.
class AndroidIncognitoSessionDurationsService : public KeyedService {
 public:
  AndroidIncognitoSessionDurationsService();
  ~AndroidIncognitoSessionDurationsService() override;
  AndroidIncognitoSessionDurationsService(
      const AndroidIncognitoSessionDurationsService&) = delete;
  AndroidIncognitoSessionDurationsService& operator=(
      const AndroidIncognitoSessionDurationsService&) = delete;

  // KeyedService:
  void Shutdown() override;

  void OnAppEnterForeground();
  void OnAppEnterBackground();

  void SetSessionStartTimeForTesting(base::Time session_start) {
    session_start_ = session_start;
  }

 private:
  base::Time session_start_;
  base::TimeDelta last_reported_duration_;
  bool is_foreground_ = false;
};

#endif  // CHROME_BROWSER_ANDROID_METRICS_ANDROID_INCOGNITO_SESSION_DURATIONS_SERVICE_H_
