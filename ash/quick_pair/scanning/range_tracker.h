// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_SCANNING_RANGE_TRACKER_H_
#define ASH_QUICK_PAIR_SCANNING_RANGE_TRACKER_H_

#include <stdint.h>
#include <cstdint>
#include <map>
#include <utility>
#include "base/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace device {
class BluetoothDevice;
}  // namespace device

namespace ash {
namespace quick_pair {

using RangeTrackerCallback = base::OnceCallback<void(device::BluetoothDevice*)>;

// RangeTracker provides an API to invoke a callback when a BluetoothDevice
// enters a given threshold range.
class RangeTracker final : public device::BluetoothAdapter::Observer {
 public:
  explicit RangeTracker(scoped_refptr<device::BluetoothAdapter> adapter);
  RangeTracker(const RangeTracker&) = delete;
  RangeTracker& operator=(const RangeTracker&) = delete;
  ~RangeTracker() override;

  // Start tracking |device| and invoke |callback| once it is within
  // |threshold_in_meters|. |callback| is invoked immediately if the device
  // is already in range, otherwise it is saved and invoked later when/if the
  // |device| comes into range.
  // Populate |known_tx_power| if you know the value the device is using,
  // otherwise BluetoothDevice::GetInquiryTxPower is used.
  // If |device| is already being tracked, the previous values are overridden.
  void Track(device::BluetoothDevice* device,
             double threshold_in_meters,
             RangeTrackerCallback callback,
             const absl::optional<int8_t>& known_tx_power = absl::nullopt);

  void StopTracking(device::BluetoothDevice* device);

  // BluetoothAdapter::Observer
  void DeviceChanged(device::BluetoothAdapter* adapter,
                     device::BluetoothDevice* device) override;

 private:
  struct TrackingInfo {
    TrackingInfo();
    TrackingInfo(double threshold_in_meters,
                 RangeTrackerCallback callback,
                 absl::optional<int8_t> known_tx_power);
    TrackingInfo& operator=(TrackingInfo&&) = default;
    ~TrackingInfo();

    double threshold_in_meters_;
    RangeTrackerCallback callback_;
    absl::optional<int8_t> known_tx_power_;
  };

  bool IsDeviceWithinThreshold(device::BluetoothDevice* device,
                               double threshold_in_meters,
                               const absl::optional<int8_t>& known_tx_power);

  scoped_refptr<device::BluetoothAdapter> adapter_;
  std::map<std::string, TrackingInfo> device_callbacks_;
  base::ScopedObservation<device::BluetoothAdapter,
                          device::BluetoothAdapter::Observer>
      observation_{this};
  base::WeakPtrFactory<RangeTracker> weak_pointer_factory_{this};
};

}  // namespace quick_pair
}  // namespace ash

#endif  //  ASH_QUICK_PAIR_SCANNING_RANGE_TRACKER_H_
