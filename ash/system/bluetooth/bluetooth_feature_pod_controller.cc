// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/bluetooth/bluetooth_feature_pod_controller.h"

#include <utility>

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/bluetooth/tray_bluetooth_helper.h"
#include "ash/system/unified/feature_pod_button.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "base/i18n/number_formatting.h"
#include "base/strings/string_number_conversions.h"
#include "services/device/public/cpp/bluetooth/bluetooth_utils.h"
#include "ui/base/l10n/l10n_util.h"

using device::mojom::BluetoothSystem;
using device::mojom::BluetoothDeviceInfo;

namespace ash {

BluetoothFeaturePodController::BluetoothFeaturePodController(
    UnifiedSystemTrayController* tray_controller)
    : tray_controller_(tray_controller) {
  Shell::Get()->tray_bluetooth_helper()->AddObserver(this);
}

BluetoothFeaturePodController::~BluetoothFeaturePodController() {
  auto* helper = Shell::Get()->tray_bluetooth_helper();
  if (helper)
    helper->RemoveObserver(this);
}

FeaturePodButton* BluetoothFeaturePodController::CreateButton() {
  DCHECK(!button_);
  button_ = new FeaturePodButton(this);
  button_->ShowDetailedViewArrow();
  UpdateButton();
  return button_;
}

void BluetoothFeaturePodController::OnIconPressed() {
  bool was_enabled = button_->IsToggled();
  Shell::Get()->tray_bluetooth_helper()->SetBluetoothEnabled(!was_enabled);

  // If Bluetooth was disabled, show device list as well as enabling Bluetooth.
  if (!was_enabled)
    tray_controller_->ShowBluetoothDetailedView();
}

void BluetoothFeaturePodController::OnLabelPressed() {
  Shell::Get()->tray_bluetooth_helper()->SetBluetoothEnabled(true);
  tray_controller_->ShowBluetoothDetailedView();
}

SystemTrayItemUmaType BluetoothFeaturePodController::GetUmaType() const {
  return SystemTrayItemUmaType::UMA_BLUETOOTH;
}

void BluetoothFeaturePodController::UpdateButton() {
  bool is_available =
      Shell::Get()->tray_bluetooth_helper()->IsBluetoothStateAvailable();
  button_->SetVisible(is_available);
  if (!is_available)
    return;

  // Bluetooth power setting is always mutable in login screen before any
  // user logs in. The changes will affect local state preferences.
  //
  // Otherwise, the bluetooth setting should be mutable only if:
  // * the active user is the primary user, and
  // * the session is not in lock screen
  // The changes will affect the primary user's preferences.
  SessionControllerImpl* session_controller =
      Shell::Get()->session_controller();
  button_->SetEnabled(!session_controller->IsActiveUserSessionStarted() ||
                      (session_controller->IsUserPrimary() &&
                       !session_controller->IsScreenLocked()));

  bool is_enabled =
      Shell::Get()->tray_bluetooth_helper()->GetBluetoothState() ==
      BluetoothSystem::State::kPoweredOn;
  button_->SetToggled(is_enabled);

  if (!is_enabled) {
    button_->SetVectorIcon(kUnifiedMenuBluetoothIcon);
    button_->SetLabel(l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_BLUETOOTH));
    button_->SetSubLabel(l10n_util::GetStringUTF16(
        IDS_ASH_STATUS_TRAY_BLUETOOTH_DISABLED_SHORT));
    SetTooltipState(l10n_util::GetStringUTF16(
        IDS_ASH_STATUS_TRAY_BLUETOOTH_DISABLED_TOOLTIP));
    return;
  }

  BluetoothDeviceList connected_devices;
  for (auto& device :
       Shell::Get()->tray_bluetooth_helper()->GetAvailableBluetoothDevices()) {
    if (device->connection_state ==
        BluetoothDeviceInfo::ConnectionState::kConnected) {
      connected_devices.push_back(device->Clone());
    }
  }

  if (connected_devices.size() > 1) {
    const size_t device_count = connected_devices.size();
    button_->SetVectorIcon(kUnifiedMenuBluetoothConnectedIcon);
    button_->SetLabel(l10n_util::GetStringUTF16(
        IDS_ASH_STATUS_TRAY_BLUETOOTH_MULTIPLE_DEVICES_CONNECTED_LABEL));
    button_->SetSubLabel(base::FormatNumber(device_count));
    SetTooltipState(l10n_util::GetPluralStringFUTF16(
        IDS_ASH_STATUS_TRAY_BLUETOOTH_MULTIPLE_DEVICES_CONNECTED_TOOLTIP,
        device_count));
  } else if (connected_devices.size() == 1) {
    const device::mojom::BluetoothDeviceInfoPtr& device =
        connected_devices.back();
    const base::string16 device_name =
        device::GetBluetoothDeviceNameForDisplay(device);
    button_->SetVectorIcon(kUnifiedMenuBluetoothConnectedIcon);
    button_->SetLabel(device_name);

    if (device->battery_info) {
      button_->SetSubLabel(l10n_util::GetStringFUTF16(
          IDS_ASH_STATUS_TRAY_BLUETOOTH_DEVICE_BATTERY_PERCENTAGE_LABEL,
          base::NumberToString16(device->battery_info->battery_percentage)));
    } else {
      button_->SetSubLabel(l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_BLUETOOTH_DEVICE_CONNECTED_LABEL));
    }
    SetTooltipState(l10n_util::GetStringFUTF16(
        IDS_ASH_STATUS_TRAY_BLUETOOTH_DEVICE_CONNECTED_TOOLTIP, device_name));
  } else {
    button_->SetVectorIcon(kUnifiedMenuBluetoothIcon);
    button_->SetLabel(l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_BLUETOOTH));
    button_->SetSubLabel(
        l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_BLUETOOTH_ENABLED_SHORT));
    SetTooltipState(l10n_util::GetStringUTF16(
        IDS_ASH_STATUS_TRAY_BLUETOOTH_ENABLED_TOOLTIP));
  }
}

void BluetoothFeaturePodController::SetTooltipState(
    const base::string16& tooltip_state) {
  if (button_->GetEnabled()) {
    button_->SetIconTooltip(l10n_util::GetStringFUTF16(
        IDS_ASH_STATUS_TRAY_BLUETOOTH_TOGGLE_TOOLTIP, tooltip_state));
    button_->SetLabelTooltip(l10n_util::GetStringFUTF16(
        IDS_ASH_STATUS_TRAY_BLUETOOTH_SETTINGS_TOOLTIP, tooltip_state));
  } else {
    // Do not show "Toggle" text in tooltip when the button is disabled (e.g.
    // when the screen is locked or for secondary users).
    button_->SetIconTooltip(tooltip_state);
    button_->SetLabelTooltip(tooltip_state);
  }
}

void BluetoothFeaturePodController::OnBluetoothSystemStateChanged() {
  UpdateButton();
}

void BluetoothFeaturePodController::OnBluetoothScanStateChanged() {
  UpdateButton();
}

void BluetoothFeaturePodController::OnBluetoothDeviceListChanged() {
  UpdateButton();
}

}  // namespace ash
