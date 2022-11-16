// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/scanning/fast_pair/fast_pair_discoverable_scanner_impl.h"

#include <cstdint>
#include <memory>
#include <vector>

#include "ash/quick_pair/common/constants.h"
#include "ash/quick_pair/common/device.h"
#include "ash/quick_pair/common/fast_pair/fast_pair_decoder.h"
#include "ash/quick_pair/common/logging.h"
#include "ash/quick_pair/common/pair_failure.h"
#include "ash/quick_pair/common/protocol.h"
#include "ash/quick_pair/repository/fast_pair_repository.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/check.h"
#include "base/containers/flat_set.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string_util.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/services/quick_pair/quick_pair_process.h"
#include "chromeos/ash/services/quick_pair/quick_pair_process_manager.h"
#include "device/bluetooth//bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_device.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace {

constexpr char kNearbyShareModelId[] = "fc128e";
constexpr int kMaxParseModelIdRetryCount = 5;

bool IsValidDeviceType(const nearby::fastpair::Device& device) {
  // TODO: Filter out based on solidified Fast Pair configuration list once
  // available.
  return device.device_type() == nearby::fastpair::DeviceType::HEADPHONES ||
         device.device_type() == nearby::fastpair::DeviceType::SPEAKER ||
         device.device_type() ==
             nearby::fastpair::DeviceType::TRUE_WIRELESS_HEADPHONES ||
         device.device_type() ==
             nearby::fastpair::DeviceType::DEVICE_TYPE_UNSPECIFIED;
}

bool IsSupportedNotificationType(const nearby::fastpair::Device& device) {
  // We only allow-list notification types that should trigger a pairing
  // notification, since we currently only support pairing. We include
  // NOTIFICATION_TYPE_UNSPECIFIED to handle the case where a Provider is
  // advertising incorrectly and conservatively allow it to show a notification,
  // matching Android behavior.
  return device.notification_type() == nearby::fastpair::NotificationType::
                                           NOTIFICATION_TYPE_UNSPECIFIED ||
         device.notification_type() ==
             nearby::fastpair::NotificationType::FAST_PAIR ||
         device.notification_type() ==
             nearby::fastpair::NotificationType::FAST_PAIR_ONE;
}

}  // namespace

