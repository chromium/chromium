// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/scanning/fast_pair/fast_pair_scanner_impl.h"

#include "ash/quick_pair/common/constants.h"
#include "ash/quick_pair/common/logging.h"
#include "base/containers/contains.h"
#include "base/time/time.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/bluetooth_low_energy_scan_filter.h"

namespace {

constexpr base::TimeDelta kFilterDeviceFoundTimeout = base::Seconds(1);
constexpr base::TimeDelta kFilterDeviceLostTimeout = base::Seconds(5);
constexpr uint8_t kFilterPatternStartPosition = 0;
const std::vector<uint8_t> kFastPairFilterPatternValue = {0x2c, 0xfe};

std::ostream& operator<<(
    std::ostream& out,
    const device::BluetoothLowEnergyScanSession::ErrorCode& error_code) {
  switch (error_code) {
    case device::BluetoothLowEnergyScanSession::ErrorCode::kFailed:
      out << "[Failed]";
      break;
  }
  return out;
}
}  // namespace

namespace ash {
namespace quick_pair {

FastPairScannerImpl::FastPairScannerImpl() {
  device::BluetoothAdapterFactory::Get()->GetAdapter(base::BindOnce(
      &FastPairScannerImpl::OnGetAdapter, weak_ptr_factory_.GetWeakPtr()));
}

FastPairScannerImpl::~FastPairScannerImpl() = default;

void FastPairScannerImpl::OnGetAdapter(
    scoped_refptr<device::BluetoothAdapter> adapter) {
  adapter_ = adapter;
  adapter_observation_.Observe(adapter_.get());

  device::BluetoothLowEnergyScanFilter::Pattern pattern(
      kFilterPatternStartPosition,
      device::BluetoothLowEnergyScanFilter::AdvertisementDataType::kServiceData,
      kFastPairFilterPatternValue);
  auto filter = device::BluetoothLowEnergyScanFilter::Create(
      device::BluetoothLowEnergyScanFilter::Range::kNear,
      kFilterDeviceFoundTimeout, kFilterDeviceLostTimeout, {pattern});
  if (!filter) {
    QP_LOG(ERROR) << "Bluetooth Low Energy Scan Session failed to start due to "
                     "failure to create filter.";
    return;
  }

  background_scan_session_ = adapter_->StartLowEnergyScanSession(
      std::move(filter), weak_ptr_factory_.GetWeakPtr());
}

void FastPairScannerImpl::AddObserver(FastPairScanner::Observer* observer) {
  observers_.AddObserver(observer);
}

void FastPairScannerImpl::RemoveObserver(FastPairScanner::Observer* observer) {
  observers_.RemoveObserver(observer);
}

void FastPairScannerImpl::OnSessionStarted(
    device::BluetoothLowEnergyScanSession* scan_session,
    absl::optional<device::BluetoothLowEnergyScanSession::ErrorCode>
        error_code) {
  if (error_code) {
    QP_LOG(ERROR) << "Bluetooth Low Energy Scan Session failed to start with "
                     "the following error: "
                  << error_code.value();
    return;
  }
}

void FastPairScannerImpl::OnSessionInvalidated(
    device::BluetoothLowEnergyScanSession* scan_session) {
  // TODO(crbug.com/1227519) Handle Session Invalidation by adding exponential
  // retry to restart the scanner.
  background_scan_session_.reset();
  device_address_advertisement_data_map_.clear();
}

void FastPairScannerImpl::OnDeviceFound(
    device::BluetoothLowEnergyScanSession* scan_session,
    device::BluetoothDevice* device) {
  const std::vector<uint8_t>* service_data =
      device->GetServiceDataForUUID(kFastPairBluetoothUuid);
  if (!service_data) {
    QP_LOG(WARNING) << "No Fast Pair service data found on device";
    return;
  }

  device_address_advertisement_data_map_[device->GetAddress()].insert(
      *service_data);
  NotifyDeviceFound(device);
}

void FastPairScannerImpl::DeviceChanged(device::BluetoothAdapter* adapter,
                                        device::BluetoothDevice* device) {
  std::string device_address = device->GetAddress();
  const std::vector<uint8_t>* service_data =
      device->GetServiceDataForUUID(kFastPairBluetoothUuid);

  // If the advertisement data we have received does not pertain to a device
  // we have seen already from the scanner, or if the advertisement data for
  // a device we have already seen is not new, then early return and do not
  // notify observers or add data to the device address advertisement data map.
  if (!base::Contains(device_address_advertisement_data_map_, device_address) ||
      base::Contains(device_address_advertisement_data_map_[device_address],
                     *service_data)) {
    return;
  }

  device_address_advertisement_data_map_[device_address].insert(*service_data);
  NotifyDeviceFound(device);
}

void FastPairScannerImpl::NotifyDeviceFound(device::BluetoothDevice* device) {
  for (auto& observer : observers_)
    observer.OnDeviceFound(device);
}

void FastPairScannerImpl::OnDeviceLost(
    device::BluetoothLowEnergyScanSession* scan_session,
    device::BluetoothDevice* device) {
  device_address_advertisement_data_map_.erase(device->GetAddress());
  for (auto& observer : observers_)
    observer.OnDeviceLost(device);
}

}  // namespace quick_pair
}  // namespace ash
