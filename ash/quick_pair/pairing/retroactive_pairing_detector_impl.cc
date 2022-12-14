// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/pairing/retroactive_pairing_detector_impl.h"

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/session/session_controller.h"
#include "ash/quick_pair/common/constants.h"
#include "ash/quick_pair/common/device.h"
#include "ash/quick_pair/common/logging.h"
#include "ash/quick_pair/common/protocol.h"
#include "ash/quick_pair/message_stream/message_stream.h"
#include "ash/quick_pair/repository/fast_pair_repository.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/containers/contains.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/bluetooth_device.h"

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
    QP_LOG(INFO)
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

  message_stream_lookup_observation_.Observe(message_stream_lookup_);
  pairer_broker_observation_.Observe(pairer_broker_);
}

void RetroactivePairingDetectorImpl::OnLoginStatusChanged(
    LoginStatus login_status) {
  if (!ShouldBeEnabledForLoginStatus(login_status) || !pairer_broker_ ||
      !message_stream_lookup_ || retroactive_pairing_detector_instatiated_) {
    return;
  }

  QP_LOG(VERBOSE)
      << __func__
      << ": Logged in user, instantiate retroactive pairing scenario.";

  retroactive_pairing_detector_instatiated_ = true;

  device::BluetoothAdapterFactory::Get()->GetAdapter(
      base::BindOnce(&RetroactivePairingDetectorImpl::OnGetAdapter,
                     weak_ptr_factory_.GetWeakPtr()));

  message_stream_lookup_observation_.Observe(message_stream_lookup_);
  pairer_broker_observation_.Observe(pairer_broker_);
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
  if (!device->classic_address())
    return;

  // The Bluetooth Adapter system event `DevicePairedChanged` fires before
  // Fast Pair's `OnDevicePaired`, and a Fast Pair pairing is expected to have
  // both events. If a device is Fast Paired, it is already inserted in the
  // |potential_retroactive_addresses_| in `DevicePairedChanged`; we need to
  // remove it to prevent a false positive.
  if (base::Contains(potential_retroactive_addresses_,
                     device->classic_address().value())) {
    QP_LOG(VERBOSE)
        << __func__
        << ": encountered a false positive for a potential retroactive pairing "
           "device. Removing device at address = "
        << device->classic_address().value();
    RemoveDeviceInformation(device->classic_address().value());
    return;
  }
}

void RetroactivePairingDetectorImpl::DevicePairedChanged(
    device::BluetoothAdapter* adapter,
    device::BluetoothDevice* device,
    bool new_paired_status) {
  QP_LOG(VERBOSE) << __func__ << ": " << device->GetNameForDisplay()
                  << " new_paired_status="
                  << (new_paired_status ? "paired" : "not paired");
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
  if (is_device_saved_to_account) {
    QP_LOG(INFO) << __func__ << ": device already saved to user's account";
    RemoveDeviceInformation(classic_address);
    return;
  }

  QP_LOG(VERBOSE) << __func__ << ": device = " << classic_address;

  // Attempt to retrieve a MessageStream instance immediately, if it was
  // already connected.
  MessageStream* message_stream =
      message_stream_lookup_->GetMessageStream(classic_address);
  if (!message_stream)
    return;

  message_streams_[classic_address] = message_stream;
  GetModelIdAndAddressFromMessageStream(classic_address, message_stream);
}

void RetroactivePairingDetectorImpl::OnMessageStreamConnected(
    const std::string& device_address,
    MessageStream* message_stream) {
  QP_LOG(VERBOSE) << __func__ << ":" << device_address;
  if (!message_stream)
    return;

  if (!base::Contains(potential_retroactive_addresses_, device_address))
    return;

  message_streams_[device_address] = message_stream;
  GetModelIdAndAddressFromMessageStream(device_address, message_stream);
}

void RetroactivePairingDetectorImpl::GetModelIdAndAddressFromMessageStream(
    const std::string& device_address,
    MessageStream* message_stream) {
  DCHECK(message_stream);
  DCHECK(device_pairing_information_.find(device_address) ==
         device_pairing_information_.end());

  // If the MessageStream is immediately available and |DevicePairedChanged|
  // fires before FastPair's |OnDevicePaired|, it might be possible for us to
  // find a false positive for a retroactive pairing scenario which we mitigate
  // here.
  if (!base::Contains(potential_retroactive_addresses_, device_address))
    return;

  RetroactivePairingInformation info;
  device_pairing_information_[device_address] = info;

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
    QP_LOG(VERBOSE)
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

  NotifyDeviceFound(device_pairing_information_[device_address].model_id,
                    device_pairing_information_[device_address].ble_address,
                    device_address);
}

