// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/model/assistant_alarm_timer_model.h"

#include "ash/assistant/model/assistant_alarm_timer_model_observer.h"
#include "base/stl_util.h"

namespace ash {

AssistantAlarmTimerModel::AssistantAlarmTimerModel() = default;

AssistantAlarmTimerModel::~AssistantAlarmTimerModel() = default;

void AssistantAlarmTimerModel::AddObserver(
    AssistantAlarmTimerModelObserver* observer) {
  observers_.AddObserver(observer);
}

void AssistantAlarmTimerModel::RemoveObserver(
    AssistantAlarmTimerModelObserver* observer) {
  observers_.RemoveObserver(observer);
}

void AssistantAlarmTimerModel::AddAlarmTimer(const AlarmTimer& alarm_timer) {
  DCHECK(!base::Contains(alarms_timers_, alarm_timer.id));
  alarms_timers_[alarm_timer.id] = alarm_timer;
  NotifyAlarmTimerAdded(alarm_timer, /*time_remaining=*/base::TimeTicks::Now() -
                                         alarm_timer.end_time);
}

void AssistantAlarmTimerModel::RemoveAllAlarmsTimers() {
  if (alarms_timers_.empty())
    return;

  alarms_timers_.clear();
  NotifyAllAlarmsTimersRemoved();
}

const AlarmTimer* AssistantAlarmTimerModel::GetAlarmTimerById(
    const std::string& id) const {
  auto it = alarms_timers_.find(id);
  return it != alarms_timers_.end() ? &it->second : nullptr;
}

void AssistantAlarmTimerModel::Tick() {
  const base::TimeTicks now = base::TimeTicks::Now();

  // Calculate remaining time for all tracked alarms/timers.
  std::map<std::string, base::TimeDelta> times_remaining;
  for (auto& alarm_timer : alarms_timers_)
    times_remaining[alarm_timer.first] = now - alarm_timer.second.end_time;

  NotifyAlarmsTimersTicked(times_remaining);
}

void AssistantAlarmTimerModel::NotifyAlarmTimerAdded(
    const AlarmTimer& alarm_timer,
    const base::TimeDelta& time_remaining) {
  for (auto& observer : observers_)
    observer.OnAlarmTimerAdded(alarm_timer, time_remaining);
}

void AssistantAlarmTimerModel::NotifyAlarmsTimersTicked(
    const std::map<std::string, base::TimeDelta>& times_remaining) {
  for (auto& observer : observers_)
    observer.OnAlarmsTimersTicked(times_remaining);
}

void AssistantAlarmTimerModel::NotifyAllAlarmsTimersRemoved() {
  for (auto& observer : observers_)
    observer.OnAllAlarmsTimersRemoved();
}

}  // namespace ash
