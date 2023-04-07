// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/bluetooth/bluetooth_feature_pod_controller.h"

#include <string>

#include "ash/constants/ash_features.h"
#include "ash/constants/quick_settings_catalogs.h"
#include "ash/public/cpp/bluetooth_config_service.h"
#include "ash/public/cpp/hats_bluetooth_revamp_trigger.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/bluetooth/bluetooth_state_cache.h"
#include "ash/system/unified/feature_pod_button.h"
#include "ash/system/unified/feature_tile.h"
#include "ash/system/unified/quick_settings_metrics_util.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/i18n/number_formatting.h"
#include "base/strings/string_number_conversions.h"
#include "chromeos/ash/services/bluetooth_config/public/cpp/cros_bluetooth_config_util.h"
#include "mojo/public/cpp/bindings/clone_traits.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {
namespace {

using bluetooth_config::GetPairedDeviceName;
using bluetooth_config::mojom::BluetoothModificationState;
using bluetooth_config::mojom::BluetoothSystemPropertiesPtr;
using bluetooth_config::mojom::BluetoothSystemState;
using bluetooth_config::mojom::DeviceConnectionState;

BluetoothSystemState GetInitialSystemState() {
  if (features::IsQsRevampEnabled()) {
    // Synchronously query the initial state so the feature tile doesn't flash
    // with the wrong state. See b/266996235
    return Shell::Get()->bluetooth_state_cache()->system_state();
  } else {
    return BluetoothSystemState::kUnavailable;
  }
}

}  // namespace

BluetoothFeaturePodController::BluetoothFeaturePodController(
    UnifiedSystemTrayController* tray_controller)
    : system_state_(GetInitialSystemState()),
      tray_controller_(tray_controller) {
  GetBluetoothConfigService(
      remote_cros_bluetooth_config_.BindNewPipeAndPassReceiver());
  remote_cros_bluetooth_config_->ObserveSystemProperties(
      cros_system_properties_observer_receiver_.BindNewPipeAndPassRemote());
}

BluetoothFeaturePodController::~BluetoothFeaturePodController() = default;

FeaturePodButton* BluetoothFeaturePodController::CreateButton() {
  DCHECK(!button_);
  button_ = new FeaturePodButton(this);
  button_->ShowDetailedViewArrow();
  // Init the button with invisible state. The `UpdateButtonStateIfExists`
  // method will update the visibility based on the current condition.
  button_->SetVisible(false);
  UpdateButtonStateIfExists();
  return button_;
}

std::unique_ptr<FeatureTile> BluetoothFeaturePodController::CreateTile(
    bool compact) {
  DCHECK(features::IsQsRevampEnabled());
  auto tile = std::make_unique<FeatureTile>(
      base::BindRepeating(&BluetoothFeaturePodController::OnIconPressed,
                          weak_factory_.GetWeakPtr()));
  tile_ = tile.get();
  tile_->CreateDrillInButton(
      base::BindRepeating(&BluetoothFeaturePodController::OnLabelPressed,
                          weak_factory_.GetWeakPtr()),
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_BLUETOOTH));
  // UpdateTileStateIfExists() will update visibility.
  tile_->SetVisible(false);
  UpdateTileStateIfExists();
  return tile;
}

QsFeatureCatalogName BluetoothFeaturePodController::GetCatalogName() {
  return QsFeatureCatalogName::kBluetooth;
}

void BluetoothFeaturePodController::OnIconPressed() {
  if (!IsButtonEnabled()) {
    return;
  }

  const bool is_toggled = IsButtonToggled();
  remote_cros_bluetooth_config_->SetBluetoothEnabledState(!is_toggled);

  if (auto* hats_bluetooth_revamp_trigger = HatsBluetoothRevampTrigger::Get()) {
    hats_bluetooth_revamp_trigger->TryToShowSurvey();
  }

  if (is_toggled) {
    TrackToggleUMA(/*target_toggle_state=*/false);
    return;
  }

  TrackDiveInUMA();
  tray_controller_->ShowBluetoothDetailedView();
}

void BluetoothFeaturePodController::OnLabelPressed() {
  if (!IsButtonEnabled()) {
    return;
  }

  TrackDiveInUMA();
  if (!IsButtonToggled()) {
    remote_cros_bluetooth_config_->SetBluetoothEnabledState(true);
  }

  if (auto* hats_bluetooth_revamp_trigger = HatsBluetoothRevampTrigger::Get()) {
    hats_bluetooth_revamp_trigger->TryToShowSurvey();
  }
  tray_controller_->ShowBluetoothDetailedView();
}

BluetoothFeaturePodController::BluetoothDeviceNameAndBatteryInfo::
    BluetoothDeviceNameAndBatteryInfo(
        const std::u16string& device_name,
        bluetooth_config::mojom::DeviceBatteryInfoPtr battery_info)
    : device_name(device_name), battery_info(std::move(battery_info)) {}

