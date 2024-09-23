// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_API_TASKS_TASKS_CONTROLLER_H_
#define ASH_API_TASKS_TASKS_CONTROLLER_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/public/cpp/session/session_observer.h"

namespace ash::api {

class TasksDelegate;

// Provides access to the active account's tasks through the `TasksDelegate`,
// which communicates with the Google Tasks API.
class ASH_EXPORT TasksController : public SessionObserver {
 public:
  explicit TasksController(std::unique_ptr<TasksDelegate> tasks_delegate);
  TasksController(const TasksController&) = delete;
  TasksController& operator=(const TasksController&) = delete;
  ~TasksController() override;

  static TasksController* Get();

  TasksDelegate* tasks_delegate() { return tasks_delegate_.get(); }

  // SessionObserver:
  void OnActiveUserSessionChanged(const AccountId& account_id) override;

 private:
  const std::unique_ptr<TasksDelegate> tasks_delegate_;
};

}  // namespace ash::api

#endif  // ASH_API_TASKS_TASKS_CONTROLLER_H_
