// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/telemetry/api/events/fake_events_service.h"

#include <tuple>
#include <utility>

#include "base/functional/callback.h"
#include "base/location.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "chromeos/crosapi/mojom/telemetry_event_service.mojom.h"
#include "chromeos/crosapi/mojom/telemetry_extension_exception.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace chromeos {

namespace {

namespace crosapi = ::crosapi::mojom;

}  // namespace

FakeEventsService::FakeEventsService() = default;

FakeEventsService::~FakeEventsService() = default;

void FakeEventsService::BindPendingReceiver(
    mojo::PendingReceiver<crosapi::TelemetryEventService> receiver) {
  receiver_.Bind(std::move(receiver));
}

mojo::PendingRemote<crosapi::TelemetryEventService>
FakeEventsService::BindNewPipeAndPassRemote() {
  return receiver_.BindNewPipeAndPassRemote();
}

void FakeEventsService::AddEventObserver(
    crosapi::TelemetryEventCategoryEnum category,
    mojo::PendingRemote<crosapi::TelemetryEventObserver> observer) {
  auto it = event_observers_.find(category);
  if (it == event_observers_.end()) {
    it = event_observers_.emplace_hint(it, std::piecewise_construct,
                                       std::forward_as_tuple(category),
                                       std::forward_as_tuple());
  }

  it->second.Add(std::move(observer));

  // Register the call to on_subscription_change in case the connection is
  // reset. SAFETY: We can pass `this` to the callback, since this can only be
  // invoked on a connected ReceiverSet, which lives shorter than `this`.
  it->second.set_disconnect_handler(
      base::BindLambdaForTesting([this](mojo::RemoteSetElementId _) {
        if (on_subscription_change_) {
          base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
              FROM_HERE, std::move(on_subscription_change_));
        }
      }));

  // Invoke the callback for on_subscription_change, since we added a
  // subscription.
  if (on_subscription_change_) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(on_subscription_change_));
  }
}

void FakeEventsService::IsEventSupported(
    crosapi::TelemetryEventCategoryEnum category,
    IsEventSupportedCallback callback) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback),
                                is_event_supported_response_.Clone()));
}

void FakeEventsService::SetIsEventSupportedResponse(
    crosapi::TelemetryExtensionSupportStatusPtr status) {
  is_event_supported_response_.Swap(&status);
}

void FakeEventsService::EmitEventForCategory(
    crosapi::TelemetryEventCategoryEnum category,
    crosapi::TelemetryEventInfoPtr info) {
  // Flush the receiver, so any pending observers are registered before the
  // event is emitted.
  if (receiver_.is_bound()) {
    receiver_.FlushForTesting();
  }

  auto it = event_observers_.find(category);
  if (it == event_observers_.end()) {
    return;
  }

  for (auto& observer : it->second) {
    observer->OnEvent(info.Clone());
  }
}

mojo::RemoteSet<crosapi::TelemetryEventObserver>*
FakeEventsService::GetObserversByCategory(
    crosapi::TelemetryEventCategoryEnum category) {
  auto it = event_observers_.find(category);
  if (it == event_observers_.end()) {
    return nullptr;
  }

  return &it->second;
}

void FakeEventsService::SetOnSubscriptionChange(
    base::RepeatingClosure callback) {
  on_subscription_change_ = std::move(callback);
}

}  // namespace chromeos