BluetoothFeaturePodController::BluetoothDeviceNameAndBatteryInfo::
    ~BluetoothDeviceNameAndBatteryInfo() = default;

bool BluetoothFeaturePodController::DoesFirstConnectedDeviceHaveBatteryInfo()
    const {
  return first_connected_device_.has_value() &&
         first_connected_device_.value().battery_info &&
         (first_connected_device_.value().battery_info->default_properties ||
          first_connected_device_.value().battery_info->left_bud_info ||
          first_connected_device_.value().battery_info->right_bud_info ||
          first_connected_device_.value().battery_info->case_info);
}

const gfx::VectorIcon& BluetoothFeaturePodController::ComputeButtonIcon()
    const {
  if (!IsButtonToggled()) {
    return kUnifiedMenuBluetoothDisabledIcon;
  }

  if (first_connected_device_.has_value())
    return kUnifiedMenuBluetoothConnectedIcon;

  return kUnifiedMenuBluetoothIcon;
}

std::u16string BluetoothFeaturePodController::ComputeButtonLabel() const {
  if (IsButtonToggled() && first_connected_device_.has_value() &&
      connected_device_count_ == 1) {
    return first_connected_device_.value().device_name;
  }
  return l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_BLUETOOTH);
}

int BluetoothFeaturePodController::
    GetFirstConnectedDeviceBatteryLevelForDisplay() const {
  // If there are any multiple battery details, we should prioritize showing
  // them over the default battery in order to match the Quick Settings
  // Bluetooth sub-page battery details shown. Android only shows the left bud
  // if there are multiple batteries, so we match that here, and if it doesn't
  // exist, we prioritize the right bud battery, then the case battery, if
  // they exist over the default battery in order to match any detailed
  // battery shown on the sub-page.
  if (first_connected_device_.value().battery_info->left_bud_info)
    return first_connected_device_.value()
        .battery_info->left_bud_info->battery_percentage;

  if (first_connected_device_.value().battery_info->right_bud_info)
    return first_connected_device_.value()
        .battery_info->right_bud_info->battery_percentage;

  if (first_connected_device_.value().battery_info->case_info)
    return first_connected_device_.value()
        .battery_info->case_info->battery_percentage;

  return first_connected_device_.value()
      .battery_info->default_properties->battery_percentage;
}

std::u16string BluetoothFeaturePodController::ComputeButtonSubLabel() const {
  if (!IsButtonToggled()) {
    return l10n_util::GetStringUTF16(
        IDS_ASH_STATUS_TRAY_BLUETOOTH_DISABLED_SHORT);
  }
  if (!first_connected_device_.has_value()) {
    return l10n_util::GetStringUTF16(
        IDS_ASH_STATUS_TRAY_BLUETOOTH_ENABLED_SHORT);
  }
  if (connected_device_count_ == 1) {
    if (DoesFirstConnectedDeviceHaveBatteryInfo()) {
      return l10n_util::GetStringFUTF16(
          IDS_ASH_STATUS_TRAY_BLUETOOTH_DEVICE_BATTERY_PERCENTAGE_LABEL,
          base::NumberToString16(
              GetFirstConnectedDeviceBatteryLevelForDisplay()));
    }
    return l10n_util::GetStringUTF16(
        IDS_ASH_STATUS_TRAY_BLUETOOTH_DEVICE_CONNECTED_LABEL);
  }

  return l10n_util::GetStringFUTF16(
      IDS_ASH_STATUS_TRAY_BLUETOOTH_MULTIPLE_DEVICES_CONNECTED_LABEL,
      base::FormatNumber(connected_device_count_));
}

std::u16string BluetoothFeaturePodController::ComputeTooltip() const {
  if (!first_connected_device_.has_value())
    return l10n_util::GetStringUTF16(
        IDS_ASH_STATUS_TRAY_BLUETOOTH_ENABLED_TOOLTIP);

  if (connected_device_count_ == 1) {
    if (DoesFirstConnectedDeviceHaveBatteryInfo()) {
      return l10n_util::GetStringFUTF16(
          IDS_ASH_STATUS_TRAY_BLUETOOTH_DEVICE_CONNECTED_WITH_BATTERY_TOOLTIP,
          first_connected_device_.value().device_name,
          base::NumberToString16(
              GetFirstConnectedDeviceBatteryLevelForDisplay()));
    }
    return l10n_util::GetStringFUTF16(
        IDS_ASH_STATUS_TRAY_BLUETOOTH_DEVICE_CONNECTED_TOOLTIP,
        first_connected_device_.value().device_name);
  }

  return l10n_util::GetStringFUTF16(
      IDS_ASH_STATUS_TRAY_BLUETOOTH_MULTIPLE_DEVICES_CONNECTED_TOOLTIP,
      base::FormatNumber(connected_device_count_));
}

