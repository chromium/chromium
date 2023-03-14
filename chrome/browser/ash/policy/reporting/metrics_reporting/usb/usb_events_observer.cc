// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/metrics_reporting/usb/usb_events_observer.h"
#include "chromeos/ash/services/cros_healthd/public/cpp/service_connection.h"
#include "components/reporting/proto/synced/metric_data.pb.h"

namespace reporting {

using ::ash::cros_healthd::mojom::UsbEventInfoPtr;

UsbEventsObserver::UsbEventsObserver()
    : MojoServiceEventsObserverBase<ash::cros_healthd::mojom::EventObserver>(
          this) {}

UsbEventsObserver::~UsbEventsObserver() = default;

void UsbEventsObserver::OnEvent(
    const ash::cros_healthd::mojom::EventInfoPtr info) {
  if (!info->is_usb_event_info()) {
    return;
  }
  const auto& usb_event_info = info->get_usb_event_info();
  MetricData metric_data;

  switch (usb_event_info->state) {
    case ash::cros_healthd::mojom::UsbEventInfo::State::kAdd:
      metric_data.mutable_event_data()->set_type(MetricEventType::USB_ADDED);
      break;
    case ash::cros_healthd::mojom::UsbEventInfo::State::kRemove:
      metric_data.mutable_event_data()->set_type(MetricEventType::USB_REMOVED);
      break;
    case ash::cros_healthd::mojom::UsbEventInfo::State::kUnmappedEnumField:
      return;
  }
  FillUsbTelemetry(metric_data.mutable_telemetry_data()
                       ->mutable_peripherals_telemetry()
                       ->add_usb_telemetry(),
                   usb_event_info);
  OnEventObserved(std::move(metric_data));
}

void UsbEventsObserver::AddObserver() {
  ash::cros_healthd::ServiceConnection::GetInstance()
      ->GetEventService()
      ->AddEventObserver(ash::cros_healthd::mojom::EventCategoryEnum::kUsb,
                         BindNewPipeAndPassRemote());
}

void UsbEventsObserver::FillUsbTelemetry(UsbTelemetry* data,
                                         const UsbEventInfoPtr& info) {
  data->set_vendor(info->vendor);
  data->set_name(info->name);
  data->set_pid(info->pid);
  data->set_vid(info->vid);

  for (auto& category : info->categories) {
    data->add_categories(category);
  }
}

}  // namespace reporting
