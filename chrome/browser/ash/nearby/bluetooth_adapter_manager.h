// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_NEARBY_BLUETOOTH_ADAPTER_MANAGER_H_
#define CHROME_BROWSER_ASH_NEARBY_BLUETOOTH_ADAPTER_MANAGER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace device {
class BluetoothAdapter;
}  // namespace device

namespace bluetooth {
class Adapter;
namespace mojom {
class Adapter;
}
}  // namespace bluetooth

namespace ash {
namespace nearby {

// Owns the mojo interface and underlying platform bluetooth adapter.  This
// ensures that we can clean up the bluetooth state on shutdown, and prevent
// blocking on pending connection requests.
class BluetoothAdapterManager {
 public:
  BluetoothAdapterManager();
  virtual ~BluetoothAdapterManager();
  virtual void Initialize(
      mojo::PendingReceiver<bluetooth::mojom::Adapter> pending_receiver,
      scoped_refptr<device::BluetoothAdapter> adapter);

  virtual void Shutdown();

  base::WeakPtr<BluetoothAdapterManager> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  raw_ptr<device::BluetoothAdapter> device_bluetooth_adapter_;
  std::unique_ptr<bluetooth::Adapter> bluetooth_adapter_;
  std::unique_ptr<mojo::Receiver<bluetooth::mojom::Adapter>>
      bluetooth_receiver_;

  base::WeakPtrFactory<BluetoothAdapterManager> weak_ptr_factory_{this};
};
}  // namespace nearby
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_NEARBY_BLUETOOTH_ADAPTER_MANAGER_H_
