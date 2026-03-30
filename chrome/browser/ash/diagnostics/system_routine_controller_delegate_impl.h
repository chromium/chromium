// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_DIAGNOSTICS_SYSTEM_ROUTINE_CONTROLLER_DELEGATE_IMPL_H_
#define CHROME_BROWSER_ASH_DIAGNOSTICS_SYSTEM_ROUTINE_CONTROLLER_DELEGATE_IMPL_H_

#include "ash/webui/diagnostics_ui/backend/system/system_routine_controller_delegate.h"
#include "base/memory/raw_ref.h"

namespace ash::network_health {
class NetworkHealthManager;
}  // namespace ash::network_health

namespace ash::diagnostics {

// Production implementation of SystemRoutineControllerDelegate. Forwards
// routine calls to `NetworkHealthManager` which owns the
// `NetworkDiagnostics` instance.
class SystemRoutineControllerDelegateImpl
    : public SystemRoutineControllerDelegate {
 public:
  SystemRoutineControllerDelegateImpl();
  SystemRoutineControllerDelegateImpl(
      const SystemRoutineControllerDelegateImpl&) = delete;
  SystemRoutineControllerDelegateImpl& operator=(
      const SystemRoutineControllerDelegateImpl&) = delete;
  ~SystemRoutineControllerDelegateImpl() override;

  // SystemRoutineControllerDelegate:
  void RunGoogleServicesConnectivity(
      RunGoogleServicesConnectivityCallback callback) override;

 private:
  const raw_ref<network_health::NetworkHealthManager> network_health_manager_;
};

}  // namespace ash::diagnostics

#endif  // CHROME_BROWSER_ASH_DIAGNOSTICS_SYSTEM_ROUTINE_CONTROLLER_DELEGATE_IMPL_H_
