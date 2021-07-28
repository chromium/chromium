// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_SCANNING_FAST_PAIR_FAST_PAIR_DISCOVERABLE_SCANNER_H_
#define ASH_QUICK_PAIR_SCANNING_FAST_PAIR_FAST_PAIR_DISCOVERABLE_SCANNER_H_

#include <memory>
#include <string>

#include "ash/quick_pair/scanning/fast_pair/fast_pair_scanner.h"
#include "ash/quick_pair/scanning/range_tracker.h"
#include "base/callback_forward.h"
#include "base/containers/flat_map.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"

namespace device {
class BluetoothDevice;
}  // namespace device

namespace ash {
namespace quick_pair {

struct Device;

using DeviceCallback = base::RepeatingCallback<void(scoped_refptr<Device>)>;

// This class detects Fast Pair 'discoverable' advertisements (see
// https://developers.google.com/nearby/fast-pair/spec#AdvertisingWhenDiscoverable)
// and invokes the |found_callback| when it finds a device within the
// appropriate range.  |lost_callback| will be invoked when that device is lost
// to the bluetooth adapter.
class FastPairDiscoverableScanner final : public FastPairScanner::Observer {
 public:
  FastPairDiscoverableScanner(scoped_refptr<FastPairScanner> scanner,
                              std::unique_ptr<RangeTracker> range_tracker,
                              DeviceCallback found_callback,
                              DeviceCallback lost_callback);
  FastPairDiscoverableScanner(const FastPairDiscoverableScanner&) = delete;
  FastPairDiscoverableScanner& operator=(const FastPairDiscoverableScanner&) =
      delete;
  ~FastPairDiscoverableScanner() override;

  // FastPairScanner::Observer
  void OnDeviceFound(device::BluetoothDevice* device) override;
  void OnDeviceLost(device::BluetoothDevice* device) override;

 private:
  absl::optional<std::string> GetModelIdForDevice(
      device::BluetoothDevice* device);
  void NotifyDeviceFound(device::BluetoothDevice* device);

  scoped_refptr<FastPairScanner> scanner_;
  std::unique_ptr<RangeTracker> range_tracker_;
  DeviceCallback found_callback_;
  DeviceCallback lost_callback_;
  base::flat_map<std::string, scoped_refptr<Device>> notified_devices_;
  base::ScopedObservation<FastPairScanner, FastPairScanner::Observer>
      observation_{this};
  base::WeakPtrFactory<FastPairDiscoverableScanner> weak_pointer_factory_{this};
};

}  // namespace quick_pair
}  // namespace ash

#endif
