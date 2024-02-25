// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_MOJO_SERVICE_MANAGER_UTILITY_PROCESS_BRIDGE_H_
#define CHROME_BROWSER_ASH_MOJO_SERVICE_MANAGER_UTILITY_PROCESS_BRIDGE_H_

#include "chromeos/ash/components/mojo_service_manager/mojom/mojo_service_manager.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace ash::mojo_service_manager {

// Binds a receiver passed from an utility process. An utility process can't
// connect to the mojo service manager directly(b/308797472). Therefore, a
// bridge in Ash is needed to forward the requests from utility process to the
// mojo service manager.
void EstablishUtilityProcessBridge(
    mojo::PendingReceiver<chromeos::mojo_service_manager::mojom::ServiceManager>
        pending_receiver);

}  // namespace ash::mojo_service_manager

#endif  // CHROME_BROWSER_ASH_MOJO_SERVICE_MANAGER_UTILITY_PROCESS_BRIDGE_H_
