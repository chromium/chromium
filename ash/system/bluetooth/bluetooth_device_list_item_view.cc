// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/bluetooth/bluetooth_device_list_item_view.h"

#include "ash/constants/ash_features.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/bluetooth/bluetooth_device_list_item_battery_view.h"
#include "ash/system/bluetooth/bluetooth_device_list_item_multiple_battery_view.h"
#include "ash/system/tray/tray_utils.h"
#include "base/check.h"
#include "chromeos/services/bluetooth_config/public/cpp/cros_bluetooth_config_util.h"
#include "chromeos/ui/vector_icons/vector_icons.h"
#include "mojo/public/cpp/bindings/clone_traits.h"
#include "ui/gfx/color_palette.h"
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

constexpr int kEnterpriseManagedIconSizeDip = 20;

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

bool HasMultipleBatteries(
    const chromeos::bluetooth_config::mojom::DeviceBatteryInfoPtr&
        battery_info) {
  return battery_info->left_bud_info || battery_info->case_info ||
         battery_info->right_bud_info;
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

  // Adds an icon to indicate that the device supports profiles or services that
  // are disabled or blocked by enterprise policy.
  if (device_properties->device_properties->is_blocked_by_policy) {
    AddRightIcon(
        CreateVectorIcon(chromeos::kEnterpriseIcon,
                         kEnterpriseManagedIconSizeDip, gfx::kGoogleGrey100),
        /*icon_size=*/kEnterpriseManagedIconSizeDip);
  }

  const DeviceConnectionState& connection_state =
      device_properties_->device_properties->connection_state;

  // Adds a sub-label to show that the device is in the process of connecting.
  if (connection_state == DeviceConnectionState::kConnecting) {
    SetupConnectingScrollListItem(this);
  } else if (connection_state == DeviceConnectionState::kConnected) {
    UpdateBatteryInfo(device_properties_->device_properties->battery_info);
  }
}

void BluetoothDeviceListItemView::UpdateBatteryInfo(
    const DeviceBatteryInfoPtr& battery_info) {
  if (!battery_info || (!battery_info->default_properties &&
                        !HasMultipleBatteries(battery_info))) {
    sub_row()->RemoveAllChildViews();
    return;
  }

  if (HasMultipleBatteries(battery_info)) {
    UpdateMultipleBatteryView(battery_info);
    return;
  }

  UpdateSingleBatteryView(battery_info);
}

void BluetoothDeviceListItemView::UpdateMultipleBatteryView(
    const DeviceBatteryInfoPtr& battery_info) {
  // Remove battery view if it is not a multiple battery view.
  if (!sub_row()->children().empty()) {
    DCHECK(sub_row()->children().size() == 1);
    if (sub_row()->children().at(0)->GetClassName() !=
        BluetoothDeviceListItemMultipleBatteryView::kViewClassName) {
      sub_row()->RemoveAllChildViews();
    }
  }

  BluetoothDeviceListItemMultipleBatteryView* battery_view = nullptr;

  // Add multiple battery view if missing.
  if (sub_row()->children().empty()) {
    battery_view = sub_row()->AddChildView(
        std::make_unique<BluetoothDeviceListItemMultipleBatteryView>());
  } else {
    DCHECK_EQ(1u, sub_row()->children().size());
    battery_view = static_cast<BluetoothDeviceListItemMultipleBatteryView*>(
        sub_row()->children().at(0));
  }

  // Update multiple battery view.
  battery_view->UpdateBatteryInfo(battery_info);
}

void BluetoothDeviceListItemView::UpdateSingleBatteryView(
    const DeviceBatteryInfoPtr& battery_info) {
  // Remove battery view if it is not a single battery view.
  if (!sub_row()->children().empty()) {
    DCHECK(sub_row()->children().size() == 1);
    if (sub_row()->children().at(0)->GetClassName() !=
        BluetoothDeviceListItemBatteryView::kViewClassName) {
      sub_row()->RemoveAllChildViews();
    }
  }

  BluetoothDeviceListItemBatteryView* battery_view = nullptr;

  // Add single battery view if missing.
  if (sub_row()->children().empty()) {
    battery_view = sub_row()->AddChildView(
        std::make_unique<BluetoothDeviceListItemBatteryView>());
  } else {
    DCHECK_EQ(1u, sub_row()->children().size());
    battery_view = static_cast<BluetoothDeviceListItemBatteryView*>(
        sub_row()->children().at(0));
  }

  // Update single battery view.
  battery_view->UpdateBatteryInfo(
      battery_info->default_properties->battery_percentage,
      IDS_ASH_STATUS_TRAY_BLUETOOTH_DEVICE_BATTERY_PERCENTAGE_ONLY_LABEL);
}

const char* BluetoothDeviceListItemView::GetClassName() const {
  return "BluetoothDeviceListItemView";
}

}  // namespace ash
