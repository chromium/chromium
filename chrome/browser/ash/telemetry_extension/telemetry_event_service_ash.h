// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_TELEMETRY_EXTENSION_TELEMETRY_EVENT_SERVICE_ASH_H_
#define CHROME_BROWSER_ASH_TELEMETRY_EXTENSION_TELEMETRY_EVENT_SERVICE_ASH_H_

#include "chromeos/crosapi/mojom/telemetry_event_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace ash {

class TelemetryEventServiceAsh : public crosapi::mojom::TelemetryEventService {
 public:
  TelemetryEventServiceAsh();
  TelemetryEventServiceAsh(const TelemetryEventServiceAsh&) = delete;
  TelemetryEventServiceAsh& operator=(const TelemetryEventServiceAsh&) = delete;
  ~TelemetryEventServiceAsh() override;

  void BindReceiver(
      mojo::PendingReceiver<crosapi::mojom::TelemetryEventService> receiver);

  // crosapi::TelemetryEventService implementation.
  void AddEventObserver(
      crosapi::mojom::TelemetryEventCategoryEnum category,
      mojo::PendingRemote<crosapi::mojom::TelemetryEventObserver> observer)
      override;
  void IsEventSupported(crosapi::mojom::TelemetryEventCategoryEnum category,
                        IsEventSupportedCallback callback) override;

 private:
  // Support any number of connections.
  mojo::ReceiverSet<crosapi::mojom::TelemetryEventService> receivers_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_TELEMETRY_EXTENSION_TELEMETRY_EVENT_SERVICE_ASH_H_
