// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/telemetry/api/diagnostics/fake_diagnostics_service_factory.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "chromeos/crosapi/mojom/diagnostics_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace chromeos {

FakeDiagnosticsServiceFactory::FakeDiagnosticsServiceFactory() = default;
FakeDiagnosticsServiceFactory::~FakeDiagnosticsServiceFactory() = default;

void FakeDiagnosticsServiceFactory::SetCreateInstanceResponse(
    std::unique_ptr<FakeDiagnosticsService> fake_service) {
  fake_service_ = std::move(fake_service);
}

std::unique_ptr<crosapi::mojom::DiagnosticsService>
FakeDiagnosticsServiceFactory::CreateInstance(
    mojo::PendingReceiver<crosapi::mojom::DiagnosticsService> receiver) {
  DCHECK(fake_service_);
  fake_service_->BindPendingReceiver(std::move(receiver));
  return std::move(fake_service_);
}

}  // namespace chromeos
