// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_MODEL_ASSISTANT_ALARM_TIMER_MODEL_H_
#define ASH_ASSISTANT_MODEL_ASSISTANT_ALARM_TIMER_MODEL_H_

#include <map>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/observer_list.h"

namespace ash {

namespace assistant {
struct AssistantTimer;
}

class AssistantAlarmTimerModelObserver;

// The model belonging to AssistantAlarmTimerController which tracks alarm/timer
// state and notifies a pool of observers.
class COMPONENT_EXPORT(ASSISTANT_MODEL) AssistantAlarmTimerModel {
 public:
  AssistantAlarmTimerModel();

  AssistantAlarmTimerModel(const AssistantAlarmTimerModel&) = delete;
  AssistantAlarmTimerModel& operator=(const AssistantAlarmTimerModel&) = delete;

  ~AssistantAlarmTimerModel();

  // Adds/removes the specified alarm/timer model |observer|.
  void AddObserver(AssistantAlarmTimerModelObserver* observer) const;
  void RemoveObserver(AssistantAlarmTimerModelObserver* observer) const;

  // Adds or updates the timer specified by |timer.id| in the model.
  void AddOrUpdateTimer(const assistant::AssistantTimer& timer);

  // Removes the timer uniquely identified by |id|.
  void RemoveTimer(const std::string& id);

  // Remove all timers from the model.
  void RemoveAllTimers();

  // Returns all timers from the model.
  std::vector<const assistant::AssistantTimer*> GetAllTimers() const;

  // Returns the timer uniquely identified by |id|.
  const assistant::AssistantTimer* GetTimerById(const std::string& id) const;

  // Returns |true| if the model contains no timers, |false| otherwise.
  bool empty() const { return timers_.empty(); }

 private:
  void NotifyTimerAdded(const assistant::AssistantTimer& timer);
  void NotifyTimerUpdated(const assistant::AssistantTimer& timer);
  void NotifyTimerRemoved(const assistant::AssistantTimer& timer);

  std::map<std::string, assistant::AssistantTimer> timers_;

  mutable base::ObserverList<AssistantAlarmTimerModelObserver> observers_;
};

}  // namespace ash

#endif  // ASH_ASSISTANT_MODEL_ASSISTANT_ALARM_TIMER_MODEL_H_
