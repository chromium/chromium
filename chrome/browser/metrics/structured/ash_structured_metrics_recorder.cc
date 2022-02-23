// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/structured/ash_structured_metrics_recorder.h"

#include "base/threading/sequenced_task_runner_handle.h"
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chromeos/crosapi/mojom/structured_metrics_service.mojom.h"
#include "components/metrics/structured/event.h"
#include "components/metrics/structured/event_base.h"
#include "components/metrics/structured/histogram_util.h"
#include "components/metrics/structured/recorder.h"

namespace metrics {
namespace structured {

AshStructuredMetricsRecorder::AshStructuredMetricsRecorder() = default;
AshStructuredMetricsRecorder::~AshStructuredMetricsRecorder() = default;

void AshStructuredMetricsRecorder::Initialize() {
  DCHECK(!is_initialized_);

  // If already initialized, do nothing.
  if (is_initialized_)
    return;

  // Crosapi may not be initialized, in which case a pipe cannot be setup.
  if (crosapi::CrosapiManager::IsInitialized()) {
    crosapi::CrosapiManager::Get()->crosapi_ash()->BindStructuredMetricsService(
        remote_.BindNewPipeAndPassReceiver());
    is_initialized_ = true;
  } else {
    VLOG(2) << "Initialize() called before CrosApi is initialized.";
  }
}

void AshStructuredMetricsRecorder::RecordEvent(Event&& event) {
  // It is OK not to check whether the remote is bound or not yet.
  std::vector<Event> events;
  events.push_back(std::move(event));
  remote_->Record(events);
}

// TODO(crbug.com/1249222): Delete this once migration is complete.
//
// EventBase should not be used with the mojo API and this function call
// will be removed in the future.
void AshStructuredMetricsRecorder::Record(EventBase&& event_base) {
  VLOG(2) << "AshStructuredMetricsRecorder should use event.";
}

bool AshStructuredMetricsRecorder::IsReadyToRecord() const {
  // Remote doesn't have to be bound to since the remote can queue up messages.
  // Should be ready to record the moment it is initialized.
  return true;
}

}  // namespace structured
}  // namespace metrics
