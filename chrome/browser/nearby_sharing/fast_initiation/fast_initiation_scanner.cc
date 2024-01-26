// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/fast_initiation/fast_initiation_scanner.h"

#include <stdint.h>

#include <optional>
#include <vector>

#include "base/time/time.h"
#include "chrome/browser/nearby_sharing/fast_initiation/constants.h"
#include "chrome/browser/nearby_sharing/nearby_share_metrics.h"
#include "components/cross_device/logging/logging.h"
#include "device/bluetooth/bluetooth_low_energy_scan_filter.h"

namespace {

// The length of time that a device must be in range before it is reported via a
// "device found" event.
constexpr base::TimeDelta kBackgroundScanningDeviceFoundTimeout =
    base::Seconds(1);

// The length of time that a device must be out of range before this is reported
// via a "device lost" event.
constexpr base::TimeDelta kBackgroundScanningDeviceLostTimeout =
    base::Seconds(7);
}  // namespace

// static
FastInitiationScanner::Factory*
    FastInitiationScanner::Factory::factory_instance_ = nullptr;

// static
std::unique_ptr<FastInitiationScanner> FastInitiationScanner::Factory::Create(
    scoped_refptr<device::BluetoothAdapter> adapter) {
  if (factory_instance_)
    return factory_instance_->CreateInstance(adapter);

  return std::make_unique<FastInitiationScanner>(adapter);
}

// static
bool FastInitiationScanner::Factory::IsHardwareSupportAvailable(
    device::BluetoothAdapter* adapter) {
  if (factory_instance_)
    return factory_instance_->IsHardwareSupportAvailable();

  // The function only returns correct status when adapter is powered.
  return adapter->IsPowered() &&
         adapter->GetLowEnergyScanSessionHardwareOffloadingStatus() ==
             device::BluetoothAdapter::
                 LowEnergyScanSessionHardwareOffloadingStatus::kSupported;
}

// static
void FastInitiationScanner::Factory::SetFactoryForTesting(
    FastInitiationScanner::Factory* factory) {
  factory_instance_ = factory;
}

FastInitiationScanner::FastInitiationScanner(
    scoped_refptr<device::BluetoothAdapter> adapter)
    : adapter_(adapter) {
  DCHECK(adapter_ && adapter_->IsPresent() && adapter_->IsPowered());
}

FastInitiationScanner::~FastInitiationScanner() {}

void FastInitiationScanner::StartScanning(
    base::RepeatingClosure devices_detected_callback,
    base::RepeatingClosure devices_not_detected_callback,
    base::OnceClosure scanner_invalidated_callback) {
  devices_detected_callback_ = std::move(devices_detected_callback);
  devices_not_detected_callback_ = std::move(devices_not_detected_callback);
  scanner_invalidated_callback_ = std::move(scanner_invalidated_callback);

  std::vector<uint8_t> pattern_value;
  // Add the service UUID in reversed byte order.
  pattern_value.insert(pattern_value.begin(),
                       std::rbegin(kFastInitiationServiceId),
                       std::rend(kFastInitiationServiceId));
  // Add the service data in the original order.
  pattern_value.insert(pattern_value.end(), std::begin(kFastInitiationModelId),
                       std::end(kFastInitiationModelId));

  device::BluetoothLowEnergyScanFilter::Pattern pattern(
      /*start_position=*/0,
      device::BluetoothLowEnergyScanFilter::AdvertisementDataType::kServiceData,
      std::move(pattern_value));
  auto filter = device::BluetoothLowEnergyScanFilter::Create(
      device::BluetoothLowEnergyScanFilter::Range::kNear,
      kBackgroundScanningDeviceFoundTimeout,
      kBackgroundScanningDeviceLostTimeout, {pattern},
      /*rssi_sampling_period=*/std::nullopt);
  if (!filter) {
    CD_LOG(ERROR, Feature::NS)
        << __func__
        << ": Failed to start Fast Initiation scanning due to "
           "failure to create filter.";
    std::move(scanner_invalidated_callback_).Run();
    return;
  }

  background_scan_session_ = adapter_->StartLowEnergyScanSession(
      std::move(filter), /*delegate=*/weak_ptr_factory_.GetWeakPtr());
}

void FastInitiationScanner::OnSessionStarted(
    device::BluetoothLowEnergyScanSession* scan_session,
    std::optional<device::BluetoothLowEnergyScanSession::ErrorCode>
        error_code) {
  if (error_code) {
    CD_LOG(WARNING, Feature::NS)
        << __func__
        << ": Error, error_code=" << static_cast<int>(error_code.value());
    std::move(scanner_invalidated_callback_).Run();
  } else {
    CD_LOG(VERBOSE, Feature::NS) << __func__ << ": Success";
  }
  RecordNearbyShareBackgroundScanningSessionStarted(/*success=*/!error_code);
}

void FastInitiationScanner::OnDeviceFound(
    device::BluetoothLowEnergyScanSession* scan_session,
    device::BluetoothDevice* device) {
  CD_LOG(VERBOSE, Feature::NS) << __func__;
  size_t device_count_prev = detected_devices_.size();
  detected_devices_.insert(device->GetAddress());

  // Invoke the callback when we go from zero devices to more than zero.
  if (device_count_prev == 0) {
    devices_detected_callback_.Run();
    RecordNearbyShareBackgroundScanningDevicesDetected();
    devices_detected_timestamp_ = base::TimeTicks::Now();
  }
}

void FastInitiationScanner::OnDeviceLost(
    device::BluetoothLowEnergyScanSession* scan_session,
    device::BluetoothDevice* device) {
  CD_LOG(VERBOSE, Feature::NS) << __func__;
  size_t device_count_prev = detected_devices_.size();
  if (detected_devices_.erase(device->GetAddress()) == 0) {
    CD_LOG(WARNING, Feature::NS)
        << __func__ << ": Received device lost event for device not in list.";
  }

  // Invoke the callback when we go from more than zero devices to zero.
  if (device_count_prev > 0 && detected_devices_.empty()) {
    devices_not_detected_callback_.Run();
    RecordNearbyShareBackgroundScanningDevicesDetectedDuration(
        base::TimeTicks::Now() - devices_detected_timestamp_);
  }
}

void FastInitiationScanner::OnSessionInvalidated(
    device::BluetoothLowEnergyScanSession* scan_session) {
  CD_LOG(VERBOSE, Feature::NS) << __func__;
  detected_devices_.clear();
  std::move(scanner_invalidated_callback_).Run();
}
