// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/scanning/fast_pair/fast_pair_discoverable_scanner.h"

#include <cstdint>
#include <memory>

#include "ash/quick_pair/common/constants.h"
#include "ash/quick_pair/common/device.h"
#include "ash/quick_pair/common/fast_pair/fast_pair_decoder.h"
#include "ash/quick_pair/common/logging.h"
#include "ash/quick_pair/common/protocol.h"
#include "ash/quick_pair/scanning/range_tracker.h"
#include "base/bind.h"
#include "base/check.h"
#include "base/containers/contains.h"
#include "base/containers/flat_set.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string_util.h"
#include "device/bluetooth/bluetooth_device.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace {

constexpr char kNearbyShareModelId[] = "fc128e";
constexpr double kDefaultRangeInMeters = 2;

}  // namespace

namespace ash {
namespace quick_pair {

FastPairDiscoverableScanner::FastPairDiscoverableScanner(
    scoped_refptr<FastPairScanner> scanner,
    std::unique_ptr<RangeTracker> range_tracker,
    DeviceCallback found_callback,
    DeviceCallback lost_callback)
    : scanner_(scanner),
      range_tracker_(std::move(range_tracker)),
      found_callback_(std::move(found_callback)),
      lost_callback_(std::move(lost_callback)) {
  observation_.Observe(scanner.get());
}

FastPairDiscoverableScanner::~FastPairDiscoverableScanner() = default;

void FastPairDiscoverableScanner::OnDeviceFound(
    device::BluetoothDevice* device) {
  QP_LOG(VERBOSE) << __func__ << ": " << device->GetNameForDisplay();
  absl::optional<std::string> model_id = GetModelIdForDevice(device);

  if (!model_id)
    return;

  // The Nearby Share feature advertises under the Fast Pair Service Data UUID
  // and uses a reserved model ID to enable their 'fast initiation' scenario.
  // We must detect this instance and ignore these advertisements since they
  // do not correspond to Fast Pair devices that are open to pairing.
  if (base::EqualsCaseInsensitiveASCII(model_id.value(), kNearbyShareModelId))
    return;

  QP_LOG(INFO) << __func__
               << ": Checking if device is in range, and waiting if not.";

  range_tracker_->Track(
      device, kDefaultRangeInMeters,
      base::BindRepeating(&FastPairDiscoverableScanner::NotifyDeviceFound,
                          weak_pointer_factory_.GetWeakPtr()));
}

absl::optional<std::string> FastPairDiscoverableScanner::GetModelIdForDevice(
    device::BluetoothDevice* device) {
  const std::vector<uint8_t>* service_data =
      device->GetServiceDataForUUID(kFastPairBluetoothUuid);

  if (!fast_pair_decoder::HasModelId(service_data))
    return absl::nullopt;

  absl::optional<std::string> model_id =
      fast_pair_decoder::GetHexModelIdFromServiceData(service_data);

  DCHECK(model_id.has_value()) << "The fast_pair_decoder::HasModelId check "
                                  "above should guarantee we get a model id";

  return model_id;
}

void FastPairDiscoverableScanner::OnDeviceLost(
    device::BluetoothDevice* device) {
  QP_LOG(VERBOSE) << __func__ << ": " << device->GetNameForDisplay();

  range_tracker_->StopTracking(device);

  auto it = notified_devices_.find(device->GetAddress());

  // Don't invoke callback if we didn't notify this device.
  if (it == notified_devices_.end())
    return;

  scoped_refptr<Device> notified_device = it->second;
  notified_devices_.erase(it);
  lost_callback_.Run(std::move(notified_device));
}

void FastPairDiscoverableScanner::NotifyDeviceFound(
    device::BluetoothDevice* bluetooth_device) {
  absl::optional<std::string> model_id = GetModelIdForDevice(bluetooth_device);

  DCHECK(model_id) << "This function shouldn't be invoked unless the device "
                      "has previously been confirmed to have a model id";

  auto device = base::MakeRefCounted<Device>(model_id.value_or(""),
                                             bluetooth_device->GetAddress(),
                                             Protocol::kFastPair);

  notified_devices_[bluetooth_device->GetAddress()] = device;

  found_callback_.Run(device);
}

}  // namespace quick_pair
}  // namespace ash
