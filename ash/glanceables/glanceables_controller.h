// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_GLANCEABLES_GLANCEABLES_CONTROLLER_H_
#define ASH_GLANCEABLES_GLANCEABLES_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/session/session_observer.h"
#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "components/account_id/account_id.h"

class PrefRegistrySimple;

namespace ash {

namespace api {
class TasksClient;
}  // namespace api

class GlanceablesClassroomClient;

// Root glanceables controller.
class ASH_EXPORT GlanceablesController : public SessionObserver {
 public:
  // Convenience wrapper to pass all clients from browser to ash at once.
  struct ClientsRegistration {
    raw_ptr<GlanceablesClassroomClient, DanglingUntriaged> classroom_client =
        nullptr;
    raw_ptr<api::TasksClient, DanglingUntriaged> tasks_client = nullptr;
  };

  GlanceablesController();
  GlanceablesController(const GlanceablesController&) = delete;
  GlanceablesController& operator=(const GlanceablesController&) = delete;
  ~GlanceablesController() override;

  // Registers syncable user profile prefs with the specified `registry`.
  static void RegisterUserProfilePrefs(PrefRegistrySimple* registry);

  // Clears glanceables user state set in `prefs` - for example, the most
  // recently selected glanceable list.
  static void ClearUserStatePrefs(PrefService* prefs);

  // SessionObserver:
  void OnActiveUserSessionChanged(const AccountId& account_id) override;

  // Whether glanceanbles are available to the `active_account_id_`.
  // Glanceables are available if the feature is enabled for the user and it has
  // at least one registered client.
  bool AreGlanceablesAvailable() const;

  // Updates `clients_registry_` for a specific `account_id`.
  void UpdateClientsRegistration(const AccountId& account_id,
                                 const ClientsRegistration& registration);

  // Returns a classroom client pointer associated with the
  // `active_account_id_`. Could return `nullptr`.
  GlanceablesClassroomClient* GetClassroomClient() const;

  // Returns a tasks client pointer associated with the `active_account_id_`.
  // Could return `nullptr`.
  api::TasksClient* GetTasksClient() const;

  // Informs registered glanceables clients that the glanceables bubble UI has
  // been closed and logs metrics.
  void NotifyGlanceablesBubbleClosed();

  // Informs registered glanceables clients that the glanceables bubble UI has
  // been closed and logs metrics.
  void RecordGlanceablesBubbleShowTime(base::TimeTicks bubble_show_timestamp);

  base::TimeTicks last_bubble_show_time() { return last_bubble_show_time_; }
  size_t bubble_shown_count() { return bubble_shown_count_; }

 private:
  // The currently active user account id.
  AccountId active_account_id_;

  // Keeps track of all created clients (owned by `GlanceablesKeyedService`) per
  // account id.
  base::flat_map<AccountId, ClientsRegistration> clients_registry_;

  // Keeps track of the time that the user logged in.
  base::Time login_time_;

  // Keeps track of the last time the glanceables bubble was shown.
  base::TimeTicks last_bubble_show_time_;

  // The number of times the glanceables bubble had been shown within a user
  // session.
  size_t bubble_shown_count_ = 0;
};

}  // namespace ash

#endif  // ASH_GLANCEABLES_GLANCEABLES_CONTROLLER_H_
