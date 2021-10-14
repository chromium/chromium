// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/pairing/retroactive_pairing_detector_impl.h"

#include "ash/quick_pair/common/constants.h"
#include "ash/quick_pair/common/device.h"
#include "ash/quick_pair/common/logging.h"
#include "ash/quick_pair/common/protocol.h"
#include "ash/quick_pair/repository/fast_pair_repository.h"
#include "ash/services/quick_pair/public/cpp/account_key_filter.h"
#include "ash/services/quick_pair/public/cpp/not_discoverable_advertisement.h"
#include "ash/services/quick_pair/quick_pair_process.h"
#include "ash/services/quick_pair/quick_pair_process_manager.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/containers/contains.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/bluetooth_device.h"

namespace {

constexpr char kNearbyShareModelId[] = "fc128e";

}  // namespace

namespace ash {
namespace quick_pair {

RetroactivePairingDetectorImpl::RetroactivePairingDetectorImpl(
    PairerBroker* pairer_broker) {
  device::BluetoothAdapterFactory::Get()->GetAdapter(
      base::BindOnce(&RetroactivePairingDetectorImpl::OnGetAdapter,
                     weak_ptr_factory_.GetWeakPtr()));

  pairer_broker_observation_.Observe(pairer_broker);
}

void RetroactivePairingDetectorImpl::OnGetAdapter(
    scoped_refptr<device::BluetoothAdapter> adapter) {
  adapter_ = adapter;
  adapter_observation_.Observe(adapter_.get());
}

RetroactivePairingDetectorImpl::~RetroactivePairingDetectorImpl() = default;

void RetroactivePairingDetectorImpl::AddObserver(
    RetroactivePairingDetector::Observer* observer) {
  observers_.AddObserver(observer);
}

void RetroactivePairingDetectorImpl::RemoveObserver(
    RetroactivePairingDetector::Observer* observer) {
  observers_.RemoveObserver(observer);
}

void RetroactivePairingDetectorImpl::NotifyDeviceFound(
    const std::string& model_id,
    const std::string& device_address) {
  device::BluetoothDevice* bluetooth_device =
      adapter_->GetDevice(device_address);
  if (!bluetooth_device) {
    QP_LOG(WARNING) << __func__
                    << ": Lost device to potentially retroactively pair to.";
    return;
  }

  auto device = base::MakeRefCounted<Device>(
      model_id, bluetooth_device->GetAddress(), Protocol::kFastPairRetroactive);
  QP_LOG(INFO) << __func__ << ": Found device for Retroactive Pairing.";

  for (auto& observer : observers_)
    observer.OnRetroactivePairFound(device);
}

void RetroactivePairingDetectorImpl::OnDevicePaired(
    scoped_refptr<Device> device) {
  // When a device is paired to via Fast Pair, we save the device's bluetooth
  // pairing address here so when we get the the BluetoothAdapter's
  // |DevicePairedChanged| fired, we can determine if it was the one we already
  // have paired to.
  QP_LOG(VERBOSE) << __func__ << ":  Storing Fast Pair device address: "
                  << device->ble_address;
  fast_pair_addresses_.insert(device->ble_address);
}

void RetroactivePairingDetectorImpl::DevicePairedChanged(
    device::BluetoothAdapter* adapter,
    device::BluetoothDevice* device,
    bool new_paired_status) {
  // This event fires whenever a device is paired with the Bluetooth adapter.
  // If the |new_paired_status| is false, it means a device was unpaired with
  // the adapter, so we early return since it would not be a device to
  // retroactively pair to. If the device that was paired to that fires this
  // event is a device we just paired to with Fast Pair, then we early return
  // since it also wouldn't be one to retroactively pair to. We want to only
  // continue our check here if we have a newly paired device that was paired
  // with classic Bluetooth pairing.
  if (!new_paired_status ||
      base::Contains(fast_pair_addresses_, device->GetAddress())) {
    return;
  }

  // We now need to check that this is a Fast Pair capable device. If we have
  // Fast Pair service data, we can continue our check by fetching the model id.
  const std::vector<uint8_t>* fast_pair_service_data =
      device->GetServiceDataForUUID(kFastPairBluetoothUuid);
  if (!fast_pair_service_data || fast_pair_service_data->empty()) {
    QP_LOG(VERBOSE) << __func__ << ": No Fast Pair service data";
    return;
  }

  quick_pair_process::GetHexModelIdFromServiceData(
      *fast_pair_service_data,
      base::BindOnce(&RetroactivePairingDetectorImpl::OnModelIdRetrieved,
                     weak_ptr_factory_.GetWeakPtr(), device->GetAddress()),
      base::BindOnce(&RetroactivePairingDetectorImpl::
                         OnUtilityProcessStoppedOnGetHexModelId,
                     weak_ptr_factory_.GetWeakPtr()));
}

void RetroactivePairingDetectorImpl::OnModelIdRetrieved(
    const std::string& device_address,
    const absl::optional<std::string>& model_id) {
  // To check if this is Fast Pair supported: if there is a model id, then we
  // can fetch the metadata from the repository and check for an
  // antispoofing-key, which if exists, implies a device that could have used
  // Fast Pair to pair (i.e. Fast Pair v2+ devices). If
  // there isn't a model id, then we check the advertisement for an account key
  // filter, and if there is one, we check the repository for a match. If a
  // match exists, it implies a device that could have used Fast Pair to pair
  // (i.e. Fast Pair v2+ devices).
  if (!model_id) {
    QP_LOG(VERBOSE)
        << __func__
        << ": Checking advertisement data since device is missing model id.";
    CheckAdvertisementData(device_address);
    return;
  }

  // The Nearby Share feature advertises under the Fast Pair Service Data UUID
  // and uses a reserved model ID to enable their 'fast initiation' scenario.
  // We must detect this instance and ignore these advertisements since they
  // do not correspond to Fast Pair devices that are open to pairing.
  if (base::EqualsCaseInsensitiveASCII(model_id.value(), kNearbyShareModelId))
    return;

  QP_LOG(VERBOSE)
      << __func__
      << ": Checking repository for metadata since device has model id.";
  FastPairRepository::Get()->GetDeviceMetadata(
      *model_id,
      base::BindOnce(&RetroactivePairingDetectorImpl::OnDeviceMetadataRetrieved,
                     weak_ptr_factory_.GetWeakPtr(), device_address,
                     model_id.value()));
}

void RetroactivePairingDetectorImpl::CheckAdvertisementData(
    const std::string& device_address) {
  device::BluetoothDevice* device = adapter_->GetDevice(device_address);
  if (!device) {
    QP_LOG(VERBOSE) << __func__
                    << ": Lost device to potentially retroactively pair to.";
    return;
  }

  const std::vector<uint8_t>* fast_pair_service_data =
      device->GetServiceDataForUUID(kFastPairBluetoothUuid);
  if (!fast_pair_service_data || fast_pair_service_data->empty()) {
    QP_LOG(VERBOSE) << __func__ << ": No Fast Pair service data";
    return;
  }

  quick_pair_process::ParseNotDiscoverableAdvertisement(
      *fast_pair_service_data,
      base::BindOnce(&RetroactivePairingDetectorImpl::OnAdvertisementParsed,
                     weak_ptr_factory_.GetWeakPtr(), device_address),
      base::BindOnce(&RetroactivePairingDetectorImpl::
                         OnUtilityProcessStoppedOnParseAdvertisement,
                     weak_ptr_factory_.GetWeakPtr()));
}

void RetroactivePairingDetectorImpl::OnAdvertisementParsed(
    const std::string& device_address,
    const absl::optional<NotDiscoverableAdvertisement>& advertisement) {
  if (!advertisement) {
    QP_LOG(VERBOSE) << __func__ << ": Missing NotDiscoverableAdvertisement.";
    return;
  }

  if (!advertisement->show_ui) {
    QP_LOG(VERBOSE) << __func__
                    << ": NotDiscoverableAdvertisement show ui is false.";
    return;
  }

  FastPairRepository::Get()->CheckAccountKeys(
      AccountKeyFilter(advertisement.value()),
      base::BindOnce(
          &RetroactivePairingDetectorImpl::OnAccountKeyFilterCheckResult,
          weak_ptr_factory_.GetWeakPtr(), device_address));
}

void RetroactivePairingDetectorImpl::OnAccountKeyFilterCheckResult(
    const std::string& device_address,
    absl::optional<PairingMetadata> metadata) {
  QP_LOG(VERBOSE) << __func__ << ": Metadata: " << (metadata ? "yes" : "no");
  if (!metadata || !metadata->device_metadata)
    return;

  // Convert the integer model id to uppercase hex string.
  auto& details = metadata->device_metadata->GetDetails();
  std::stringstream model_id_stream;
  model_id_stream << std::uppercase << std::hex << details.id();

  NotifyDeviceFound(model_id_stream.str(), device_address);
}

void RetroactivePairingDetectorImpl::OnDeviceMetadataRetrieved(
    const std::string& device_address,
    const std::string model_id,
    DeviceMetadata* device_metadata) {
  if (!device_metadata)
    return;

  const std::string& public_anti_spoofing_key =
      device_metadata->GetDetails().anti_spoofing_key_pair().public_key();
  if (public_anti_spoofing_key.empty())
    return;

  NotifyDeviceFound(model_id, device_address);
}

void RetroactivePairingDetectorImpl::OnUtilityProcessStoppedOnGetHexModelId(
    QuickPairProcessManager::ShutdownReason shutdown_reason) {
  QP_LOG(WARNING) << __func__ << ": ShutdownReason = " << shutdown_reason;
}

void RetroactivePairingDetectorImpl::
    OnUtilityProcessStoppedOnParseAdvertisement(
        QuickPairProcessManager::ShutdownReason shutdown_reason) {
  QP_LOG(WARNING) << __func__ << ": ShutdownReason = " << shutdown_reason;
}

void RetroactivePairingDetectorImpl::OnPairFailure(scoped_refptr<Device> device,
                                                   PairFailure failure) {}

void RetroactivePairingDetectorImpl::OnAccountKeyWrite(
    scoped_refptr<Device> device,
    absl::optional<AccountKeyFailure> error) {}

}  // namespace quick_pair
}  // namespace ash
