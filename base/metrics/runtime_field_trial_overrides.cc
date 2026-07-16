// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/metrics/runtime_field_trial_overrides.h"

#include "base/no_destructor.h"

namespace base {

RuntimeFieldTrialOverrides::~RuntimeFieldTrialOverrides() = default;

// static
RuntimeFieldTrialOverrides* RuntimeFieldTrialOverrides::GetInstance() {
  static base::NoDestructor<RuntimeFieldTrialOverrides> instance;
  return instance.get();
}

bool RuntimeFieldTrialOverrides::ApplyRuntimeOverride(
    base::PassKey<variations::VariationsService>,
    std::string_view trial_name,
    std::string_view group_name,
    const FieldTrial* overridden_trial,
    std::string_view previous_override_trial_name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // If this override is replacing a previous override, remove it first.
  if (!previous_override_trial_name.empty()) {
    auto previous_override = overrides_.find(previous_override_trial_name);
    if (previous_override == overrides_.end()) {
      return false;
    }

    // The previous override should be overriding the same trial as the
    // new override. (Or, they should both be null -- i.e. not be overriding
    // any specific trial).
    if (previous_override->second.overridden_trial != overridden_trial) {
      return false;
    }
    overrides_.erase(previous_override);
  }

  RuntimeOverrideInfo override_info{
      .trial_name = std::string(trial_name),
      .group_name = std::string(group_name),
      .overridden_trial = overridden_trial,
  };
  auto [it, inserted] = overrides_.try_emplace(override_info.trial_name,
                                               std::move(override_info));
  // The override trial name should not already exist. The scenario where an
  // override replaces a previous override with the same name is valid and is
  // handled above (by removing the previous override first). E.g. it is valid
  // if "MyTrial" is overridden by "MyTrialKillswitch/Disabled50Pct" and then
  // later overridden again by "MyTrialKillswitch/Disabled100Pct".
  // On the other hand, if there is an actual trial name collision, it is not
  // valid. For example, if a seed is received with a very generic runtime
  // override trial name "Killswitch" that killswitches "MyTrial". Then, later,
  // a new seed is received, this time with the same runtime override trial name
  // "Killswitch" that this time killswitches a completely different and
  // unrelated trial "MyOtherTrial". In this case, the second override should be
  // rejected (otherwise it would result in reporting duplicate trial names with
  // potentially different groups in metrics systems like UMA).
  // The caller should ensure that this does not happen.
  if (!inserted) {
    return false;
  }

  for (auto& observer : observers_) {
    observer.OnRuntimeFieldTrialOverride(it->second,
                                         previous_override_trial_name);
  }
  return true;
}

const flat_map<std::string, RuntimeFieldTrialOverrides::RuntimeOverrideInfo>&
RuntimeFieldTrialOverrides::GetRuntimeOverrides() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return overrides_;
}

std::optional<RuntimeFieldTrialOverrides::RuntimeOverrideInfo>
RuntimeFieldTrialOverrides::GetRuntimeOverride(
    std::string_view trial_name) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = overrides_.find(trial_name);
  if (it == overrides_.end()) {
    return std::nullopt;
  }
  return it->second;
}

bool RuntimeFieldTrialOverrides::IsFieldTrialOverridden(
    const FieldTrial& trial) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (const auto& [trial_name, override_info] : overrides_) {
    if (override_info.overridden_trial == &trial) {
      return true;
    }
  }
  return false;
}

void RuntimeFieldTrialOverrides::AddObserver(Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.AddObserver(observer);
}

void RuntimeFieldTrialOverrides::RemoveObserver(Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.RemoveObserver(observer);
}

void RuntimeFieldTrialOverrides::ResetForTesting() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  overrides_.clear();
  observers_.Clear();
}

RuntimeFieldTrialOverrides::RuntimeFieldTrialOverrides() = default;

}  // namespace base
