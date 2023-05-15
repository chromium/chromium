// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_METRICS_FEATURE_DISCOVERY_DURATION_REPORTER_IMPL_H_
#define ASH_METRICS_FEATURE_DISCOVERY_DURATION_REPORTER_IMPL_H_

#include <map>

#include "ash/public/cpp/feature_discovery_duration_reporter.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/session/session_controller_impl.h"
#include "base/gtest_prod_util.h"
#include "base/scoped_observation.h"

class PrefRegistrySimple;
class PrefService;

namespace base {
class TimeTicks;
}

namespace ash {

class FeatureDiscoveryDurationReporterImpl
    : public FeatureDiscoveryDurationReporter,
      public SessionObserver {
 public:
  explicit FeatureDiscoveryDurationReporterImpl(
      SessionController* session_controller);
  FeatureDiscoveryDurationReporterImpl(
      const FeatureDiscoveryDurationReporterImpl&) = delete;
  FeatureDiscoveryDurationReporterImpl& operator=(
      const FeatureDiscoveryDurationReporterImpl&) = delete;
  ~FeatureDiscoveryDurationReporterImpl() override;

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // FeatureDiscoveryDurationReporter:
  void MaybeActivateObservation(
      feature_discovery::TrackableFeature feature) override;
  void MaybeFinishObservation(
      feature_discovery::TrackableFeature feature) override;
  void AddObserver(ReporterObserver* observer) override;
  void RemoveObserver(ReporterObserver* observer) override;

 private:
  friend class FeatureDiscoveryDurationReporterImplTest;

  FRIEND_TEST_ALL_PREFIXES(FeatureDiscoveryDurationReporterBrowserTest,
                           SaveCumulatedTimeWhenSignout);

  // Activates/deactivates the reporter.
  void SetActive(bool active);

  // Implements activation/deactivation.
  void Activate();
  void Deactivate();

  // SessionObserver:
  void OnSessionStateChanged(session_manager::SessionState state) override;
  void OnActiveUserPrefServiceChanged(PrefService* pref_service) override;

  bool is_active() const { return is_active_; }

  base::ObserverList<ReporterObserver> observers_;

  // The mappings from trackable feature enum types to observation start
  // timestamps. It gets cleared when the reporter is deactivated. New entries
  // are added when:
  // 1. a new observation starts; or
  // 2. unfinished observations resume when the reporter is activated.
  std::map<feature_discovery::TrackableFeature, base::TimeTicks>
      active_time_recordings_;

  // Specifies the pref service whose data is read/written by the reporter. It
  // is set when the active user pref service changes.
  // NOTE: `active_pref_service_` is not reset when signing out. Because the
  // reporter instance should be destroyed in this scenario.
  raw_ptr<PrefService> active_pref_service_ = nullptr;

  // If true, starting observations on feature discovery is allowed.
  bool is_active_ = false;

  base::ScopedObservation<SessionController, SessionObserver>
      session_controller_observation_{this};
};

}  // namespace ash

#endif  // ASH_METRICS_FEATURE_DISCOVERY_DURATION_REPORTER_IMPL_H_
