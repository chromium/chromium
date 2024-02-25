// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/telemetry/api/routines/fake_diagnostic_routines_service_factory.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/routines/fake_diagnostic_routines_service.h"
#include "chromeos/crosapi/mojom/telemetry_diagnostic_routine_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace chromeos {

namespace {
namespace crosapi = ::crosapi::mojom;
}  // namespace

FakeDiagnosticRoutinesServiceFactory::FakeDiagnosticRoutinesServiceFactory() =
    default;
FakeDiagnosticRoutinesServiceFactory::~FakeDiagnosticRoutinesServiceFactory() =
    default;

void FakeDiagnosticRoutinesServiceFactory::SetCreateInstanceResponse(
    std::unique_ptr<FakeDiagnosticRoutinesService> fake_service) {
  fake_service_ = std::move(fake_service);
}

std::unique_ptr<crosapi::TelemetryDiagnosticRoutinesService>
FakeDiagnosticRoutinesServiceFactory::CreateInstance(
    mojo::PendingReceiver<crosapi::TelemetryDiagnosticRoutinesService>
        receiver) {
  CHECK(fake_service_);
  fake_service_->receiver().Bind(std::move(receiver));
  return std::move(fake_service_);
}

}  // namespace chromeos
