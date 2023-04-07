// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_EVENTS_FAKE_EVENTS_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_EVENTS_FAKE_EVENTS_SERVICE_H_

#include "base/functional/callback_forward.h"
#include "chromeos/crosapi/mojom/telemetry_event_service.mojom.h"
#include "chromeos/crosapi/mojom/telemetry_extension_exception.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace chromeos {

class FakeEventsService : public crosapi::mojom::TelemetryEventService {
 public:
  FakeEventsService();
  FakeEventsService(const FakeEventsService&) = delete;
  FakeEventsService& operator=(const FakeEventsService&) = delete;
  ~FakeEventsService() override;

  void BindPendingReceiver(
      mojo::PendingReceiver<crosapi::mojom::TelemetryEventService> receiver);

  mojo::PendingRemote<crosapi::mojom::TelemetryEventService>
  BindNewPipeAndPassRemote();

  // TelemetryEventService:
  void AddEventObserver(
      crosapi::mojom::TelemetryEventCategoryEnum category,
      mojo::PendingRemote<crosapi::mojom::TelemetryEventObserver> observer)
      override;
  void IsEventSupported(crosapi::mojom::TelemetryEventCategoryEnum category,
                        IsEventSupportedCallback callback) override;

  // Sets the response for a call to `IsEventSupported`.
  void SetIsEventSupportedResponse(
      crosapi::mojom::TelemetryExtensionSupportStatusPtr status);

  // Emits an `info` for all observers subscribed to `category`.
  void EmitEventForCategory(crosapi::mojom::TelemetryEventCategoryEnum category,
                            crosapi::mojom::TelemetryEventInfoPtr info);

  // Returns the remotes that are observing a certain category, returns nullptr
  // if no remote is observing a certain category.
  mojo::RemoteSet<crosapi::mojom::TelemetryEventObserver>*
  GetObserversByCategory(crosapi::mojom::TelemetryEventCategoryEnum category);

  // Sets a callback that is invoked in two cases:
  // - A new subscription is added (`AddEventObserver` was called).
  // - A subscription is removed (the mojo connection was cut).
  // This is useful since in tests we usually want to perform a certain action
  // as soon as an observation is registered or removed.
  void SetOnSubscriptionChange(base::RepeatingClosure callback);

 private:
  mojo::Receiver<crosapi::mojom::TelemetryEventService> receiver_{this};

  // Collection of registered general observers grouped by category.
  std::map<crosapi::mojom::TelemetryEventCategoryEnum,
           mojo::RemoteSet<crosapi::mojom::TelemetryEventObserver>>
      event_observers_;

  // Called when a new subscription is opened or an existing one is closed.
  base::RepeatingClosure on_subscription_change_;

  // Used as the response to any IsEventSupported IPCs received.
  crosapi::mojom::TelemetryExtensionSupportStatusPtr
      is_event_supported_response_{
          crosapi::mojom::TelemetryExtensionSupportStatus::
              NewUnmappedUnionField(0)};
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_EVENTS_FAKE_EVENTS_SERVICE_H_
