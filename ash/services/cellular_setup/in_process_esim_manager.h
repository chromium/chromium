// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SERVICES_CELLULAR_SETUP_IN_PROCESS_ESIM_MANAGER_H_
#define ASH_SERVICES_CELLULAR_SETUP_IN_PROCESS_ESIM_MANAGER_H_

#include "ash/services/cellular_setup/public/mojom/esim_manager.mojom.h"
#include "base/component_export.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace ash::cellular_setup {

COMPONENT_EXPORT(IN_PROCESS_ESIM_MANAGER)
void BindToInProcessESimManager(
    mojo::PendingReceiver<mojom::ESimManager> receiver);

}  // namespace ash::cellular_setup

#endif  // ASH_SERVICES_CELLULAR_SETUP_IN_PROCESS_ESIM_MANAGER_H_
