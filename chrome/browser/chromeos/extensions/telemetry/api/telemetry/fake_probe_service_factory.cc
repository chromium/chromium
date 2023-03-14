// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/telemetry/api/telemetry/fake_probe_service_factory.h"

#include <memory>
#include <utility>

#include "chromeos/crosapi/mojom/probe_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace chromeos {

FakeProbeServiceFactory::FakeProbeServiceFactory() = default;
FakeProbeServiceFactory::~FakeProbeServiceFactory() = default;

void FakeProbeServiceFactory::SetCreateInstanceResponse(
    std::unique_ptr<FakeProbeService> fake_service) {
  fake_service_ = std::move(fake_service);
}

std::unique_ptr<crosapi::mojom::TelemetryProbeService>
FakeProbeServiceFactory::CreateInstance(
    mojo::PendingReceiver<crosapi::mojom::TelemetryProbeService> receiver) {
  fake_service_->BindPendingReceiver(std::move(receiver));
  return std::move(fake_service_);
}

}  // namespace chromeos
