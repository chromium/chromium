// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/telemetry/api/remote_probe_service_strategy.h"

#include <memory>

#include "chrome/browser/ash/telemetry_extension/probe_service.h"
#include "chromeos/crosapi/mojom/probe_service.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace chromeos {

namespace {

class RemoteProbeServiceStrategyAsh : public RemoteProbeServiceStrategy {
 public:
  RemoteProbeServiceStrategyAsh()
      : probe_service_(ash::ProbeService::Factory::Create(
            remote_probe_service_.BindNewPipeAndPassReceiver())) {}

  ~RemoteProbeServiceStrategyAsh() override = default;

  // RemoteProbeServiceStrategy override:
  mojo::Remote<ash::health::mojom::ProbeService>& GetRemoteService() override {
    return remote_probe_service_;
  }

 private:
  mojo::Remote<ash::health::mojom::ProbeService> remote_probe_service_;

  std::unique_ptr<ash::health::mojom::ProbeService> probe_service_;
};

}  // namespace

// static
std::unique_ptr<RemoteProbeServiceStrategy>
RemoteProbeServiceStrategy::Create() {
  return std::make_unique<RemoteProbeServiceStrategyAsh>();
}

RemoteProbeServiceStrategy::RemoteProbeServiceStrategy() = default;
RemoteProbeServiceStrategy::~RemoteProbeServiceStrategy() = default;

}  // namespace chromeos
