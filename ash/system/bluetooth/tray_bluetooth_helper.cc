// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/bluetooth/tray_bluetooth_helper.h"
#include "ash/constants/ash_features.h"
#include "base/bind.h"
#include "base/check.h"
#include "base/time/time.h"

using device::mojom::BluetoothSystem;

namespace ash {

namespace {

constexpr base::TimeDelta kUpdateFrequencyMs = base::Milliseconds(1000);

}  // namespace

TrayBluetoothHelper::TrayBluetoothHelper() {
  DCHECK(!ash::features::IsBluetoothRevampEnabled());
}

TrayBluetoothHelper::~TrayBluetoothHelper() = default;

void TrayBluetoothHelper::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void TrayBluetoothHelper::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

const BluetoothDeviceList& TrayBluetoothHelper::GetAvailableBluetoothDevices()
    const {
  return cached_devices_;
}

bool TrayBluetoothHelper::IsBluetoothStateAvailable() {
  switch (GetBluetoothState()) {
    case BluetoothSystem::State::kUnsupported:
    case BluetoothSystem::State::kUnavailable:
      return false;
    case BluetoothSystem::State::kPoweredOff:
    case BluetoothSystem::State::kTransitioning:
    case BluetoothSystem::State::kPoweredOn:
      return true;
  }
}

void TrayBluetoothHelper::StartOrStopRefreshingDeviceList() {
  if (GetBluetoothState() == BluetoothSystem::State::kPoweredOn) {
    DCHECK(!timer_.IsRunning());
    UpdateDeviceCache();
    timer_.Start(FROM_HERE, kUpdateFrequencyMs, this,
                 &TrayBluetoothHelper::UpdateDeviceCache);
    return;
  }

  timer_.Stop();
  cached_devices_.clear();
  NotifyBluetoothDeviceListChanged();
}

void TrayBluetoothHelper::UpdateDeviceCache() {
  GetBluetoothDevices(
      base::BindOnce(&TrayBluetoothHelper::OnGetBluetoothDevices,
                     weak_ptr_factory_.GetWeakPtr()));
}

void TrayBluetoothHelper::OnGetBluetoothDevices(BluetoothDeviceList devices) {
  cached_devices_ = std::move(devices);
  NotifyBluetoothDeviceListChanged();
}

void TrayBluetoothHelper::NotifyBluetoothDeviceListChanged() {
  for (auto& observer : observers_)
    observer.OnBluetoothDeviceListChanged();
}

void TrayBluetoothHelper::NotifyBluetoothSystemStateChanged() {
  for (auto& observer : observers_)
    observer.OnBluetoothSystemStateChanged();
}

void TrayBluetoothHelper::NotifyBluetoothScanStateChanged() {
  for (auto& observer : observers_)
    observer.OnBluetoothScanStateChanged();
}

}  // namespace ash
