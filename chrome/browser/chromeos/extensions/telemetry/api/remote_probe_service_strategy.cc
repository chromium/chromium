// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/telemetry/api/remote_probe_service_strategy.h"

#include <memory>

#include "build/chromeos_buildflags.h"
#include "chromeos/crosapi/mojom/probe_service.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/telemetry_extension/probe_service_ash.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/lacros/lacros_service.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

namespace chromeos {

namespace {

#if BUILDFLAG(IS_CHROMEOS_ASH)
class RemoteProbeServiceStrategyAsh : public RemoteProbeServiceStrategy {
 public:
  RemoteProbeServiceStrategyAsh()
      : probe_service_(ash::ProbeServiceAsh::Factory::Create(
            remote_probe_service_.BindNewPipeAndPassReceiver())) {}

  ~RemoteProbeServiceStrategyAsh() override = default;

  // RemoteProbeServiceStrategy override:
  mojo::Remote<crosapi::mojom::TelemetryProbeService>& GetRemoteService()
      override {
    return remote_probe_service_;
  }

 private:
  mojo::Remote<crosapi::mojom::TelemetryProbeService> remote_probe_service_;

  std::unique_ptr<crosapi::mojom::TelemetryProbeService> probe_service_;
};
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
class RemoteProbeServiceStrategyLacros : public RemoteProbeServiceStrategy {
 public:
  RemoteProbeServiceStrategyLacros() = default;

  ~RemoteProbeServiceStrategyLacros() override = default;

  // RemoteProbeServiceStrategy override:
  mojo::Remote<crosapi::mojom::TelemetryProbeService>& GetRemoteService()
      override {
    return LacrosService::Get()
        ->GetRemote<crosapi::mojom::TelemetryProbeService>();
  }
};
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

}  // namespace

// static
std::unique_ptr<RemoteProbeServiceStrategy>
RemoteProbeServiceStrategy::Create() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return std::make_unique<RemoteProbeServiceStrategyAsh>();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  if (!LacrosService::Get()
           ->IsAvailable<crosapi::mojom::TelemetryProbeService>()) {
    return nullptr;
  }
  return std::make_unique<RemoteProbeServiceStrategyLacros>();
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
}

RemoteProbeServiceStrategy::RemoteProbeServiceStrategy() = default;
RemoteProbeServiceStrategy::~RemoteProbeServiceStrategy() = default;

}  // namespace chromeos
