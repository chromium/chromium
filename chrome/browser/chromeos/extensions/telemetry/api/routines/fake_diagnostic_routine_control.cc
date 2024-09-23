// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/telemetry/api/routines/fake_diagnostic_routine_control.h"

#include <algorithm>
#include <utility>

#include "chromeos/crosapi/mojom/telemetry_diagnostic_routine_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace chromeos {

namespace {
namespace crosapi = ::crosapi::mojom;
}  // namespace

FakeDiagnosticRoutineControl::FakeDiagnosticRoutineControl(
    mojo::PendingReceiver<crosapi::TelemetryDiagnosticRoutineControl>
        pending_receiver,
    mojo::PendingRemote<crosapi::TelemetryDiagnosticRoutineObserver> observer)
    : receiver_(this, std::move(pending_receiver)) {
  auto init_state = crosapi::TelemetryDiagnosticRoutineState::New();
  init_state->percentage = 0;
  init_state->state_union =
      crosapi::TelemetryDiagnosticRoutineStateUnion::NewInitialized(
          crosapi::TelemetryDiagnosticRoutineStateInitialized::New());

  if (observer.is_valid()) {
    routine_observer_.Bind(std::move(observer));
  }

  SetState(std::move(init_state));
}

FakeDiagnosticRoutineControl::~FakeDiagnosticRoutineControl() = default;

void FakeDiagnosticRoutineControl::GetState(GetStateCallback callback) {
  std::move(callback).Run(get_state_response_->Clone());
}

void FakeDiagnosticRoutineControl::Start() {
  auto running_state = crosapi::TelemetryDiagnosticRoutineState::New();
  running_state->percentage = 10;
  running_state->state_union =
      crosapi::TelemetryDiagnosticRoutineStateUnion::NewRunning(
          crosapi::TelemetryDiagnosticRoutineStateRunning::New());

  SetState(std::move(running_state));
}

void FakeDiagnosticRoutineControl::ReplyToInquiry(
    crosapi::TelemetryDiagnosticRoutineInquiryReplyPtr reply) {
  if (on_reply_to_inquiry_called_) {
    on_reply_to_inquiry_called_.Run(std::move(reply));
  }
}

void FakeDiagnosticRoutineControl::SetState(
    crosapi::TelemetryDiagnosticRoutineStatePtr state) {
  get_state_response_ = std::move(state);

  NotifyObserverAboutCurrentState();
}

void FakeDiagnosticRoutineControl::SetOnReplyToInquiryCalled(
    OnReplyToInquiryCalled callback) {
  on_reply_to_inquiry_called_ = callback;
}

void FakeDiagnosticRoutineControl::NotifyObserverAboutCurrentState() {
  if (routine_observer_.is_bound()) {
    routine_observer_->OnRoutineStateChange(get_state_response_->Clone());
  }
}

}  // namespace chromeos
