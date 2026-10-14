// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/metrics/runtime_field_trial_overrides.h"

#include "base/check.h"
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
    std::unique_ptr<RuntimeFieldTrialInfo> override_info,
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
    if (previous_override->second->overridden_trial() !=
        override_info->overridden_trial()) {
      return false;
    }
    previous_override->second->trial_registered_ = false;
    overrides_.erase(previous_override);
  }

  auto [it, inserted] = overrides_.try_emplace(override_info->trial_name(),
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
    observer.OnRuntimeFieldTrialOverride(*it->second,
                                         previous_override_trial_name);
  }
  // Set `trial_registered_` only after all observers have been notified via
  // `OnRuntimeFieldTrialOverride()`. If an observer activates `*it->second`
  // inside `OnRuntimeFieldTrialOverride()` (or if `override_info` was already
  // activated before `ApplyRuntimeOverride()`), deferring
  // `NotifyRuntimeOverrideActivated()` until after the loop guarantees that
  // every observer receives `OnRuntimeFieldTrialOverride()` for this trial
  // before `OnRuntimeFieldTrialOverrideActivated()`.
  it->second->trial_registered_ = true;
  if (it->second->is_active()) {
    NotifyRuntimeOverrideActivated(*it->second);
  }
  return true;
}

bool RuntimeFieldTrialOverrides::IsRuntimeOverrideActive(
    std::string_view trial_name) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = overrides_.find(trial_name);
  return it != overrides_.end() && it->second->is_active();
}

const flat_map<std::string, std::unique_ptr<RuntimeFieldTrialInfo>>&
RuntimeFieldTrialOverrides::GetRuntimeOverrides() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return overrides_;
}

RuntimeFieldTrialInfo* RuntimeFieldTrialOverrides::GetRuntimeOverride(
    std::string_view trial_name) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = overrides_.find(trial_name);
  if (it == overrides_.end()) {
    return nullptr;
  }
  return it->second.get();
}

bool RuntimeFieldTrialOverrides::IsFieldTrialOverridden(
    const FieldTrial& trial) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (const auto& [trial_name, override_info] : overrides_) {
    if (override_info->overridden_trial() == &trial) {
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

void RuntimeFieldTrialOverrides::NotifyRuntimeOverrideActivated(
    const RuntimeFieldTrialInfo& override_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (auto& observer : observers_) {
    observer.OnRuntimeFieldTrialOverrideActivated(override_info);
  }
}

RuntimeFieldTrialInfo::RuntimeFieldTrialInfo(std::string trial_name,
                                             std::string group_name,
                                             base::FieldTrialParams params,
                                             const FieldTrial* overridden_trial)
    : trial_name_(std::move(trial_name)),
      group_name_(std::move(group_name)),
      params_(std::move(params)),
      overridden_trial_(overridden_trial) {
  CHECK(!trial_name_.empty());
  CHECK(!group_name_.empty());
}

RuntimeFieldTrialInfo::~RuntimeFieldTrialInfo() = default;

void RuntimeFieldTrialInfo::Activate() {
  if (is_active_) {
    return;
  }
  is_active_ = true;
  if (trial_registered_) {
    RuntimeFieldTrialOverrides::GetInstance()->NotifyRuntimeOverrideActivated(
        *this);
  }
}

}  // namespace base