bool BluetoothFeaturePodController::IsButtonEnabled() const {
  return features::IsQsRevampEnabled() ? tile_->GetEnabled()
                                       : button_->GetEnabled();
}

bool BluetoothFeaturePodController::IsButtonToggled() const {
  return features::IsQsRevampEnabled() ? tile_->IsToggled()
                                       : button_->IsToggled();
}

void BluetoothFeaturePodController::UpdateButtonStateIfExists() {
  // Check |button_| here so that calling functions don't need to.
  if (!button_)
    return;
  if (system_state_ == BluetoothSystemState::kUnavailable) {
    button_->SetVisible(false);
    button_->SetEnabled(false);
    return;
  }

  // If the button's visibility changes from invisible to visible, log its
  // visibility.
  if (!button_->GetVisible())
    TrackVisibilityUMA();

  button_->SetEnabled(modification_state_ ==
                      BluetoothModificationState::kCanModifyBluetooth);
  button_->SetToggled(
      bluetooth_config::IsBluetoothEnabledOrEnabling(system_state_));
  button_->SetVisible(true);
  button_->SetVectorIcon(ComputeButtonIcon());
  button_->SetLabel(ComputeButtonLabel());
  button_->SetSubLabel(ComputeButtonSubLabel());

  if (!button_->IsToggled()) {
    button_->SetIconAndLabelTooltips(l10n_util::GetStringFUTF16(
        IDS_ASH_STATUS_TRAY_BLUETOOTH_TOGGLE_TOOLTIP,
        l10n_util::GetStringUTF16(
            IDS_ASH_STATUS_TRAY_BLUETOOTH_DISABLED_TOOLTIP)));
  } else {
    button_->SetIconTooltip(l10n_util::GetStringFUTF16(
        IDS_ASH_STATUS_TRAY_BLUETOOTH_TOGGLE_TOOLTIP, ComputeTooltip()));
    button_->SetLabelTooltip(l10n_util::GetStringFUTF16(
        IDS_ASH_STATUS_TRAY_BLUETOOTH_SETTINGS_TOOLTIP, ComputeTooltip()));
  }
}

void BluetoothFeaturePodController::UpdateTileStateIfExists() {
  if (!tile_) {
    return;
  }
  if (system_state_ == BluetoothSystemState::kUnavailable) {
    tile_->SetVisible(false);
    tile_->SetEnabled(false);
    return;
  }

  // If the button's visibility changes from invisible to visible, log its
  // visibility.
  if (!tile_->GetVisible()) {
    TrackVisibilityUMA();
  }
  tile_->SetToggled(
      bluetooth_config::IsBluetoothEnabledOrEnabling(system_state_));
  tile_->SetEnabled(modification_state_ ==
                    BluetoothModificationState::kCanModifyBluetooth);
  tile_->SetVisible(true);
  tile_->SetVectorIcon(ComputeButtonIcon());
  tile_->SetLabel(ComputeButtonLabel());
  tile_->SetSubLabel(ComputeButtonSubLabel());

  if (!tile_->IsToggled()) {
    std::u16string tooltip = l10n_util::GetStringFUTF16(
        IDS_ASH_STATUS_TRAY_BLUETOOTH_TOGGLE_TOOLTIP,
        l10n_util::GetStringUTF16(
            IDS_ASH_STATUS_TRAY_BLUETOOTH_DISABLED_TOOLTIP));
    tile_->SetTooltipText(tooltip);
    tile_->SetDrillInButtonTooltipText(tooltip);
    return;
  }
  std::u16string tooltip_core = ComputeTooltip();
  tile_->SetTooltipText(l10n_util::GetStringFUTF16(
      IDS_ASH_STATUS_TRAY_BLUETOOTH_TOGGLE_TOOLTIP, tooltip_core));
  tile_->SetDrillInButtonTooltipText(l10n_util::GetStringFUTF16(
      IDS_ASH_STATUS_TRAY_BLUETOOTH_SETTINGS_TOOLTIP, tooltip_core));
}

void BluetoothFeaturePodController::OnPropertiesUpdated(
    BluetoothSystemPropertiesPtr properties) {
  connected_device_count_ = 0;
  first_connected_device_.reset();

  // Counts the number of connected devices and caches the name and battery
  // information of the first connected device found.
  for (const auto& paired_device : properties->paired_devices) {
    if (paired_device->device_properties->connection_state !=
        DeviceConnectionState::kConnected) {
      continue;
    }
    ++connected_device_count_;
    if (first_connected_device_.has_value())
      continue;
    first_connected_device_.emplace(
        GetPairedDeviceName(paired_device),
        mojo::Clone(paired_device->device_properties->battery_info));
  }
  modification_state_ = properties->modification_state;
  system_state_ = properties->system_state;
  UpdateButtonStateIfExists();
  UpdateTileStateIfExists();
}

}  // namespace ash
