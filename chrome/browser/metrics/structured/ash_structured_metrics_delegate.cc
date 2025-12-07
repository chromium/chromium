// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/structured/ash_structured_metrics_delegate.h"

#include <memory>

#include "chromeos/ash/components/login/session/session_termination_manager.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "components/metrics/structured/event.h"
#include "components/metrics/structured/histogram_util.h"
#include "components/metrics/structured/recorder.h"
#include "components/metrics/structured/structured_metrics_features.h"

namespace metrics::structured {

AshStructuredMetricsDelegate::AshStructuredMetricsDelegate() = default;
AshStructuredMetricsDelegate::~AshStructuredMetricsDelegate() = default;

void AshStructuredMetricsDelegate::Initialize() {
  DCHECK(!is_initialized_);

  // If already initialized, do nothing.
  if (is_initialized_) {
    return;
  }

  // Crosapi may not be initialized, in which case a pipe cannot be setup.
  key_events_observer_ = std::make_unique<StructuredMetricsKeyEventsObserver>(
      user_manager::UserManager::Get(), ash::SessionTerminationManager::Get(),
      chromeos::PowerManagerClient::Get());
  is_initialized_ = true;
}

void AshStructuredMetricsDelegate::RecordEvent(Event&& event) {
  metrics::structured::Recorder::GetInstance()->RecordEvent(std::move(event));
}

bool AshStructuredMetricsDelegate::IsReadyToRecord() const {
  // Remote doesn't have to be bound to since the remote can queue up
  // messages. Should be ready to record the moment it is initialized.
  return true;
}

}  // namespace metrics::structured
