// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/bluetooth/bluetooth_detailed_view_controller.h"

#include <optional>

#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/bluetooth_config_service.h"
#include "ash/public/cpp/hats_bluetooth_revamp_trigger.h"
#include "ash/public/cpp/system_tray_client.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/bluetooth/hid_preserving_controller/hid_preserving_bluetooth_state_service.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "build/chromeos_buildflags.h"
#include "chromeos/ash/components/network/network_event_log.h"
#include "chromeos/ash/services/bluetooth_config/public/cpp/cros_bluetooth_config_util.h"
#include "chromeos/constants/chromeos_features.h"
#include "mojo/public/cpp/bindings/clone_traits.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/view.h"

namespace ash {

using bluetooth_config::IsBluetoothEnabledOrEnabling;
using bluetooth_config::mojom::AudioOutputCapability;
using bluetooth_config::mojom::BatteryProperties;
using bluetooth_config::mojom::BluetoothDeviceProperties;
using bluetooth_config::mojom::BluetoothDevicePropertiesPtr;
using bluetooth_config::mojom::BluetoothSystemState;
using bluetooth_config::mojom::DeviceBatteryInfo;
using bluetooth_config::mojom::DeviceBatteryInfoPtr;
using bluetooth_config::mojom::DeviceConnectionState;
using bluetooth_config::mojom::DeviceType;
using bluetooth_config::mojom::PairedBluetoothDeviceProperties;
using bluetooth_config::mojom::PairedBluetoothDevicePropertiesPtr;

BluetoothDetailedViewController::BluetoothDetailedViewController(
    UnifiedSystemTrayController* tray_controller)
    : detailed_view_delegate_(
          std::make_unique<DetailedViewDelegate>(tray_controller)),
      tray_controller_(tray_controller) {
  GetBluetoothConfigService(
      remote_cros_bluetooth_config_.BindNewPipeAndPassReceiver());
  remote_cros_bluetooth_config_->ObserveSystemProperties(
      cros_system_properties_observer_receiver_.BindNewPipeAndPassRemote());

  if (features::IsBluetoothDisconnectWarningEnabled()) {
    GetHidPreservingBluetoothStateControllerService(
        remote_hid_preserving_bluetooth_.BindNewPipeAndPassReceiver());
  }
}

BluetoothDetailedViewController::~BluetoothDetailedViewController() = default;

std::unique_ptr<views::View> BluetoothDetailedViewController::CreateView() {
  DCHECK(!view_);
  std::unique_ptr<BluetoothDetailedView> bluetooth_detailed_view =
      BluetoothDetailedView::Factory::Create(detailed_view_delegate_.get(),
                                             /*delegate=*/this);
  view_ = bluetooth_detailed_view.get();
  device_list_controller_ = BluetoothDeviceListController::Factory::Create(
      bluetooth_detailed_view.get());
  BluetoothEnabledStateChanged();

  if (IsBluetoothEnabledOrEnabling(system_state_)) {
    device_list_controller_->UpdateDeviceList(connected_devices_,
                                              previously_connected_devices_);
  }

  // `bluetooth_detailed_view` is not a views::View, so we must GetAsView().
  return base::WrapUnique(bluetooth_detailed_view.release()->GetAsView());
}

std::u16string BluetoothDetailedViewController::GetAccessibleName() const {
  return l10n_util::GetStringUTF16(
      IDS_ASH_QUICK_SETTINGS_BUBBLE_BLUETOOTH_SETTINGS_ACCESSIBLE_DESCRIPTION);
}

void BluetoothDetailedViewController::OnPropertiesUpdated(
    bluetooth_config::mojom::BluetoothSystemPropertiesPtr properties) {
  // The tray controller should only be transitioning to the main view when this
  // feature is disabled since the detailed tray view and the Bluetooth Pod in
  // QS would be hidden. However, when the feature is enabled, the Bluetooth Pod
  // is visible and the user should be able to see the detailed tray view.
  if (!chromeos::features::IsBluetoothWifiQSPodRefreshEnabled() &&
      properties->system_state == BluetoothSystemState::kUnavailable) {
    tray_controller_->TransitionToMainView(
        /*restore_focus=*/true);  // Deletes |this|.
    return;
  }

  const bool has_bluetooth_enabled_state_changed =
      system_state_ != properties->system_state;
  system_state_ = properties->system_state;

  if (has_bluetooth_enabled_state_changed)
    BluetoothEnabledStateChanged();

  connected_devices_.clear();
  previously_connected_devices_.clear();

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kQsAddFakeBluetoothDevices)) {
    AddFakeBluetoothDevices();
  }

  for (auto& paired_device : properties->paired_devices) {
    if (paired_device->device_properties->connection_state ==
        DeviceConnectionState::kConnected) {
      connected_devices_.push_back(std::move(paired_device));
    } else {
      previously_connected_devices_.push_back(std::move(paired_device));
    }
  }
  if (device_list_controller_ && IsBluetoothEnabledOrEnabling(system_state_)) {
    device_list_controller_->UpdateDeviceList(connected_devices_,
                                              previously_connected_devices_);
  }
}

