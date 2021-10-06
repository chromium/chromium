// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/bluetooth/bluetooth_device_list_item_view.h"

#include "ash/constants/ash_features.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/bluetooth/bluetooth_device_list_item_battery_view.h"
#include "ash/system/tray/tray_utils.h"
#include "base/check.h"
#include "chromeos/services/bluetooth_config/public/cpp/cros_bluetooth_config_util.h"
#include "mojo/public/cpp/bindings/clone_traits.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/controls/label.h"

namespace ash {
namespace {

using chromeos::bluetooth_config::GetPairedDeviceName;
using chromeos::bluetooth_config::mojom::DeviceBatteryInfoPtr;
using chromeos::bluetooth_config::mojom::DeviceConnectionState;
using chromeos::bluetooth_config::mojom::DeviceType;
using chromeos::bluetooth_config::mojom::PairedBluetoothDevicePropertiesPtr;

// Returns the icon corresponding to the provided |device_type| and
// |connection_state|.
const gfx::VectorIcon& GetDeviceIcon(const DeviceType device_type) {
  switch (device_type) {
    case DeviceType::kComputer:
      return ash::kSystemMenuComputerIcon;
    case DeviceType::kPhone:
      return ash::kSystemMenuPhoneIcon;
    case DeviceType::kHeadset:
      return ash::kSystemMenuHeadsetIcon;
    case DeviceType::kVideoCamera:
      return ash::kSystemMenuVideocamIcon;
    case DeviceType::kGameController:
      return ash::kSystemMenuGamepadIcon;
    case DeviceType::kKeyboard:
      return ash::kSystemMenuKeyboardIcon;
    case DeviceType::kMouse:
      return ash::kSystemMenuMouseIcon;
    case DeviceType::kTablet:
      return ash::kSystemMenuTabletIcon;
    case DeviceType::kUnknown:
      return ash::kSystemMenuBluetoothIcon;
  }
}

}  // namespace

BluetoothDeviceListItemView::BluetoothDeviceListItemView(
    ViewClickListener* listener)
    : HoverHighlightView(listener) {
  DCHECK(ash::features::IsBluetoothRevampEnabled());
}

BluetoothDeviceListItemView::~BluetoothDeviceListItemView() = default;

void BluetoothDeviceListItemView::UpdateDeviceProperties(
    const PairedBluetoothDevicePropertiesPtr& device_properties) {
  device_properties_ = mojo::Clone(device_properties);

  // We can only add an icon and label if the view has not already been
  // populated with one or both of these views. For simplicity, instead of
  // trying to determine which views exist and modifying them, and creating the
  // missing views, we instead clear all of the views and recreate them.
  if (is_populated())
    Reset();

  const DeviceType& device_type =
      device_properties_->device_properties->device_type;

  AddIconAndLabel(
      gfx::CreateVectorIcon(
          GetDeviceIcon(device_type),
          AshColorProvider::Get()->GetContentLayerColor(
              AshColorProvider::ContentLayerType::kIconColorPrimary)),
      GetPairedDeviceName(device_properties_.get()));

  const DeviceConnectionState& connection_state =
      device_properties_->device_properties->connection_state;

  // Adds a sub-label to show that the device is in the process of connecting.
  if (connection_state == DeviceConnectionState::kConnecting) {
    SetupConnectingScrollListItem(this);
  } else if (connection_state == DeviceConnectionState::kConnected) {
    const DeviceBatteryInfoPtr& battery_info =
        device_properties_->device_properties->battery_info;

    if (!battery_info || !battery_info->default_properties) {
      sub_row()->RemoveAllChildViews();
      return;
    }
    if (!battery_view_) {
      battery_view_ = sub_row()->AddChildView(
          std::make_unique<BluetoothDeviceListItemBatteryView>());
    }
    battery_view_->UpdateBatteryInfo(battery_info->default_properties);
  }
}

const char* BluetoothDeviceListItemView::GetClassName() const {
  return "BluetoothDeviceListItemView";
}

}  // namespace ash
