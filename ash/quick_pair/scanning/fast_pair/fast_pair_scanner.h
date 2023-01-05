// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_SCANNING_FAST_PAIR_FAST_PAIR_SCANNER_H_
#define ASH_QUICK_PAIR_SCANNING_FAST_PAIR_FAST_PAIR_SCANNER_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "device/bluetooth/bluetooth_device.h"

namespace ash {
namespace quick_pair {

class Device;

// This registers a BluetoothLowEnergyScanner with the Advertisement Monitoring
// API and exposes the Fast Pair devices found/lost events to its observers.
class FastPairScanner : public base::RefCounted<FastPairScanner> {
 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnDeviceFound(device::BluetoothDevice* device) = 0;
    virtual void OnDeviceLost(device::BluetoothDevice* device) = 0;
  };

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;
  virtual void OnDevicePaired(scoped_refptr<Device> device) = 0;

 protected:
  virtual ~FastPairScanner() = default;

 private:
  friend base::RefCounted<FastPairScanner>;
};

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_SCANNING_FAST_PAIR_FAST_PAIR_SCANNER_H_
