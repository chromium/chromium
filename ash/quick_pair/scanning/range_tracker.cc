// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/scanning/range_tracker.h"

#include <tuple>

#include "ash/quick_pair/common/logging.h"
#include "ash/quick_pair/scanning/range_calculations.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/timer/timer.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_device.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

using device::BluetoothAdapter;
using device::BluetoothDevice;

namespace ash {
namespace quick_pair {

// The amount of time we will wait for the rssi and tx power values to be
// reported before timing out and assuming the device is in range.
constexpr int kTimeOutInSeconds = 1;

RangeTracker::RangeTracker(scoped_refptr<BluetoothAdapter> adapter)
    : adapter_(adapter) {
  observation_.Observe(adapter_.get());
}

RangeTracker::~RangeTracker() = default;

void RangeTracker::Track(BluetoothDevice* device,
                         double threshold_in_meters,
                         RangeTrackerCallback callback,
                         const absl::optional<int8_t>& known_tx_power) {
  // Immediately save a TrackingInfo object because IsDevicewithinThreshold will
  // query for it.
  auto it = device_callbacks_.emplace(
      device->GetAddress(),
      TrackingInfo(threshold_in_meters, std::move(callback), known_tx_power));

  if (IsDeviceWithinThreshold(device, threshold_in_meters, known_tx_power)) {
    // Immediately invoke callback if device is already in range.
    std::move(it.first->second.callback).Run(device);
    device_callbacks_.erase(device->GetAddress());
  }
}

bool RangeTracker::IsDeviceWithinThreshold(
    BluetoothDevice* device,
    double threshold_in_meters,
    const absl::optional<int8_t>& known_tx_power) {
  absl::optional<int8_t> rssi = device->GetInquiryRSSI();
  absl::optional<int8_t> tx_power =
      known_tx_power.has_value() ? known_tx_power : device->GetInquiryTxPower();

  auto it = device_callbacks_.find(device->GetAddress());
  DCHECK(it != device_callbacks_.end());

  if ((!rssi || !tx_power) && !it->second.timeout_timer.IsRunning()) {
    // Start timeout if there is no rssi or tx power values currently.
    it->second.timeout_timer.Start(
        FROM_HERE, base::Seconds(kTimeOutInSeconds),
        base::BindOnce(&RangeTracker::OnTimeout,
                       weak_pointer_factory_.GetWeakPtr(),
                       device->GetAddress()));
  }

  if (!rssi) {
    QP_LOG(WARNING) << "RSSI value isn't available for device.";
    return false;
  }

  if (!tx_power) {
    QP_LOG(WARNING) << "TxPower value isn't available for device.";
    return false;
  }

  // We have a rssi and tx_power value for this device, stop the timeout.
  it->second.timeout_timer.AbandonAndStop();

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

  if (IsDeviceWithinThreshold(device, info.threshold_in_meters,
                              info.known_tx_power)) {
    std::move(info.callback).Run(device);
    device_callbacks_.erase(it);
  }
}

void RangeTracker::StopTracking(BluetoothDevice* device) {
  device_callbacks_.erase(device->GetAddress());
}

void RangeTracker::OnTimeout(const std::string& address) {
  QP_LOG(WARNING) << __func__
                  << ": Timed out waiting for device info (rssi, tx power).";
  auto it = device_callbacks_.find(address);

  // Return early if we are no longer tracking this device.
  if (it == device_callbacks_.end()) {
    QP_LOG(WARNING) << __func__ << ": No device info from callbacks";
    return;
  }

  BluetoothDevice* device = adapter_->GetDevice(address);

  if (!device) {
    QP_LOG(WARNING) << __func__ << ": No device from adapter";
    device_callbacks_.erase(it);
    return;
  }

  std::move(it->second.callback).Run(device);
  device_callbacks_.erase(it);
}

RangeTracker::TrackingInfo::TrackingInfo() = default;

RangeTracker::TrackingInfo::TrackingInfo(double threshold_in_meters,
                                         RangeTrackerCallback callback,
                                         absl::optional<int8_t> known_tx_power)
    : threshold_in_meters(threshold_in_meters),
      callback(std::move(callback)),
      known_tx_power(known_tx_power) {}

RangeTracker::TrackingInfo::TrackingInfo(TrackingInfo&& other) {
  DCHECK(!other.timeout_timer.IsRunning())
      << "This move constructor is written with the assumption that the timer"
         "field is not running, and thus can be made new again";
  threshold_in_meters = other.threshold_in_meters;
  callback = std::move(other.callback);
  known_tx_power = std::move(other.known_tx_power);
}

RangeTracker::TrackingInfo::~TrackingInfo() = default;

}  // namespace quick_pair
}  // namespace ash
