// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_TELEMETRY_EXTENSION_EVENTS_TELEMETRY_EVENT_SERVICE_ASH_H_
#define CHROME_BROWSER_ASH_TELEMETRY_EXTENSION_EVENTS_TELEMETRY_EVENT_SERVICE_ASH_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/telemetry_extension/common/self_owned_mojo_proxy.h"
#include "chromeos/crosapi/mojom/telemetry_event_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace ash {

class TelemetryEventServiceAsh : public crosapi::mojom::TelemetryEventService {
 public:
  class Factory {
   public:
    static std::unique_ptr<crosapi::mojom::TelemetryEventService> Create(
        mojo::PendingReceiver<crosapi::mojom::TelemetryEventService> receiver);

    static void SetForTesting(Factory* test_factory);

    virtual ~Factory();

   protected:
    virtual std::unique_ptr<crosapi::mojom::TelemetryEventService>
    CreateInstance(mojo::PendingReceiver<crosapi::mojom::TelemetryEventService>
                       receiver) = 0;

   private:
    static Factory* test_factory_;
  };

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

  // Called by a connection when it is reset from either side (crosapi or
  // cros_healthd). Unregisters the connection.
  void OnConnectionClosed(SelfOwnedMojoProxyInterface* closed_connection);

 private:
  // Currently open connections.
  std::vector<raw_ptr<SelfOwnedMojoProxyInterface>> observers_;

  // Support any number of connections.
  mojo::ReceiverSet<crosapi::mojom::TelemetryEventService> receivers_;

  base::WeakPtrFactory<TelemetryEventServiceAsh> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_TELEMETRY_EXTENSION_EVENTS_TELEMETRY_EVENT_SERVICE_ASH_H_
