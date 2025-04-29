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

  FillDisplayDevice(*(metric_data.mutable_info_data()
                          ->mutable_display_info()
                          ->add_display_device()),
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
void DisplayEventsObserver::FillDisplayDevice(
    DisplayDevice& display_device,
    const ExternalDisplayInfoPtr& display_info) {
  // Set is_internal to false since this is for external displays
  display_device.set_is_internal(false);

  if (display_info->display_name.has_value()) {
    display_device.set_display_name(display_info->display_name.value());
  }
  if (display_info->display_width) {
    display_device.set_display_width(display_info->display_width->value);
  }
  if (display_info->display_height) {
    display_device.set_display_height(display_info->display_height->value);
  }
  if (display_info->manufacturer.has_value()) {
    display_device.set_manufacturer(display_info->manufacturer.value());
  }
  if (display_info->model_id) {
    display_device.set_model_id(display_info->model_id->value);
  }
  if (display_info->manufacture_year) {
    display_device.set_manufacture_year(display_info->manufacture_year->value);
  }
  if (display_info->serial_number) {
    display_device.set_serial_number(display_info->serial_number->value);
  }
  if (display_info->edid_version.has_value()) {
    display_device.set_edid_version(display_info->edid_version.value());
  }
}

}  // namespace reporting
