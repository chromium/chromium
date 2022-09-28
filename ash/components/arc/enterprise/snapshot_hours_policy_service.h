// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_ENTERPRISE_SNAPSHOT_HOURS_POLICY_SERVICE_H_
#define ASH_COMPONENTS_ARC_ENTERPRISE_SNAPSHOT_HOURS_POLICY_SERVICE_H_

#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/time/time.h"
#include "base/timer/wall_clock_timer.h"
#include "chromeos/ash/components/policy/weekly_time/weekly_time_interval.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/user_manager/user_manager.h"

class PrefService;

namespace arc {
namespace data_snapshotd {

// This class handles "DeviceArcDataSnapshotHours" policy, enables/disables ARC
// data snapshot feature, handles ARC data snapshot update intervals.
//
// ArcDataSnapshotdManager is an owner of this object.
class SnapshotHoursPolicyService : public user_manager::UserManager::Observer {
 public:
  // Observer interface.
  class Observer : public base::CheckedObserver {
   public:
    // Called once ARC data snapshot feature gets disabled by policy.
    virtual void OnSnapshotsDisabled() {}
    // Called once ARC data snapshot feature gets enabled by policy.
    virtual void OnSnapshotsEnabled() {}
    // Called once the interval for allowed ARC data snapshot update has
    // started/finished.
    // The observer can get an end time for started interval via
    // snapshot_update_end_time().
    virtual void OnSnapshotUpdateEndTimeChanged() {}
  };

  explicit SnapshotHoursPolicyService(PrefService* local_state);
  SnapshotHoursPolicyService(const SnapshotHoursPolicyService&) = delete;
  SnapshotHoursPolicyService& operator=(const SnapshotHoursPolicyService&) =
      delete;
  ~SnapshotHoursPolicyService() override;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Starts observing primary profile prefs only for MGS.
  void StartObservingPrimaryProfilePrefs(PrefService* profile_prefs);
  // Stops observing primary profile prefs.
  void StopObservingPrimaryProfilePrefs();

  // user_manager::UserManager::Observer overrides:
  void LocalStateChanged(user_manager::UserManager* user_manager) override;

  // Returns the end time of the current interval when ARC data snapshot update
  // is possible.
  // Returns null outside of the interval.
  base::Time snapshot_update_end_time() const {
    return snapshot_update_end_time_;
  }

  bool is_snapshot_enabled() const { return is_snapshot_enabled_; }

  const std::vector<policy::WeeklyTimeInterval>& get_intervals_for_testing()
      const {
    return intervals_;
  }
  const base::WallClockTimer* get_timer_for_testing() const { return &timer_; }

  void set_snapshot_update_end_time_for_testing(base::Time time) {
    snapshot_update_end_time_ = time;
  }

 private:
  // Processes the policy update: either ArcEnabled and
  // DeviceArcDataSnapshotHours.
  void UpdatePolicy();

  // Disables ARC data snapshot feature and notifies observers if necessary.
  void DisableSnapshots();
  // Enables ARC data snaoshot feature and notifies observers if necessary.
  void EnableSnapshots();

  // Updates ARC data snapshot update timer according to the policy.
  void UpdateTimer();

  // Starts timer with |update_time|.
  void StartTimer(const base::Time& update_time);
  // Stops timer.
  void StopTimer();
  // Changes |snapshot_update_end_time_| and notifies observers if necessary.
  void SetEndTime(base::Time end_time);

  // Notifies observers about relevant events.
  void NotifySnapshotsDisabled();
  void NotifySnapshotsEnabled();
  void NotifySnapshotUpdateEndTimeChanged();

  // Returns false if ARC is disabled for a logged-in MGS, otherwise returns
  // true.
  bool IsArcEnabled() const;

  // Returns true if MGS is configured for a device, otherwise returns false.
  bool IsMgsConfigured() const;

  // The feature is disabled when either kArcDataSnapshotHours policy is not set
  // or ARC is disabled by policy for MGS.
  bool is_snapshot_enabled_ = false;

  // Not owned.
  PrefService* const local_state_ = nullptr;

  // Owned by primary profile.
  PrefService* profile_prefs_ = nullptr;

  // Registrar for pref changes in local_state_.
  PrefChangeRegistrar pref_change_registrar_;
  // Registrar for pref changes in profile_prefs_.
  PrefChangeRegistrar profile_pref_change_registrar_;

  // The end time of the current interval if ARC data snapshot update is
  // possible. The value is null outside of all intervals.
  base::Time snapshot_update_end_time_;

  base::ObserverList<Observer> observers_;

  // Current "ArcDataSnapshotHours" time intervals.
  std::vector<policy::WeeklyTimeInterval> intervals_;

  // Timer for updating ARC data snapshot at the begin of next interval or at
  // the end of current interval.
  base::WallClockTimer timer_;

  base::WeakPtrFactory<SnapshotHoursPolicyService> weak_ptr_factory_{this};
};

}  // namespace data_snapshotd
}  // namespace arc

#endif  // ASH_COMPONENTS_ARC_ENTERPRISE_SNAPSHOT_HOURS_POLICY_SERVICE_H_
