// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_CONNECTIVITY_SERVICES_H_
#define ASH_PUBLIC_CPP_CONNECTIVITY_SERVICES_H_

#include "ash/public/cpp/ash_public_export.h"
#include "chromeos/ash/services/connectivity/public/mojom/passpoint.mojom-forward.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace ash {

ASH_PUBLIC_EXPORT void GetPasspointService(
    mojo::PendingReceiver<chromeos::connectivity::mojom::PasspointService>
        receiver);

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_CONNECTIVITY_SERVICES_H_
