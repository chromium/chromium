// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_MODEL_ASSISTANT_ALARM_TIMER_MODEL_OBSERVER_H_
#define ASH_ASSISTANT_MODEL_ASSISTANT_ALARM_TIMER_MODEL_OBSERVER_H_

#include "ash/public/cpp/assistant/controller/assistant_alarm_timer_controller.h"
#include "base/component_export.h"
#include "base/observer_list_types.h"

namespace ash {

// A checked observer which receives notification of changes to the Assistant
// alarm/timer model.
class COMPONENT_EXPORT(ASSISTANT_MODEL) AssistantAlarmTimerModelObserver
    : public base::CheckedObserver {
 public:
  // Invoked when the specified timer has been added.
  virtual void OnTimerAdded(const assistant::AssistantTimer& timer) {}

  // Invoked when the specified timer has been updated.
  virtual void OnTimerUpdated(const assistant::AssistantTimer& timer) {}

  // Invoked when the specified timer has been removed.
  virtual void OnTimerRemoved(const assistant::AssistantTimer& timer) {}

 protected:
  ~AssistantAlarmTimerModelObserver() override = default;
};

}  // namespace ash

#endif  // ASH_ASSISTANT_MODEL_ASSISTANT_ALARM_TIMER_MODEL_OBSERVER_H_
