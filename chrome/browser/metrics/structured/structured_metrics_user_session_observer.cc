// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/structured/structured_metrics_user_session_observer.h"
#include "components/metrics/structured/structured_events.h"

namespace metrics::structured {

StructuredMetricsUserSessionObserver::StructuredMetricsUserSessionObserver(
    user_manager::UserManager* user_manager)
    : user_manager_(user_manager) {
  user_manager_->AddSessionStateObserver(this);
}

StructuredMetricsUserSessionObserver::~StructuredMetricsUserSessionObserver() {
  user_manager_->RemoveSessionStateObserver(this);
}

void StructuredMetricsUserSessionObserver::ActiveUserChanged(
    user_manager::User* active_user) {
  if (active_user->is_active()) {
    events::v2::cr_os_events::UserLogin().Record();
  }
}

}  // namespace metrics::structured
