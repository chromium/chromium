// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SERVICES_CELLULAR_SETUP_ESIM_MOJO_UTILS_H_
#define ASH_SERVICES_CELLULAR_SETUP_ESIM_MOJO_UTILS_H_

#include "ash/services/cellular_setup/public/mojom/esim_manager.mojom-forward.h"
#include "chromeos/dbus/hermes/hermes_response_status.h"
#include "chromeos/network/cellular_esim_profile.h"
#include "third_party/cros_system_api/dbus/hermes/dbus-constants.h"

namespace ash::cellular_setup {

// Returns the mojo ProfileInstallResult status corresponding to
// HermesResponseStatus from D-Bus clients.
mojom::ProfileInstallResult InstallResultFromStatus(
    HermesResponseStatus status);

// Returns mojo ProfileState corresponding to state CellularESimProfile object.
mojom::ProfileState ProfileStateToMojo(CellularESimProfile::State state);

// Returns mojo ESimOperationResult corresponding to response status
// from D-Bus clients.
mojom::ESimOperationResult OperationResultFromStatus(
    HermesResponseStatus status);

}  // namespace ash::cellular_setup

#endif  // ASH_SERVICES_CELLULAR_SETUP_ESIM_MOJO_UTILS_H_