namespace ash {
namespace quick_pair {

// static
FastPairDiscoverableScannerImpl::Factory*
    FastPairDiscoverableScannerImpl::Factory::g_test_factory_ = nullptr;

// static
std::unique_ptr<FastPairDiscoverableScanner>
FastPairDiscoverableScannerImpl::Factory::Create(
    scoped_refptr<FastPairScanner> scanner,
    scoped_refptr<device::BluetoothAdapter> adapter,
    DeviceCallback found_callback,
    DeviceCallback lost_callback) {
  if (g_test_factory_) {
    return g_test_factory_->CreateInstance(
        std::move(scanner), std::move(adapter), std::move(found_callback),
        std::move(lost_callback));
  }

  return base::WrapUnique(new FastPairDiscoverableScannerImpl(
      std::move(scanner), std::move(adapter), std::move(found_callback),
      std::move(lost_callback)));
}

// static
void FastPairDiscoverableScannerImpl::Factory::SetFactoryForTesting(
    Factory* g_test_factory) {
  g_test_factory_ = g_test_factory;
}

FastPairDiscoverableScannerImpl::Factory::~Factory() = default;

FastPairDiscoverableScannerImpl::FastPairDiscoverableScannerImpl(
    scoped_refptr<FastPairScanner> scanner,
    scoped_refptr<device::BluetoothAdapter> adapter,
    DeviceCallback found_callback,
    DeviceCallback lost_callback)
    : scanner_(std::move(scanner)),
      adapter_(std::move(adapter)),
      found_callback_(std::move(found_callback)),
      lost_callback_(std::move(lost_callback)) {
  observation_.Observe(scanner_.get());
  NetworkHandler::Get()->network_state_handler()->AddObserver(this, FROM_HERE);
}

FastPairDiscoverableScannerImpl::~FastPairDiscoverableScannerImpl() {
  NetworkHandler::Get()->network_state_handler()->RemoveObserver(this,
                                                                 FROM_HERE);
}

void FastPairDiscoverableScannerImpl::OnDeviceFound(
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

  QP_LOG(INFO) << __func__ << ": Attempting to get model ID";
  quick_pair_process::GetHexModelIdFromServiceData(
      *fast_pair_service_data,
      base::BindOnce(&FastPairDiscoverableScannerImpl::OnModelIdRetrieved,
                     weak_pointer_factory_.GetWeakPtr(), device->GetAddress()),
      base::BindOnce(&FastPairDiscoverableScannerImpl::OnUtilityProcessStopped,
                     weak_pointer_factory_.GetWeakPtr(), device->GetAddress()));
}

void FastPairDiscoverableScannerImpl::OnModelIdRetrieved(
    const std::string& address,
    const absl::optional<std::string>& model_id) {
  auto it = model_id_parse_attempts_.find(address);

  // If there's no entry in the map, the device was lost while parsing.
  if (it == model_id_parse_attempts_.end()) {
    QP_LOG(WARNING)
        << __func__
        << ": Returning early because device as lost while parsing.";
    return;
  }

  model_id_parse_attempts_.erase(it);

  if (!model_id) {
    QP_LOG(INFO) << __func__
                 << ": Returning early because no model id was parsed.";
    return;
  }

  // The Nearby Share feature advertises under the Fast Pair Service Data UUID
  // and uses a reserved model ID to enable their 'fast initiation' scenario.
  // We must detect this instance and ignore these advertisements since they
  // do not correspond to Fast Pair devices that are open to pairing.
  if (base::EqualsCaseInsensitiveASCII(model_id.value(), kNearbyShareModelId))
    return;

  FastPairRepository::Get()->GetDeviceMetadata(
      *model_id,
      base::BindOnce(
          &FastPairDiscoverableScannerImpl::OnDeviceMetadataRetrieved,
          weak_pointer_factory_.GetWeakPtr(), address, model_id.value()));
}

void FastPairDiscoverableScannerImpl::OnDeviceMetadataRetrieved(
    const std::string& address,
    const std::string model_id,
    DeviceMetadata* device_metadata,
    bool has_retryable_error) {
  if (has_retryable_error) {
    pending_devices_address_to_model_id_[address] = model_id;
    QP_LOG(WARNING) << __func__
                    << ": Could not retrieve metadata for id: " << model_id
                    << ". Waiting for network change before retry.";
    return;
  }

  if (!device_metadata) {
    QP_LOG(WARNING) << __func__
                    << ": No metadata available for id: " << model_id
                    << ". Ignoring this advertisement";
    return;
  }

  // Ignore advertisements that aren't for Fast Pair but leverage the service
  // UUID.
  if (!IsValidDeviceType(device_metadata->GetDetails())) {
    QP_LOG(WARNING)
        << __func__
        << ": Invalid device type for Fast Pair. Ignoring this advertisement";
    return;
  }

  // Ignore advertisements for unsupported notification types, such as
  // APP_LAUNCH which should launch a companion app instead of beginning Fast
  // Pair.
  if (!IsSupportedNotificationType(device_metadata->GetDetails())) {
    QP_LOG(WARNING) << __func__
                    << ": Unsupported notification type for Fast Pair. "
                       "Ignoring this advertisement";
    return;
  }

  auto device = base::MakeRefCounted<Device>(model_id, address,
                                             Protocol::kFastPairInitial);

  QP_LOG(INFO) << __func__ << ": Id: " << model_id;

  NotifyDeviceFound(std::move(device));
}

void FastPairDiscoverableScannerImpl::NotifyDeviceFound(
    scoped_refptr<Device> device) {
  device::BluetoothDevice* classic_device =
      device->classic_address().has_value()
          ? adapter_->GetDevice(device->classic_address().value())
          : nullptr;

  if (classic_device && classic_device->IsPaired()) {
    QP_LOG(ERROR) << __func__
                  << ": A discoverable advertisement "
                     "was notified for a paired classic device.";
    return;
  }

  device::BluetoothDevice* ble_device =
      adapter_->GetDevice(device->ble_address);

  if (ble_device && ble_device->IsPaired()) {
    QP_LOG(ERROR) << __func__
                  << ": A discoverable advertisement "
                     "was notified for a paired BLE device.";
    return;
  }

  // TODO(b/242100708): We currently have no way to tell if a device in pairing
  // mode is already paired to the Chromebook; the BLE device has no information
  // on the pairing state of the Classic device.

  QP_LOG(INFO) << __func__ << ": Running found callback";
  notified_devices_[device->ble_address] = device;
  found_callback_.Run(device);
}

void FastPairDiscoverableScannerImpl::OnDeviceLost(
    device::BluetoothDevice* device) {
  QP_LOG(VERBOSE) << __func__ << ": " << device->GetNameForDisplay();

  // No need to retry fetching metadata for devices that are no longer in range.
  pending_devices_address_to_model_id_.erase(device->GetAddress());

  // If we have an in-progress attempt to parse for this device, removing it
  // from this map will ensure the result is ignored.
  model_id_parse_attempts_.erase(device->GetAddress());

  auto it = notified_devices_.find(device->GetAddress());

  // Don't invoke callback if we didn't notify this device.
  if (it == notified_devices_.end())
    return;

  QP_LOG(INFO) << __func__ << ": Running lost callback";
  scoped_refptr<Device> notified_device = it->second;
  notified_devices_.erase(it);
  lost_callback_.Run(std::move(notified_device));
}

void FastPairDiscoverableScannerImpl::OnUtilityProcessStopped(
    const std::string& address,
    QuickPairProcessManager::ShutdownReason shutdown_reason) {
  int current_retry_count = model_id_parse_attempts_[address];
  if (current_retry_count > kMaxParseModelIdRetryCount) {
    QP_LOG(WARNING) << "Failed to parse model id from device more than "
                    << kMaxParseModelIdRetryCount << " times.";
    // Clean up the state here which enables trying again in the future if this
    // device is re-discovered.
    model_id_parse_attempts_.erase(address);
    return;
  }

  // Don't try to parse the model ID again if the device was lost.
  device::BluetoothDevice* device = adapter_->GetDevice(address);
  if (!device) {
    QP_LOG(WARNING) << __func__ << ": Lost device in between parse attempts.";
    model_id_parse_attempts_.erase(address);
    return;
  }

  model_id_parse_attempts_[address] = current_retry_count + 1;

  const std::vector<uint8_t>* fast_pair_service_data =
      device->GetServiceDataForUUID(kFastPairBluetoothUuid);

  QP_LOG(INFO) << __func__ << ": Retrying call to get model ID";
  quick_pair_process::GetHexModelIdFromServiceData(
      *fast_pair_service_data,
      base::BindOnce(&FastPairDiscoverableScannerImpl::OnModelIdRetrieved,
                     weak_pointer_factory_.GetWeakPtr(), address),
      base::BindOnce(&FastPairDiscoverableScannerImpl::OnUtilityProcessStopped,
                     weak_pointer_factory_.GetWeakPtr(), address));
}

void FastPairDiscoverableScannerImpl::DefaultNetworkChanged(
    const NetworkState* network) {
  // Only retry when we have an active connected network.
  if (!network || !network->IsConnectedState()) {
    return;
  }

  auto it = pending_devices_address_to_model_id_.begin();
  while (it != pending_devices_address_to_model_id_.end()) {
    FastPairRepository::Get()->GetDeviceMetadata(
        /*model_id=*/it->second,
        base::BindOnce(
            &FastPairDiscoverableScannerImpl::OnDeviceMetadataRetrieved,
            weak_pointer_factory_.GetWeakPtr(),
            /*address=*/it->first,
            /*model_id=*/it->second));

    pending_devices_address_to_model_id_.erase(it);
  }
}

}  // namespace quick_pair
}  // namespace ash
