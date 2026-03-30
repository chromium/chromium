// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_DIAGNOSTICS_UI_BACKEND_SYSTEM_SYSTEM_ROUTINE_CONTROLLER_DELEGATE_H_
#define ASH_WEBUI_DIAGNOSTICS_UI_BACKEND_SYSTEM_SYSTEM_ROUTINE_CONTROLLER_DELEGATE_H_

#include "base/functional/callback.h"
#include "chromeos/services/network_health/public/mojom/network_diagnostics.mojom.h"

namespace ash::diagnostics {

// Pure virtual interface for running network diagnostic routines directly
// (bypassing Mojo IPC). Production implementation lives in chrome/ and calls
// `NetworkHealthManager`; test fakes live alongside the unit tests.
class SystemRoutineControllerDelegate {
 public:
  using RunGoogleServicesConnectivityCallback = base::OnceCallback<void(
      chromeos::network_diagnostics::mojom::RoutineResultPtr)>;

  SystemRoutineControllerDelegate(const SystemRoutineControllerDelegate&) =
      delete;
  SystemRoutineControllerDelegate& operator=(
      const SystemRoutineControllerDelegate&) = delete;

  virtual ~SystemRoutineControllerDelegate();

  // Runs the GoogleServicesConnectivity routine and invokes `callback` with
  // the result. The callback may be invoked synchronously or asynchronously.
  virtual void RunGoogleServicesConnectivity(
      RunGoogleServicesConnectivityCallback callback) = 0;

 protected:
  SystemRoutineControllerDelegate();
};

}  // namespace ash::diagnostics

#endif  // ASH_WEBUI_DIAGNOSTICS_UI_BACKEND_SYSTEM_SYSTEM_ROUTINE_CONTROLLER_DELEGATE_H_
