// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/pcie_peripheral/pcie_peripheral_manager.h"

#include "base/callback_helpers.h"

namespace ash {

namespace {
PciePeripheralManager* g_instance = nullptr;
}  // namespace

PciePeripheralManager::PciePeripheralManager(bool is_guest_profile,
                                             bool is_pcie_tunneling_allowed)
    : is_guest_profile_(is_guest_profile),
      is_pcie_tunneling_allowed_(is_pcie_tunneling_allowed) {}

PciePeripheralManager::~PciePeripheralManager() {
  CHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

void PciePeripheralManager::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void PciePeripheralManager::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void PciePeripheralManager::NotifyLimitedPerformancePeripheralReceived() {
  // Show no notifications if PCIe tunneling is allowed.
  if (is_pcie_tunneling_allowed_)
    return;

  for (auto& observer : observer_list_)
    observer.OnLimitedPerformancePeripheralReceived();
}

void PciePeripheralManager::NotifyGuestModeNotificationReceived(
    bool is_thunderbolt_only) {
  for (auto& observer : observer_list_)
    observer.OnGuestModeNotificationReceived(is_thunderbolt_only);
}

void PciePeripheralManager::SetPcieTunnelingAllowedState(
    bool is_pcie_tunneling_allowed) {
  is_pcie_tunneling_allowed_ = is_pcie_tunneling_allowed;
}

// static
void PciePeripheralManager::Initialize(bool is_guest_profile,
                                       bool is_pcie_tunneling_allowed) {
  CHECK(!g_instance);
  g_instance =
      new PciePeripheralManager(is_guest_profile, is_pcie_tunneling_allowed);
}

// static
void PciePeripheralManager::Shutdown() {
  CHECK(g_instance);
  delete g_instance;
  g_instance = NULL;
}

// static
PciePeripheralManager* PciePeripheralManager::Get() {
  CHECK(g_instance);
  return g_instance;
}

// static
bool PciePeripheralManager::IsInitialized() {
  return g_instance;
}

}  // namespace ash
