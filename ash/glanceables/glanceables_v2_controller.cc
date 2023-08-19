// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/glanceables/glanceables_v2_controller.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/glanceables/classroom/glanceables_classroom_client.h"
#include "ash/glanceables/tasks/glanceables_tasks_client.h"
#include "ash/public/cpp/session/session_controller.h"
#include "base/check.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_registry_simple.h"

namespace ash {

GlanceablesV2Controller::GlanceablesV2Controller() {
  DCHECK(SessionController::Get());
  SessionController::Get()->AddObserver(this);
}

GlanceablesV2Controller::~GlanceablesV2Controller() {
  DCHECK(SessionController::Get());
  SessionController::Get()->RemoveObserver(this);
}

// static
void GlanceablesV2Controller::RegisterUserProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kGlanceablesEnabled, true);
}

void GlanceablesV2Controller::OnActiveUserSessionChanged(
    const AccountId& account_id) {
  active_account_id_ = account_id;
}

bool GlanceablesV2Controller::AreGlanceablesAvailable() const {
  return features::AreGlanceablesV2Enabled() &&
         (GetClassroomClient() != nullptr || GetTasksClient() != nullptr);
}

void GlanceablesV2Controller::UpdateClientsRegistration(
    const AccountId& account_id,
    const ClientsRegistration& registration) {
  clients_registry_.insert_or_assign(account_id, registration);
}

GlanceablesClassroomClient* GlanceablesV2Controller::GetClassroomClient()
    const {
  const auto iter = clients_registry_.find(active_account_id_);
  return iter != clients_registry_.end() ? iter->second.classroom_client.get()
                                         : nullptr;
}

GlanceablesTasksClient* GlanceablesV2Controller::GetTasksClient() const {
  const auto iter = clients_registry_.find(active_account_id_);
  return iter != clients_registry_.end() ? iter->second.tasks_client.get()
                                         : nullptr;
}

void GlanceablesV2Controller::NotifyGlanceablesBubbleClosed() {
  for (auto& clients : clients_registry_) {
    if (clients.second.classroom_client) {
      clients.second.classroom_client->OnGlanceablesBubbleClosed();
    }
    if (clients.second.tasks_client) {
      clients.second.tasks_client->OnGlanceablesBubbleClosed();
    }
  }
}

}  // namespace ash
