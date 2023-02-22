// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/metrics_reporting/audio/audio_events_observer.h"

#include <utility>

#include "chromeos/ash/services/cros_healthd/public/cpp/service_connection.h"
#include "components/reporting/proto/synced/metric_data.pb.h"

namespace reporting {

AudioEventsObserver::AudioEventsObserver()
    : MojoServiceEventsObserverBase<ash::cros_healthd::mojom::EventObserver>(
          this) {}

AudioEventsObserver::~AudioEventsObserver() = default;

void AudioEventsObserver::OnEvent(
    const ash::cros_healthd::mojom::EventInfoPtr info) {
  if (!info->is_audio_event_info()) {
    return;
  }
  const auto& audio_event_info = info->get_audio_event_info();
  if (audio_event_info->state ==
      ash::cros_healthd::mojom::AudioEventInfo::State::kSevereUnderrun) {
    MetricData metric_data;
    metric_data.mutable_event_data()->set_type(
        reporting::MetricEventType::AUDIO_SEVERE_UNDERRUN);
    OnEventObserved(std::move(metric_data));
  }
}

void AudioEventsObserver::AddObserver() {
  ash::cros_healthd::ServiceConnection::GetInstance()
      ->GetEventService()
      ->AddEventObserver(ash::cros_healthd::mojom::EventCategoryEnum::kAudio,
                         BindNewPipeAndPassRemote());
}

}  // namespace reporting
