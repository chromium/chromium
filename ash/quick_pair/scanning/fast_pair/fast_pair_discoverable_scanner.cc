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
#include "ash/quick_pair/common/pair_failure.h"
#include "ash/quick_pair/common/protocol.h"
#include "ash/quick_pair/fast_pair_handshake/fast_pair_handshake.h"
#include "ash/quick_pair/fast_pair_handshake/fast_pair_handshake_lookup.h"
#include "ash/quick_pair/repository/fast_pair_repository.h"
#include "ash/services/quick_pair/quick_pair_process.h"
#include "ash/services/quick_pair/quick_pair_process_manager.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/check.h"
#include "base/containers/flat_set.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string_util.h"
#include "device/bluetooth//bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_device.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace {

constexpr char kNearbyShareModelId[] = "fc128e";
constexpr int kMaxParseModelIdRetryCount = 5;

}  // namespace

namespace ash {
namespace quick_pair {

FastPairDiscoverableScanner::FastPairDiscoverableScanner(
    scoped_refptr<FastPairScanner> scanner,
    scoped_refptr<device::BluetoothAdapter> adapter,
    DeviceCallback found_callback,
    DeviceCallback lost_callback)
    : scanner_(std::move(scanner)),
      adapter_(std::move(adapter)),
      found_callback_(std::move(found_callback)),
      lost_callback_(std::move(lost_callback)) {
  observation_.Observe(scanner_.get());
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
    device::BluetoothDevice* bluetooth_device,
    const std::string model_id,
    DeviceMetadata* device_metadata) {
  if (!device_metadata) {
    QP_LOG(WARNING) << __func__
                    << ": Could not get metadata for id: " << model_id
                    << ". Ignoring this advertisement";
    return;
  }

  QP_LOG(VERBOSE) << __func__ << ": Id: " << model_id;

  auto device = base::MakeRefCounted<Device>(
      model_id, bluetooth_device->GetAddress(), Protocol::kFastPairInitial);

  // Anti-spoofing keys were introduced in Fast Pair v2, so if this isn't
  // available then the device is v1.
  if (device_metadata->GetDetails()
          .anti_spoofing_key_pair()
          .public_key()
          .empty()) {
    NotifyDeviceFound(std::move(device));
    return;
  }

  FastPairHandshakeLookup::GetInstance()->Create(
      adapter_, device,
      base::BindOnce(&FastPairDiscoverableScanner::OnHandshakeComplete,
                     weak_pointer_factory_.GetWeakPtr()));
}

void FastPairDiscoverableScanner::OnHandshakeComplete(
    scoped_refptr<Device> device,
    absl::optional<PairFailure> failure) {
  if (failure) {
    QP_LOG(WARNING) << __func__ << ": Handshake failed with " << device
                    << " because: " << failure.value();
    return;
  }

  NotifyDeviceFound(std::move(device));
}

void FastPairDiscoverableScanner::NotifyDeviceFound(
    scoped_refptr<Device> device) {
  device::BluetoothDevice* classic_device =
      device->classic_address()
          ? adapter_->GetDevice(device->classic_address().value())
          : nullptr;

  device::BluetoothDevice* ble_device =
      adapter_->GetDevice(device->ble_address);

  bool is_already_paired =
      (classic_device && classic_device->IsPaired()) || ble_device->IsPaired();

  if (is_already_paired) {
    QP_LOG(INFO) << __func__ << ": Already paired with " << device;
    return;
  }

  notified_devices_[device->ble_address] = device;
  found_callback_.Run(device);
}

void FastPairDiscoverableScanner::OnDeviceLost(
    device::BluetoothDevice* device) {
  QP_LOG(VERBOSE) << __func__ << ": " << device->GetNameForDisplay();

  // If we have an in-progress attempt to parse for this device, removing it
  // from this map will ensure the result is ignored.
  model_id_parse_attempts_.erase(device->GetAddress());

  auto it = notified_devices_.find(device->GetAddress());

  // Don't invoke callback if we didn't notify this device.
  if (it == notified_devices_.end())
    return;

  scoped_refptr<Device> notified_device = it->second;
  notified_devices_.erase(it);
  lost_callback_.Run(std::move(notified_device));
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
