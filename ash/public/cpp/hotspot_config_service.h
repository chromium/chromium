// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_HOTSPOT_CONFIG_SERVICE_H_
#define ASH_PUBLIC_CPP_HOTSPOT_CONFIG_SERVICE_H_

#include "ash/public/cpp/ash_public_export.h"
#include "chromeos/ash/services/hotspot_config/public/mojom/cros_hotspot_config.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace ash {

// Binds |pending_receiver| to CrosHotspotConfig. Clients should use this
// function as the main entrypoint to the Mojo API.
//
// Internally, this function delegates to an implementation in the browser
// process. We declare this function in //ash to ensure that clients do not have
// any direct dependencies on the implementation.
ASH_PUBLIC_EXPORT void GetHotspotConfigService(
    mojo::PendingReceiver<hotspot_config::mojom::CrosHotspotConfig>
        pending_receiver);

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_HOTSPOT_CONFIG_SERVICE_H_