// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/telemetry_extension/telemetry_event_service_ash.h"

#include <utility>

#include "base/notreached.h"
#include "chromeos/crosapi/mojom/telemetry_event_service.mojom.h"
#include "chromeos/crosapi/mojom/telemetry_extension_exception.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace ash {

TelemetryEventServiceAsh::TelemetryEventServiceAsh() = default;

TelemetryEventServiceAsh::~TelemetryEventServiceAsh() = default;

void TelemetryEventServiceAsh::BindReceiver(
    mojo::PendingReceiver<crosapi::mojom::TelemetryEventService> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void TelemetryEventServiceAsh::AddEventObserver(
    crosapi::mojom::TelemetryEventCategoryEnum category,
    mojo::PendingRemote<crosapi::mojom::TelemetryEventObserver> observer) {
  NOTIMPLEMENTED();
}

void TelemetryEventServiceAsh::IsEventSupported(
    crosapi::mojom::TelemetryEventCategoryEnum category,
    IsEventSupportedCallback callback) {
  NOTIMPLEMENTED();
}

}  // namespace ash
