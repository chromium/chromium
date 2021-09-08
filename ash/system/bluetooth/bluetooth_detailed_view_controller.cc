// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/bluetooth/bluetooth_detailed_view_controller.h"

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/bluetooth_config_service.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/check.h"
#include "chromeos/services/bluetooth_config/public/cpp/cros_bluetooth_config_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/view.h"

namespace ash {
namespace {
using chromeos::bluetooth_config::IsBluetoothEnabledOrEnabling;
}  // namespace

BluetoothDetailedViewController::BluetoothDetailedViewController(
    UnifiedSystemTrayController* tray_controller)
    : detailed_view_delegate_(
          std::make_unique<DetailedViewDelegate>(tray_controller)) {
  DCHECK(ash::features::IsBluetoothRevampEnabled());
  GetBluetoothConfigService(
      remote_cros_bluetooth_config_.BindNewPipeAndPassReceiver());
  remote_cros_bluetooth_config_->ObserveSystemProperties(
      cros_system_properties_observer_receiver_.BindNewPipeAndPassRemote());
}

BluetoothDetailedViewController::~BluetoothDetailedViewController() = default;

views::View* BluetoothDetailedViewController::CreateView() {
  DCHECK(!view_);
  std::unique_ptr<tray::BluetoothDetailedView> bluetooth_detailed_view =
      tray::BluetoothDetailedView::Factory::Create(
          detailed_view_delegate_.get(),
          /*delegate=*/this);
  view_ = bluetooth_detailed_view.get();
  device_list_controller_ = BluetoothDeviceListController::Factory::Create(
      bluetooth_detailed_view.get());
  BluetoothEnabledStateChanged();

  // We are expected to return an unowned pointer that the caller is responsible
  // for deleting.
  return bluetooth_detailed_view.release()->GetAsView();
}

std::u16string BluetoothDetailedViewController::GetAccessibleName() const {
  return l10n_util::GetStringUTF16(
      IDS_ASH_QUICK_SETTINGS_BUBBLE_BLUETOOTH_SETTINGS_ACCESSIBLE_DESCRIPTION);
}

void BluetoothDetailedViewController::OnPropertiesUpdated(
    chromeos::bluetooth_config::mojom::BluetoothSystemPropertiesPtr
        properties) {
  const bool has_bluetooth_enabled_state_changed =
      system_state_ != properties->system_state;
  system_state_ = properties->system_state;

  if (has_bluetooth_enabled_state_changed)
    BluetoothEnabledStateChanged();
}

void BluetoothDetailedViewController::OnToggleClicked(bool new_state) {
  remote_cros_bluetooth_config_->SetBluetoothEnabledState(new_state);
}

void BluetoothDetailedViewController::OnPairNewDeviceRequested() {}

void BluetoothDetailedViewController::OnDeviceListItemSelected(
    const chromeos::bluetooth_config::mojom::PairedBluetoothDevicePropertiesPtr&
        device) {}

void BluetoothDetailedViewController::BluetoothEnabledStateChanged() {
  const bool bluetooth_enabled_state =
      IsBluetoothEnabledOrEnabling(system_state_);
  if (view_)
    view_->UpdateBluetoothEnabledState(bluetooth_enabled_state);
  if (device_list_controller_) {
    device_list_controller_->UpdateBluetoothEnabledState(
        bluetooth_enabled_state);
  }
}

}  // namespace ash
