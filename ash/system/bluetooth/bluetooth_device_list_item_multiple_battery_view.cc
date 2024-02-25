// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/bluetooth/bluetooth_device_list_item_multiple_battery_view.h"

#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ash/system/tray/unfocusable_label.h"
#include "base/check.h"
#include "base/strings/string_number_conversions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view_utils.h"

namespace ash {

BluetoothDeviceListItemMultipleBatteryView::
    BluetoothDeviceListItemMultipleBatteryView() {
  auto box_layout = std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal);
  box_layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  SetLayoutManager(std::move(box_layout));
}

BluetoothDeviceListItemMultipleBatteryView::
    ~BluetoothDeviceListItemMultipleBatteryView() = default;

void BluetoothDeviceListItemMultipleBatteryView::UpdateBatteryInfo(
    const bluetooth_config::mojom::DeviceBatteryInfoPtr& battery_info) {
  int index = 0;
  if (battery_info->left_bud_info) {
    if (!left_bud_battery_view_) {
      left_bud_battery_view_ = AddChildViewAt(
          std::make_unique<BluetoothDeviceListItemBatteryView>(), index);
      index++;
    }

    left_bud_battery_view_->UpdateBatteryInfo(
        battery_info->left_bud_info->battery_percentage,
        IDS_ASH_STATUS_TRAY_BLUETOOTH_DEVICE_BATTERY_PERCENTAGE_LEFT_BUD_LABEL);
  } else if (left_bud_battery_view_) {
    RemoveChildViewT(left_bud_battery_view_.get());
    left_bud_battery_view_ = nullptr;
  }

  if (battery_info->case_info) {
    if (!case_battery_view_) {
      case_battery_view_ = AddChildViewAt(
          std::make_unique<BluetoothDeviceListItemBatteryView>(), index);
      index++;
    }

    case_battery_view_->UpdateBatteryInfo(
        battery_info->case_info->battery_percentage,
        IDS_ASH_STATUS_TRAY_BLUETOOTH_DEVICE_BATTERY_PERCENTAGE_CASE_LABEL);
  } else if (case_battery_view_) {
    RemoveChildViewT(case_battery_view_.get());
    case_battery_view_ = nullptr;
  }

  if (battery_info->right_bud_info) {
    if (!right_bud_battery_view_) {
      right_bud_battery_view_ = AddChildViewAt(
          std::make_unique<BluetoothDeviceListItemBatteryView>(), index);
      index++;
    }

    right_bud_battery_view_->UpdateBatteryInfo(
        battery_info->right_bud_info->battery_percentage,
        IDS_ASH_STATUS_TRAY_BLUETOOTH_DEVICE_BATTERY_PERCENTAGE_RIGHT_BUD_LABEL);
  } else if (right_bud_battery_view_) {
    RemoveChildViewT(right_bud_battery_view_.get());
    right_bud_battery_view_ = nullptr;
  }
}

BEGIN_METADATA(BluetoothDeviceListItemMultipleBatteryView)
END_METADATA

}  // namespace ash
