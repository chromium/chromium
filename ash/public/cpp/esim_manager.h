// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_ESIM_MANAGER_H_
#define ASH_PUBLIC_CPP_ESIM_MANAGER_H_

#include "ash/public/cpp/ash_public_export.h"
#include "chromeos/ash/services/cellular_setup/public/mojom/esim_manager.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace ash {

ASH_PUBLIC_EXPORT void GetESimManager(
    mojo::PendingReceiver<cellular_setup::mojom::ESimManager> receiver);

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_ESIM_MANAGER_H_
