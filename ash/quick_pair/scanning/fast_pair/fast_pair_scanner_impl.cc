// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/scanning/fast_pair/fast_pair_scanner_impl.h"

#include "ash/constants/ash_features.h"
#include "ash/quick_pair/common/constants.h"
#include "ash/quick_pair/common/device.h"
#include "ash/quick_pair/common/fast_pair/fast_pair_metrics.h"
#include "ash/quick_pair/fast_pair_handshake/fast_pair_handshake.h"
#include "ash/quick_pair/fast_pair_handshake/fast_pair_handshake_lookup.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "components/cross_device/logging/logging.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/bluetooth_low_energy_scan_filter.h"

namespace {

constexpr base::TimeDelta kFilterDeviceFoundTimeout = base::Seconds(1);

// We use a high value here for the time out because we want to give our E2E
// flow enough time to complete during it. We do this because the platform will
// consider the device 'lost' if it doesn't receive an advertisement within this
// timeframe, BUT the E2E pairing flow will cause the device to stop
// advertising (and therefore we can see false 'lost' events if this is too
// short).
constexpr base::TimeDelta kFilterDeviceLostTimeout = base::Seconds(40);

constexpr uint8_t kFilterPatternStartPosition = 0;
const std::vector<uint8_t> kFastPairFilterPatternValue = {0x2c, 0xfe};
constexpr base::TimeDelta kRssiSamplingPeriod = base::Milliseconds(500);

}  // namespace

