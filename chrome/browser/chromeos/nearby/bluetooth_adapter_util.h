// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_NEARBY_BLUETOOTH_ADAPTER_UTIL_H_
#define CHROME_BROWSER_CHROMEOS_NEARBY_BLUETOOTH_ADAPTER_UTIL_H_

#include "base/memory/ref_counted.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace bluetooth {
namespace mojom {
class Adapter;
}  // namespace mojom
}  // namespace bluetooth

namespace device {
class BluetoothAdapter;
}  // namespace device

namespace chromeos {
namespace nearby {

// Note: This helper function is implemented in its own file so that it can
// depend on the bluetooth::mojom::Adapter interface, which is only visible to a
// limited set of clients.
void MakeSelfOwnedBluetoothAdapter(
    mojo::PendingReceiver<bluetooth::mojom::Adapter> pending_receiver,
    scoped_refptr<device::BluetoothAdapter> adapter);

}  // namespace nearby
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_NEARBY_BLUETOOTH_ADAPTER_UTIL_H_
