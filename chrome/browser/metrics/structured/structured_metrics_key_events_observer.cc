// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/structured/structured_metrics_key_events_observer.h"

#include <utility>

#include "components/metrics/structured/structured_events.h"
#include "components/metrics/structured/structured_metrics_client.h"

namespace metrics::structured {

namespace cros_events = events::v2::cr_os_events;

StructuredMetricsKeyEventsObserver::StructuredMetricsKeyEventsObserver(
    user_manager::UserManager* user_manager,
    ash::SessionTerminationManager* session_termination_manager,
    chromeos::PowerManagerClient* power_manager_client)
    : user_manager_(user_manager),
      session_termination_manager_(session_termination_manager),
      power_manager_client_(power_manager_client) {
  DCHECK(user_manager_);
  DCHECK(session_termination_manager_);
  DCHECK(power_manager_client_);
  user_manager_->AddSessionStateObserver(this);
  session_termination_manager_->AddObserver(this);
  power_manager_client_->AddObserver(this);
}

StructuredMetricsKeyEventsObserver::~StructuredMetricsKeyEventsObserver() {
  user_manager_->RemoveSessionStateObserver(this);
  session_termination_manager_->RemoveObserver(this);
  power_manager_client_->RemoveObserver(this);
}

void StructuredMetricsKeyEventsObserver::ActiveUserChanged(
    user_manager::User* user) {
  if (user->is_active()) {
    StructuredMetricsClient::Record(cros_events::UserLogin());
  }
}

void StructuredMetricsKeyEventsObserver::OnSessionWillBeTerminated() {
  StructuredMetricsClient::Record(std::move(cros_events::UserLogout()));
}

void StructuredMetricsKeyEventsObserver::SuspendImminent(
    power_manager::SuspendImminent::Reason reason) {
  StructuredMetricsClient::Record(
      std::move(cros_events::SystemSuspended().SetReason(reason)));
}

}  // namespace metrics::structured
