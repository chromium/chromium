// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_ASSISTANT_ALARM_TIMER_CONTROLLER_H_
#define ASH_ASSISTANT_ASSISTANT_ALARM_TIMER_CONTROLLER_H_

#include <map>
#include <string>

#include "ash/assistant/assistant_controller_observer.h"
#include "ash/assistant/model/assistant_alarm_timer_model.h"
#include "ash/assistant/model/assistant_alarm_timer_model_observer.h"
#include "ash/assistant/model/assistant_ui_model_observer.h"
#include "ash/public/mojom/assistant_controller.mojom.h"
#include "base/macros.h"
#include "base/timer/timer.h"
#include "chromeos/services/assistant/public/mojom/assistant.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace ash {

namespace assistant {
namespace util {
enum class AlarmTimerAction;
}  // namespace util
}  // namespace assistant

class AssistantController;

// The AssistantAlarmTimerController is a sub-controller of AssistantController
// tasked with tracking alarm/timer state and providing alarm/timer APIs.
class AssistantAlarmTimerController
    : public mojom::AssistantAlarmTimerController,
      public AssistantControllerObserver,
      public AssistantAlarmTimerModelObserver,
      public AssistantUiModelObserver {
 public:
  explicit AssistantAlarmTimerController(
      AssistantController* assistant_controller);
  ~AssistantAlarmTimerController() override;

  void BindReceiver(
      mojo::PendingReceiver<mojom::AssistantAlarmTimerController> receiver);

  // Returns the underlying model.
  const AssistantAlarmTimerModel* model() const { return &model_; }

  // Adds/removes the specified model |observer|.
  void AddModelObserver(AssistantAlarmTimerModelObserver* observer);
  void RemoveModelObserver(AssistantAlarmTimerModelObserver* observer);

  // mojom::AssistantAlarmTimerController:
  void OnAlarmTimerStateChanged(
      mojom::AssistantAlarmTimerEventPtr event) override;

  // AssistantAlarmTimerModelObserver:
  void OnAlarmTimerAdded(const AlarmTimer& alarm_timer,
                         const base::TimeDelta& time_remaining) override;
  void OnAlarmsTimersTicked(
      const std::map<std::string, base::TimeDelta>& times_remaining) override;
  void OnAllAlarmsTimersRemoved() override;

  // Provides a pointer to the |assistant| owned by AssistantController.
  void SetAssistant(chromeos::assistant::mojom::Assistant* assistant);

  // AssistantControllerObserver:
  void OnAssistantControllerConstructed() override;
  void OnAssistantControllerDestroying() override;
  void OnDeepLinkReceived(
      assistant::util::DeepLinkType type,
      const std::map<std::string, std::string>& params) override;

  // AssistantUiModelObserver:
  void OnUiVisibilityChanged(
      AssistantVisibility new_visibility,
      AssistantVisibility old_visibility,
      base::Optional<AssistantEntryPoint> entry_point,
      base::Optional<AssistantExitPoint> exit_point) override;

 private:
  void PerformAlarmTimerAction(
      const assistant::util::AlarmTimerAction& action,
      const base::Optional<std::string>& alarm_timer_id,
      const base::Optional<base::TimeDelta>& duration);

  AssistantController* const assistant_controller_;  // Owned by Shell.

  mojo::Receiver<mojom::AssistantAlarmTimerController> receiver_{this};

  AssistantAlarmTimerModel model_;

  base::RepeatingTimer timer_;

  // Owned by AssistantController.
  chromeos::assistant::mojom::Assistant* assistant_;

  DISALLOW_COPY_AND_ASSIGN(AssistantAlarmTimerController);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_ASSISTANT_ALARM_TIMER_CONTROLLER_H_
