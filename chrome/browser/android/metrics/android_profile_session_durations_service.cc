// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/metrics/android_profile_session_durations_service.h"

AndroidProfileSessionDurationsService::AndroidProfileSessionDurationsService(
    syncer::SyncService* sync_service,
    signin::IdentityManager* identity_manager)
    : metrics_recorder_(
          std::make_unique<syncer::SyncSessionDurationsMetricsRecorder>(
              sync_service,
              identity_manager)) {
  // The AndroidProfileSessionDurationsService object is created as soon as
  // the profile is initialized. On Android, the profile is initialized as part
  // of the native code initialization, which is done soon after the application
  // enters foreground and before any of the Chrome UI is shown. Let's start
  // tracking the session now.
  OnAppEnterForeground(base::TimeTicks::Now());
}

AndroidProfileSessionDurationsService::
    ~AndroidProfileSessionDurationsService() = default;

void AndroidProfileSessionDurationsService::Shutdown() {
  metrics_recorder_.reset();
}

void AndroidProfileSessionDurationsService::OnAppEnterForeground(
    base::TimeTicks session_start) {
  metrics_recorder_->OnSessionStarted(session_start);
}

void AndroidProfileSessionDurationsService::OnAppEnterBackground(
    base::TimeDelta session_length) {
  metrics_recorder_->OnSessionEnded(session_length);
}
