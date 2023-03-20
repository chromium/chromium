// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_STRUCTURED_STRUCTURED_METRICS_KEY_EVENTS_OBSERVER_H_
#define CHROME_BROWSER_METRICS_STRUCTURED_STRUCTURED_METRICS_KEY_EVENTS_OBSERVER_H_

#include "chromeos/ash/components/login/session/session_termination_manager.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "components/user_manager/user_manager.h"

namespace metrics::structured {

// An observer to detect when key events occur, whether by the user or system.
// Theses events are:
// - Login
// - Logout
// - System Suspend
class StructuredMetricsKeyEventsObserver
    : public user_manager::UserManager::UserSessionStateObserver,
      public ash::SessionTerminationManager::Observer,
      public chromeos::PowerManagerClient::Observer {
 public:
  StructuredMetricsKeyEventsObserver(
      user_manager::UserManager* user_manager,
      ash::SessionTerminationManager* session_termination_manager,
      chromeos::PowerManagerClient* power_manager_client);

  ~StructuredMetricsKeyEventsObserver() override;

  // user_manager::UserSessionStateObserver:
  void ActiveUserChanged(user_manager::User* user) override;

  // ash::SessionTerminationManager::Observer:
  void OnSessionWillBeTerminated() override;

  // chromeos::PowerManagerClient::Observer:
  void SuspendImminent(power_manager::SuspendImminent::Reason reason) override;

 private:
  user_manager::UserManager* user_manager_;
  ash::SessionTerminationManager* session_termination_manager_;
  chromeos::PowerManagerClient* power_manager_client_;
};

}  // namespace metrics::structured

#endif  // CHROME_BROWSER_METRICS_STRUCTURED_STRUCTURED_METRICS_KEY_EVENTS_OBSERVER_H_
