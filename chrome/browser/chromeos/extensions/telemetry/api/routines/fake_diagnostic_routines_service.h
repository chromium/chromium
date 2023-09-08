// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_ROUTINES_FAKE_DIAGNOSTIC_ROUTINES_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_ROUTINES_FAKE_DIAGNOSTIC_ROUTINES_SERVICE_H_

#include "base/functional/callback_forward.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/routines/fake_diagnostic_routine_control.h"
#include "chromeos/crosapi/mojom/telemetry_diagnostic_routine_service.mojom.h"
#include "chromeos/crosapi/mojom/telemetry_extension_exception.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace chromeos {

class FakeDiagnosticRoutinesService
    : public crosapi::mojom::TelemetryDiagnosticRoutinesService {
 public:
  FakeDiagnosticRoutinesService();
  FakeDiagnosticRoutinesService(const FakeDiagnosticRoutinesService&) = delete;
  FakeDiagnosticRoutinesService& operator=(
      const FakeDiagnosticRoutinesService&) = delete;
  ~FakeDiagnosticRoutinesService() override;

  // Gets the `FakeDiagnosticRoutineControl` for a certain type of routine. The
  // returned object allows for setting expectations in tests and accessing
  // certain properties that might change during tests. If there is no
  // `FakeRoutineControl` registered for a certain type of routine, this
  // returns `nullptr`.
  FakeDiagnosticRoutineControl* GetCreatedRoutineControlForRoutineType(
      crosapi::mojom::TelemetryDiagnosticRoutineArgument::Tag tag);

  void FlushForTesting();

  // Sets the response value for a call to `IsRoutineArgumentSupported`.
  void SetIsRoutineArgumentSupportedResponse(
      crosapi::mojom::TelemetryExtensionSupportStatusPtr result);

  // `TelemetryDiagnosticRoutinesService`:
  void CreateRoutine(
      crosapi::mojom::TelemetryDiagnosticRoutineArgumentPtr routine_argument,
      mojo::PendingReceiver<crosapi::mojom::TelemetryDiagnosticRoutineControl>
          control_remote,
      mojo::PendingRemote<crosapi::mojom::TelemetryDiagnosticRoutineObserver>
          observer_receiver) override;
  void IsRoutineArgumentSupported(
      crosapi::mojom::TelemetryDiagnosticRoutineArgumentPtr routine_argument,
      IsRoutineArgumentSupportedCallback callback) override;

  // Sets a callback that is invoked when `CreateRoutine` was called.
  // This is useful since in tests we usually want to perform a certain action
  // as soon as an observation is registered or removed.
  void SetOnCreateRoutineCalled(base::RepeatingClosure callback);

  mojo::Receiver<crosapi::mojom::TelemetryDiagnosticRoutinesService>&
  receiver() {
    return receiver_;
  }

 private:
  mojo::Receiver<crosapi::mojom::TelemetryDiagnosticRoutinesService> receiver_{
      this};

  crosapi::mojom::TelemetryExtensionSupportStatusPtr
      is_routine_argument_supported_response_{
          crosapi::mojom::TelemetryExtensionSupportStatus::
              NewUnmappedUnionField(0)};

  base::RepeatingClosure on_routine_created_;
  std::map<crosapi::mojom::TelemetryDiagnosticRoutineArgument::Tag,
           FakeDiagnosticRoutineControl>
      routine_controls_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_ROUTINES_FAKE_DIAGNOSTIC_ROUTINES_SERVICE_H_
