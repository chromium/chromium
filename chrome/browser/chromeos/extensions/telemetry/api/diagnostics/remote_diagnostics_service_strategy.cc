// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/telemetry/api/diagnostics/remote_diagnostics_service_strategy.h"

#include <memory>

#include "build/chromeos_buildflags.h"
#include "chromeos/crosapi/mojom/diagnostics_service.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/ash/components/telemetry_extension/diagnostics/diagnostics_service_ash.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/lacros/lacros_service.h"
#endif  // BUILDFLAG (IS_CHROMEOS_LACROS)

namespace chromeos {

namespace {

#if BUILDFLAG(IS_CHROMEOS_ASH)
class RemoteDiagnosticsServiceStrategyAsh
    : public RemoteDiagnosticsServiceStrategy {
 public:
  RemoteDiagnosticsServiceStrategyAsh()
      : diagnostics_service_(ash::DiagnosticsServiceAsh::Factory::Create(
            remote_diagnostics_service_.BindNewPipeAndPassReceiver())) {}

  ~RemoteDiagnosticsServiceStrategyAsh() override = default;

  // RemoteDiagnosticsServiceStrategy override:
  mojo::Remote<crosapi::mojom::DiagnosticsService>& GetRemoteService()
      override {
    return remote_diagnostics_service_;
  }

 private:
  mojo::Remote<crosapi::mojom::DiagnosticsService> remote_diagnostics_service_;

  std::unique_ptr<crosapi::mojom::DiagnosticsService> diagnostics_service_;
};
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
class RemoteDiagnosticsServiceStrategyLacros
    : public RemoteDiagnosticsServiceStrategy {
 public:
  RemoteDiagnosticsServiceStrategyLacros() = default;

  ~RemoteDiagnosticsServiceStrategyLacros() override = default;

  // RemoteDiagnosticsServiceStrategy override:
  mojo::Remote<crosapi::mojom::DiagnosticsService>& GetRemoteService()
      override {
    return LacrosService::Get()
        ->GetRemote<crosapi::mojom::DiagnosticsService>();
  }
};
#endif  // BUILDFLAG (IS_CHROMEOS_LACROS)

}  // namespace

// static
std::unique_ptr<RemoteDiagnosticsServiceStrategy>
RemoteDiagnosticsServiceStrategy::Create() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return std::make_unique<RemoteDiagnosticsServiceStrategyAsh>();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  if (!LacrosService::Get()
           ->IsAvailable<crosapi::mojom::DiagnosticsService>()) {
    return nullptr;
  }
  return std::make_unique<RemoteDiagnosticsServiceStrategyLacros>();
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
}

RemoteDiagnosticsServiceStrategy::RemoteDiagnosticsServiceStrategy() = default;
RemoteDiagnosticsServiceStrategy::~RemoteDiagnosticsServiceStrategy() = default;

}  // namespace chromeos
