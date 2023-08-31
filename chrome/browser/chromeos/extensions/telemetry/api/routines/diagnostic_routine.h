// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_ROUTINES_DIAGNOSTIC_ROUTINE_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_ROUTINES_DIAGNOSTIC_ROUTINE_H_

#include <cstdint>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/routines/diagnostic_routine_observation.h"
#include "chromeos/crosapi/mojom/telemetry_diagnostic_routine_service.mojom.h"
#include "content/public/browser/browser_context.h"
#include "extensions/common/extension_id.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace chromeos {

// A class that represents one Diagnostic routine that was created on the
// system. Holds both the `TelemetryDiagnosticRoutineControl` connection as well
// as the corresponding observation for this routine.
// This class also handles error handling of a routine. When an error occurs, a
// mojo disconnect will occur with the reason encoded from
// `crosapi::mojom::TelemetryExtensionException::Reason` and an optional debug
// message. This will be forwarded to an extension by dispatching the
// `onRoutineException` callback.
class DiagnosticRoutine {
 public:
  struct RoutineInfo {
    extensions::ExtensionId extension_id;
    base::Uuid uuid;
    raw_ptr<content::BrowserContext, ExperimentalAsh> browser_context;
  };

  using DeleterCallback = base::OnceCallback<void(DiagnosticRoutine*)>;

  explicit DiagnosticRoutine(
      mojo::PendingRemote<crosapi::mojom::TelemetryDiagnosticRoutineControl>
          control_remote,
      mojo::PendingReceiver<crosapi::mojom::TelemetryDiagnosticRoutineObserver>
          observer_receiver,
      RoutineInfo info,
      DeleterCallback deleter_callback);

  DiagnosticRoutine(const DiagnosticRoutine&) = delete;
  DiagnosticRoutine& operator=(const DiagnosticRoutine&) = delete;

  ~DiagnosticRoutine();

  mojo::Remote<crosapi::mojom::TelemetryDiagnosticRoutineControl>& GetRemote();

  // Called when the `mojo::Remote` for the RoutineControl interface
  // disconnects. This triggers the `onRoutineException` event with the
  // information from the mojo disconnection.
  void OnRoutineControlDisconnect(uint32_t error_code,
                                  const std::string& message);

  // Signals that `this` can be destructed.
  void CallDeleter();

 private:
  friend class TelemetryExtensionDiagnosticRoutinesManagerTest;

  mojo::Remote<crosapi::mojom::TelemetryDiagnosticRoutineControl>
      routine_control_;
  DiagnosticRoutineObservation observation_;
  RoutineInfo info_;

  DeleterCallback deleter_callback_;
  base::WeakPtrFactory<DiagnosticRoutine> weak_factory{this};
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_ROUTINES_DIAGNOSTIC_ROUTINE_H_
