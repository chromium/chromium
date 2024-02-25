// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/api/tasks/tasks_controller.h"

#include <memory>

#include "ash/api/tasks/tasks_delegate.h"
#include "ash/public/cpp/session/session_controller.h"
#include "base/check_op.h"
#include "components/account_id/account_id.h"

namespace ash::api {

namespace {

TasksController* g_instance = nullptr;

}  // namespace

TasksController::TasksController(std::unique_ptr<TasksDelegate> tasks_delegate)
    : tasks_delegate_(std::move(tasks_delegate)) {
  SessionController::Get()->AddObserver(this);

  CHECK_EQ(g_instance, nullptr);
  g_instance = this;
}

TasksController::~TasksController() {
  SessionController::Get()->RemoveObserver(this);

  CHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

// static
TasksController* TasksController::Get() {
  CHECK(g_instance);
  return g_instance;
}

void TasksController::OnActiveUserSessionChanged(const AccountId& account_id) {
  tasks_delegate_->UpdateClientForProfileSwitch(account_id);
}

}  // namespace ash::api
