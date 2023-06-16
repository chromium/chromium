// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/telemetry_extension/events/telemetry_event_forwarder.h"

#include <cstdint>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "chrome/browser/ash/telemetry_extension/events/telemetry_event_service_converters.h"
#include "chromeos/ash/services/cros_healthd/public/cpp/service_connection.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_events.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash {

CrosHealthdEventForwarder::CrosHealthdEventForwarder(
    crosapi::mojom::TelemetryEventCategoryEnum category,
    base::OnceCallback<void(CrosHealthdEventForwarder*)> on_disconnect,
    mojo::PendingRemote<crosapi::mojom::TelemetryEventObserver> crosapi_remote)
    : category_(category),
      deleter_callback_(std::move(on_disconnect)),
      crosapi_observer_(std::move(crosapi_remote)),
      cros_healthd_receiver_(this) {
  cros_healthd::ServiceConnection::GetInstance()
      ->GetEventService()
      ->AddEventObserver(converters::Convert(category),
                         cros_healthd_receiver_.BindNewPipeAndPassRemote());

  cros_healthd_receiver_.set_disconnect_with_reason_handler(
      base::BindOnce(&CrosHealthdEventForwarder::OnCrosHealthdDisconnect,
                     weak_factory.GetWeakPtr()));

  crosapi_observer_.set_disconnect_handler(
      base::BindOnce(&CrosHealthdEventForwarder::OnCrosapiDisconnect,
                     weak_factory.GetWeakPtr()));
}

CrosHealthdEventForwarder::~CrosHealthdEventForwarder() = default;

void CrosHealthdEventForwarder::OnEvent(
    cros_healthd::mojom::EventInfoPtr info) {
  auto event = converters::ConvertStructPtr(std::move(info));
  switch (category_) {
    case crosapi::mojom::TelemetryEventCategoryEnum::kTouchpadButton: {
      if (event->is_touchpad_button_event_info()) {
        crosapi_observer_->OnEvent(std::move(event));
      }
      return;
    }
    case crosapi::mojom::TelemetryEventCategoryEnum::kTouchpadTouch: {
      if (event->is_touchpad_touch_event_info()) {
        crosapi_observer_->OnEvent(std::move(event));
      }
      return;
    }
    case crosapi::mojom::TelemetryEventCategoryEnum::kTouchpadConnected: {
      if (event->is_touchpad_connected_event_info()) {
        crosapi_observer_->OnEvent(std::move(event));
      }
      return;
    }
    case crosapi::mojom::TelemetryEventCategoryEnum::kAudioJack:
    case crosapi::mojom::TelemetryEventCategoryEnum::kLid:
    case crosapi::mojom::TelemetryEventCategoryEnum::kUsb:
    case crosapi::mojom::TelemetryEventCategoryEnum::kSdCard:
    case crosapi::mojom::TelemetryEventCategoryEnum::kPower:
    case crosapi::mojom::TelemetryEventCategoryEnum::kKeyboardDiagnostic:
    case crosapi::mojom::TelemetryEventCategoryEnum::kStylusGarage: {
      crosapi_observer_->OnEvent(std::move(event));
      return;
    }
    case crosapi::mojom::TelemetryEventCategoryEnum::kUnmappedEnumField: {
      LOG(ERROR) << "Unrecognized event category";
      return;
    }
  }
}

void CrosHealthdEventForwarder::OnCrosHealthdDisconnect(
    uint32_t custom_reason,
    const std::string& description) {
  crosapi_observer_.ResetWithReason(custom_reason, description);
  CallDeleter();
}

void CrosHealthdEventForwarder::OnCrosapiDisconnect() {
  cros_healthd_receiver_.reset();
  CallDeleter();
}

void CrosHealthdEventForwarder::CallDeleter() {
  DCHECK(deleter_callback_) << "The connection has been reset twice";
  // After calling `deleter_callback_`, this is destroyed.
  std::move(deleter_callback_).Run(this);
}

}  // namespace ash
