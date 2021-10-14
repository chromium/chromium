// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/scanning/fast_pair/fast_pair_discoverable_scanner.h"

#include <cstdint>
#include <memory>
#include <vector>

#include "ash/quick_pair/common/constants.h"
#include "ash/quick_pair/common/device.h"
#include "ash/quick_pair/common/fast_pair/fast_pair_decoder.h"
#include "ash/quick_pair/common/logging.h"
#include "ash/quick_pair/common/protocol.h"
#include "ash/quick_pair/repository/fast_pair_repository.h"
#include "ash/quick_pair/scanning/range_tracker.h"
#include "ash/services/quick_pair/quick_pair_process.h"
#include "ash/services/quick_pair/quick_pair_process_manager.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
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
constexpr int kMaxParseModelIdRetryCount = 5;

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

  const std::vector<uint8_t>* fast_pair_service_data =
      device->GetServiceDataForUUID(kFastPairBluetoothUuid);

  if (!fast_pair_service_data) {
    QP_LOG(WARNING) << __func__
                    << ": Device doesn't have any Fast Pair Service Data.";
    return;
  }

  model_id_parse_attempts_[device->GetAddress()] = 1;

  quick_pair_process::GetHexModelIdFromServiceData(
      *fast_pair_service_data,
      base::BindOnce(&FastPairDiscoverableScanner::OnModelIdRetrieved,
                     weak_pointer_factory_.GetWeakPtr(), device),
      base::BindOnce(&FastPairDiscoverableScanner::OnUtilityProcessStopped,
                     weak_pointer_factory_.GetWeakPtr(), device));
}

void FastPairDiscoverableScanner::OnModelIdRetrieved(
    device::BluetoothDevice* device,
    const absl::optional<std::string>& model_id) {
  auto it = model_id_parse_attempts_.find(device->GetAddress());

  // If there's no entry in the map, the device was lost while parsing.
  if (it == model_id_parse_attempts_.end())
    return;

  model_id_parse_attempts_.erase(it);

  if (!model_id)
    return;

  // The Nearby Share feature advertises under the Fast Pair Service Data UUID
  // and uses a reserved model ID to enable their 'fast initiation' scenario.
  // We must detect this instance and ignore these advertisements since they
  // do not correspond to Fast Pair devices that are open to pairing.
  if (base::EqualsCaseInsensitiveASCII(model_id.value(), kNearbyShareModelId))
    return;

  FastPairRepository::Get()->GetDeviceMetadata(
      *model_id,
      base::BindOnce(&FastPairDiscoverableScanner::OnDeviceMetadataRetrieved,
                     weak_pointer_factory_.GetWeakPtr(), device,
                     model_id.value()));
}

void FastPairDiscoverableScanner::OnDeviceMetadataRetrieved(
    device::BluetoothDevice* device,
    const std::string model_id,
    DeviceMetadata* device_metadata) {
  if (!device_metadata) {
    QP_LOG(WARNING) << __func__
                    << ": Could not get metadata for id: " << model_id
                    << ". Ignoring this advertisement";
    return;
  }

  auto& details = device_metadata->GetDetails();
  double trigger_distance;
  if (details.trigger_distance() > 0) {
    trigger_distance = details.trigger_distance();
  } else {
    NOTREACHED();
    trigger_distance = kDefaultRangeInMeters;
  }

  QP_LOG(INFO) << __func__
               << ": Checking if device is in range, and waiting if not.  "
                  "trigger_distance="
               << trigger_distance;

  int tx_power = details.ble_tx_power();

  range_tracker_->Track(
      device, trigger_distance,
      base::BindRepeating(&FastPairDiscoverableScanner::NotifyDeviceFound,
                          weak_pointer_factory_.GetWeakPtr(), model_id),
      tx_power == 0 ? absl::nullopt : absl::make_optional(tx_power));
}

void FastPairDiscoverableScanner::OnDeviceLost(
    device::BluetoothDevice* device) {
  QP_LOG(VERBOSE) << __func__ << ": " << device->GetNameForDisplay();

  // If we have an in-progress attempt to parse for this device, removing it
  // from this map will ensure the result is ignored.
  model_id_parse_attempts_.erase(device->GetAddress());

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
    const std::string model_id,
    device::BluetoothDevice* bluetooth_device) {
  QP_LOG(VERBOSE) << __func__ << ": Id: " << model_id;

  auto device = base::MakeRefCounted<Device>(
      model_id, bluetooth_device->GetAddress(), Protocol::kFastPairInitial);

  notified_devices_[bluetooth_device->GetAddress()] = device;

  found_callback_.Run(device);
}

void FastPairDiscoverableScanner::OnUtilityProcessStopped(
    device::BluetoothDevice* device,
    QuickPairProcessManager::ShutdownReason shutdown_reason) {
  int current_retry_count = model_id_parse_attempts_[device->GetAddress()];
  if (current_retry_count > kMaxParseModelIdRetryCount) {
    QP_LOG(WARNING) << "Failed to parse model id from device more than "
                    << kMaxParseModelIdRetryCount << " times.";
    // Clean up the state here which enables trying again in the future if this
    // device is re-discovered.
    model_id_parse_attempts_.erase(device->GetAddress());
    return;
  }

  model_id_parse_attempts_[device->GetAddress()] = current_retry_count + 1;

  const std::vector<uint8_t>* fast_pair_service_data =
      device->GetServiceDataForUUID(kFastPairBluetoothUuid);

  quick_pair_process::GetHexModelIdFromServiceData(
      *fast_pair_service_data,
      base::BindOnce(&FastPairDiscoverableScanner::OnModelIdRetrieved,
                     weak_pointer_factory_.GetWeakPtr(), device),
      base::BindOnce(&FastPairDiscoverableScanner::OnUtilityProcessStopped,
                     weak_pointer_factory_.GetWeakPtr(), device));
}

}  // namespace quick_pair
}  // namespace ash
