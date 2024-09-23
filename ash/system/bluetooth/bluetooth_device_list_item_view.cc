// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/bluetooth/bluetooth_device_list_item_view.h"

#include <string>
#include <string_view>

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/typography.h"
#include "ash/system/bluetooth/bluetooth_device_list_item_battery_view.h"
#include "ash/system/bluetooth/bluetooth_device_list_item_multiple_battery_view.h"
#include "ash/system/tray/hover_highlight_view.h"
#include "ash/system/tray/tray_utils.h"
#include "base/check.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "chromeos/ash/services/bluetooth_config/public/cpp/cros_bluetooth_config_util.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "chromeos/ui/vector_icons/vector_icons.h"
#include "mojo/public/cpp/bindings/clone_traits.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/label.h"

namespace ash {

namespace {

using bluetooth_config::GetPairedDeviceName;
using bluetooth_config::mojom::BatteryPropertiesPtr;
using bluetooth_config::mojom::DeviceBatteryInfoPtr;
using bluetooth_config::mojom::DeviceConnectionState;
using bluetooth_config::mojom::DeviceType;
using bluetooth_config::mojom::PairedBluetoothDevicePropertiesPtr;

constexpr int kEnterpriseManagedIconSizeDip = 20;

bool HasMultipleBatteryInfos(const DeviceBatteryInfoPtr& battery_info) {
  DCHECK(battery_info);
  return battery_info->left_bud_info || battery_info->case_info ||
         battery_info->right_bud_info;
}

// Returns the text ID corresponding to the provided |device_connection_state|.
int GetDeviceConnectionStateA11yTextId(
    const DeviceConnectionState device_connection_state) {
  switch (device_connection_state) {
    case DeviceConnectionState::kConnected:
      return IDS_BLUETOOTH_A11Y_DEVICE_CONNECTION_STATE_CONNECTED;
    case DeviceConnectionState::kConnecting:
      return IDS_BLUETOOTH_A11Y_DEVICE_CONNECTION_STATE_CONNECTING;
    case DeviceConnectionState::kNotConnected:
      return IDS_BLUETOOTH_A11Y_DEVICE_CONNECTION_STATE_NOT_CONNECTED;
  }
  NOTREACHED();
}

// Returns the text ID corresponding to the provided |device_type|.
int GetDeviceTypeA11yTextId(const DeviceType device_type) {
  switch (device_type) {
    case DeviceType::kComputer:
      return IDS_BLUETOOTH_A11Y_DEVICE_TYPE_COMPUTER;
    case DeviceType::kPhone:
      return IDS_BLUETOOTH_A11Y_DEVICE_TYPE_PHONE;
    case DeviceType::kHeadset:
      return IDS_BLUETOOTH_A11Y_DEVICE_TYPE_HEADSET;
    case DeviceType::kVideoCamera:
      return IDS_BLUETOOTH_A11Y_DEVICE_TYPE_VIDEO_CAMERA;
    case DeviceType::kGameController:
      return IDS_BLUETOOTH_A11Y_DEVICE_TYPE_GAME_CONTROLLER;
    case DeviceType::kKeyboard:
      return IDS_BLUETOOTH_A11Y_DEVICE_TYPE_KEYBOARD;
    case DeviceType::kKeyboardMouseCombo:
      return IDS_BLUETOOTH_A11Y_DEVICE_TYPE_KEYBOARD_MOUSE_COMBO;
    case DeviceType::kMouse:
      return IDS_BLUETOOTH_A11Y_DEVICE_TYPE_MOUSE;
    case DeviceType::kTablet:
      return IDS_BLUETOOTH_A11Y_DEVICE_TYPE_TABLET;
    case DeviceType::kUnknown:
      return IDS_BLUETOOTH_A11Y_DEVICE_TYPE_UNKNOWN;
  }
  NOTREACHED();
}

// Returns the formatted a11y text describing the battery information of the
// provided |battery_info|.
const std::u16string GetDeviceBatteryA11yText(
    const DeviceBatteryInfoPtr& battery_info) {
  if (!battery_info) {
    return std::u16string();
  }

  if (HasMultipleBatteryInfos(battery_info)) {
    std::u16string battery_text;

    auto add_battery_text_if_exists =
        [&battery_text](const BatteryPropertiesPtr& battery_properties,
                        int text_id) {
          if (!battery_properties) {
            return;
          }
          if (!battery_text.empty()) {
            battery_text = base::StrCat({battery_text, u" "});
          }
          battery_text = base::StrCat(
              {battery_text,
               l10n_util::GetStringFUTF16(
                   text_id, base::NumberToString16(
                                battery_properties->battery_percentage))});
        };

    add_battery_text_if_exists(
        battery_info->left_bud_info,
        IDS_BLUETOOTH_A11Y_DEVICE_NAMED_BATTERY_INFO_LEFT_BUD);
    add_battery_text_if_exists(
        battery_info->case_info,
        IDS_BLUETOOTH_A11Y_DEVICE_NAMED_BATTERY_INFO_CASE);
    add_battery_text_if_exists(
        battery_info->right_bud_info,
        IDS_BLUETOOTH_A11Y_DEVICE_NAMED_BATTERY_INFO_RIGHT_BUD);

    return battery_text;
  }

  if (battery_info->default_properties) {
    return l10n_util::GetStringFUTF16(
        IDS_BLUETOOTH_A11Y_DEVICE_BATTERY_INFO,
        base::NumberToString16(
            battery_info->default_properties->battery_percentage));
  }
  return std::u16string();
}

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
    case DeviceType::kKeyboardMouseCombo:
      return ash::kSystemMenuKeyboardIcon;
    case DeviceType::kMouse:
      return ash::kSystemMenuMouseIcon;
    case DeviceType::kTablet:
      return ash::kSystemMenuTabletIcon;
    case DeviceType::kUnknown:
      return ash::kSystemMenuBluetoothIcon;
  }
  NOTREACHED();
}

}  // namespace

