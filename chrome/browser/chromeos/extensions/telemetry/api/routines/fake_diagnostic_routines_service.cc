// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/telemetry/api/routines/fake_diagnostic_routines_service.h"

#include <utility>

#include "base/notreached.h"
#include "chromeos/crosapi/mojom/telemetry_diagnostic_routine_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace chromeos {

namespace {
namespace crosapi = ::crosapi::mojom;
}  // namespace

FakeDiagnosticRoutinesService::FakeDiagnosticRoutinesService() = default;

FakeDiagnosticRoutinesService::~FakeDiagnosticRoutinesService() = default;

FakeDiagnosticRoutineControl*
FakeDiagnosticRoutinesService::GetCreatedRoutineControlForRoutineType(
    crosapi::TelemetryDiagnosticRoutineArgument::Tag tag) {
  auto it = routine_controls_.find(tag);
  if (it == routine_controls_.end()) {
    return nullptr;
  }

  return &(it->second);
}

void FakeDiagnosticRoutinesService::CreateRoutine(
    crosapi::TelemetryDiagnosticRoutineArgumentPtr routine_argument,
    mojo::PendingReceiver<crosapi::TelemetryDiagnosticRoutineControl>
        control_remote,
    mojo::PendingRemote<crosapi::TelemetryDiagnosticRoutineObserver>
        observer_receiver) {
  routine_controls_.emplace(
      std::piecewise_construct,
      std::forward_as_tuple(routine_argument->which()),
      std::forward_as_tuple(std::move(control_remote),
                            std::move(observer_receiver)));
}

void FakeDiagnosticRoutinesService::IsRoutineArgumentSupported(
    crosapi::TelemetryDiagnosticRoutineArgumentPtr routine_argument,
    IsRoutineArgumentSupportedCallback callback) {
  NOTIMPLEMENTED();
}

}  // namespace chromeos
