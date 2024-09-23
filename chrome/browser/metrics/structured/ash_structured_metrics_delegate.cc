// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/structured/ash_structured_metrics_delegate.h"

#include <memory>

#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chromeos/ash/components/login/session/session_termination_manager.h"
#include "chromeos/crosapi/mojom/structured_metrics_service.mojom.h"
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
  if (crosapi::CrosapiManager::IsInitialized()) {
    crosapi::CrosapiManager::Get()->crosapi_ash()->BindStructuredMetricsService(
        remote_.BindNewPipeAndPassReceiver());

    key_events_observer_ = std::make_unique<StructuredMetricsKeyEventsObserver>(
        user_manager::UserManager::Get(), ash::SessionTerminationManager::Get(),
        chromeos::PowerManagerClient::Get());
    is_initialized_ = true;
  } else {
    VLOG(2) << "Initialize() called before CrosApi is initialized.";
  }
}

void AshStructuredMetricsDelegate::RecordEvent(Event&& event) {
  // It is OK not to check whether the remote is bound or not yet.
  std::vector<Event> events;
  events.emplace_back(std::move(event));
  remote_->Record(std::move(events));
}

bool AshStructuredMetricsDelegate::IsReadyToRecord() const {
  // Remote doesn't have to be bound to since the remote can queue up
  // messages. Should be ready to record the moment it is initialized.
  return true;
}

}  // namespace metrics::structured
