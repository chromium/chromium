// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/model/assistant_alarm_timer_model.h"

#include <utility>

#include "ash/assistant/model/assistant_alarm_timer_model_observer.h"
#include "base/time/time.h"
#include "chromeos/ash/services/libassistant/public/cpp/assistant_timer.h"

namespace ash {

using assistant::AssistantTimer;

AssistantAlarmTimerModel::AssistantAlarmTimerModel() = default;

AssistantAlarmTimerModel::~AssistantAlarmTimerModel() = default;

void AssistantAlarmTimerModel::AddObserver(
    AssistantAlarmTimerModelObserver* observer) const {
  observers_.AddObserver(observer);
}

void AssistantAlarmTimerModel::RemoveObserver(
    AssistantAlarmTimerModelObserver* observer) const {
  observers_.RemoveObserver(observer);
}

void AssistantAlarmTimerModel::AddOrUpdateTimer(
    const AssistantTimer& const_timer) {
  AssistantTimer timer = const_timer;

  auto it = timers_.find(timer.id);
  if (it == timers_.end()) {
    timer.creation_time = timer.creation_time.value_or(base::Time::Now());
    timers_[timer.id] = timer;
    timers_[timer.id] = timer;
    NotifyTimerAdded(timer);
    return;
  }

  // If not explicitly provided, carry forward |creation_time|. This allows us
  // to track the lifetime of |timer| across updates.
  timer.creation_time =
      timer.creation_time.value_or(timers_[timer.id].creation_time.value());

  timers_[timer.id] = timer;
  NotifyTimerUpdated(timer);
}

void AssistantAlarmTimerModel::RemoveTimer(const std::string& id) {
  auto it = timers_.find(id);
  if (it == timers_.end())
    return;

  AssistantTimer timer = it->second;
  timers_.erase(it);

  NotifyTimerRemoved(timer);
}

void AssistantAlarmTimerModel::RemoveAllTimers() {
  while (!timers_.empty())
    RemoveTimer(timers_.begin()->second.id);
}

std::vector<const AssistantTimer*> AssistantAlarmTimerModel::GetAllTimers()
    const {
  std::vector<const AssistantTimer*> timers;
  for (const auto& pair : timers_)
    timers.push_back(&pair.second);
  return timers;
}

const AssistantTimer* AssistantAlarmTimerModel::GetTimerById(
    const std::string& id) const {
  auto it = timers_.find(id);
  return it != timers_.end() ? &it->second : nullptr;
}

void AssistantAlarmTimerModel::NotifyTimerAdded(const AssistantTimer& timer) {
  for (auto& observer : observers_)
    observer.OnTimerAdded(timer);
}

void AssistantAlarmTimerModel::NotifyTimerUpdated(const AssistantTimer& timer) {
  for (auto& observer : observers_)
    observer.OnTimerUpdated(timer);
}

void AssistantAlarmTimerModel::NotifyTimerRemoved(const AssistantTimer& timer) {
  for (auto& observer : observers_)
    observer.OnTimerRemoved(timer);
}

}  // namespace ash
