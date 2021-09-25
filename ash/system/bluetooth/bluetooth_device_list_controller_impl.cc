// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/bluetooth/bluetooth_device_list_controller_impl.h"

#include "ash/constants/ash_features.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/bluetooth/bluetooth_detailed_view.h"
#include "ash/system/tray/tri_view.h"
#include "base/check.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/view.h"

namespace ash {

BluetoothDeviceListControllerImpl::BluetoothDeviceListControllerImpl(
    tray::BluetoothDetailedView* bluetooth_detailed_view)
    : bluetooth_detailed_view_(bluetooth_detailed_view) {
  DCHECK(ash::features::IsBluetoothRevampEnabled());
}

void BluetoothDeviceListControllerImpl::UpdateBluetoothEnabledState(
    bool enabled) {
  if (is_bluetooth_enabled_ && !enabled)
    bluetooth_detailed_view_->device_list()->RemoveAllChildViews();
  is_bluetooth_enabled_ = enabled;
}

void BluetoothDeviceListControllerImpl::UpdateDeviceList(
    const PairedBluetoothDevicePropertiesPtrs& connected,
    const PairedBluetoothDevicePropertiesPtrs& previously_connected) {
  DCHECK(is_bluetooth_enabled_);
  currently_connected_devices_sub_header_ = AddOrReorderSubHeader(
      currently_connected_devices_sub_header_,
      IDS_ASH_STATUS_TRAY_BLUETOOTH_CURRENTLY_CONNECTED_DEVICES, 1);
  previously_connected_devices_sub_header_ = AddOrReorderSubHeader(
      previously_connected_devices_sub_header_,
      IDS_ASH_STATUS_TRAY_BLUETOOTH_PREVIOUSLY_CONNECTED_DEVICES, 2);
  bluetooth_detailed_view_->NotifyDeviceListChanged();
}

TriView* BluetoothDeviceListControllerImpl::AddOrReorderSubHeader(
    TriView* sub_header,
    int text_id,
    int index) {
  if (!sub_header) {
    sub_header = bluetooth_detailed_view_->AddDeviceListSubHeader(
        gfx::kNoneIcon, text_id);
  }
  bluetooth_detailed_view_->device_list()->ReorderChildView(sub_header, index);
  return sub_header;
}

}  // namespace ash
