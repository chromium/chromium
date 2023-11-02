// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/ash_interfaces.h"

#include <utility>

#include "ash/display/cros_display_config.h"
#include "ash/shell.h"
#include "ash/tray_action/tray_action.h"

namespace ash {

void BindCrosDisplayConfigController(
    mojo::PendingReceiver<crosapi::mojom::CrosDisplayConfigController>
        receiver) {
  if (Shell::HasInstance())
    Shell::Get()->cros_display_config()->BindReceiver(std::move(receiver));
}

void BindTrayAction(mojo::PendingReceiver<mojom::TrayAction> receiver) {
  if (Shell::HasInstance())
    Shell::Get()->tray_action()->BindReceiver(std::move(receiver));
}

}  // namespace ash