void BluetoothDetailedViewController::OnToggleClicked(bool new_state) {
  if (features::IsBluetoothDisconnectWarningEnabled()) {
    remote_hid_preserving_bluetooth_->TryToSetBluetoothEnabledState(
        new_state, mojom::HidWarningDialogSource::kQuickSettings);
  } else {
    remote_cros_bluetooth_config_->SetBluetoothEnabledState(new_state);
  }

  if (auto* hats_bluetooth_revamp_trigger = HatsBluetoothRevampTrigger::Get()) {
    hats_bluetooth_revamp_trigger->TryToShowSurvey();
  }
}

void BluetoothDetailedViewController::OnPairNewDeviceRequested() {
  tray_controller_->CloseBubble();  // Deletes |this|.
  NET_LOG(EVENT) << "Attempting to show the bluetooth pairing dialog";
  Shell::Get()->system_tray_model()->client()->ShowBluetoothPairingDialog(
      /*device_address=*/std::nullopt);

  if (auto* hats_bluetooth_revamp_trigger = HatsBluetoothRevampTrigger::Get()) {
    hats_bluetooth_revamp_trigger->TryToShowSurvey();
  }
}

void BluetoothDetailedViewController::OnDeviceListItemSelected(
    const bluetooth_config::mojom::PairedBluetoothDevicePropertiesPtr& device) {
  // When CloseBubble() is called |device| will be deleted so we need to make a
  // copy of the device ID that was selected.
  const std::string device_id = device->device_properties->id;

  // Non-HID devices can be explicitly connected to, so we detect when this is
  // the case and attempt to connect to the device instead of navigating to the
  // Bluetooth Settings.
  if (device->device_properties->audio_capability ==
          AudioOutputCapability::kCapableOfAudioOutput &&
      device->device_properties->connection_state ==
          DeviceConnectionState::kNotConnected) {
    remote_cros_bluetooth_config_->Connect(device_id,
                                           /*callback=*/base::DoNothing());
    return;
  }
  tray_controller_->CloseBubble();  // Deletes |this|.
  Shell::Get()->system_tray_model()->client()->ShowBluetoothSettings(device_id);
}

void BluetoothDetailedViewController::BluetoothEnabledStateChanged() {
  if (view_)
    view_->UpdateBluetoothEnabledState(system_state_);
  if (device_list_controller_) {
    device_list_controller_->UpdateBluetoothEnabledState(
        IsBluetoothEnabledOrEnabling(system_state_));
  }
}

