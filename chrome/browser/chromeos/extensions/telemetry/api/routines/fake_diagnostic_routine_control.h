// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_ROUTINES_FAKE_DIAGNOSTIC_ROUTINE_CONTROL_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_ROUTINES_FAKE_DIAGNOSTIC_ROUTINE_CONTROL_H_

#include "chromeos/crosapi/mojom/telemetry_diagnostic_routine_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace chromeos {

class FakeDiagnosticRoutineControl
    : crosapi::mojom::TelemetryDiagnosticRoutineControl {
 public:
  explicit FakeDiagnosticRoutineControl(
      mojo::PendingReceiver<crosapi::mojom::TelemetryDiagnosticRoutineControl>
          pending_receiver,
      mojo::PendingRemote<crosapi::mojom::TelemetryDiagnosticRoutineObserver>
          observer = mojo::PendingRemote<
              crosapi::mojom::TelemetryDiagnosticRoutineObserver>());
  ~FakeDiagnosticRoutineControl() override;

  FakeDiagnosticRoutineControl(const FakeDiagnosticRoutineControl&) = delete;
  FakeDiagnosticRoutineControl& operator=(const FakeDiagnosticRoutineControl&) =
      delete;

  // Sets the current state of this routine. Note: This will also inform the
  // observer.
  void SetState(crosapi::mojom::TelemetryDiagnosticRoutineStatePtr state);

  mojo::Receiver<crosapi::mojom::TelemetryDiagnosticRoutineControl>&
  receiver() {
    return receiver_;
  }

 private:
  // `TelemetryDiagnosticRoutineControl`:
  void GetState(GetStateCallback callback) override;
  void Start() override;

  // Notifies an observer if the observer is bound.
  void NotifyObserverAboutCurrentState();

  // Returned on a call to `GetState`.
  crosapi::mojom::TelemetryDiagnosticRoutineStatePtr get_state_response_{
      crosapi::mojom::TelemetryDiagnosticRoutineState::New()};

  mojo::Remote<crosapi::mojom::TelemetryDiagnosticRoutineObserver>
      routine_observer_;
  mojo::Receiver<crosapi::mojom::TelemetryDiagnosticRoutineControl> receiver_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_ROUTINES_FAKE_DIAGNOSTIC_ROUTINE_CONTROL_H_
