// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_EVENTS_FAKE_EVENTS_SERVICE_FACTORY_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_EVENTS_FAKE_EVENTS_SERVICE_FACTORY_H_

#include <memory>

#include "chrome/browser/chromeos/extensions/telemetry/api/events/fake_events_service.h"
#include "chromeos/ash/components/telemetry_extension/events/telemetry_event_service_ash.h"
#include "chromeos/crosapi/mojom/telemetry_event_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace chromeos {

class FakeEventsServiceFactory : public ash::TelemetryEventServiceAsh::Factory {
 public:
  FakeEventsServiceFactory();
  ~FakeEventsServiceFactory() override;

  void SetCreateInstanceResponse(
      std::unique_ptr<FakeEventsService> fake_service);

 protected:
  // TelemetryEventServiceAsh::Factory:
  std::unique_ptr<crosapi::mojom::TelemetryEventService> CreateInstance(
      mojo::PendingReceiver<crosapi::mojom::TelemetryEventService> receiver)
      override;

 private:
  std::unique_ptr<FakeEventsService> fake_service_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_EVENTS_FAKE_EVENTS_SERVICE_FACTORY_H_
