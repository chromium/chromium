// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/metrics_reporting/external_display/display_events_observer.h"

#include "chromeos/ash/services/cros_healthd/public/cpp/service_connection.h"
#include "components/reporting/proto/synced/metric_data.pb.h"

namespace reporting {

DisplayEventsObserver::DisplayEventsObserver()
    : MojoServiceEventsObserverBase<ash::cros_healthd::mojom::EventObserver>(
          this) {}

DisplayEventsObserver::~DisplayEventsObserver() = default;

void DisplayEventsObserver::OnEvent(
    const ash::cros_healthd::mojom::EventInfoPtr info) {
  if (!info->is_external_display_event_info()) {
    return;
  }

  const auto& display_event_info = info->get_external_display_event_info();
  if (!display_event_info->display_info) {
    return;
  }

  MetricData metric_data;
  switch (display_event_info->state) {
    case ash::cros_healthd::mojom::ExternalDisplayEventInfo::State::kAdd:
      metric_data.mutable_event_data()->set_type(
          MetricEventType::EXTERNAL_DISPLAY_CONNECTED);
      break;
    case ash::cros_healthd::mojom::ExternalDisplayEventInfo::State::kRemove:
      metric_data.mutable_event_data()->set_type(
          MetricEventType::EXTERNAL_DISPLAY_DISCONNECTED);
      break;
    case ash::cros_healthd::mojom::ExternalDisplayEventInfo::State::
        kUnmappedEnumField:
      return;
  }

  FillDisplayStatus(*(metric_data.mutable_telemetry_data()
                          ->mutable_displays_telemetry()
                          ->add_display_status()),
                    display_event_info->display_info);
  OnEventObserved(std::move(metric_data));
}

void DisplayEventsObserver::AddObserver() {
  ash::cros_healthd::ServiceConnection::GetInstance()
      ->GetEventService()
      ->AddEventObserver(
          ash::cros_healthd::mojom::EventCategoryEnum::kExternalDisplay,
          BindNewPipeAndPassRemote());
}

// static
void DisplayEventsObserver::FillDisplayStatus(
    DisplayStatus& display_status,
    const ExternalDisplayInfoPtr& display_info) {
  // Set is_internal to false since this is for external displays
  display_status.set_is_internal(false);

  if (display_info->display_name.has_value()) {
    display_status.set_display_name(display_info->display_name.value());
  }
  if (display_info->resolution_horizontal) {
    display_status.set_resolution_horizontal(
        display_info->resolution_horizontal->value);
  }
  if (display_info->resolution_vertical) {
    display_status.set_resolution_vertical(
        display_info->resolution_vertical->value);
  }
  if (display_info->refresh_rate) {
    display_status.set_refresh_rate(display_info->refresh_rate->value);
  }
  if (display_info->serial_number) {
    display_status.set_serial_number(display_info->serial_number->value);
  }
  if (display_info->edid_version) {
    display_status.set_edid_version(display_info->edid_version.value());
  }
}

}  // namespace reporting