void RetroactivePairingDetectorImpl::OnModelIdMessage(
    const std::string& device_address,
    const std::string& model_id) {
  QP_LOG(VERBOSE) << __func__ << ": model id = " << model_id
                  << "for device = " << device_address;
  device_pairing_information_[device_address].model_id = model_id;
  CheckPairingInformation(device_address);
}

void RetroactivePairingDetectorImpl::OnBleAddressUpdateMessage(
    const std::string& device_address,
    const std::string& ble_address) {
  QP_LOG(VERBOSE) << __func__ << ": ble address " << ble_address
                  << " for device = " << device_address;
  device_pairing_information_[device_address].ble_address = ble_address;
  CheckPairingInformation(device_address);
}

void RetroactivePairingDetectorImpl::CheckPairingInformation(
    const std::string& device_address) {
  DCHECK(device_pairing_information_.find(device_address) !=
         device_pairing_information_.end());

  // If the MessageStream is immediately available and |DevicePairedChanged|
  // fires before FastPair's |OnDevicePaired|, it might be possible for us to
  // find a false positive for a retroactive pairing scenario which we mitigate
  // here.
  if (!base::Contains(potential_retroactive_addresses_, device_address))
    return;

  if (device_pairing_information_[device_address].model_id.empty() ||
      device_pairing_information_[device_address].ble_address.empty()) {
    QP_LOG(VERBOSE)
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
  QP_LOG(VERBOSE) << __func__;
  message_streams_[device_address]->RemoveObserver(this);
  message_streams_.erase(device_address);
}

void RetroactivePairingDetectorImpl::OnMessageStreamDestroyed(
    const std::string& device_address) {
  QP_LOG(VERBOSE) << __func__;
  message_streams_[device_address]->RemoveObserver(this);
  message_streams_.erase(device_address);
}

void RetroactivePairingDetectorImpl::NotifyDeviceFound(
    const std::string& model_id,
    const std::string& ble_address,
    const std::string& classic_address) {
  QP_LOG(INFO) << __func__;

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
  QP_LOG(INFO) << __func__;

  if (status != nearby::fastpair::OptInStatus::STATUS_OPTED_IN) {
    QP_LOG(INFO) << __func__
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
  QP_LOG(INFO) << __func__;

  device::BluetoothDevice* bluetooth_device =
      adapter_->GetDevice(classic_address);
  if (!bluetooth_device) {
    QP_LOG(WARNING) << __func__
                    << ": Lost device to potentially retroactively pair to.";
    RemoveDeviceInformation(classic_address);
    return;
  }

  auto device = base::MakeRefCounted<Device>(model_id, ble_address,
                                             Protocol::kFastPairRetroactive);
  device->set_classic_address(classic_address);
  device->set_display_name(bluetooth_device->GetName());
  QP_LOG(INFO) << __func__ << ": Found device for Retroactive Pairing "
               << device;

  for (auto& observer : observers_)
    observer.OnRetroactivePairFound(device);

  DCHECK(device->classic_address());
  RemoveDeviceInformation(device->classic_address().value());
}

void RetroactivePairingDetectorImpl::RemoveDeviceInformation(
    const std::string& device_address) {
  QP_LOG(VERBOSE) << __func__ << ": device = " << device_address;
  potential_retroactive_addresses_.erase(device_address);
  device_pairing_information_.erase(device_address);

  // We can potentially get to a state where we need to RemoveDeviceInformation
  // before the MessageStreams are observed, connected, and/or added to our
  // list here if we get a false positive instance of a potential retroactive
  // pairing device.
  if (!base::Contains(message_streams_, device_address))
    return;

  message_streams_[device_address]->RemoveObserver(this);
  message_streams_.erase(device_address);
}

void RetroactivePairingDetectorImpl::OnPairFailure(scoped_refptr<Device> device,
                                                   PairFailure failure) {}

void RetroactivePairingDetectorImpl::OnAccountKeyWrite(
    scoped_refptr<Device> device,
    absl::optional<AccountKeyFailure> error) {}

}  // namespace quick_pair
}  // namespace ash
