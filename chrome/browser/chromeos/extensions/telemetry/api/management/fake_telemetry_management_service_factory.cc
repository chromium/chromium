// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/telemetry/api/management/fake_telemetry_management_service_factory.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "chromeos/crosapi/mojom/telemetry_management_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace chromeos {

namespace {

namespace crosapi = ::crosapi::mojom;

}  // namespace

FakeTelemetryManagementServiceFactory::FakeTelemetryManagementServiceFactory() =
    default;
FakeTelemetryManagementServiceFactory::
    ~FakeTelemetryManagementServiceFactory() = default;

void FakeTelemetryManagementServiceFactory::SetCreateInstanceResponse(
    std::unique_ptr<FakeTelemetryManagementService> fake_service) {
  fake_service_ = std::move(fake_service);
}

std::unique_ptr<crosapi::TelemetryManagementService>
FakeTelemetryManagementServiceFactory::CreateInstance(
    mojo::PendingReceiver<crosapi::TelemetryManagementService> receiver) {
  DCHECK(fake_service_);
  fake_service_->BindPendingReceiver(std::move(receiver));
  return std::move(fake_service_);
}

}  // namespace chromeos
