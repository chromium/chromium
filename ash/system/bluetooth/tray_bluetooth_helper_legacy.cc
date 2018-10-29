// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/bluetooth/tray_bluetooth_helper_legacy.h"

#include <string>
#include <utility>

#include "ash/shell.h"
#include "ash/system/bluetooth/bluetooth_power_controller.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/metrics/user_metrics.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_discovery_session.h"
#include "device/bluetooth/chromeos/bluetooth_utils.h"

using device::mojom::BluetoothSystem;

namespace ash {
namespace {

// System tray shows a limited number of bluetooth devices.
const int kMaximumDevicesShown = 50;

void BluetoothSetDiscoveringError() {
  LOG(ERROR) << "BluetoothSetDiscovering failed.";
}

void BluetoothDeviceConnectError(
    device::BluetoothDevice::ConnectErrorCode error_code) {}

ash::SystemTrayNotifier* GetSystemTrayNotifier() {
  return Shell::Get()->system_tray_notifier();
}

BluetoothDeviceInfo GetBluetoothDeviceInfo(device::BluetoothDevice* device) {
  BluetoothDeviceInfo info;
  info.address = device->GetAddress();
  info.display_name = device->GetNameForDisplay();
  info.connected = device->IsConnected();
  info.connecting = device->IsConnecting();
  info.paired = device->IsPaired();
  info.device_type = device->GetDeviceType();
  return info;
}

}  // namespace

TrayBluetoothHelperLegacy::TrayBluetoothHelperLegacy()
    : weak_ptr_factory_(this) {}

TrayBluetoothHelperLegacy::~TrayBluetoothHelperLegacy() {
  if (adapter_)
    adapter_->RemoveObserver(this);
}

void TrayBluetoothHelperLegacy::InitializeOnAdapterReady(
    scoped_refptr<device::BluetoothAdapter> adapter) {
  adapter_ = adapter;
  CHECK(adapter_);
  adapter_->AddObserver(this);
}

void TrayBluetoothHelperLegacy::Initialize() {
  device::BluetoothAdapterFactory::GetAdapter(
      base::Bind(&TrayBluetoothHelperLegacy::InitializeOnAdapterReady,
                 weak_ptr_factory_.GetWeakPtr()));
}

BluetoothDeviceList TrayBluetoothHelperLegacy::GetAvailableBluetoothDevices()
    const {
  BluetoothDeviceList device_list;
  device::BluetoothAdapter::DeviceList devices =
      device::FilterBluetoothDeviceList(adapter_->GetDevices(),
                                        device::BluetoothFilterType::KNOWN,
                                        kMaximumDevicesShown);
  for (device::BluetoothDevice* device : devices)
    device_list.push_back(GetBluetoothDeviceInfo(device));

  return device_list;
}

void TrayBluetoothHelperLegacy::StartBluetoothDiscovering() {
  if (HasBluetoothDiscoverySession()) {
    LOG(WARNING) << "Already have active Bluetooth device discovery session.";
    return;
  }
  VLOG(1) << "Requesting new Bluetooth device discovery session.";
  should_run_discovery_ = true;
  adapter_->StartDiscoverySession(
      base::Bind(&TrayBluetoothHelperLegacy::OnStartDiscoverySession,
                 weak_ptr_factory_.GetWeakPtr()),
      base::Bind(&BluetoothSetDiscoveringError));
}

void TrayBluetoothHelperLegacy::StopBluetoothDiscovering() {
  should_run_discovery_ = false;
  if (!HasBluetoothDiscoverySession()) {
    LOG(WARNING) << "No active Bluetooth device discovery session.";
    return;
  }
  VLOG(1) << "Stopping Bluetooth device discovery session.";
  discovery_session_->Stop(base::DoNothing(),
                           base::Bind(&BluetoothSetDiscoveringError));
}

void TrayBluetoothHelperLegacy::ConnectToBluetoothDevice(
    const std::string& address) {
  device::BluetoothDevice* device = adapter_->GetDevice(address);
  if (!device || device->IsConnecting() ||
      (device->IsConnected() && device->IsPaired())) {
    return;
  }
  if (device->IsPaired() && !device->IsConnectable())
    return;
  if (device->IsPaired() || !device->IsPairable()) {
    base::RecordAction(
        base::UserMetricsAction("StatusArea_Bluetooth_Connect_Known"));
    device->Connect(NULL, base::DoNothing(),
                    base::Bind(&BluetoothDeviceConnectError));
    return;
  }
  // Show pairing dialog for the unpaired device.
  Shell::Get()->system_tray_model()->client_ptr()->ShowBluetoothPairingDialog(
      device->GetAddress(), device->GetNameForDisplay(), device->IsPaired(),
      device->IsConnected());
}

BluetoothSystem::State TrayBluetoothHelperLegacy::GetBluetoothState() {
  // Eventually this will use the BluetoothSystem Mojo interface, but for now
  // use the current Bluetooth API to get a BluetoothSystem::State.
  if (!adapter_)
    return BluetoothSystem::State::kUnavailable;
  if (!adapter_->IsPresent())
    return BluetoothSystem::State::kUnavailable;
  if (adapter_->IsPowered())
    return BluetoothSystem::State::kPoweredOn;

  return BluetoothSystem::State::kPoweredOff;
}

void TrayBluetoothHelperLegacy::SetBluetoothEnabled(bool enabled) {
  if (enabled != (GetBluetoothState() == BluetoothSystem::State::kPoweredOn)) {
    Shell::Get()->metrics()->RecordUserMetricsAction(
        enabled ? UMA_STATUS_AREA_BLUETOOTH_ENABLED
                : UMA_STATUS_AREA_BLUETOOTH_DISABLED);
  }

  Shell::Get()->bluetooth_power_controller()->SetBluetoothEnabled(enabled);
}

bool TrayBluetoothHelperLegacy::HasBluetoothDiscoverySession() {
  return discovery_session_ && discovery_session_->IsActive();
}

////////////////////////////////////////////////////////////////////////////////
// BluetoothAdapter::Observer:

void TrayBluetoothHelperLegacy::AdapterPresentChanged(
    device::BluetoothAdapter* adapter,
    bool present) {
  GetSystemTrayNotifier()->NotifyRefreshBluetooth();
}

void TrayBluetoothHelperLegacy::AdapterPoweredChanged(
    device::BluetoothAdapter* adapter,
    bool powered) {
  GetSystemTrayNotifier()->NotifyRefreshBluetooth();
}

void TrayBluetoothHelperLegacy::AdapterDiscoveringChanged(
    device::BluetoothAdapter* adapter,
    bool discovering) {
  GetSystemTrayNotifier()->NotifyBluetoothDiscoveringChanged();
}

void TrayBluetoothHelperLegacy::DeviceAdded(device::BluetoothAdapter* adapter,
                                            device::BluetoothDevice* device) {
  GetSystemTrayNotifier()->NotifyRefreshBluetooth();
}

void TrayBluetoothHelperLegacy::DeviceChanged(device::BluetoothAdapter* adapter,
                                              device::BluetoothDevice* device) {
  GetSystemTrayNotifier()->NotifyRefreshBluetooth();
}

void TrayBluetoothHelperLegacy::DeviceRemoved(device::BluetoothAdapter* adapter,
                                              device::BluetoothDevice* device) {
  GetSystemTrayNotifier()->NotifyRefreshBluetooth();
}

void TrayBluetoothHelperLegacy::OnStartDiscoverySession(
    std::unique_ptr<device::BluetoothDiscoverySession> discovery_session) {
  // If the discovery session was returned after a request to stop discovery
  // (e.g. the user dismissed the Bluetooth detailed view before the call
  // returned), don't claim the discovery session and let it clean up.
  if (!should_run_discovery_)
    return;
  VLOG(1) << "Claiming new Bluetooth device discovery session.";
  discovery_session_ = std::move(discovery_session);
  GetSystemTrayNotifier()->NotifyBluetoothDiscoveringChanged();
}

}  // namespace ash
