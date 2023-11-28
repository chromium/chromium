// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/scanning/fast_pair/fast_pair_not_discoverable_scanner_impl.h"

#include <cstdint>
#include <iomanip>
#include <memory>
#include <optional>
#include <sstream>
#include <vector>

#include "ash/quick_pair/common/constants.h"
#include "ash/quick_pair/common/device.h"
#include "ash/quick_pair/common/fast_pair/fast_pair_decoder.h"
#include "ash/quick_pair/common/pair_failure.h"
#include "ash/quick_pair/common/protocol.h"
#include "ash/quick_pair/repository/fast_pair/device_metadata.h"
#include "ash/quick_pair/repository/fast_pair/pairing_metadata.h"
#include "ash/quick_pair/repository/fast_pair_repository.h"
#include "base/check.h"
#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "chromeos/ash/services/quick_pair/public/cpp/account_key_filter.h"
#include "chromeos/ash/services/quick_pair/public/cpp/not_discoverable_advertisement.h"
#include "chromeos/ash/services/quick_pair/quick_pair_process.h"
#include "chromeos/ash/services/quick_pair/quick_pair_process_manager.h"
#include "components/cross_device/logging/logging.h"
#include "device/bluetooth//bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_device.h"

namespace {

constexpr int kMaxParseAdvertisementRetryCount = 5;

device::BluetoothDevice::BatteryInfo GetBatteryInfo(
    const ash::quick_pair::BatteryInfo& battery_info,
    const device::BluetoothDevice::BatteryType& battery_type) {
  return device::BluetoothDevice::BatteryInfo(
      battery_type, battery_info.percentage,
      battery_info.is_charging
          ? device::BluetoothDevice::BatteryInfo::ChargeState::kCharging
          : device::BluetoothDevice::BatteryInfo::ChargeState::kDischarging);
}

void SetBatteryInfo(
    device::BluetoothDevice* device,
    const ash::quick_pair::BatteryNotification& battery_notification) {
  device::BluetoothDevice::BatteryInfo left_bud_info =
      GetBatteryInfo(/*battery_info=*/battery_notification.left_bud_info,
                     /*battery_type=*/device::BluetoothDevice::BatteryType::
                         kLeftBudTrueWireless);
  device->SetBatteryInfo(left_bud_info);

  device::BluetoothDevice::BatteryInfo right_bud_info =
      GetBatteryInfo(/*battery_info=*/battery_notification.right_bud_info,
                     /*battery_type=*/device::BluetoothDevice::BatteryType::
                         kRightBudTrueWireless);
  device->SetBatteryInfo(right_bud_info);

  device::BluetoothDevice::BatteryInfo case_info = GetBatteryInfo(
      /*battery_info=*/battery_notification.case_info,
      /*battery_type=*/device::BluetoothDevice::BatteryType::kCaseTrueWireless);
  device->SetBatteryInfo(case_info);
}

}  // namespace

