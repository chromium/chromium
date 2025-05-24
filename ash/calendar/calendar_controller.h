// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CALENDAR_CALENDAR_CONTROLLER_H_
#define ASH_CALENDAR_CALENDAR_CONTROLLER_H_

#include <map>
#include <memory>

#include "ash/ash_export.h"
#include "ash/public/cpp/session/session_observer.h"
#include "base/memory/raw_ptr.h"
#include "components/account_id/account_id.h"

class PrefRegistrySimple;

namespace ash {

class CalendarClient;

// Keeps track of all calendar clients per user account and makes
// sure the current active client belongs to the current active user.
// There is expected to exist at most one instance of this class at a time. In
// production the instance is owned by ash::Shell.
class ASH_EXPORT CalendarController : public SessionObserver {
 public:
  CalendarController();
  CalendarController(const CalendarController& other) = delete;
  CalendarController& operator=(const CalendarController& other) = delete;
  ~CalendarController() override;

  // Registers profile prefs for Calendar client.
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // Adds a client to it's corresponding user account id in a map.
  void RegisterClientForUser(const AccountId& account_id,
                             CalendarClient* client);

  // Gets the currently active calendar client. Returns `nullptr` if the user
  // profile is not registered with the ProfileManager, e.g. before logging in,
  // guest user, etc.
  CalendarClient* GetClient();

  // For testing only, directly assign `active_user_account_id_`.
  void SetActiveUserAccountIdForTesting(const AccountId& account_id);

 private:
  // SessionObserver:
  void OnActiveUserSessionChanged(const AccountId& account_id) override;

  // The currently active user account id.
  AccountId active_user_account_id_;

  std::map<const AccountId, raw_ptr<CalendarClient, CtnExperimental>>
      clients_by_account_id_;
};

}  // namespace ash

#endif  // ASH_CALENDAR_CALENDAR_CONTROLLER_H_
