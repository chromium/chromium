// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_DIAGNOSTICS_UI_BACKEND_SYSTEM_SYSTEM_ROUTINE_CONTROLLER_H_
#define ASH_WEBUI_DIAGNOSTICS_UI_BACKEND_SYSTEM_SYSTEM_ROUTINE_CONTROLLER_H_

#include <memory>
#include <optional>

#include "ash/webui/diagnostics_ui/mojom/system_routine_controller.mojom.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd.mojom.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_diagnostics.mojom-forward.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "services/device/public/mojom/wake_lock.mojom.h"
#include "services/device/public/mojom/wake_lock_provider.mojom.h"

namespace base {
class OneShotTimer;
}  // namespace base

namespace ash::diagnostics {

using RunRoutineCallback =
    base::OnceCallback<void(cros_healthd::mojom::RunRoutineResponsePtr)>;

constexpr int32_t kInvalidRoutineId = 0;

class SystemRoutineController : public mojom::SystemRoutineController {
 public:
  SystemRoutineController();
  ~SystemRoutineController() override;

  SystemRoutineController(const SystemRoutineController&) = delete;
  SystemRoutineController& operator=(const SystemRoutineController&) = delete;

  // mojom::SystemRoutineController:
  void GetSupportedRoutines(GetSupportedRoutinesCallback callback) override;
  void RunRoutine(mojom::RoutineType type,
                  mojo::PendingRemote<mojom::RoutineRunner> runner) override;

  void BindInterface(
      mojo::PendingReceiver<mojom::SystemRoutineController> pending_receiver);
  // Handler for when remote attached to |receiver_| disconnects.
  void OnBoundInterfaceDisconnect();
  bool IsReceiverBoundForTesting();

  void SetWakeLockProviderForTesting(
      mojo::Remote<device::mojom::WakeLockProvider> provider) {
    wake_lock_provider_ = std::move(provider);
  }

 private:
  friend class SystemRoutineControllerTest;

  void OnAvailableRoutinesFetched(
      GetSupportedRoutinesCallback callback,
      const std::vector<cros_healthd::mojom::DiagnosticRoutineEnum>&
          supported_routines);

  void ExecuteRoutine(mojom::RoutineType routine_type);

  void OnRoutineStarted(
      mojom::RoutineType routine_type,
      cros_healthd::mojom::RunRoutineResponsePtr response_ptr);

  void OnPowerRoutineStarted(
      mojom::RoutineType routine_type,
      cros_healthd::mojom::RunRoutineResponsePtr response_ptr);

  void ContinuePowerRoutine(mojom::RoutineType routine_type);

  void OnPowerRoutineContinued(
      mojom::RoutineType routine_type,
      cros_healthd::mojom::RoutineUpdatePtr update_ptr);

  void CheckRoutineStatus(mojom::RoutineType routine_type);

  void OnRoutineStatusUpdated(mojom::RoutineType routine_type,
                              cros_healthd::mojom::RoutineUpdatePtr update_ptr);

  void HandlePowerRoutineStatusUpdate(
      mojom ::RoutineType routine_type,
      cros_healthd::mojom::RoutineUpdatePtr update_ptr);

  bool IsRoutineRunning() const;

  void ScheduleCheckRoutineStatus(uint32_t duration_in_seconds,
                                  mojom::RoutineType routine_type);

  void ParsePowerRoutineResult(mojom::RoutineType routine_type,
                               mojom::StandardRoutineResult result,
                               mojo::ScopedHandle output_handle);

  void OnPowerRoutineResultFetched(mojom::RoutineType routine_type,
                                   const std::string& file_contents);

  void OnPowerRoutineJsonParsed(mojom::RoutineType routine_type,
                                data_decoder::DataDecoder::ValueOrError result);

  void OnStandardRoutineResult(mojom::RoutineType routine_type,
                               mojom::StandardRoutineResult result);

  void OnPowerRoutineResult(mojom::RoutineType routine_type,
                            mojom::StandardRoutineResult result,
                            double percent_change,
                            uint32_t seconds_elapsed);

  void SendRoutineResult(mojom::RoutineResultInfoPtr result_info);

  void BindCrosHealthdDiagnosticsServiceIfNeccessary();

  void OnDiagnosticsServiceDisconnected();

  void OnInflightRoutineRunnerDisconnected();

  void OnRoutineCancelAttempted(
      cros_healthd::mojom::RoutineUpdatePtr update_ptr);

  void AcquireWakeLock();

  void ReleaseWakeLock();

  // Keeps track of the id created by CrosHealthd for the currently running
  // routine.
  int32_t inflight_routine_id_ = kInvalidRoutineId;

  // The currently inflight routine (if any). This is used to correctly
  // attribute cancellations.
  std::optional<mojom::RoutineType> inflight_routine_type_;

  // Records the number of routines that a user attempts to run during one
  // session in the app. Emitted when the app is closed.
  uint16_t routine_count_ = 0;

  // Timestamp of when the memory routine was started. Undefined if the memory
  // routine is not running.
  base::Time memory_routine_start_timestamp_;

  mojo::Remote<mojom::RoutineRunner> inflight_routine_runner_;
  std::unique_ptr<base::OneShotTimer> inflight_routine_timer_;

  mojo::Remote<cros_healthd::mojom::CrosHealthdDiagnosticsService>
      diagnostics_service_;

  mojo::Receiver<mojom::SystemRoutineController> receiver_{this};

  // `wake_lock_` is used to prevent the device from sleeping during the
  // memory test.
  mojo::Remote<device::mojom::WakeLock> wake_lock_;

  mojo::Remote<device::mojom::WakeLockProvider> wake_lock_provider_;

  // Keeps track of supported routines. Allows us to bypass querying CrosHealthd
  // on subsequent "GetSupportedRoutines" calls since the list never changes.
  std::vector<mojom::RoutineType> supported_routines_;

  base::WeakPtrFactory<SystemRoutineController> weak_factory_{this};
};

}  // namespace ash::diagnostics

#endif  // ASH_WEBUI_DIAGNOSTICS_UI_BACKEND_SYSTEM_SYSTEM_ROUTINE_CONTROLLER_H_
