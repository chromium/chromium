// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/telemetry/api/management/remote_telemetry_management_service_strategy.h"

#include <memory>

#include "chromeos/ash/components/telemetry_extension/management/telemetry_management_service_ash.h"
#include "chromeos/crosapi/mojom/telemetry_management_service.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace chromeos {

namespace {

namespace crosapi = ::crosapi::mojom;

class RemoteTelemetryManagementServiceStrategyAsh
    : public RemoteTelemetryManagementServiceStrategy {
 public:
  RemoteTelemetryManagementServiceStrategyAsh()
      : management_service_(ash::TelemetryManagementServiceAsh::Factory::Create(
            remote_management_service_.BindNewPipeAndPassReceiver())) {}

  ~RemoteTelemetryManagementServiceStrategyAsh() override = default;

  // RemoteTelemetryManagementServiceStrategy override:
  mojo::Remote<crosapi::TelemetryManagementService>& GetRemoteService()
      override {
    return remote_management_service_;
  }

 private:
  mojo::Remote<crosapi::TelemetryManagementService> remote_management_service_;

  std::unique_ptr<crosapi::TelemetryManagementService> management_service_;
};

}  // namespace

// static
std::unique_ptr<RemoteTelemetryManagementServiceStrategy>
RemoteTelemetryManagementServiceStrategy::Create() {
  return std::make_unique<RemoteTelemetryManagementServiceStrategyAsh>();
}

RemoteTelemetryManagementServiceStrategy::
    RemoteTelemetryManagementServiceStrategy() = default;
RemoteTelemetryManagementServiceStrategy::
    ~RemoteTelemetryManagementServiceStrategy() = default;

}  // namespace chromeos
