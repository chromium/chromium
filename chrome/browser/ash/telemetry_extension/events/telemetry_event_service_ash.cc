// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/telemetry_extension/events/telemetry_event_service_ash.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "chrome/browser/ash/telemetry_extension/events/telemetry_event_forwarder.h"
#include "chrome/browser/ash/telemetry_extension/events/telemetry_event_service_converters.h"
#include "chromeos/ash/services/cros_healthd/public/cpp/service_connection.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd.mojom.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_events.mojom.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_exception.mojom.h"
#include "chromeos/crosapi/mojom/telemetry_event_service.mojom.h"
#include "chromeos/crosapi/mojom/telemetry_extension_exception.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace ash {

// static
TelemetryEventServiceAsh::Factory*
    TelemetryEventServiceAsh::Factory::test_factory_ = nullptr;

// static
std::unique_ptr<crosapi::mojom::TelemetryEventService>
TelemetryEventServiceAsh::Factory::Create(
    mojo::PendingReceiver<crosapi::mojom::TelemetryEventService> receiver) {
  if (test_factory_) {
    return test_factory_->CreateInstance(std::move(receiver));
  }

  auto event_service = std::make_unique<TelemetryEventServiceAsh>();
  event_service->BindReceiver(std::move(receiver));
  return event_service;
}

// static
void TelemetryEventServiceAsh::Factory::SetForTesting(Factory* test_factory) {
  test_factory_ = test_factory;
}

TelemetryEventServiceAsh::Factory::~Factory() = default;

TelemetryEventServiceAsh::TelemetryEventServiceAsh() = default;

TelemetryEventServiceAsh::~TelemetryEventServiceAsh() = default;

void TelemetryEventServiceAsh::BindReceiver(
    mojo::PendingReceiver<crosapi::mojom::TelemetryEventService> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void TelemetryEventServiceAsh::AddEventObserver(
    crosapi::mojom::TelemetryEventCategoryEnum category,
    mojo::PendingRemote<crosapi::mojom::TelemetryEventObserver> observer) {
  auto cb = base::BindOnce(&TelemetryEventServiceAsh::OnConnectionClosed,
                           weak_factory_.GetWeakPtr());
  observers_.push_back(std::make_unique<CrosHealthdEventForwarder>(
      category, std::move(cb), std::move(observer)));
}

void TelemetryEventServiceAsh::IsEventSupported(
    crosapi::mojom::TelemetryEventCategoryEnum category,
    IsEventSupportedCallback callback) {
  cros_healthd::ServiceConnection::GetInstance()
      ->GetEventService()
      ->IsEventSupported(
          converters::Convert(category),
          base::BindOnce(
              [](IsEventSupportedCallback callback,
                 cros_healthd::mojom::SupportStatusPtr ptr) {
                std::move(callback).Run(
                    converters::ConvertStructPtr(std::move(ptr)));
              },
              std::move(callback)));
}

void TelemetryEventServiceAsh::OnConnectionClosed(
    CrosHealthdEventForwarder* closed_connection) {
  observers_.erase(
      std::remove_if(
          observers_.begin(), observers_.end(),
          [closed_connection](
              const std::unique_ptr<CrosHealthdEventForwarder>& ptr) {
            return ptr.get() == closed_connection;
          }),
      observers_.end());
}

}  // namespace ash
