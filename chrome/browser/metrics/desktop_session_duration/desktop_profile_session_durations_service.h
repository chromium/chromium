// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_DESKTOP_SESSION_DURATION_DESKTOP_PROFILE_SESSION_DURATIONS_SERVICE_H_
#define CHROME_BROWSER_METRICS_DESKTOP_SESSION_DURATION_DESKTOP_PROFILE_SESSION_DURATIONS_SERVICE_H_

#include "base/scoped_observation.h"
#include "chrome/browser/metrics/desktop_session_duration/desktop_session_duration_tracker.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/password_manager/core/browser/password_session_durations_metrics_recorder.h"
#include "components/sync/service/sync_session_durations_metrics_recorder.h"
#include "components/unified_consent/msbb_session_durations_metrics_recorder.h"

namespace signin {
class IdentityManager;
}
namespace signin_metrics {
enum class SingleProfileSigninStatus;
}
namespace syncer {
class SyncService;
}
class PrefService;

namespace metrics {

// Tracks the user's active browsing time and forwards session start/end events
// to feature-specific recorders.
class DesktopProfileSessionDurationsService
    : public KeyedService,
      public DesktopSessionDurationTracker::Observer {
 public:
  // Callers must ensure that the parameters outlive this object.
  DesktopProfileSessionDurationsService(
      PrefService* pref_service,
      syncer::SyncService* sync_service,
      signin::IdentityManager* identity_manager,
      DesktopSessionDurationTracker* tracker);

  DesktopProfileSessionDurationsService(
      const DesktopProfileSessionDurationsService&) = delete;
  DesktopProfileSessionDurationsService& operator=(
      const DesktopProfileSessionDurationsService&) = delete;

  ~DesktopProfileSessionDurationsService() override;

  signin_metrics::SingleProfileSigninStatus GetSigninStatus() const;
  bool IsSyncing() const;

  // DesktopSessionDurationtracker::Observer:
  void OnSessionStarted(base::TimeTicks session_start) override;
  void OnSessionEnded(base::TimeDelta session_length,
                      base::TimeTicks session_end) override;

  // KeyedService:
  void Shutdown() override;

 private:
  std::unique_ptr<syncer::SyncSessionDurationsMetricsRecorder>
      sync_metrics_recorder_;
  std::unique_ptr<unified_consent::MsbbSessionDurationsMetricsRecorder>
      msbb_metrics_recorder_;
  std::unique_ptr<password_manager::PasswordSessionDurationsMetricsRecorder>
      password_metrics_recorder_;

  base::ScopedObservation<DesktopSessionDurationTracker,
                          DesktopSessionDurationTracker::Observer>
      session_duration_observation_{this};
};

}  // namespace metrics

#endif  // CHROME_BROWSER_METRICS_DESKTOP_SESSION_DURATION_DESKTOP_PROFILE_SESSION_DURATIONS_SERVICE_H_
