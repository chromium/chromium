// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_STRUCTURED_STRUCTURED_METRICS_USER_SESSION_OBSERVER_H_
#define CHROME_BROWSER_METRICS_STRUCTURED_STRUCTURED_METRICS_USER_SESSION_OBSERVER_H_

#include "chromeos/ash/components/login/session/session_termination_manager.h"
#include "components/user_manager/user_manager.h"

namespace metrics::structured {

// An UserSession observer to detect a login for an event to be recorded
// when such a user action takes place.
class StructuredMetricsUserSessionObserver
    : public user_manager::UserManager::UserSessionStateObserver,
      public ash::SessionTerminationManager::Observer {
 public:
  StructuredMetricsUserSessionObserver(
      user_manager::UserManager* user_manager,
      ash::SessionTerminationManager* session_termination_manager);

  ~StructuredMetricsUserSessionObserver() override;

  // user_manager::UserSessionStateObserver:
  void ActiveUserChanged(user_manager::User* user) override;

  // ash::SessionTerminationManager::Observer:
  void OnSessionWillBeTerminated() override;

 private:
  user_manager::UserManager* user_manager_;
  ash::SessionTerminationManager* session_termination_manager_;
};

}  // namespace metrics::structured

#endif  // CHROME_BROWSER_METRICS_STRUCTURED_STRUCTURED_METRICS_USER_SESSION_OBSERVER_H_
