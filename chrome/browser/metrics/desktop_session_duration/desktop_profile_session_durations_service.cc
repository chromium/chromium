// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/desktop_session_duration/desktop_profile_session_durations_service.h"

#include "base/metrics/histogram_macros.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/sync/driver/sync_service.h"
#include "content/public/browser/browser_context.h"

namespace metrics {

DesktopProfileSessionDurationsService::DesktopProfileSessionDurationsService(
    syncer::SyncService* sync_service,
    signin::IdentityManager* identity_manager,
    DesktopSessionDurationTracker* tracker)
    : metrics_recorder_(
          std::make_unique<syncer::SyncSessionDurationsMetricsRecorder>(
              sync_service,
              identity_manager)),
      session_duration_observer_(this) {
  session_duration_observer_.Add(tracker);
  if (tracker->in_session()) {
    // The session was started before this service was created. Let's start
    // tracking now.
    OnSessionStarted(base::TimeTicks::Now());
  }
}

DesktopProfileSessionDurationsService::
    ~DesktopProfileSessionDurationsService() = default;

void DesktopProfileSessionDurationsService::Shutdown() {
  metrics_recorder_.reset();
}

void DesktopProfileSessionDurationsService::OnSessionStarted(
    base::TimeTicks session_start) {
  metrics_recorder_->OnSessionStarted(session_start);
}

void DesktopProfileSessionDurationsService::OnSessionEnded(
    base::TimeDelta session_length,
    base::TimeTicks session_end) {
  metrics_recorder_->OnSessionEnded(session_length);
}

}  // namespace metrics
