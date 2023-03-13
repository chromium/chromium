// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/glanceables/glanceables_v2_controller.h"

#include "ash/glanceables/tasks/glanceables_tasks_client.h"
#include "ash/public/cpp/session/session_controller.h"
#include "base/check.h"
#include "components/account_id/account_id.h"

namespace ash {

GlanceablesV2Controller::GlanceablesV2Controller() {
  DCHECK(SessionController::Get());
  SessionController::Get()->AddObserver(this);
}

GlanceablesV2Controller::~GlanceablesV2Controller() {
  DCHECK(SessionController::Get());
  SessionController::Get()->RemoveObserver(this);
}

void GlanceablesV2Controller::OnActiveUserSessionChanged(
    const AccountId& account_id) {
  active_account_id_ = account_id;
}

void GlanceablesV2Controller::UpdateClientsRegistration(
    const AccountId& account_id,
    const ClientsRegistration& registration) {
  clients_registry_.insert_or_assign(account_id, registration);
}

GlanceablesTasksClient* GlanceablesV2Controller::GetTasksClient() const {
  const auto iter = clients_registry_.find(active_account_id_);
  return iter != clients_registry_.end() ? iter->second.tasks_client : nullptr;
}

}  // namespace ash