void BluetoothDetailedViewController::AddFakeBluetoothDevices() {
#if !BUILDFLAG(IS_CHROMEOS_DEVICE)
  // Only add fake devices in the linux-chromeos emulator. We don't want to
  // increase the binary size for real devices.
  {
    PairedBluetoothDevicePropertiesPtr paired_device_properties =
        PairedBluetoothDeviceProperties::New();
    paired_device_properties->device_properties =
        BluetoothDeviceProperties::New();
    paired_device_properties->device_properties->id = "Unknown";
    paired_device_properties->device_properties->public_name = u"Unknown";
    paired_device_properties->device_properties->connection_state =
        DeviceConnectionState::kConnected;
    paired_device_properties->device_properties->device_type =
        DeviceType::kUnknown;

    connected_devices_.push_back(mojo::Clone(paired_device_properties));
  }
  {
    PairedBluetoothDevicePropertiesPtr paired_device_properties =
        PairedBluetoothDeviceProperties::New();
    paired_device_properties->device_properties =
        BluetoothDeviceProperties::New();
    paired_device_properties->device_properties->id = "Computer";
    paired_device_properties->device_properties->public_name = u"Computer";
    paired_device_properties->device_properties->device_type =
        DeviceType::kComputer;
    paired_device_properties->device_properties->connection_state =
        DeviceConnectionState::kConnected;
    paired_device_properties->device_properties->battery_info =
        DeviceBatteryInfo::New();
    paired_device_properties->device_properties->battery_info
        ->default_properties = BatteryProperties::New();
    paired_device_properties->device_properties->battery_info
        ->default_properties->battery_percentage = 75;

    connected_devices_.push_back(mojo::Clone(paired_device_properties));
  }
  {
    PairedBluetoothDevicePropertiesPtr paired_device_properties =
        PairedBluetoothDeviceProperties::New();
    paired_device_properties->device_properties =
        BluetoothDeviceProperties::New();
    paired_device_properties->device_properties->id = "Phone";
    paired_device_properties->device_properties->public_name = u"Phone";
    paired_device_properties->device_properties->device_type =
        DeviceType::kPhone;
    paired_device_properties->device_properties->connection_state =
        DeviceConnectionState::kConnected;
    paired_device_properties->device_properties->battery_info =
        DeviceBatteryInfo::New();
    paired_device_properties->device_properties->battery_info->left_bud_info =
        BatteryProperties::New();
    paired_device_properties->device_properties->battery_info->left_bud_info
        ->battery_percentage = 5;
    paired_device_properties->device_properties->battery_info->case_info =
        BatteryProperties::New();
    paired_device_properties->device_properties->battery_info->case_info
        ->battery_percentage = 64;
    paired_device_properties->device_properties->battery_info->right_bud_info =
        BatteryProperties::New();
    paired_device_properties->device_properties->battery_info->right_bud_info
        ->battery_percentage = 9;

    connected_devices_.push_back(mojo::Clone(paired_device_properties));
  }
  {
    PairedBluetoothDevicePropertiesPtr paired_device_properties =
        PairedBluetoothDeviceProperties::New();
    paired_device_properties->device_properties =
        BluetoothDeviceProperties::New();
    paired_device_properties->device_properties->id = "Game Controller";
    paired_device_properties->device_properties->public_name =
        u"Game Controller";
    paired_device_properties->device_properties->device_type =
        DeviceType::kGameController;

    connected_devices_.push_back(mojo::Clone(paired_device_properties));
  }
  {
    PairedBluetoothDevicePropertiesPtr paired_device_properties =
        PairedBluetoothDeviceProperties::New();
    paired_device_properties->device_properties =
        BluetoothDeviceProperties::New();
    paired_device_properties->device_properties->id = "Keyboard";
    paired_device_properties->device_properties->public_name = u"Keyboard";
    paired_device_properties->device_properties->device_type =
        DeviceType::kKeyboard;

    connected_devices_.push_back(mojo::Clone(paired_device_properties));
  }
  {
    PairedBluetoothDevicePropertiesPtr paired_device_properties =
        PairedBluetoothDeviceProperties::New();
    paired_device_properties->device_properties =
        BluetoothDeviceProperties::New();
    paired_device_properties->device_properties->id = "Mouse";
    paired_device_properties->device_properties->public_name = u"Mouse";
    paired_device_properties->device_properties->device_type =
        DeviceType::kMouse;

    connected_devices_.push_back(mojo::Clone(paired_device_properties));
  }
  {
    PairedBluetoothDevicePropertiesPtr paired_device_properties =
        PairedBluetoothDeviceProperties::New();
    paired_device_properties->device_properties =
        BluetoothDeviceProperties::New();
    paired_device_properties->device_properties->id = "Tablet";
    paired_device_properties->device_properties->public_name = u"Tablet";
    paired_device_properties->device_properties->device_type =
        DeviceType::kTablet;

    connected_devices_.push_back(mojo::Clone(paired_device_properties));
  }
  {
    PairedBluetoothDevicePropertiesPtr paired_device_properties =
        PairedBluetoothDeviceProperties::New();
    paired_device_properties->device_properties =
        BluetoothDeviceProperties::New();
    paired_device_properties->device_properties->id = "Headset";
    paired_device_properties->device_properties->public_name = u"Headset";
    paired_device_properties->device_properties->device_type =
        DeviceType::kHeadset;
    paired_device_properties->device_properties->connection_state =
        DeviceConnectionState::kConnecting;

    previously_connected_devices_.push_back(
        mojo::Clone(paired_device_properties));
  }
  {
    PairedBluetoothDevicePropertiesPtr paired_device_properties =
        PairedBluetoothDeviceProperties::New();
    paired_device_properties->device_properties =
        BluetoothDeviceProperties::New();
    paired_device_properties->device_properties->id = "Video Camera";
    paired_device_properties->device_properties->public_name =
        u"Video Camera with a very very very very very very very long name";
    paired_device_properties->device_properties->device_type =
        DeviceType::kVideoCamera;
    paired_device_properties->device_properties->connection_state =
        DeviceConnectionState::kNotConnected;

    previously_connected_devices_.push_back(
        mojo::Clone(paired_device_properties));
  }
#endif  // !BUILDFLAG(IS_CHROMEOS_DEVICE)
}

void BluetoothDetailedViewController::ShutDown() {
  device_list_controller_.reset();
}

}  // namespace ash
