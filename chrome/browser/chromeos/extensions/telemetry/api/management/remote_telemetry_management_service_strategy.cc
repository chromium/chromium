// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/telemetry/api/management/remote_telemetry_management_service_strategy.h"

#include <memory>

#include "build/chromeos_buildflags.h"
#include "chromeos/crosapi/mojom/telemetry_management_service.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/ash/components/telemetry_extension/management/telemetry_management_service_ash.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/lacros/lacros_service.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

namespace chromeos {

namespace {

namespace crosapi = ::crosapi::mojom;

#if BUILDFLAG(IS_CHROMEOS_ASH)
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
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
class RemoteTelemetryManagementServiceStrategyLacros
    : public RemoteTelemetryManagementServiceStrategy {
 public:
  RemoteTelemetryManagementServiceStrategyLacros() = default;

  ~RemoteTelemetryManagementServiceStrategyLacros() override = default;

  // RemoteTelemetryManagementServiceStrategy:
  mojo::Remote<crosapi::TelemetryManagementService>& GetRemoteService()
      override {
    return LacrosService::Get()
        ->GetRemote<crosapi::TelemetryManagementService>();
  }
};
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

}  // namespace

// static
std::unique_ptr<RemoteTelemetryManagementServiceStrategy>
RemoteTelemetryManagementServiceStrategy::Create() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return std::make_unique<RemoteTelemetryManagementServiceStrategyAsh>();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  if (!LacrosService::Get()
           ->IsAvailable<crosapi::TelemetryManagementService>()) {
    return nullptr;
  }
  return std::make_unique<RemoteTelemetryManagementServiceStrategyLacros>();
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
}

RemoteTelemetryManagementServiceStrategy::
    RemoteTelemetryManagementServiceStrategy() = default;
RemoteTelemetryManagementServiceStrategy::
    ~RemoteTelemetryManagementServiceStrategy() = default;

}  // namespace chromeos
