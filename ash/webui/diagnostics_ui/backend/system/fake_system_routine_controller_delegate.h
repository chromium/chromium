// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_DIAGNOSTICS_UI_BACKEND_SYSTEM_FAKE_SYSTEM_ROUTINE_CONTROLLER_DELEGATE_H_
#define ASH_WEBUI_DIAGNOSTICS_UI_BACKEND_SYSTEM_FAKE_SYSTEM_ROUTINE_CONTROLLER_DELEGATE_H_

#include "ash/webui/diagnostics_ui/backend/system/system_routine_controller_delegate.h"
#include "chromeos/services/network_health/public/mojom/network_diagnostics.mojom.h"

namespace ash::diagnostics {

// State-based fake of SystemRoutineControllerDelegate for testing.
class FakeSystemRoutineControllerDelegate
    : public SystemRoutineControllerDelegate {
 public:
  FakeSystemRoutineControllerDelegate();
  FakeSystemRoutineControllerDelegate(
      const FakeSystemRoutineControllerDelegate&) = delete;
  FakeSystemRoutineControllerDelegate& operator=(
      const FakeSystemRoutineControllerDelegate&) = delete;
  ~FakeSystemRoutineControllerDelegate() override;

  // Configures the result returned by RunGoogleServicesConnectivity.
  void SetGoogleServicesConnectivityResult(
      chromeos::network_diagnostics::mojom::RoutineResultPtr result);

  // If true, RunGoogleServicesConnectivity holds the callback instead
  // of invoking it immediately. Use `RunHeldCallback()` to fire it.
  void set_hold_callback(bool hold) { hold_callback_ = hold; }

  // Fires the held callback with the preset result.
  // CHECK-fails if no callback is held.
  void RunHeldCallback();

  // Returns true if a callback is currently held.
  bool has_held_callback() const;

  // SystemRoutineControllerDelegate:
  void RunGoogleServicesConnectivity(
      RunGoogleServicesConnectivityCallback callback) override;

 private:
  chromeos::network_diagnostics::mojom::RoutineResultPtr result_;
  bool hold_callback_ = false;
  RunGoogleServicesConnectivityCallback held_callback_;
};

}  // namespace ash::diagnostics

#endif  // ASH_WEBUI_DIAGNOSTICS_UI_BACKEND_SYSTEM_FAKE_SYSTEM_ROUTINE_CONTROLLER_DELEGATE_H_