namespace ash {
namespace quick_pair {

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

// static
FastPairScannerImpl::Factory* FastPairScannerImpl::Factory::g_test_factory_ =
    nullptr;

// static
scoped_refptr<FastPairScanner> FastPairScannerImpl::Factory::Create() {
  if (g_test_factory_) {
    return g_test_factory_->CreateInstance();
  }

  return base::MakeRefCounted<FastPairScannerImpl>();
}

// static
void FastPairScannerImpl::Factory::SetFactoryForTesting(
    Factory* g_test_factory) {
  g_test_factory_ = g_test_factory;
}

FastPairScannerImpl::Factory::~Factory() = default;

FastPairScannerImpl::FastPairScannerImpl()
    : task_runner_(base::SequencedTaskRunner::GetCurrentDefault()) {
  device::BluetoothAdapterFactory::Get()->GetAdapter(base::BindOnce(
      &FastPairScannerImpl::OnGetAdapter, weak_ptr_factory_.GetWeakPtr()));
}

FastPairScannerImpl::~FastPairScannerImpl() = default;

void FastPairScannerImpl::OnGetAdapter(
    scoped_refptr<device::BluetoothAdapter> adapter) {
  adapter_ = adapter;
  adapter_observation_.Observe(adapter_.get());

  task_runner_->PostTask(FROM_HERE,
                         base::BindOnce(&FastPairScannerImpl::StartScanning,
                                        weak_ptr_factory_.GetWeakPtr()));
}

void FastPairScannerImpl::StartScanning() {
  device::BluetoothLowEnergyScanFilter::Pattern pattern(
      kFilterPatternStartPosition,
      device::BluetoothLowEnergyScanFilter::AdvertisementDataType::kServiceData,
      kFastPairFilterPatternValue);
  auto filter = device::BluetoothLowEnergyScanFilter::Create(
      device::BluetoothLowEnergyScanFilter::Range::kNear,
      kFilterDeviceFoundTimeout, kFilterDeviceLostTimeout, {pattern},
      kRssiSamplingPeriod);

  RecordBluetoothLowEnergyScanFilterResult(/*success=*/filter != nullptr);
  if (!filter) {
    CD_LOG(ERROR, Feature::FP)
        << "Bluetooth Low Energy Scan Session failed to start due to "
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
    std::optional<device::BluetoothLowEnergyScanSession::ErrorCode>
        error_code) {
  RecordBluetoothLowEnergyScannerStartSessionResult(
      /*success=*/!error_code.has_value());

  if (error_code) {
    CD_LOG(ERROR, Feature::FP)
        << "Bluetooth Low Energy Scan Session failed to start with "
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
}

void FastPairScannerImpl::OnDeviceFound(
    device::BluetoothLowEnergyScanSession* scan_session,
    device::BluetoothDevice* device) {
  const std::vector<uint8_t>* service_data =
      device->GetServiceDataForUUID(kFastPairBluetoothUuid);

  if (!service_data) {
    CD_LOG(WARNING, Feature::FP) << "No Fast Pair service data found on device";
    return;
  }

  if (base::Contains(device_address_advertisement_data_map_,
                     device->GetAddress())) {
    CD_LOG(INFO, Feature::FP)
        << __func__ << ": Ignoring found device because it was already found.";
    return;
  }

  FastPairHandshake* handshake =
      FastPairHandshakeLookup::GetInstance()->Get(device->GetAddress());

  if (handshake) {
    CD_LOG(INFO, Feature::FP)
        << __func__
        << ": We have an active handshake for this device, which "
           "means we never 'lost' it. We ignore this event.";
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

  if (!service_data || service_data->empty())
    return;

  // If the advertisement data we have received does not pertain to a device
  // we have seen already from the scanner, or if the advertisement data for
  // a device we have already seen is not new, then early return and do not
  // notify observers or add data to the device address advertisement data map.
  if (!base::Contains(device_address_advertisement_data_map_, device_address) ||
      base::Contains(device_address_advertisement_data_map_[device_address],
                     *service_data)) {
    return;
  }

  // TODO(b/219600346): Handle Subsequent pair service data changing more
  // robustly. During Subsequent pair, the service data can change during
  // handshake--we can differentiate this from other pairing scenarios by
  // checking that the service data is the same size. Don't notify observers in
  // this case.
  if (!device_address_advertisement_data_map_[device_address].empty() &&
      (device_address_advertisement_data_map_[device_address]
           .rbegin()
           ->size() == service_data->size())) {
    device_address_advertisement_data_map_[device_address].insert(
        *service_data);
    return;
  }

  CD_LOG(INFO, Feature::FP) << __func__ << ": Notifying device found.";
  device_address_advertisement_data_map_[device_address].insert(*service_data);
  NotifyDeviceFound(device);
}

void FastPairScannerImpl::DeviceRemoved(device::BluetoothAdapter* adapter,
                                        device::BluetoothDevice* device) {
  device_address_advertisement_data_map_.erase(device->GetAddress());
}

void FastPairScannerImpl::DevicePairedChanged(device::BluetoothAdapter* adapter,
                                              device::BluetoothDevice* device,
                                              bool new_paired_status) {
  device_address_advertisement_data_map_.erase(device->GetAddress());
}

void FastPairScannerImpl::NotifyDeviceFound(device::BluetoothDevice* device) {
  auto it = ble_address_to_classic_.find(device->GetAddress());

  if (it != ble_address_to_classic_.end()) {
    device::BluetoothDevice* classic_device = adapter_->GetDevice(it->second);

    if (classic_device && classic_device->IsPaired()) {
      CD_LOG(INFO, Feature::FP)
          << __func__ << ": Skipping notify for already paired device.";
      return;
    }
  }

  for (auto& observer : observers_)
    observer.OnDeviceFound(device);
}

void FastPairScannerImpl::OnDeviceLost(
    device::BluetoothLowEnergyScanSession* scan_session,
    device::BluetoothDevice* device) {
  FastPairHandshakeLookup::GetInstance()->Erase(device->GetAddress());
  device_address_advertisement_data_map_.erase(device->GetAddress());

  for (auto& observer : observers_)
    observer.OnDeviceLost(device);
}

void FastPairScannerImpl::OnDevicePaired(scoped_refptr<Device> device) {
  CD_LOG(INFO, Feature::FP) << __func__ << ": device: " << device;
  if (device->classic_address()) {
    ble_address_to_classic_[device->ble_address()] =
        device->classic_address().value();
  }
}

}  // namespace quick_pair
}  // namespace ash
