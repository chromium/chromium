// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/connectivity_services.h"

#include "chromeos/ash/services/connectivity/public/cpp/connectivity_services.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace ash {

void GetPasspointService(
    mojo::PendingReceiver<chromeos::connectivity::mojom::PasspointService>
        receiver) {
  ash::connectivity::BindToPasspointService(std::move(receiver));
}

}  // namespace ash
