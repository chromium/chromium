// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/enterprise/snapshot_hours_policy_service.h"

#include <memory>
#include <tuple>
#include <utility>

#include "ash/components/arc/arc_prefs.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/time/default_clock.h"
#include "base/time/tick_clock.h"
#include "base/values.h"
#include "chromeos/ash/components/policy/weekly_time/time_utils.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"

namespace arc {
namespace data_snapshotd {

SnapshotHoursPolicyService::SnapshotHoursPolicyService(PrefService* local_state)
    : local_state_(local_state) {
  DCHECK(local_state_);
  pref_change_registrar_.Init(local_state_);
  pref_change_registrar_.Add(
      prefs::kArcSnapshotHours,
      base::BindRepeating(&SnapshotHoursPolicyService::UpdatePolicy,
                          weak_ptr_factory_.GetWeakPtr()));

  DCHECK(user_manager::UserManager::Get());
  user_manager::UserManager::Get()->AddObserver(this);

  UpdatePolicy();
}

SnapshotHoursPolicyService::~SnapshotHoursPolicyService() {
  DCHECK(user_manager::UserManager::Get());
  user_manager::UserManager::Get()->RemoveObserver(this);
}

void SnapshotHoursPolicyService::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void SnapshotHoursPolicyService::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void SnapshotHoursPolicyService::StartObservingPrimaryProfilePrefs(
    PrefService* profile_prefs) {
  if (!user_manager::UserManager::Get()->IsLoggedInAsPublicAccount()) {
    // Do not care about ArcEnabled policy for other than MGS.
    return;
  }
  profile_prefs_ = profile_prefs;
  profile_pref_change_registrar_.Init(profile_prefs_);
  profile_pref_change_registrar_.Add(
      prefs::kArcEnabled,
      base::BindRepeating(&SnapshotHoursPolicyService::UpdatePolicy,
                          weak_ptr_factory_.GetWeakPtr()));

  UpdatePolicy();
}

void SnapshotHoursPolicyService::StopObservingPrimaryProfilePrefs() {
  if (!profile_prefs_)
    return;
  profile_pref_change_registrar_.RemoveAll();
  profile_prefs_ = nullptr;
  UpdatePolicy();
}

void SnapshotHoursPolicyService::LocalStateChanged(
    user_manager::UserManager* user_manager) {
  UpdatePolicy();
}

void SnapshotHoursPolicyService::UpdatePolicy() {
  intervals_.clear();
  base::ScopedClosureRunner snapshot_disabler(
      base::BindOnce(&SnapshotHoursPolicyService::DisableSnapshots,
                     weak_ptr_factory_.GetWeakPtr()));

  if (!IsMgsConfigured())
    return;
  if (!IsArcEnabled())
    return;

  const base::Value::Dict& dict =
      local_state_->GetDict(prefs::kArcSnapshotHours);

  const auto* timezone = dict.FindString("timezone");
  std::string timezone_str = "";
  if (!timezone || *timezone == "UNSET") {
    std::unique_ptr<icu::TimeZone> zone(icu::TimeZone::detectHostTimeZone());
    icu::UnicodeString zone_id;
    zone->getID(zone_id).toUTF8String(timezone_str);
    VLOG(2) << "Local timezone detected: " << timezone_str;
  } else {
    timezone_str = *timezone;
  }

  int offset;
  if (!policy::weekly_time_utils::GetOffsetFromTimezoneToGmt(
          timezone_str, base::DefaultClock::GetInstance(), &offset)) {
    return;
  }

  const auto* intervals = dict.FindList("intervals");
  if (!intervals)
    return;

  for (const auto& entry : *intervals) {
    if (!entry.is_dict())
      continue;
    auto interval =
        policy::WeeklyTimeInterval::ExtractFromDict(entry.GetDict(), -offset);
    if (interval)
      intervals_.push_back(*interval);
  }
  intervals_ = policy::weekly_time_utils::ConvertIntervalsToGmt(intervals_);
  if (intervals_.empty())
    return;

  std::ignore = snapshot_disabler.Release();
  EnableSnapshots();
}

void SnapshotHoursPolicyService::DisableSnapshots() {
  if (!is_snapshot_enabled_)
    return;

  is_snapshot_enabled_ = false;
  StopTimer();
  SetEndTime(base::Time());
  NotifySnapshotsDisabled();
}

void SnapshotHoursPolicyService::EnableSnapshots() {
  if (is_snapshot_enabled_)
    return;
  is_snapshot_enabled_ = true;

  UpdateTimer();
  NotifySnapshotsEnabled();
}

void SnapshotHoursPolicyService::UpdateTimer() {
  namespace wtu = ::policy::weekly_time_utils;
  const base::Time now = base::Time::Now();
  const bool in_interval = wtu::Contains(now, intervals_);
  const absl::optional<base::Time> update_time =
      wtu::GetNextEventTime(now, intervals_);

  SetEndTime(in_interval ? update_time.value() : base::Time{});
  if (update_time)
    StartTimer(update_time.value());
  else
    StopTimer();
}

void SnapshotHoursPolicyService::StartTimer(const base::Time& update_time) {
  timer_.Start(FROM_HERE, update_time,
               base::BindOnce(&SnapshotHoursPolicyService::UpdateTimer,
                              weak_ptr_factory_.GetWeakPtr()));
}

void SnapshotHoursPolicyService::StopTimer() {
  timer_.Stop();
}

void SnapshotHoursPolicyService::SetEndTime(base::Time end_time) {
  if (snapshot_update_end_time_ == end_time)
    return;
  snapshot_update_end_time_ = end_time;
  NotifySnapshotUpdateEndTimeChanged();
}

void SnapshotHoursPolicyService::NotifySnapshotsDisabled() {
  for (auto& observer : observers_)
    observer.OnSnapshotsDisabled();
}

void SnapshotHoursPolicyService::NotifySnapshotsEnabled() {
  for (auto& observer : observers_)
    observer.OnSnapshotsEnabled();
}

void SnapshotHoursPolicyService::NotifySnapshotUpdateEndTimeChanged() {
  for (auto& observer : observers_)
    observer.OnSnapshotUpdateEndTimeChanged();
}

bool SnapshotHoursPolicyService::IsArcEnabled() const {
  // Assume ARC is enabled if there is no profile prefs.
  return !profile_prefs_ || profile_prefs_->GetBoolean(prefs::kArcEnabled);
}

bool SnapshotHoursPolicyService::IsMgsConfigured() const {
  for (auto* const user : user_manager::UserManager::Get()->GetUsers()) {
    if (user->GetType() == user_manager::UserType::USER_TYPE_PUBLIC_ACCOUNT)
      return true;
  }
  return false;
}

}  // namespace data_snapshotd
}  // namespace arc
