// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_ROUTINES_DIAGNOSTIC_ROUTINE_OBSERVATION_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_ROUTINES_DIAGNOSTIC_ROUTINE_OBSERVATION_H_

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/uuid.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/routines/diagnostic_routine_info.h"
#include "chromeos/crosapi/mojom/telemetry_diagnostic_routine_service.mojom.h"
#include "content/public/browser/browser_context.h"
#include "extensions/common/extension_id.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace chromeos {

class DiagnosticRoutineObservation
    : public crosapi::mojom::TelemetryDiagnosticRoutineObserver {
 public:
  using OnRoutineFinished = base::OnceCallback<void(DiagnosticRoutineInfo)>;

  explicit DiagnosticRoutineObservation(
      DiagnosticRoutineInfo info,
      OnRoutineFinished on_routine_finished,
      mojo::PendingReceiver<crosapi::mojom::TelemetryDiagnosticRoutineObserver>
          pending_receiver);

  DiagnosticRoutineObservation(const DiagnosticRoutineObservation&) = delete;
  DiagnosticRoutineObservation& operator=(const DiagnosticRoutineObservation&) =
      delete;

  ~DiagnosticRoutineObservation() override;

  // `TelemetryDiagnosticRoutineObserver`:
  void OnRoutineStateChange(
      crosapi::mojom::TelemetryDiagnosticRoutineStatePtr state) override;

 private:
  // `ExtensionId` associated with this observation.
  DiagnosticRoutineInfo info_;
  OnRoutineFinished on_routine_finished_;
  mojo::Receiver<crosapi::mojom::TelemetryDiagnosticRoutineObserver> receiver_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_ROUTINES_DIAGNOSTIC_ROUTINE_OBSERVATION_H_
