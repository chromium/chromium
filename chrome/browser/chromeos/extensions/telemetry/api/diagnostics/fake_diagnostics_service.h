// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_DIAGNOSTICS_FAKE_DIAGNOSTICS_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_DIAGNOSTICS_FAKE_DIAGNOSTICS_SERVICE_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/values.h"
#include "chromeos/crosapi/mojom/diagnostics_service.mojom.h"
#include "chromeos/crosapi/mojom/nullable_primitives.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace chromeos {

class FakeDiagnosticsService : public crosapi::mojom::DiagnosticsService {
 public:
  FakeDiagnosticsService();
  FakeDiagnosticsService(const FakeDiagnosticsService&) = delete;
  FakeDiagnosticsService& operator=(const FakeDiagnosticsService&) = delete;
  ~FakeDiagnosticsService() override;

  void BindPendingReceiver(
      mojo::PendingReceiver<crosapi::mojom::DiagnosticsService> receiver);

  mojo::PendingRemote<crosapi::mojom::DiagnosticsService>
  BindNewPipeAndPassRemote();

  // Sets the return value for |Run*Routine|.
  void SetRunRoutineResponse(
      crosapi::mojom::DiagnosticsRunRoutineResponsePtr expected_response);

  // Sets the return value for |GetRoutineUpdate|.
  void SetRoutineUpdateResponse(
      crosapi::mojom::DiagnosticsRoutineUpdatePtr routine_update);

  // Set expectation about the parameter that is passed to a call of
  // |Run*Routine| or |GetAvailableRoutines|.
  void SetExpectedLastPassedParameters(
      base::DictValue expected_passed_parameter);

  // Set expectation about the type of routine that is called.
  void SetExpectedLastCalledRoutine(
      crosapi::mojom::DiagnosticsRoutineEnum expected_called_routine);

 private:
  mojo::Receiver<crosapi::mojom::DiagnosticsService> receiver_;

  // Response for a call to |Run*Routine|.
  crosapi::mojom::DiagnosticsRunRoutineResponsePtr run_routine_response_;

  // Response for a call to |GetAvailableRoutines|.
  std::vector<crosapi::mojom::DiagnosticsRoutineEnum>
      available_routines_response_;

  // Response for a call to |GetRoutineUpdate|.
  crosapi::mojom::DiagnosticsRoutineUpdatePtr routine_update_response_;

  // Expectation of the passed parameters to a |Run*Routine| call.
  base::DictValue expected_passed_parameters_;
  // Actually passed parameter.
  base::DictValue actual_passed_parameters_;

  // Expectation of the called routine.
  crosapi::mojom::DiagnosticsRoutineEnum expected_called_routine_{
      crosapi::mojom::DiagnosticsRoutineEnum::kUnknown};
  // Actually called routine.
  crosapi::mojom::DiagnosticsRoutineEnum actual_called_routine_{
      crosapi::mojom::DiagnosticsRoutineEnum::kUnknown};
};
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_DIAGNOSTICS_FAKE_DIAGNOSTICS_SERVICE_H_
