// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/pairing/retroactive_pairing_detector_impl.h"

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/session/session_controller.h"
#include "ash/quick_pair/common/constants.h"
#include "ash/quick_pair/common/device.h"
#include "ash/quick_pair/common/protocol.h"
#include "ash/quick_pair/fast_pair_handshake/fast_pair_gatt_service_client_lookup_impl.h"
#include "ash/quick_pair/message_stream/message_stream.h"
#include "ash/quick_pair/repository/fast_pair_repository.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "components/cross_device/logging/logging.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/floss/floss_features.h"

namespace {

bool ShouldBeEnabledForLoginStatus(ash::LoginStatus status) {
  switch (status) {
    case ash::LoginStatus::NOT_LOGGED_IN:
    case ash::LoginStatus::LOCKED:
    case ash::LoginStatus::KIOSK_APP:
    case ash::LoginStatus::GUEST:
    case ash::LoginStatus::PUBLIC:
      return false;
    case ash::LoginStatus::USER:
    case ash::LoginStatus::CHILD:
    default:
      return true;
  }
}

// Enforce a 60 second timeout to discover a device for the retroactive pairing
// scenario to align with Android implementation and adhere to the Fast Pair
// spec where providers can only allow account keys to be written within the
// 60 seconds following classic Bluetooth pairing:
// https://developers.google.com/nearby/fast-pair/specifications/extensions/retroactiveacctkey#RetroactivelyWritingAccountKey
// The device is expected to enforce this requirement, however as mentioned
// above, to align with Android, ChromeOS will include consideration for the
// 60 seconds expected to retroactively write the account key.
constexpr base::TimeDelta kRetroactiveDevicePairingTimeout = base::Seconds(60);

}  // namespace

