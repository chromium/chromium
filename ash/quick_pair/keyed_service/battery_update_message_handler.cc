// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/keyed_service/battery_update_message_handler.h"

#include "base/containers/adapters.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "components/cross_device/logging/logging.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/bluetooth_device.h"

namespace {

device::BluetoothDevice::BatteryInfo GetBatteryInfo(
    const ash::quick_pair::mojom::BatteryInfoPtr& battery_info,
    const device::BluetoothDevice::BatteryType& battery_type) {
  if (battery_info->percentage == -1) {
    return device::BluetoothDevice::BatteryInfo(
        battery_type,
        /*percentage=*/std::nullopt,
        battery_info->is_charging
            ? device::BluetoothDevice::BatteryInfo::ChargeState::kCharging
            : device::BluetoothDevice::BatteryInfo::ChargeState::kDischarging);
  }

  return device::BluetoothDevice::BatteryInfo(
      battery_type, battery_info->percentage,
      battery_info->is_charging
          ? device::BluetoothDevice::BatteryInfo::ChargeState::kCharging
          : device::BluetoothDevice::BatteryInfo::ChargeState::kDischarging);
}

}  // namespace

namespace ash {
namespace quick_pair {

BatteryUpdateMessageHandler::BatteryUpdateMessageHandler(
    MessageStreamLookup* message_stream_lookup) {
  device::BluetoothAdapterFactory::Get()->GetAdapter(
      base::BindOnce(&BatteryUpdateMessageHandler::OnGetAdapter,
                     weak_ptr_factory_.GetWeakPtr()));
  message_stream_lookup_observation_.Observe(message_stream_lookup);
}

void BatteryUpdateMessageHandler::OnGetAdapter(
    scoped_refptr<device::BluetoothAdapter> adapter) {
  adapter_ = adapter;
}

BatteryUpdateMessageHandler::~BatteryUpdateMessageHandler() {
  // Remove any observation of remaining MessageStreams.
  for (auto it = message_streams_.begin(); it != message_streams_.end(); it++) {
    it->second->RemoveObserver(this);
  }
}

void BatteryUpdateMessageHandler::OnMessageStreamConnected(
    const std::string& device_address,
    MessageStream* message_stream) {
  if (!message_stream)
    return;

  message_stream->AddObserver(this);
  message_streams_[device_address] = message_stream;
  GetBatteryUpdateFromMessageStream(device_address, message_stream);
}

void BatteryUpdateMessageHandler::GetBatteryUpdateFromMessageStream(
    const std::string& device_address,
    MessageStream* message_stream) {
  DCHECK(message_stream);

  // Iterate over messages for battery update if it already exists.
  for (const auto& message : base::Reversed(message_stream->messages())) {
    if (message->is_battery_update()) {
      SetBatteryInfo(device_address, message->get_battery_update());
      return;
    }
  }
}

void BatteryUpdateMessageHandler::OnBatteryUpdateMessage(
    const std::string& device_address,
    const mojom::BatteryUpdatePtr& battery_update) {
  SetBatteryInfo(device_address, battery_update);
}

void BatteryUpdateMessageHandler::OnDisconnected(
    const std::string& device_address) {
  CleanUpMessageStream(device_address);
}

void BatteryUpdateMessageHandler::OnMessageStreamDestroyed(
    const std::string& device_address) {
  CleanUpMessageStream(device_address);
}

void BatteryUpdateMessageHandler::SetBatteryInfo(
    const std::string& device_address,
    const mojom::BatteryUpdatePtr& battery_update) {
  device::BluetoothDevice* device = adapter_->GetDevice(device_address);
  if (!device) {
    CD_LOG(INFO, Feature::FP)
        << "Device lost from adapter before battery info was set.";
    CleanUpMessageStream(device_address);
    return;
  }

  device::BluetoothDevice::BatteryInfo left_bud_info =
      GetBatteryInfo(/*battery_info=*/battery_update->left_bud_info,
                     /*battery_type=*/device::BluetoothDevice::BatteryType::
                         kLeftBudTrueWireless);
  device->SetBatteryInfo(left_bud_info);

  device::BluetoothDevice::BatteryInfo right_bud_info =
      GetBatteryInfo(/*battery_info=*/battery_update->right_bud_info,
                     /*battery_type=*/device::BluetoothDevice::BatteryType::
                         kRightBudTrueWireless);
  device->SetBatteryInfo(right_bud_info);

  device::BluetoothDevice::BatteryInfo case_info = GetBatteryInfo(
      /*battery_info=*/battery_update->case_info,
      /*battery_type=*/device::BluetoothDevice::BatteryType::kCaseTrueWireless);
  device->SetBatteryInfo(case_info);
}

void BatteryUpdateMessageHandler::CleanUpMessageStream(
    const std::string& device_address) {
  if (!base::Contains(message_streams_, device_address)) {
    return;
  }

  message_streams_[device_address]->RemoveObserver(this);
  message_streams_.erase(device_address);
}

}  // namespace quick_pair
}  // namespace ash
