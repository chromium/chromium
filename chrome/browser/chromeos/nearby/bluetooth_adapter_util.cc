// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/nearby/bluetooth_adapter_util.h"

#include <memory>
#include <utility>

#include "device/bluetooth/adapter.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace chromeos {
namespace nearby {

void MakeSelfOwnedBluetoothAdapter(
    mojo::PendingReceiver<bluetooth::mojom::Adapter> pending_receiver,
    scoped_refptr<device::BluetoothAdapter> adapter) {
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<bluetooth::Adapter>(std::move(adapter)),
      std::move(pending_receiver));
}

}  // namespace nearby
}  // namespace chromeos
