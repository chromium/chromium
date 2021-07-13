// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/scanning/range_tracker.h"

#include <tuple>

#include "ash/quick_pair/common/logging.h"
#include "ash/quick_pair/scanning/range_calculations.h"
#include "base/callback.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_device.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

using device::BluetoothAdapter;
using device::BluetoothDevice;

namespace ash {
namespace quick_pair {

RangeTracker::RangeTracker(scoped_refptr<BluetoothAdapter> adapter)
    : adapter_(adapter) {
  observation_.Observe(adapter_.get());
}

RangeTracker::~RangeTracker() = default;

void RangeTracker::Track(BluetoothDevice* device,
                         double threshold_in_meters,
                         RangeTrackerCallback callback,
                         const absl::optional<int8_t>& known_tx_power) {
  if (IsDeviceWithinThreshold(device, threshold_in_meters, known_tx_power)) {
    // Immediately invoke callback if device is already in range.
    std::move(callback).Run(device);
  } else {
    // Otherwise save device info and check again in
    // BluetoothAdapter::Observer::DeviceChanged
    device_callbacks_[device->GetAddress()] =
        TrackingInfo(threshold_in_meters, std::move(callback), known_tx_power);
  }
}

bool RangeTracker::IsDeviceWithinThreshold(
    BluetoothDevice* device,
    double threshold_in_meters,
    const absl::optional<int8_t>& known_tx_power) {
  absl::optional<int8_t> rssi = device->GetInquiryRSSI();
  absl::optional<int8_t> tx_power =
      known_tx_power.has_value() ? known_tx_power : device->GetInquiryTxPower();

  if (!rssi) {
    QP_LOG(WARNING) << "RSSI value isn't available for device.";
    return false;
  }

  if (!tx_power) {
    QP_LOG(WARNING) << "TxPower value isn't available for device.";
    return false;
  }

  return range_calculations::DistanceFromRssiAndTxPower(
             rssi.value(), tx_power.value()) <= threshold_in_meters;
}

void RangeTracker::DeviceChanged(BluetoothAdapter* adapter,
                                 BluetoothDevice* device) {
  auto it = device_callbacks_.find(device->GetAddress());

  // Early return if we aren't tracking this device.
  if (it == device_callbacks_.end())
    return;

  TrackingInfo& info = it->second;

  if (IsDeviceWithinThreshold(device, info.threshold_in_meters_,
                              info.known_tx_power_)) {
    std::move(info.callback_).Run(device);
    device_callbacks_.erase(it);
  }
}

void RangeTracker::StopTracking(BluetoothDevice* device) {
  device_callbacks_.erase(device->GetAddress());
}

RangeTracker::TrackingInfo::TrackingInfo() = default;

RangeTracker::TrackingInfo::TrackingInfo(double threshold_in_meters,
                                         RangeTrackerCallback callback,
                                         absl::optional<int8_t> known_tx_power)
    : threshold_in_meters_(threshold_in_meters),
      callback_(std::move(callback)),
      known_tx_power_(known_tx_power) {}

RangeTracker::TrackingInfo::~TrackingInfo() = default;

}  // namespace quick_pair
}  // namespace ash
