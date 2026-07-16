// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_METRICS_RUNTIME_FIELD_TRIAL_OVERRIDES_H_
#define BASE_METRICS_RUNTIME_FIELD_TRIAL_OVERRIDES_H_

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/base_export.h"
#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/field_trial.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/types/pass_key.h"

namespace variations {
class VariationsService;
}

namespace base {

// Manages (applying and retrieving) runtime FieldTrial overrides for features
// that have been declared as runtime mutable (BASE_RUNTIME_MUTABLE_FEATURE).
// This class is not thread-safe and should only be used on the main sequence.
class BASE_EXPORT RuntimeFieldTrialOverrides {
 public:
  // Information about a FieldTrial runtime override.
  struct RuntimeOverrideInfo {
    // The trial name of the override.
    std::string trial_name;
    // The group name of the override.
    std::string group_name;
    // The FieldTrial that is being overridden. This may be null if no
    // specific trial is being overridden (e.g. a killswitch for an
    // ENABLED_BY_DEFAULT feature). Weak pointer (owned by the FieldTrialList
    // singleton).
    raw_ptr<const FieldTrial> overridden_trial;
  };

  // Observer interface for being notified when a runtime override is applied.
  class Observer : public CheckedObserver {
   public:
    // Called when a runtime override is applied. `override_info` contains
    // the information about the override. `previous_override_trial_name`, if
    // non-empty, is the name of the previous override that is being replaced
    // by this override.
    //
    // For example, if a trial named "MyTrial" is being overridden and
    // killswitched for the first time by the trial "MyTrialKillswitch50Pct",
    // then `previous_override_trial_name` will be empty. If it is then
    // overridden again by "MyTrialKillswitch100Pct", then this will be called
    // with `previous_override_trial_name` set to "MyTrialKillswitch50Pct" to
    // indicate that this previous override has been superseded.
    //
    // NOTE: Do not add/remove observers in this callback.
    virtual void OnRuntimeFieldTrialOverride(
        const RuntimeOverrideInfo& override_info,
        std::string_view previous_override_trial_name) = 0;
  };

  RuntimeFieldTrialOverrides(const RuntimeFieldTrialOverrides&) = delete;
  RuntimeFieldTrialOverrides& operator=(const RuntimeFieldTrialOverrides&) =
      delete;
  ~RuntimeFieldTrialOverrides();

  // Returns the singleton instance.
  static RuntimeFieldTrialOverrides* GetInstance();

  // Applies a FieldTrial runtime override. Returns false if the application
  // fails.
  //
  // `trial_name`: The override's trial name.
  // `group_name`: The override's group name.
  // `overridden_trial`: The FieldTrial that is being overridden. This may be
  //     nullptr if no specific trial is being overridden (e.g. a killswitch for
  //     an ENABLED_BY_DEFAULT feature that is not being controlled by any
  //     FieldTrial).
  // `previous_override_trial_name`: The name of the previous override that this
  //     override is replacing, if any. If this is non-empty, the previous
  //     override will be removed before the new override is applied.
  //     For example, consider a field trial "MyTrial" being overridden by
  //     "Killswitch50Pct" which is later overridden by "Killswitch100Pct".
  //     When "MyTrial" is overridden by "Killswitch50Pct", this field should
  //     be empty. When "Killswitch50Pct" is later overridden by
  //     "Killswitch100Pct", this field should be set to "Killswitch50Pct".
  //     Note: in a more realistic scenario, the override trial name would stay
  //     the same, and only the group name would change. For example,
  //     "MyTrial/Enabled" would be overridden by "Killswitch/Disabled50Pct"
  //     and then later overridden by "Killswitch/Disabled100Pct" (this field
  //     should be set to "Killswitch" at that time). In this final state, the
  //     final override is "Killswitch/Disabled100Pct" (i.e. this is what should
  //     be reported in the various metrics systems like UMA).
  //
  // TODO(crbug.com/482451143): Consider making `previous_override_trial_name`
  // not an input but rather something that is tracked by this class internally.
  bool ApplyRuntimeOverride(base::PassKey<variations::VariationsService>,
                            std::string_view trial_name,
                            std::string_view group_name,
                            const FieldTrial* overridden_trial,
                            std::string_view previous_override_trial_name = "");

  // Returns all applied runtime overrides. The returned map should not be
  // accessed other than on the main sequence.
  const flat_map<std::string, RuntimeOverrideInfo>& GetRuntimeOverrides() const;

  // Returns the registered runtime trial override with name `trial_name`, if
  // any.
  std::optional<RuntimeOverrideInfo> GetRuntimeOverride(
      std::string_view trial_name) const;

  // Returns true if the given `trial` is currently being overridden by a
  // runtime FieldTrial override.
  bool IsFieldTrialOverridden(const FieldTrial& trial) const;

  // Adds/removes an observer to be notified when a runtime override is
  // applied. Observers are not notified of existing overrides when they are
  // added.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Clears internal state for testing.
  void ResetForTesting();

 private:
  friend class base::NoDestructor<RuntimeFieldTrialOverrides>;

  // Private constructor to enforce singleton pattern.
  RuntimeFieldTrialOverrides();

  // Map of all applied runtime FieldTrial overrides. Keyed by the override's
  // trial name. E.g., if the runtime override trial
  // "MyFeatureKillswitch" overrides the "MyFeature" trial, the key would be
  // "MyFeatureKillswitch".
  flat_map<std::string, RuntimeOverrideInfo> overrides_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // List of observers to notify when a runtime override is applied.
  base::ObserverList<Observer> observers_ GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace base

#endif  // BASE_METRICS_RUNTIME_FIELD_TRIAL_OVERRIDES_H_
