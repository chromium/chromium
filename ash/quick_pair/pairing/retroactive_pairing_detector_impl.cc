// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/pairing/retroactive_pairing_detector_impl.h"

#include "ash/quick_pair/common/constants.h"
#include "ash/quick_pair/common/device.h"
#include "ash/quick_pair/common/logging.h"
#include "ash/quick_pair/common/protocol.h"
#include "ash/quick_pair/message_stream/message_stream.h"
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

namespace ash {
namespace quick_pair {

RetroactivePairingDetectorImpl::RetroactivePairingDetectorImpl(
    PairerBroker* pairer_broker,
    MessageStreamLookup* message_stream_lookup)
    : message_stream_lookup_(message_stream_lookup) {
  device::BluetoothAdapterFactory::Get()->GetAdapter(
      base::BindOnce(&RetroactivePairingDetectorImpl::OnGetAdapter,
                     weak_ptr_factory_.GetWeakPtr()));

  message_stream_lookup_observation_.Observe(message_stream_lookup_);
  pairer_broker_observation_.Observe(pairer_broker);
}

void RetroactivePairingDetectorImpl::OnGetAdapter(
    scoped_refptr<device::BluetoothAdapter> adapter) {
  adapter_ = adapter;
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
  // When a device is paired to via Fast Pair, we save the device's classic
  // pairing address here so when we get the the BluetoothAdapter's
  // |DevicePairedChanged| fired, we can determine if it was the one we already
  // have paired to. The classic address is assigned to the Device during the
  // initial Fast Pair pairing protocol during the key exchange, and if it
  // doesn't exist, then it wasn't properly paired during initial Fast Pair
  // pairing.
  if (!device->classic_address())
    return;

  QP_LOG(VERBOSE) << __func__ << ":  Storing Fast Pair device address: "
                  << device->classic_address().value();
  fast_pair_addresses_.insert(device->classic_address().value());
}

void RetroactivePairingDetectorImpl::DevicePairedChanged(
    device::BluetoothAdapter* adapter,
    device::BluetoothDevice* device,
    bool new_paired_status) {
  // This event fires whenever a device pairing has changed with the adapter.
  // If the |new_paired_status| is false, it means a device was unpaired with
  // the adapter, so we early return since it would not be a device to
  // retroactively pair to. If the device that was paired to that fires this
  // event is a device we just paired to with Fast Pair, then we early return
  // since it also wouldn't be one to retroactively pair to. We want to only
  // continue our check here if we have a newly paired device that was paired
  // with classic Bluetooth pairing.
  const std::string& classic_address = device->GetAddress();
  if (!new_paired_status ||
      base::Contains(fast_pair_addresses_, classic_address)) {
    return;
  }

  potential_retroactive_addresses_.insert(classic_address);

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
  device_pairing_information_[device_address].model_id = model_id;
  CheckPairingInformation(device_address);
}

void RetroactivePairingDetectorImpl::OnBleAddressUpdateMessage(
    const std::string& device_address,
    const std::string& ble_address) {
  device_pairing_information_[device_address].ble_address = ble_address;
  CheckPairingInformation(device_address);
}

void RetroactivePairingDetectorImpl::CheckPairingInformation(
    const std::string& device_address) {
  DCHECK(device_pairing_information_.find(device_address) !=
         device_pairing_information_.end());

  if (device_pairing_information_[device_address].model_id.empty() ||
      device_pairing_information_[device_address].ble_address.empty()) {
    return;
  }

  NotifyDeviceFound(device_pairing_information_[device_address].model_id,
                    device_pairing_information_[device_address].ble_address,
                    device_address);
}

void RetroactivePairingDetectorImpl::OnDisconnected(
    const std::string& device_address) {
  message_streams_[device_address]->RemoveObserver(this);
  message_streams_.erase(device_address);
}

void RetroactivePairingDetectorImpl::OnMessageStreamDestroyed(
    const std::string& device_address) {
  message_streams_[device_address]->RemoveObserver(this);
  message_streams_.erase(device_address);
}

void RetroactivePairingDetectorImpl::NotifyDeviceFound(
    const std::string& model_id,
    const std::string& ble_address,
    const std::string& classic_address) {
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
  QP_LOG(INFO) << __func__ << ": Found device for Retroactive Pairing.";

  for (auto& observer : observers_)
    observer.OnRetroactivePairFound(device);

  RemoveDeviceInformation(classic_address);
}

void RetroactivePairingDetectorImpl::RemoveDeviceInformation(
    const std::string& device_address) {
  potential_retroactive_addresses_.erase(device_address);
  device_pairing_information_.erase(device_address);
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