namespace ash {
namespace quick_pair {

// static
FastPairNotDiscoverableScannerImpl::Factory*
    FastPairNotDiscoverableScannerImpl::Factory::g_test_factory_ = nullptr;

// static
std::unique_ptr<FastPairNotDiscoverableScanner>
FastPairNotDiscoverableScannerImpl::Factory::Create(
    scoped_refptr<FastPairScanner> scanner,
    scoped_refptr<device::BluetoothAdapter> adapter,
    DeviceCallback found_callback,
    DeviceCallback lost_callback) {
  if (g_test_factory_) {
    return g_test_factory_->CreateInstance(
        std::move(scanner), std::move(adapter), std::move(found_callback),
        std::move(lost_callback));
  }

  return base::WrapUnique(new FastPairNotDiscoverableScannerImpl(
      std::move(scanner), std::move(adapter), std::move(found_callback),
      std::move(lost_callback)));
}

// static
void FastPairNotDiscoverableScannerImpl::Factory::SetFactoryForTesting(
    Factory* g_test_factory) {
  g_test_factory_ = g_test_factory;
}

FastPairNotDiscoverableScannerImpl::Factory::~Factory() = default;

FastPairNotDiscoverableScannerImpl::FastPairNotDiscoverableScannerImpl(
    scoped_refptr<FastPairScanner> scanner,
    scoped_refptr<device::BluetoothAdapter> adapter,
    DeviceCallback found_callback,
    DeviceCallback lost_callback)
    : scanner_(scanner),
      adapter_(std::move(adapter)),
      found_callback_(std::move(found_callback)),
      lost_callback_(std::move(lost_callback)) {
  observation_.Observe(scanner.get());
}

FastPairNotDiscoverableScannerImpl::~FastPairNotDiscoverableScannerImpl() =
    default;

void FastPairNotDiscoverableScannerImpl::OnDeviceFound(
    device::BluetoothDevice* device) {
  CD_LOG(VERBOSE, Feature::FP)
      << __func__ << ": " << device->GetNameForDisplay();

  const std::vector<uint8_t>* fast_pair_service_data =
      device->GetServiceDataForUUID(kFastPairBluetoothUuid);

  if (!fast_pair_service_data) {
    CD_LOG(WARNING, Feature::FP)
        << __func__ << ": Device doesn't have any Fast Pair Service Data.";
    return;
  }

  advertisement_parse_attempts_[device->GetAddress()] = 1;

  CD_LOG(INFO, Feature::FP)
      << __func__ << ": Attempting to parse advertisement.";
  quick_pair_process::ParseNotDiscoverableAdvertisement(
      *fast_pair_service_data, device->GetAddress(),
      base::BindOnce(&FastPairNotDiscoverableScannerImpl::OnAdvertisementParsed,
                     weak_pointer_factory_.GetWeakPtr(), device->GetAddress()),
      base::BindOnce(
          &FastPairNotDiscoverableScannerImpl::OnUtilityProcessStopped,
          weak_pointer_factory_.GetWeakPtr(), device->GetAddress()));
}

void FastPairNotDiscoverableScannerImpl::OnDeviceLost(
    device::BluetoothDevice* device) {
  CD_LOG(VERBOSE, Feature::FP)
      << __func__ << ": " << device->GetNameForDisplay();

  // If we have an in-progess parse attempt for this device, this will ensure
  // the result is ignored.
  advertisement_parse_attempts_.erase(device->GetAddress());

  auto it = notified_devices_.find(device->GetAddress());

  // Don't invoke callback if we didn't notify this device.
  if (it == notified_devices_.end())
    return;

  CD_LOG(INFO, Feature::FP) << __func__ << ": Running lost callback";
  scoped_refptr<Device> notified_device = it->second;
  notified_devices_.erase(it);
  lost_callback_.Run(std::move(notified_device));
}

void FastPairNotDiscoverableScannerImpl::OnAdvertisementParsed(
    const std::string& address,
    const std::optional<NotDiscoverableAdvertisement>& advertisement) {
  CD_LOG(INFO, Feature::FP)
      << __func__
      << ": Has value: " << (advertisement.has_value() ? "yes" : "no");

  auto it = advertisement_parse_attempts_.find(address);

  // If this check fails, the device was lost during parsing
  if (it == advertisement_parse_attempts_.end()) {
    CD_LOG(WARNING, Feature::FP)
        << __func__ << ": Ignoring because parse attempt was cancelled";
    return;
  }

  advertisement_parse_attempts_.erase(it);

  if (!advertisement)
    return;

  // Don't continue if device was lost.
  device::BluetoothDevice* device = adapter_->GetDevice(address);
  if (!device) {
    CD_LOG(WARNING, Feature::FP)
        << __func__ << "Lost device after advertisement parsed.";
    return;
  }

  // Set the battery notification if the advertisement contains battery
  // notification information
  if (advertisement->battery_notification)
    SetBatteryInfo(device, advertisement->battery_notification.value());

  if (!advertisement->show_ui) {
    CD_LOG(INFO, Feature::FP)
        << __func__ << ": Ignoring because show UI flag is false";
    return;
  }

  auto filter_iterator =
      account_key_filters_
          .insert_or_assign(address, AccountKeyFilter(advertisement.value()))
          .first;

  FastPairRepository::Get()->CheckAccountKeys(
      filter_iterator->second,
      base::BindOnce(
          &FastPairNotDiscoverableScannerImpl::OnAccountKeyFilterCheckResult,
          weak_pointer_factory_.GetWeakPtr(), address));
}

void FastPairNotDiscoverableScannerImpl::OnAccountKeyFilterCheckResult(
    const std::string& address,
    std::optional<PairingMetadata> metadata) {
  account_key_filters_.erase(address);

  CD_LOG(INFO, Feature::FP)
      << __func__ << " Metadata: " << (metadata ? "yes" : "no");

  if (!metadata || !metadata->device_metadata)
    return;

  // A paired device still emits not discoverable advertisements, so we check
  // here to prevent showing an incorrect notification.
  if (FastPairRepository::Get()->IsAccountKeyPairedLocally(
          metadata->account_key)) {
    CD_LOG(INFO, Feature::FP)
        << __func__ << ": device already paired and saved to this Chromebook";
    return;
  }

  auto& details = metadata->device_metadata->GetDetails();

  // Convert the integer model id to uppercase hex string.
  std::stringstream model_id_stream;
  model_id_stream << std::uppercase << std::hex << details.id();
  std::string model_id = model_id_stream.str();

  CD_LOG(INFO, Feature::FP) << __func__ << ": Id: " << model_id;
  auto device = base::MakeRefCounted<Device>(model_id, address,
                                             Protocol::kFastPairSubsequent);
  device->set_account_key(metadata->account_key);
  device->set_version(metadata->device_metadata->InferFastPairVersion());

  device::BluetoothDevice* ble_device =
      adapter_->GetDevice(device->ble_address());

  if (ble_device && ble_device->IsPaired()) {
    CD_LOG(ERROR, Feature::FP) << __func__
                               << ": A discoverable advertisement "
                                  "was notified for a paired BLE device.";
    return;
  }

  CD_LOG(INFO, Feature::FP) << __func__ << ": Running found callback";
  notified_devices_[device->ble_address()] = device;
  found_callback_.Run(device);
}

void FastPairNotDiscoverableScannerImpl::OnUtilityProcessStopped(
    const std::string& address,
    QuickPairProcessManager::ShutdownReason shutdown_reason) {
  int current_retry_count = advertisement_parse_attempts_[address];
  if (current_retry_count > kMaxParseAdvertisementRetryCount) {
    CD_LOG(WARNING, Feature::FP)
        << "Failed to parse advertisement from device more than "
        << kMaxParseAdvertisementRetryCount << " times.";
    // Clean up the state here which enables trying again in the future if
    // this device is re-discovered.
    advertisement_parse_attempts_.erase(address);
    return;
  }

  // Don't try to parse the advertisement again if the device was lost.
  device::BluetoothDevice* device = adapter_->GetDevice(address);
  if (!device) {
    CD_LOG(WARNING, Feature::FP)
        << __func__ << ": Lost device in between parse attempts.";
    advertisement_parse_attempts_.erase(address);
    return;
  }

  const std::vector<uint8_t>* fast_pair_service_data =
      device->GetServiceDataForUUID(kFastPairBluetoothUuid);

  if (!fast_pair_service_data) {
    CD_LOG(WARNING, Feature::FP)
        << "Failed to get service data for a device we previously "
           "did get it for.";
    advertisement_parse_attempts_.erase(address);
    return;
  }

  advertisement_parse_attempts_[address] = current_retry_count + 1;

  CD_LOG(INFO, Feature::FP)
      << __func__ << ": Retrying call to parse advertisement";
  quick_pair_process::ParseNotDiscoverableAdvertisement(
      *fast_pair_service_data, address,
      base::BindOnce(&FastPairNotDiscoverableScannerImpl::OnAdvertisementParsed,
                     weak_pointer_factory_.GetWeakPtr(), address),
      base::BindOnce(
          &FastPairNotDiscoverableScannerImpl::OnUtilityProcessStopped,
          weak_pointer_factory_.GetWeakPtr(), address));
}

}  // namespace quick_pair
}  // namespace ash
