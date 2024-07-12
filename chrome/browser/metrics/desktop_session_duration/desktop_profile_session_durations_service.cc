// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/desktop_session_duration/desktop_profile_session_durations_service.h"

#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/signin/core/browser/signin_status_metrics_provider_helpers.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_session_durations_metrics_recorder.h"

namespace metrics {

DesktopProfileSessionDurationsService::DesktopProfileSessionDurationsService(
    PrefService* pref_service,
    syncer::SyncService* sync_service,
    signin::IdentityManager* identity_manager,
    DesktopSessionDurationTracker* tracker)
    : sync_metrics_recorder_(
          std::make_unique<syncer::SyncSessionDurationsMetricsRecorder>(
              sync_service,
              identity_manager)),
      msbb_metrics_recorder_(
          std::make_unique<
              unified_consent::MsbbSessionDurationsMetricsRecorder>(
              pref_service)),
      password_metrics_recorder_(
          std::make_unique<
              password_manager::PasswordSessionDurationsMetricsRecorder>(
              pref_service,
              sync_service)) {
  session_duration_observation_.Observe(tracker);
  if (tracker->in_session()) {
    // The session was started before this service was created. Let's start
    // tracking now.
    OnSessionStarted(base::TimeTicks::Now());
  }
}

DesktopProfileSessionDurationsService::
    ~DesktopProfileSessionDurationsService() = default;

void DesktopProfileSessionDurationsService::Shutdown() {
  // The Profile is being destroyed, e.g. because the
  // DestroyProfileOnBrowserClose flag is enabled. Recorders expect every call
  // to OnSessionStarted() to have a corresponding OnSessionEnded().
  //
  // Use a |session_length| of zero, so each recorder can infer the duration
  // based on their internal state.
  OnSessionEnded(base::TimeDelta(), base::TimeTicks::Now());

  password_metrics_recorder_.reset();
  msbb_metrics_recorder_.reset();
  sync_metrics_recorder_.reset();
}

signin_metrics::SingleProfileSigninStatus
DesktopProfileSessionDurationsService::GetSigninStatus() const {
  switch (sync_metrics_recorder_->GetSigninStatus()) {
    case syncer::SyncSessionDurationsMetricsRecorder::SigninStatus::kSignedIn:
      return signin_metrics::SingleProfileSigninStatus::kSignedIn;
    case syncer::SyncSessionDurationsMetricsRecorder::SigninStatus::
        kSignedInWithError:
      return signin_metrics::SingleProfileSigninStatus::kSignedInWithError;
    case syncer::SyncSessionDurationsMetricsRecorder::SigninStatus::kSignedOut:
      return signin_metrics::SingleProfileSigninStatus::kSignedOut;
  }
}

bool DesktopProfileSessionDurationsService::IsSyncing() const {
  return sync_metrics_recorder_->IsSyncing();
}

void DesktopProfileSessionDurationsService::OnSessionStarted(
    base::TimeTicks session_start) {
  sync_metrics_recorder_->OnSessionStarted(session_start);
  msbb_metrics_recorder_->OnSessionStarted(session_start);
  password_metrics_recorder_->OnSessionStarted(session_start);
}

void DesktopProfileSessionDurationsService::OnSessionEnded(
    base::TimeDelta session_length,
    base::TimeTicks session_end) {
  sync_metrics_recorder_->OnSessionEnded(session_length);
  msbb_metrics_recorder_->OnSessionEnded(session_length);
  password_metrics_recorder_->OnSessionEnded(session_length);
}

}  // namespace metrics
