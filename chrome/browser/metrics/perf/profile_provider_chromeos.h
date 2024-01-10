// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_PERF_PROFILE_PROVIDER_CHROMEOS_H_
#define CHROME_BROWSER_METRICS_PERF_PROFILE_PROVIDER_CHROMEOS_H_

#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/power_monitor/power_observer.h"
#include "base/time/time.h"
#include "chrome/browser/sessions/session_restore.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "content/public/browser/jank_monitor.h"

namespace metrics {

class MetricProvider;
class SampledProfile;

// Provides access to ChromeOS profile data using different metric collectors.
// It detects certain system triggers, such as device resuming from suspend
// mode, or user logging in, which it forwards to the registered collectors.
class ProfileProvider : public chromeos::PowerManagerClient::Observer,
                        public ash::LoginState::Observer,
                        public content::JankMonitor::Observer,
                        public base::PowerThermalObserver {
 public:
  ProfileProvider();

  ProfileProvider(const ProfileProvider&) = delete;
  ProfileProvider& operator=(const ProfileProvider&) = delete;

  ~ProfileProvider() override;

  void Init();

  // Stores collected perf data protobufs in |sampled_profiles|. Clears all the
  // stored profile data. Returns true if it wrote to |sampled_profiles|.
  bool GetSampledProfiles(std::vector<SampledProfile>* sampled_profiles);

  // Called when the metrics recording state changes and the corresponding
  // callback in ChromeOSMetricsProvider is invoked.
  void OnRecordingEnabled();
  void OnRecordingDisabled();

 protected:
  // Called when either the login state or the logged in user type changes.
  // Activates the registered collectors to start collecting. Inherited from
  // LoginState::Observer.
  void LoggedInStateChanged() override;

  // Called when a suspend finishes. This is either a successful suspend
  // followed by a resume, or a suspend that was canceled. Inherited from
  // PowerManagerClient::Observer.
  void SuspendDone(base::TimeDelta sleep_duration) override;

  // Called when a session restore has finished.
  void OnSessionRestoreDone(Profile* profile, int num_tabs_restored);

  // Called when a jank is observed by the JankMonitor. Note that these 2
  // methods don't run on the UI thread.
  void OnJankStarted() override;
  void OnJankStopped() override;

  // base::PowerThermalObserver overrides.
  void OnThermalStateChange(
      base::PowerThermalObserver::DeviceThermalState new_state) override;
  void OnSpeedLimitChange(int new_limit) override;

  // For testing.
  scoped_refptr<content::JankMonitor> jank_monitor() const {
    return jank_monitor_;
  }
  // For testing.
  base::TimeDelta jankiness_collection_min_interval() const {
    return jankiness_collection_min_interval_;
  }

  // Vector of registered metric collectors.
  std::vector<std::unique_ptr<MetricProvider>> collectors_;

 private:
  // Points to the on-session-restored callback that was registered with
  // SessionRestore's callback list. When objects of this class are destroyed,
  // the subscription's destructor will automatically unregister the callback in
  // SessionRestore, so that the callback list does not contain any obsolete
  // callbacks.
  base::CallbackListSubscription on_session_restored_callback_subscription_;

  scoped_refptr<content::JankMonitor> jank_monitor_;

  // Timestamp of the most recent jank observed.
  base::TimeTicks last_jank_start_time_;

  const base::TimeDelta jankiness_collection_min_interval_;

  // To pass around the "this" pointer across threads safely.
  base::WeakPtrFactory<ProfileProvider> weak_factory_{this};
};

}  // namespace metrics

#endif  // CHROME_BROWSER_METRICS_PERF_PROFILE_PROVIDER_CHROMEOS_H_
