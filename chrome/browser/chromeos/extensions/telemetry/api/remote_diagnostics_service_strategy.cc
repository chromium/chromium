// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/telemetry/api/remote_diagnostics_service_strategy.h"

#include <memory>

#include "chrome/browser/ash/telemetry_extension/diagnostics_service_ash.h"
#include "chromeos/crosapi/mojom/diagnostics_service.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace chromeos {

namespace {

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

}  // namespace

// static
std::unique_ptr<RemoteDiagnosticsServiceStrategy>
RemoteDiagnosticsServiceStrategy::Create() {
  return std::make_unique<RemoteDiagnosticsServiceStrategyAsh>();
}

RemoteDiagnosticsServiceStrategy::RemoteDiagnosticsServiceStrategy() = default;
RemoteDiagnosticsServiceStrategy::~RemoteDiagnosticsServiceStrategy() = default;

}  // namespace chromeos
