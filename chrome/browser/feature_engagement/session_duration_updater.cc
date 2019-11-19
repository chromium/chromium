// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/feature_engagement/session_duration_updater.h"

#include "base/timer/elapsed_timer.h"
#include "base/values.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/scoped_user_pref_update.h"

namespace feature_engagement {

SessionDurationUpdater::SessionDurationUpdater(
    PrefService* pref_service,
    const char* observed_session_time_dict_key)
    : duration_tracker_observer_(this),
      pref_service_(pref_service),
      observed_session_time_dict_key_(observed_session_time_dict_key) {
  AddDurationTrackerObserver();
}

SessionDurationUpdater::~SessionDurationUpdater() = default;

// static
void SessionDurationUpdater::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterDictionaryPref(prefs::kObservedSessionTime);
}

base::TimeDelta SessionDurationUpdater::GetCumulativeElapsedSessionTime()
    const {
  base::TimeDelta elapsed_time = GetRecordedObservedSessionTime();
  return current_session_timer_
             ? elapsed_time + current_session_timer_.get()->Elapsed()
             : elapsed_time;
}

base::TimeDelta SessionDurationUpdater::GetRecordedObservedSessionTime() const {
  const base::DictionaryValue* dict =
      pref_service_->GetDictionary(prefs::kObservedSessionTime);
  const double stored_value =
      dict->FindDoubleKey(observed_session_time_dict_key_).value_or(0);
  return base::TimeDelta::FromSeconds(stored_value);
}

void SessionDurationUpdater::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);

  // Re-adds SessionDurationUpdater as an observer of
  // DesktopSessionDurationTracker if another feature is added after
  // SessionDurationUpdater was removed.
  if (!duration_tracker_observer_.IsObserving(
          metrics::DesktopSessionDurationTracker::Get())) {
    if (!current_session_timer_)
      current_session_timer_ = std::make_unique<base::ElapsedTimer>();
    AddDurationTrackerObserver();
  }
}

void SessionDurationUpdater::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
  // If all the observer Features have removed themselves due to their active
  // time limits have been reached, the SessionDurationUpdater removes itself
  // as an observer of DesktopSessionDurationTracker.
  if (!observer_list_.might_have_observers()) {
    current_session_timer_.reset();
    RemoveDurationTrackerObserver();
  }
}

void SessionDurationUpdater::OnSessionStarted(base::TimeTicks session_start) {
  current_session_timer_ = std::make_unique<base::ElapsedTimer>();
}

void SessionDurationUpdater::OnSessionEnded(base::TimeDelta elapsed) {
  // This case is only used during testing as that is the only case that
  // DesktopSessionDurationTracker isn't calling this on its observer.
  if (!duration_tracker_observer_.IsObserving(
          metrics::DesktopSessionDurationTracker::Get())) {
    return;
  }

  const base::DictionaryValue* dict =
      pref_service_->GetDictionary(prefs::kObservedSessionTime);
  const double stored_value =
      dict->FindDoubleKey(observed_session_time_dict_key_).value_or(0);

  base::TimeDelta elapsed_session_time =
      base::TimeDelta::FromSeconds(stored_value) + elapsed;

  DictionaryPrefUpdate update(pref_service_, prefs::kObservedSessionTime);
  update->SetKey(
      observed_session_time_dict_key_,
      base::Value(static_cast<double>(elapsed_session_time.InSeconds())));

  current_session_timer_.reset();

  for (Observer& observer : observer_list_)
    observer.OnSessionEnded(elapsed_session_time);
}

void SessionDurationUpdater::AddDurationTrackerObserver() {
  duration_tracker_observer_.Add(metrics::DesktopSessionDurationTracker::Get());
}

void SessionDurationUpdater::RemoveDurationTrackerObserver() {
  duration_tracker_observer_.Remove(
      metrics::DesktopSessionDurationTracker::Get());
}

PrefService* SessionDurationUpdater::GetPrefs() {
  return pref_service_;
}

}  // namespace feature_engagement
