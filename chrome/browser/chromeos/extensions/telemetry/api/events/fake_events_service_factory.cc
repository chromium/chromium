// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/telemetry/api/events/fake_events_service_factory.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/events/fake_events_service.h"
#include "chromeos/crosapi/mojom/telemetry_event_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace chromeos {

FakeEventsServiceFactory::FakeEventsServiceFactory() = default;
FakeEventsServiceFactory::~FakeEventsServiceFactory() = default;

void FakeEventsServiceFactory::SetCreateInstanceResponse(
    std::unique_ptr<FakeEventsService> fake_service) {
  fake_service_ = std::move(fake_service);
}

std::unique_ptr<crosapi::mojom::TelemetryEventService>
FakeEventsServiceFactory::CreateInstance(
    mojo::PendingReceiver<crosapi::mojom::TelemetryEventService> receiver) {
  DCHECK(fake_service_);
  fake_service_->BindPendingReceiver(std::move(receiver));
  return std::move(fake_service_);
}

}  // namespace chromeos
