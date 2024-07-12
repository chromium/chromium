// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_METRICS_ANDROID_SESSION_DURATIONS_SERVICE_H_
#define CHROME_BROWSER_ANDROID_METRICS_ANDROID_SESSION_DURATIONS_SERVICE_H_

#include "base/time/time.h"
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

namespace {
class IncognitoSessionDurationsMetricsRecorder;
}

// Tracks the duration of a browsing session.
// For regular profiles, tracks the active browsing time that the user spends
// signed in and/or syncing as fraction of their total browsing time. A session
// is defined as the time spent with the application in foreground (the time
// duration between the application enters foreground until the application
// enters background).
// For Incognito profiles, the session is defined as the time between the
// Incognito profile is created and when it is closed by the user or Chrome is
// closed. In cases that an Incognito profile exists and the application dies in
// the background, the time between the last time it went to background and the
// dying time is not covered.
class AndroidSessionDurationsService : public KeyedService {
 public:
  AndroidSessionDurationsService();
  ~AndroidSessionDurationsService() override;

  // Callers must ensure that the parameters outlive this object.
  void InitializeForRegularProfile(PrefService* pref_service,
                                   syncer::SyncService* sync_service,
                                   signin::IdentityManager* identity_manager);

  void InitializeForIncognitoProfile();

  AndroidSessionDurationsService(const AndroidSessionDurationsService&) =
      delete;
  AndroidSessionDurationsService& operator=(
      const AndroidSessionDurationsService&) = delete;

  signin_metrics::SingleProfileSigninStatus GetSigninStatus() const;
  bool IsSyncing() const;

  // KeyedService:
  void Shutdown() override;

  void OnAppEnterForeground(base::TimeTicks session_start);
  void OnAppEnterBackground(base::TimeDelta session_length);

  void SetSessionStartTimeForTesting(base::Time session_start);

  void GetIncognitoSessionData(base::Time& session_start,
                               base::TimeDelta& last_reported_duration);
  void RestoreIncognitoSession(base::Time session_start,
                               base::TimeDelta last_reported_duration);

 private:
  std::unique_ptr<syncer::SyncSessionDurationsMetricsRecorder>
      sync_session_metrics_recorder_;
  std::unique_ptr<password_manager::PasswordSessionDurationsMetricsRecorder>
      password_session_duration_metrics_recorder_;
  std::unique_ptr<unified_consent::MsbbSessionDurationsMetricsRecorder>
      msbb_session_metrics_recorder_;
  std::unique_ptr<IncognitoSessionDurationsMetricsRecorder>
      incognito_session_metrics_recorder_;
};

#endif  // CHROME_BROWSER_ANDROID_METRICS_ANDROID_SESSION_DURATIONS_SERVICE_H_
