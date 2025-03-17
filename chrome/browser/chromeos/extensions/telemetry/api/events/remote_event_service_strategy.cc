// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/telemetry/api/events/remote_event_service_strategy.h"

#include <memory>

#include "base/notreached.h"
#include "chromeos/ash/components/telemetry_extension/events/telemetry_event_service_ash.h"
#include "chromeos/crosapi/mojom/telemetry_event_service.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace chromeos {

namespace {

namespace crosapi = ::crosapi::mojom;

class RemoteEventServiceStrategyAsh : public RemoteEventServiceStrategy {
 public:
  RemoteEventServiceStrategyAsh()
      : event_service_(ash::TelemetryEventServiceAsh::Factory::Create(
            remote_event_service_.BindNewPipeAndPassReceiver())) {}

  ~RemoteEventServiceStrategyAsh() override = default;

  // RemoteEventServiceStrategy:
  mojo::Remote<crosapi::TelemetryEventService>& GetRemoteService() override {
    return remote_event_service_;
  }

 private:
  mojo::Remote<crosapi::TelemetryEventService> remote_event_service_;

  std::unique_ptr<crosapi::TelemetryEventService> event_service_;
};

}  // namespace

// static
std::unique_ptr<RemoteEventServiceStrategy>
RemoteEventServiceStrategy::Create() {
  return std::make_unique<RemoteEventServiceStrategyAsh>();
}

RemoteEventServiceStrategy::RemoteEventServiceStrategy() = default;
RemoteEventServiceStrategy::~RemoteEventServiceStrategy() = default;

}  // namespace chromeos