BluetoothDeviceListItemView::BluetoothDeviceListItemView(
    ViewClickListener* listener)
    : HoverHighlightView(listener) {}

BluetoothDeviceListItemView::~BluetoothDeviceListItemView() = default;

void BluetoothDeviceListItemView::UpdateDeviceProperties(
    size_t device_index,
    size_t total_device_count,
    const PairedBluetoothDevicePropertiesPtr& device_properties) {
  device_properties_ = mojo::Clone(device_properties);

  // We can only add an icon and label if the view has not already been
  // populated with one or both of these views. For simplicity, instead of
  // trying to determine which views exist and modifying them, and creating the
  // missing views, we instead clear all of the views and recreate them.
  if (is_populated()) {
    Reset();
  }

  const DeviceType& device_type =
      device_properties_->device_properties->device_type;

  AddIconAndLabel(ui::ImageModel::FromVectorIcon(
                      GetDeviceIcon(device_type),
                      static_cast<ui::ColorId>(cros_tokens::kCrosSysOnSurface)),
                  GetPairedDeviceName(device_properties_));
  text_label()->SetEnabledColorId(cros_tokens::kCrosSysOnSurface);
  TypographyProvider::Get()->StyleLabel(ash::TypographyToken::kCrosButton2,
                                        *text_label());

  UpdateAccessibleName(device_index, total_device_count);

  // Adds an icon to indicate that the device supports profiles or services that
  // are disabled or blocked by enterprise policy.
  if (device_properties->device_properties->is_blocked_by_policy) {
    AddRightIcon(ui::ImageModel::FromVectorIcon(
                     chromeos::kEnterpriseIcon,
                     static_cast<ui::ColorId>(cros_tokens::kCrosSysOnSurface),
                     kEnterpriseManagedIconSizeDip),
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

void BluetoothDeviceListItemView::UpdateAccessibleName(
    size_t device_index,
    size_t total_device_count) {
  DCHECK(device_properties_);

  // It is not best practice to concatenate translated strings together, but in
  // this case we would have an explosion in the number of strings if we had a
  // unique string for each permutation. Instead, below we concatenate related
  // but complete sentences here with a hard stop, e.g. a period.

  // Add the device name information.
  std::u16string a11y_text = l10n_util::GetStringFUTF16(
      IDS_BLUETOOTH_A11Y_DEVICE_NAME, base::NumberToString16(device_index + 1),
      base::NumberToString16(total_device_count),
      GetPairedDeviceName(device_properties_));

  // Add the device connection status information.
  a11y_text = base::StrCat(
      {a11y_text, u" ",
       l10n_util::GetStringUTF16(GetDeviceConnectionStateA11yTextId(
           device_properties_->device_properties->connection_state))});

  // Add the device type information.
  a11y_text =
      base::StrCat({a11y_text, u" ",
                    l10n_util::GetStringUTF16(GetDeviceTypeA11yTextId(
                        device_properties_->device_properties->device_type))});

  const std::u16string battery_text = GetDeviceBatteryA11yText(
      device_properties_->device_properties->battery_info);

  if (!battery_text.empty()) {
    a11y_text = base::StrCat({a11y_text, u" ", battery_text});
  }

  GetViewAccessibility().SetName(a11y_text);
}

void BluetoothDeviceListItemView::UpdateBatteryInfo(
    const DeviceBatteryInfoPtr& battery_info) {
  if (!battery_info || (!battery_info->default_properties &&
                        !HasMultipleBatteryInfos(battery_info))) {
    sub_row()->RemoveAllChildViews();
    return;
  }

  if (HasMultipleBatteryInfos(battery_info)) {
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
    if (std::string_view(sub_row()->children().at(0)->GetClassName()) !=
        std::string_view(
            BluetoothDeviceListItemMultipleBatteryView::kViewClassName)) {
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
    if (std::string_view(sub_row()->children().at(0)->GetClassName()) !=
        std::string_view(BluetoothDeviceListItemBatteryView::kViewClassName)) {
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

BEGIN_METADATA(BluetoothDeviceListItemView)
END_METADATA

}  // namespace ash
