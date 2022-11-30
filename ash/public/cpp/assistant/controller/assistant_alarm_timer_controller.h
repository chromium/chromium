// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_ASSISTANT_CONTROLLER_ASSISTANT_ALARM_TIMER_CONTROLLER_H_
#define ASH_PUBLIC_CPP_ASSISTANT_CONTROLLER_ASSISTANT_ALARM_TIMER_CONTROLLER_H_

#include <memory>
#include <string>
#include <vector>

#include "ash/public/cpp/ash_public_export.h"

namespace ash {

namespace assistant {
struct AssistantTimer;
}

class AssistantAlarmTimerModel;

// Interface to the AssistantAlarmTimerController which is owned by the
// AssistantController. Currently used by the Assistant service to notify Ash
// of changes to the underlying alarm/timer state in LibAssistant.
class ASH_PUBLIC_EXPORT AssistantAlarmTimerController {
 public:
  AssistantAlarmTimerController();
  AssistantAlarmTimerController(const AssistantAlarmTimerController&) = delete;
  AssistantAlarmTimerController& operator=(
      const AssistantAlarmTimerController&) = delete;
  virtual ~AssistantAlarmTimerController();

  // Returns the singleton instance owned by AssistantController.
  static AssistantAlarmTimerController* Get();

  // Returns a pointer to the underlying model.
  virtual const AssistantAlarmTimerModel* GetModel() const = 0;

  // Invoked when timer state has changed. Note that |timers| may be empty.
  virtual void OnTimerStateChanged(
      const std::vector<assistant::AssistantTimer>& timers) = 0;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_ASSISTANT_CONTROLLER_ASSISTANT_ALARM_TIMER_CONTROLLER_H_
