// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/telemetry/api/diagnostics/fake_diagnostics_service.h"

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/task/sequenced_task_runner.h"
#include "base/values.h"
#include "chromeos/crosapi/mojom/diagnostics_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace {

namespace crosapi = ::crosapi::mojom;

}  // namespace

FakeDiagnosticsService::FakeDiagnosticsService() : receiver_(this) {}

FakeDiagnosticsService::~FakeDiagnosticsService() {
  // Test if the previously set expectations are met.
  EXPECT_EQ(actual_passed_parameters_, expected_passed_parameters_);
  EXPECT_EQ(actual_called_routine_, expected_called_routine_);
}

void FakeDiagnosticsService::BindPendingReceiver(
    mojo::PendingReceiver<crosapi::DiagnosticsService> receiver) {
  receiver_.Bind(std::move(receiver));
}

mojo::PendingRemote<crosapi::DiagnosticsService>
FakeDiagnosticsService::BindNewPipeAndPassRemote() {
  return receiver_.BindNewPipeAndPassRemote();
}

void FakeDiagnosticsService::SetRunRoutineResponse(
    crosapi::DiagnosticsRunRoutineResponsePtr response) {
  run_routine_response_ = std::move(response);
}

void FakeDiagnosticsService::SetRoutineUpdateResponse(
    crosapi::DiagnosticsRoutineUpdatePtr routine_update) {
  routine_update_response_ = std::move(routine_update);
}

void FakeDiagnosticsService::SetExpectedLastPassedParameters(
    base::DictValue expected_passed_parameter) {
  expected_passed_parameters_ = std::move(expected_passed_parameter);
}

void FakeDiagnosticsService::SetExpectedLastCalledRoutine(
    crosapi::DiagnosticsRoutineEnum expected_called_routine) {
  expected_called_routine_ = expected_called_routine;
}

}  // namespace chromeos
