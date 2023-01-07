// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_ASH_INTERFACES_H_
#define ASH_PUBLIC_ASH_INTERFACES_H_

#include "ash/ash_export.h"
#include "ash/public/mojom/tray_action.mojom.h"
#include "chromeos/crosapi/mojom/cros_display_config.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace ash {

// Helper methods for binding interfaces exposed by Ash. Must only be called on
// the main thread.
ASH_EXPORT void BindCrosDisplayConfigController(
    mojo::PendingReceiver<crosapi::mojom::CrosDisplayConfigController>
        receiver);
ASH_EXPORT
void BindTrayAction(mojo::PendingReceiver<mojom::TrayAction> receiver);

}  // namespace ash

#endif  // ASH_PUBLIC_ASH_INTERFACES_H_
