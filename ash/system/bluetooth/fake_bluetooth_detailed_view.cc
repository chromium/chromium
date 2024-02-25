// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/bluetooth/fake_bluetooth_detailed_view.h"

#include "ash/system/bluetooth/bluetooth_device_list_item_view.h"
#include "ash/system/tray/tri_view.h"
#include "chromeos/ash/services/bluetooth_config/public/cpp/cros_bluetooth_config_util.h"
#include "chromeos/ash/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/label.h"

namespace ash {

using bluetooth_config::IsBluetoothEnabledOrEnabling;
using bluetooth_config::mojom::BluetoothSystemState;

FakeBluetoothDetailedView::FakeBluetoothDetailedView(Delegate* delegate)
    : BluetoothDetailedView(delegate),
      device_list_(std::make_unique<views::View>()) {}

FakeBluetoothDetailedView::~FakeBluetoothDetailedView() = default;

views::View* FakeBluetoothDetailedView::GetAsView() {
  return this;
}

void FakeBluetoothDetailedView::UpdateBluetoothEnabledState(
    const BluetoothSystemState system_state) {
  last_bluetooth_enabled_state_ = IsBluetoothEnabledOrEnabling(system_state);
}

BluetoothDeviceListItemView* FakeBluetoothDetailedView::AddDeviceListItem() {
  return device_list_->AddChildView(
      new BluetoothDeviceListItemView(/*listener=*/nullptr));
}

views::View* FakeBluetoothDetailedView::AddDeviceListSubHeader(
    const gfx::VectorIcon& /*icon*/,
    int text_id) {
  std::unique_ptr<TriView> sub_header = std::make_unique<TriView>();
  sub_header->AddView(TriView::Container::CENTER,
                      new views::Label(l10n_util::GetStringUTF16(text_id)));
  device_list_->AddChildView(sub_header.get());
  return sub_header.release();
}

void FakeBluetoothDetailedView::NotifyDeviceListChanged() {
  notify_device_list_changed_call_count_++;
}

views::View* FakeBluetoothDetailedView::device_list() {
  return device_list_.get();
}

void FakeBluetoothDetailedView::OnViewClicked(views::View* view) {
  last_clicked_device_list_item_ =
      static_cast<BluetoothDeviceListItemView*>(view);
}

}  // namespace ash
