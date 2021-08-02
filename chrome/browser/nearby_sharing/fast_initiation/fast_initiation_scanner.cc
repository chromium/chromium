// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/fast_initiation/fast_initiation_scanner.h"

#include <stdint.h>
#include <vector>

#include "base/time/time.h"
#include "chrome/browser/nearby_sharing/fast_initiation/constants.h"
#include "chrome/browser/nearby_sharing/logging/logging.h"
#include "device/bluetooth/bluetooth_low_energy_scan_filter.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace {

constexpr base::TimeDelta kBackgroundScanningDeviceFoundTimeout =
    base::TimeDelta::FromSeconds(1);
constexpr base::TimeDelta kBackgroundScanningDeviceLostTimeout =
    base::TimeDelta::FromSeconds(3);

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
    base::RepeatingClosure device_found_callback,
    base::RepeatingClosure device_lost_callback,
    base::OnceClosure scanner_invalidated_callback) {
  device_found_callback_ = std::move(device_found_callback);
  device_lost_callback_ = std::move(device_lost_callback);
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
      kBackgroundScanningDeviceLostTimeout, {pattern});
  if (!filter) {
    NS_LOG(ERROR) << __func__
                  << ": Failed to start Fast Initiation scanning due to "
                     "failure to create filter.";
    std::move(scanner_invalidated_callback_).Run();
    return;
  }

  background_scan_session_ = adapter_->StartLowEnergyScanSession(
      std::move(filter), /*delegate=*/weak_ptr_factory_.GetWeakPtr());
}

bool FastInitiationScanner::AreFastInitiationDevicesDetected() const {
  return !devices_attempting_to_share_.empty();
}

void FastInitiationScanner::OnSessionStarted(
    device::BluetoothLowEnergyScanSession* scan_session,
    absl::optional<device::BluetoothLowEnergyScanSession::ErrorCode>
        error_code) {
  if (error_code) {
    NS_LOG(WARNING) << __func__ << ": Error, error_code="
                    << static_cast<int>(error_code.value());
    std::move(scanner_invalidated_callback_).Run();
  } else {
    NS_LOG(VERBOSE) << __func__ << ": Success";
  }
}

void FastInitiationScanner::OnDeviceFound(
    device::BluetoothLowEnergyScanSession* scan_session,
    device::BluetoothDevice* device) {
  NS_LOG(VERBOSE) << __func__;
  devices_attempting_to_share_.insert(device->GetAddress());
  device_found_callback_.Run();
}

void FastInitiationScanner::OnDeviceLost(
    device::BluetoothLowEnergyScanSession* scan_session,
    device::BluetoothDevice* device) {
  NS_LOG(VERBOSE) << __func__;
  devices_attempting_to_share_.erase(device->GetAddress());
  device_lost_callback_.Run();
}

void FastInitiationScanner::OnSessionInvalidated(
    device::BluetoothLowEnergyScanSession* scan_session) {
  NS_LOG(VERBOSE) << __func__;
  devices_attempting_to_share_.clear();
  std::move(scanner_invalidated_callback_).Run();
}
