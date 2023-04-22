// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_GLANCEABLES_GLANCEABLES_V2_CONTROLLER_H_
#define ASH_GLANCEABLES_GLANCEABLES_V2_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/session/session_observer.h"
#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "components/account_id/account_id.h"

namespace ash {

class GlanceablesTasksClient;

// Root glanceables controller.
// TODO(b/270948434): Remove "V2" from the name once `GlanceablesController`
// is removed.
class ASH_EXPORT GlanceablesV2Controller : public SessionObserver {
 public:
  // Convenience wrapper to pass all clients from browser to ash at once.
  struct ClientsRegistration {
    raw_ptr<GlanceablesTasksClient, ExperimentalAsh> tasks_client = nullptr;
  };

  GlanceablesV2Controller();
  GlanceablesV2Controller(const GlanceablesV2Controller&) = delete;
  GlanceablesV2Controller& operator=(const GlanceablesV2Controller&) = delete;
  ~GlanceablesV2Controller() override;

  // SessionObserver:
  void OnActiveUserSessionChanged(const AccountId& account_id) override;

  // Updates `clients_registry_` for a specific `account_id`.
  void UpdateClientsRegistration(const AccountId& account_id,
                                 const ClientsRegistration& registration);

  // Returns a tasks client pointer associated with the `active_account_id_`.
  // Could return `nullptr`.
  GlanceablesTasksClient* GetTasksClient() const;

 private:
  // The currently active user account id.
  AccountId active_account_id_;

  // Keeps track of all created clients (owned by `GlanceablesKeyedService`) per
  // account id.
  base::flat_map<AccountId, ClientsRegistration> clients_registry_;
};

}  // namespace ash

#endif  // ASH_GLANCEABLES_GLANCEABLES_V2_CONTROLLER_H_
