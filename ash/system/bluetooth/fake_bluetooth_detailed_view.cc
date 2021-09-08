// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/bluetooth/fake_bluetooth_detailed_view.h"

#include "ash/system/bluetooth/bluetooth_device_list_item_view.h"
#include "ash/system/tray/tri_view.h"

namespace ash {
namespace tray {

FakeBluetoothDetailedView::FakeBluetoothDetailedView(Delegate* delegate)
    : BluetoothDetailedView(delegate),
      device_list_(std::make_unique<views::ScrollView>()) {}

FakeBluetoothDetailedView::~FakeBluetoothDetailedView() = default;

views::View* FakeBluetoothDetailedView::GetAsView() {
  return nullptr;
}

void FakeBluetoothDetailedView::UpdateBluetoothEnabledState(bool enabled) {
  last_bluetooth_enabled_state_ = enabled;
}

BluetoothDeviceListItemView* FakeBluetoothDetailedView::AddDeviceListItem() {
  return device_list_->AddChildView(
      new BluetoothDeviceListItemView(/*listener=*/nullptr));
}

ash::TriView* FakeBluetoothDetailedView::AddDeviceListSubHeader(
    const gfx::VectorIcon&,
    int) {
  return device_list_->AddChildView(new ash::TriView());
}

void FakeBluetoothDetailedView::NotifyDeviceListChanged() {
  notify_device_list_changed_call_count_++;
}

views::View* FakeBluetoothDetailedView::device_list() {
  return device_list_->contents();
}

}  // namespace tray
}  // namespace ash
