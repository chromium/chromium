// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/esim_manager.h"

#include "chromeos/ash/services/cellular_setup/in_process_esim_manager.h"

namespace ash {

void GetESimManager(
    mojo::PendingReceiver<cellular_setup::mojom::ESimManager> receiver) {
  cellular_setup::BindToInProcessESimManager(std::move(receiver));
}

}  // namespace ash