namespace ash {
namespace quick_pair {

RetroactivePairingDetectorImpl::RetroactivePairingDetectorImpl(
    PairerBroker* pairer_broker,
    MessageStreamLookup* message_stream_lookup)
    : pairer_broker_(pairer_broker),
      message_stream_lookup_(message_stream_lookup) {
  // If there is no signed in user, don't enabled the retroactive pairing
  // scenario, so don't initiate any objects or observations, but store the
  // pointers in the case that we get logged in later on.
  if (!ShouldBeEnabledForLoginStatus(
          Shell::Get()->session_controller()->login_status())) {
    CD_LOG(INFO, Feature::FP)
        << __func__
        << ": No logged in user to enable retroactive pairing scenario";

    // Observe log in events in the case the login was delayed.
    shell_observation_.Observe(Shell::Get()->session_controller());
    return;
  }

  // If we get to this point in the constructor, it means that the user is
  // logged in to enable this scenario, so we can being our observations. If we
  // get any log in events, we know to ignore them, since we already
  // instantiated our retroactive pairing detector.
  retroactive_pairing_detector_instatiated_ = true;

  device::BluetoothAdapterFactory::Get()->GetAdapter(
      base::BindOnce(&RetroactivePairingDetectorImpl::OnGetAdapter,
                     weak_ptr_factory_.GetWeakPtr()));

  message_stream_lookup_observation_.Observe(message_stream_lookup_.get());
  pairer_broker_observation_.Observe(pairer_broker_.get());
}

void RetroactivePairingDetectorImpl::OnLoginStatusChanged(
    LoginStatus login_status) {
  if (!ShouldBeEnabledForLoginStatus(login_status) || !pairer_broker_ ||
      !message_stream_lookup_ || retroactive_pairing_detector_instatiated_) {
    return;
  }

  CD_LOG(VERBOSE, Feature::FP)
      << __func__
      << ": Logged in user, instantiate retroactive pairing scenario.";

  retroactive_pairing_detector_instatiated_ = true;

  device::BluetoothAdapterFactory::Get()->GetAdapter(
      base::BindOnce(&RetroactivePairingDetectorImpl::OnGetAdapter,
                     weak_ptr_factory_.GetWeakPtr()));

  message_stream_lookup_observation_.Observe(message_stream_lookup_.get());
  pairer_broker_observation_.Observe(pairer_broker_.get());
}

void RetroactivePairingDetectorImpl::OnGetAdapter(
    scoped_refptr<device::BluetoothAdapter> adapter) {
  adapter_ = adapter;
  adapter_observation_.Reset();
  adapter_observation_.Observe(adapter_.get());
}

RetroactivePairingDetectorImpl::~RetroactivePairingDetectorImpl() {
  // Remove any observation of remaining MessageStreams.
  for (auto it = message_streams_.begin(); it != message_streams_.end(); it++) {
    it->second->RemoveObserver(this);
  }
}

void RetroactivePairingDetectorImpl::AddObserver(
    RetroactivePairingDetector::Observer* observer) {
  observers_.AddObserver(observer);
}

void RetroactivePairingDetectorImpl::RemoveObserver(
    RetroactivePairingDetector::Observer* observer) {
  observers_.RemoveObserver(observer);
}

void RetroactivePairingDetectorImpl::OnDevicePaired(
    scoped_refptr<Device> device) {
  // The classic address is assigned to the Device during the
  // initial Fast Pair pairing protocol and if it doesn't exist,
  // then it wasn't properly paired during initial Fast Pair
  // pairing.
  if (!device->classic_address()) {
    return;
  }

  // The Bluetooth Adapter system event `DevicePairedChanged` fires before
  // Fast Pair's `OnDevicePaired`, and a Fast Pair pairing is expected to have
  // both events. If a device is Fast Paired, it is already inserted in the
  // |potential_retroactive_addresses_| in `DevicePairedChanged`; we need to
  // remove it to prevent a false positive.
  if (base::Contains(potential_retroactive_addresses_,
                     device->classic_address().value())) {
    CD_LOG(VERBOSE, Feature::FP)
        << __func__
        << ": encountered a false positive for a potential retroactive pairing "
           "device. Removing device at address = "
        << device->classic_address().value();
    RemoveDeviceInformation(device->classic_address().value());
    return;
  }
  // With the introduction of BLE Fast Pair devices, some devices could be
  // paired with their BLE address. Check BLE address for false positives as
  // well.
  if (ash::features::IsFastPairKeyboardsEnabled() &&
      base::Contains(potential_retroactive_addresses_, device->ble_address())) {
    CD_LOG(VERBOSE, Feature::FP)
        << __func__
        << ": encountered a false positive for a potential retroactive pairing "
           "device. Removing device at address = "
        << device->ble_address();
    RemoveDeviceInformation(device->ble_address());
    return;
  }
}

void RetroactivePairingDetectorImpl::DevicePairedChanged(
    device::BluetoothAdapter* adapter,
    device::BluetoothDevice* device,
    bool new_paired_status) {
  CD_LOG(VERBOSE, Feature::FP)
      << __func__ << ": " << device->GetNameForDisplay()
      << " new_paired_status=" << (new_paired_status ? "paired" : "not paired");
  // This event fires whenever a device pairing has changed with the adapter.
  // If the |new_paired_status| is false, it means a device was unpaired with
  // the adapter, so we early return since it would not be a device to
  // retroactively pair to.
  if (!new_paired_status) {
    return;
  }

  // Both classic paired and Fast paired devices call this function, so we
  // have to add the device to |potential_retroactive_addresses_|. We expect
  // devices paired via Fast Pair to always call `OnDevicePaired` after calling
  // this function, which will remove the device from
  // |potential_retroactive_addresses_|.
  const std::string& classic_address = device->GetAddress();
  potential_retroactive_addresses_.insert(classic_address);
  AddDevicePairingInformation(classic_address);

  // In order to confirm that this device is a retroactive pairing, we need to
  // first check if it has already been saved to the user's account. If it has
  // already been saved, we don't want to prompt the user to save a device
  // again.
  FastPairRepository::Get()->IsDeviceSavedToAccount(
      classic_address,
      base::BindOnce(&RetroactivePairingDetectorImpl::AttemptRetroactivePairing,
                     weak_ptr_factory_.GetWeakPtr(), classic_address));
}

void RetroactivePairingDetectorImpl::AttemptRetroactivePairing(
    const std::string& classic_address,
    bool is_device_saved_to_account) {
  // This check handles the case where the request for checked if the device is
  // saved takes longer than expected. We register `AttemptRetroactivePairing`
  // as a callback for when this request completes, but it gets called after
  // we get the call to `OnDevicePaired`, which removes the device information.
  // If the device is removed via `OnDevicePaired`, this indicated a Fast Pair
  // pairing event, in which case we will never show a retroactive pairing
  // notification, so we can stop the flow here for this device.
  if (!base::Contains(potential_retroactive_addresses_, classic_address)) {
    CD_LOG(VERBOSE, Feature::FP)
        << __func__ << ": device at " << classic_address
        << ": was removed before call to Footprints completed";
    return;
  }

  if (is_device_saved_to_account) {
    CD_LOG(INFO, Feature::FP)
        << __func__ << ": device already saved to user's account";
    RemoveDeviceInformation(classic_address);
    return;
  }

  device::BluetoothDevice* device = adapter_->GetDevice(classic_address);
  if (!device) {
    CD_LOG(WARNING, Feature::FP)
        << __func__ << ": Lost device to potentially retroactively pair to.";
    RemoveDeviceInformation(classic_address);
    return;
  }

  CD_LOG(VERBOSE, Feature::FP) << __func__ << ": device = " << classic_address;

  // For BLE devices, check it supports Fast Pair. Then, since the message
  // stream is optional for BLE HIDs, and the BLE address is already known, the
  // only remaining parameter needed is the model ID, which we retrieve via GATT
  // characteristic.
  if (ash::features::IsFastPairHIDEnabled() &&
      // Fast Pair HID only works on Floss.
      floss::features::IsFlossEnabled() &&
      device->GetType() == device::BLUETOOTH_TRANSPORT_LE &&
      base::Contains(device->GetUUIDs(), kFastPairBluetoothUuid)) {
    CD_LOG(VERBOSE, Feature::FP)
        << __func__
        << ": BLE fast pair device detected, creating GATT connection";
    CreateGattConnection(device);
    return;
  }

  // Attempt to retrieve a MessageStream instance immediately, if it was
  // already connected.
  MessageStream* message_stream =
      message_stream_lookup_->GetMessageStream(classic_address);
  if (!message_stream) {
    return;
  }

  message_streams_[classic_address] = message_stream;
  GetModelIdAndAddressFromMessageStream(classic_address, message_stream);
}

void RetroactivePairingDetectorImpl::CreateGattConnection(
    device::BluetoothDevice* device) {
  auto* fast_pair_gatt_service_client =
      FastPairGattServiceClientLookup::GetInstance()->Get(device);

  if (fast_pair_gatt_service_client) {
    if (fast_pair_gatt_service_client->IsConnected()) {
      CD_LOG(VERBOSE, Feature::FP)
          << __func__
          << ": Reusing existing GATT service client to retrieve model ID";
      fast_pair_gatt_service_client->ReadModelIdAsync(
          base::BindOnce(&RetroactivePairingDetectorImpl::OnReadModelId,
                         weak_ptr_factory_.GetWeakPtr(), device->GetAddress()));
      return;
    } else {
      // If the previous gatt service client did not connect successfully
      // or is no longer connected, erase it before attempting to create a new
      // gatt connection for the device.
      FastPairGattServiceClientLookup::GetInstance()->Erase(device);
    }
  }

  CD_LOG(VERBOSE, Feature::FP)
      << __func__ << ": Creating new GATT service client to retrieve model ID";

  FastPairGattServiceClientLookup::GetInstance()->Create(
      adapter_, device,
      base::BindOnce(
          &RetroactivePairingDetectorImpl::OnGattClientInitializedCallback,
          weak_ptr_factory_.GetWeakPtr(), device->GetAddress()));
}

void RetroactivePairingDetectorImpl::OnGattClientInitializedCallback(
    const std::string& address,
    std::optional<PairFailure> failure) {
  if (failure) {
    CD_LOG(WARNING, Feature::FP)
        << __func__
        << ": Failed to initialize GATT service client with failure = "
        << failure.value();
    return;
  }

  device::BluetoothDevice* device = adapter_->GetDevice(address);
  if (!device) {
    CD_LOG(WARNING, Feature::FP)
        << __func__ << ": Lost device to potentially retroactively pair to.";
    return;
  }

  auto* fast_pair_gatt_service_client =
      FastPairGattServiceClientLookup::GetInstance()->Get(device);

  if (!fast_pair_gatt_service_client ||
      !fast_pair_gatt_service_client->IsConnected()) {
    CD_LOG(WARNING, Feature::FP) << __func__
                                 << ": Fast Pair Gatt Service Client failed to "
                                    "be created or is no longer connected.";
    FastPairGattServiceClientLookup::GetInstance()->Erase(device);
    return;
  }

  CD_LOG(VERBOSE, Feature::FP) << __func__
                               << ": Fast Pair GATT service client initialized "
                                  "successfully. Reading Model ID.";

  fast_pair_gatt_service_client->ReadModelIdAsync(
      base::BindOnce(&RetroactivePairingDetectorImpl::OnReadModelId,
                     weak_ptr_factory_.GetWeakPtr(), device->GetAddress()));
}

void RetroactivePairingDetectorImpl::OnReadModelId(
    const std::string& address,
    std::optional<device::BluetoothGattService::GattErrorCode> error_code,
    const std::vector<uint8_t>& value) {
  if (error_code) {
    CD_LOG(WARNING, Feature::FP)
        << __func__ << ": Failed to read model ID with failure = "
        << static_cast<uint32_t>(error_code.value());
    return;
  }

  if (value.size() != 3) {
    CD_LOG(WARNING, Feature::FP) << __func__ << ": model ID malformed.";
    return;
  }

  std::string model_id;
  for (auto byte : value) {
    model_id.append(base::StringPrintf("%02X", byte));
  }

  CD_LOG(INFO, Feature::FP) << __func__ << ": Model ID " << model_id
                            << " found for device " << address;
  NotifyDeviceFound(model_id, address, address);
}

void RetroactivePairingDetectorImpl::OnMessageStreamConnected(
    const std::string& device_address,
    MessageStream* message_stream) {
  CD_LOG(VERBOSE, Feature::FP) << __func__ << ":" << device_address;
  if (!message_stream) {
    return;
  }

  if (!base::Contains(potential_retroactive_addresses_, device_address)) {
    return;
  }

  message_streams_[device_address] = message_stream;
  GetModelIdAndAddressFromMessageStream(device_address, message_stream);
}

void RetroactivePairingDetectorImpl::AddDevicePairingInformation(
    const std::string& device_address) {
  CD_LOG(VERBOSE, Feature::FP) << __func__;

  // There is potential for the device at |device_address| to already be in
  // the map (in the case of repairing for example). If it is already in the
  // map, update the timeout with the new timestamp. If it isn't already in
  // the map, create a value with default empty values, and add the expiry
  // timeout.
  device_pairing_information_[device_address].expiry_timestamp =
      base::Time::Now() + kRetroactiveDevicePairingTimeout;

  // Anytime |device_pairing_information_| is updated, parse list to remove
  // expired devices.
  RemoveExpiredDevicesFromStoredDeviceData();
}

void RetroactivePairingDetectorImpl::GetModelIdAndAddressFromMessageStream(
    const std::string& device_address,
    MessageStream* message_stream) {
  DCHECK(message_stream);

  // The device at |device_address| is expected to be added in
  // `AddDevicePairingInformation` once discovered.
  DCHECK(base::Contains(device_pairing_information_, device_address));

  // If the MessageStream is immediately available and |DevicePairedChanged|
  // fires before FastPair's |OnDevicePaired|, it might be possible for us to
  // find a false positive for a retroactive pairing scenario which we mitigate
  // here.
  if (!base::Contains(potential_retroactive_addresses_, device_address)) {
    return;
  }

  // Iterate over messages for ble address and model id, which is what we
  // need for retroactive pairing.
  for (auto& message : message_stream->messages()) {
    if (message->is_model_id()) {
      device_pairing_information_[device_address].model_id =
          message->get_model_id();
    } else if (message->is_ble_address_update()) {
      device_pairing_information_[device_address].ble_address =
          message->get_ble_address_update();
    }
  }

  // If we don't have model id and ble address for device, then we will add
  // ourselves as an observer and wait for these messages to come in. There is
  // a possibility that they will not come in if the device does not
  // support retroactive pairing.
  if (device_pairing_information_[device_address].model_id.empty() ||
      device_pairing_information_[device_address].ble_address.empty()) {
    CD_LOG(VERBOSE, Feature::FP)
        << __func__ << ": BLE address = "
        << (device_pairing_information_[device_address].ble_address.empty()
                ? "empty"
                : device_pairing_information_[device_address].ble_address)
        << " model ID = "
        << (device_pairing_information_[device_address].model_id.empty()
                ? "empty"
                : device_pairing_information_[device_address].model_id)
        << " observing Message Stream for future messages for device = "
        << device_address;
    message_stream->AddObserver(this);
    return;
  }

  // At this point, we have both the model id and BLE address for the device,
  // but we check if it has reached its expiry timeout. If so, we do not
  // notify of the scenario being detected. `CheckAndRemoveIfDeviceExpired`
  // will remove corresponding device information if it has expired.
  if (CheckAndRemoveIfDeviceExpired(device_address)) {
    return;
  }

  NotifyDeviceFound(device_pairing_information_[device_address].model_id,
                    device_pairing_information_[device_address].ble_address,
                    device_address);
}

bool RetroactivePairingDetectorImpl::CheckAndRemoveIfDeviceExpired(
    const std::string& device_address) {
  if (base::Time::Now() >=
      device_pairing_information_[device_address].expiry_timestamp) {
    CD_LOG(VERBOSE, Feature::FP)
        << __func__ << ": device at " << device_address
        << " has exceeded the time allotted for detecting "
           "retroactive scenario. Removing device information.";
    RemoveDeviceInformation(device_address);
    return true;
  }

  return false;
}

void RetroactivePairingDetectorImpl::OnModelIdMessage(
    const std::string& device_address,
    const std::string& model_id) {
  CD_LOG(VERBOSE, Feature::FP) << __func__ << ": model id = " << model_id
                               << "for device = " << device_address;
  device_pairing_information_[device_address].model_id = model_id;
  CheckPairingInformation(device_address);
}

void RetroactivePairingDetectorImpl::OnBleAddressUpdateMessage(
    const std::string& device_address,
    const std::string& ble_address) {
  CD_LOG(VERBOSE, Feature::FP) << __func__ << ": ble address " << ble_address
                               << " for device = " << device_address;
  device_pairing_information_[device_address].ble_address = ble_address;
  CheckPairingInformation(device_address);
}

void RetroactivePairingDetectorImpl::CheckPairingInformation(
    const std::string& device_address) {
  // The device at |device_address| is expected to be added in
  // `AddDevicePairingInformation` once discovered.
  DCHECK(base::Contains(device_pairing_information_, device_address));

  // If the MessageStream is immediately available and |DevicePairedChanged|
  // fires before FastPair's |OnDevicePaired|, it might be possible for us to
  // find a false positive for a retroactive pairing scenario which we mitigate
  // here. Also check if the device has expired for detecting scenario, if so
  // do not continue. `CheckAndRemoveIfDeviceExpired` will remove device
  // information if it has expired.
  if (!base::Contains(potential_retroactive_addresses_, device_address) ||
      CheckAndRemoveIfDeviceExpired(device_address)) {
    return;
  }

  if (device_pairing_information_[device_address].model_id.empty() ||
      device_pairing_information_[device_address].ble_address.empty()) {
    CD_LOG(VERBOSE, Feature::FP)
        << __func__
        << ": don't have both model id and ble address for device = "
        << device_address;
    return;
  }

  NotifyDeviceFound(device_pairing_information_[device_address].model_id,
                    device_pairing_information_[device_address].ble_address,
                    device_address);
}

void RetroactivePairingDetectorImpl::OnDisconnected(
    const std::string& device_address) {
  CD_LOG(VERBOSE, Feature::FP) << __func__;
  message_streams_[device_address]->RemoveObserver(this);
  message_streams_.erase(device_address);
}

void RetroactivePairingDetectorImpl::OnMessageStreamDestroyed(
    const std::string& device_address) {
  CD_LOG(VERBOSE, Feature::FP) << __func__;
  message_streams_[device_address]->RemoveObserver(this);
  message_streams_.erase(device_address);
}

void RetroactivePairingDetectorImpl::NotifyDeviceFound(
    const std::string& model_id,
    const std::string& ble_address,
    const std::string& classic_address) {
  CD_LOG(INFO, Feature::FP) << __func__;

  // Before we notify that the device is found for retroactive pairing, we
  // should check if the user is opted in to saving devices to their account.
  // The reason why we check this every time we want to notify a device is found
  // rather than having the user's opt-in status determine whether or not the
  // retroactive pairing scenario is instantiated is because the user might be
  // opted out when the user initially logs in to the Chromebook (when this
  // class is created), but then opted-in later one, and then unable to save
  // devices to their account, or vice versa. By checking every time we want
  // to notify a device is found, we can accurately reflect a user's status
  // in the moment. This is flagged on whether the user has the Fast Pair
  // Saved Devices flag enabled.
  if (features::IsFastPairSavedDevicesEnabled() &&
      features::IsFastPairSavedDevicesStrictOptInEnabled()) {
    FastPairRepository::Get()->CheckOptInStatus(
        base::BindOnce(&RetroactivePairingDetectorImpl::OnCheckOptInStatus,
                       weak_ptr_factory_.GetWeakPtr(), model_id, ble_address,
                       classic_address));
    return;
  }

  // If the SavedDevices flag is not enabled, we don't have to check opt in
  // status and can move forward with verifying the device found.
  VerifyDeviceFound(model_id, ble_address, classic_address);
}

void RetroactivePairingDetectorImpl::OnCheckOptInStatus(
    const std::string& model_id,
    const std::string& ble_address,
    const std::string& classic_address,
    nearby::fastpair::OptInStatus status) {
  CD_LOG(INFO, Feature::FP) << __func__;

  if (status != nearby::fastpair::OptInStatus::STATUS_OPTED_IN) {
    CD_LOG(INFO, Feature::FP)
        << __func__
        << ": User is not opted in to save devices to their account";
    RemoveDeviceInformation(classic_address);
    return;
  }

  VerifyDeviceFound(model_id, ble_address, classic_address);
}

void RetroactivePairingDetectorImpl::VerifyDeviceFound(
    const std::string& model_id,
    const std::string& ble_address,
    const std::string& classic_address) {
  CD_LOG(INFO, Feature::FP) << __func__;

  device::BluetoothDevice* bluetooth_device =
      adapter_->GetDevice(classic_address);
  if (!bluetooth_device) {
    CD_LOG(WARNING, Feature::FP)
        << __func__ << ": Lost device to potentially retroactively pair to.";
    RemoveDeviceInformation(classic_address);
    return;
  }

  auto device = base::MakeRefCounted<Device>(model_id, ble_address,
                                             Protocol::kFastPairRetroactive);
  device->set_classic_address(classic_address);
  device->set_display_name(bluetooth_device->GetName());
  CD_LOG(INFO, Feature::FP)
      << __func__ << ": Found device for Retroactive Pairing " << device;

  for (auto& observer : observers_) {
    observer.OnRetroactivePairFound(device);
  }

  DCHECK(device->classic_address());
  RemoveDeviceInformation(device->classic_address().value());
}

void RetroactivePairingDetectorImpl::RemoveDeviceInformation(
    const std::string& device_address) {
  CD_LOG(VERBOSE, Feature::FP) << __func__ << ": device = " << device_address;
  RemoveDeviceInformationHelper(device_address);

  // Anytime |device_pairing_information_| is updated, parse list to remove
  // expired devices.
  RemoveExpiredDevicesFromStoredDeviceData();
}

void RetroactivePairingDetectorImpl::RemoveDeviceInformationHelper(
    const std::string& device_address) {
  CD_LOG(INFO, Feature::FP) << __func__;
  potential_retroactive_addresses_.erase(device_address);
  device_pairing_information_.erase(device_address);

  // We can potentially get to a state where we need to RemoveDeviceInformation
  // before the MessageStreams are observed, connected, and/or added to our
  // list here if we get a false positive instance of a potential retroactive
  // pairing device.
  if (base::Contains(message_streams_, device_address)) {
    message_streams_[device_address]->RemoveObserver(this);
    message_streams_.erase(device_address);
  }
}

void RetroactivePairingDetectorImpl::
    RemoveExpiredDevicesFromStoredDeviceData() {
  // If the RetroactivePairingDetector never receives the model id or
  // BLE address from the MessageStream, it will not be removed in
  // `CheckPairingInformation` if it has exceeded the allotted time for
  // detecting the scenario (kDetectRetroactiveScenarioTimeout). We clean up
  // these devices here.
  std::vector<std::string> devices_to_remove;
  for (auto it = device_pairing_information_.begin();
       it != device_pairing_information_.end(); ++it) {
    if (base::Time::Now() >= it->second.expiry_timestamp) {
      devices_to_remove.push_back(it->first);
    }
  }

  for (const std::string& device_address : devices_to_remove) {
    CD_LOG(VERBOSE, Feature::FP)
        << __func__ << ": Removing device at " << device_address
        << "that has exceeded the time allotted for detecting "
           "retroactive scenario.";
    RemoveDeviceInformationHelper(device_address);
  }
}

void RetroactivePairingDetectorImpl::OnPairFailure(scoped_refptr<Device> device,
                                                   PairFailure failure) {}

void RetroactivePairingDetectorImpl::OnAccountKeyWrite(
    scoped_refptr<Device> device,
    std::optional<AccountKeyFailure> error) {}

}  // namespace quick_pair
}  // namespace ash
