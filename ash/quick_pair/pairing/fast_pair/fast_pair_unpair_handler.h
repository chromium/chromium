// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_PAIRING_FAST_PAIR_FAST_PAIR_UNPAIR_HANDLER_H_
#define ASH_QUICK_PAIR_PAIRING_FAST_PAIR_FAST_PAIR_UNPAIR_HANDLER_H_

#include "base/memory/scoped_refptr.h"
#include "base/scoped_observation.h"
#include "device/bluetooth/bluetooth_adapter.h"

namespace device {
class BluetoothDevice;
}  // namespace device

namespace ash {
namespace quick_pair {

// Class which observes bluetooth device pair state changes and invokes the
// Fast Pair delete flow on devices that have been unpaired.
class FastPairUnpairHandler : public device::BluetoothAdapter::Observer {
 public:
  explicit FastPairUnpairHandler(
      scoped_refptr<device::BluetoothAdapter> adapter);
  FastPairUnpairHandler(const FastPairUnpairHandler&) = delete;
  FastPairUnpairHandler& operator=(const FastPairUnpairHandler&) = delete;
  ~FastPairUnpairHandler() override;

  // BluetoothAdapter::Observer
  void DeviceRemoved(device::BluetoothAdapter* adapter,
                     device::BluetoothDevice* device) override;

 private:
  scoped_refptr<device::BluetoothAdapter> adapter_;
  base::ScopedObservation<device::BluetoothAdapter,
                          device::BluetoothAdapter::Observer>
      observation_{this};
};

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_PAIRING_FAST_PAIR_FAST_PAIR_UNPAIR_HANDLER_H_
