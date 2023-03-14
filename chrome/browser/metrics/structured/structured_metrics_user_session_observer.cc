// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/structured/structured_metrics_user_session_observer.h"
#include "base/logging.h"
#include "components/metrics/structured/structured_events.h"
#include "structured_metrics_user_session_observer.h"

namespace metrics::structured {

StructuredMetricsUserSessionObserver::StructuredMetricsUserSessionObserver(
    user_manager::UserManager* user_manager,
    ash::SessionTerminationManager* session_termination_manager)
    : user_manager_(user_manager),
      session_termination_manager_(session_termination_manager) {
  user_manager_->AddSessionStateObserver(this);
  session_termination_manager_->AddObserver(this);
}

StructuredMetricsUserSessionObserver::~StructuredMetricsUserSessionObserver() {
  user_manager_->RemoveSessionStateObserver(this);
  session_termination_manager_->RemoveObserver(this);
}

void StructuredMetricsUserSessionObserver::ActiveUserChanged(
    user_manager::User* user) {
  if (user->is_active()) {
    events::v2::cr_os_events::UserLogin().Record();
  }
}

void StructuredMetricsUserSessionObserver::OnSessionWillBeTerminated() {
  events::v2::cr_os_events::UserLogout().Record();
}

}  // namespace metrics::structured
