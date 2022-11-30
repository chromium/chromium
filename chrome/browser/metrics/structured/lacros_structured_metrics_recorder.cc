// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/structured/lacros_structured_metrics_recorder.h"

#include <vector>

#include "base/task/sequenced_task_runner.h"
#include "chromeos/crosapi/mojom/structured_metrics_service.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "components/metrics/structured/event.h"
#include "components/metrics/structured/structured_metrics_client.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace metrics {
namespace structured {

LacrosStructuredMetricsRecorder::LacrosStructuredMetricsRecorder() = default;
LacrosStructuredMetricsRecorder::~LacrosStructuredMetricsRecorder() = default;

void LacrosStructuredMetricsRecorder::SetSequence(
    const scoped_refptr<base::SequencedTaskRunner> sequence_task_runner) {
  sequence_task_runner_ = sequence_task_runner;
}

void LacrosStructuredMetricsRecorder::RecordEvent(Event&& event) {
  DCHECK(IsReadyToRecord());

  // No-op if not properly initialized.
  if (!IsReadyToRecord())
    return;

  // Ensure that the task is always run on the designated
  // |sequence_task_runner_|. Lacros::GetRemote requires that the remote be
  // called within the same sequence.
  if (!sequence_task_runner_->RunsTasksInCurrentSequence()) {
    sequence_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&LacrosStructuredMetricsRecorder::RecordEvent,
                       weak_ptr_factory_.GetWeakPtr(), std::move(event)));
    return;
  }

  auto* service = chromeos::LacrosService::Get();

  // If the service is not available yet, keep it in memory until the next
  // record call and service is available.
  if (!service ||
      !service->IsAvailable<crosapi::mojom::StructuredMetricsService>()) {
    // Notify observers.
    for (auto& observer : observers_)
      observer.OnRecord(event);
    enqueued_events_.emplace_back(std::move(event));
    return;
  }

  const auto& remote =
      service->GetRemote<crosapi::mojom::StructuredMetricsService>();

  // Lacros service has no observer to callback when a service is bound and
  // ready. Thus, events will be enqueued best effort.
  if (!enqueued_events_.empty()) {
    for (auto& observer : observers_)
      observer.OnFlush();

    remote->Record(enqueued_events_);
    enqueued_events_.clear();
  }

  // Notify observers.
  for (auto& observer : observers_)
    observer.OnRecord(event);

  std::vector<Event> events;
  events.push_back(std::move(event));
  remote->Record(events);
}

bool LacrosStructuredMetricsRecorder::IsReadyToRecord() const {
  return static_cast<bool>(sequence_task_runner_);
}

void LacrosStructuredMetricsRecorder::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void LacrosStructuredMetricsRecorder::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

}  // namespace structured
}  // namespace metrics
