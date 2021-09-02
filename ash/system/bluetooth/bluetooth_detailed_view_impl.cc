// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/bluetooth/bluetooth_detailed_view_impl.h"

#include "ash/constants/ash_features.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/bluetooth/bluetooth_device_list_item_view.h"
#include "ash/system/bluetooth/bluetooth_disabled_detailed_view.h"
#include "ash/system/tray/hover_highlight_view.h"
#include "ash/system/tray/tri_view.h"
#include "base/check.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/view.h"

namespace ash {
namespace tray {

BluetoothDetailedViewImpl::BluetoothDetailedViewImpl(
    DetailedViewDelegate* detailed_view_delegate,
    BluetoothDetailedView::Delegate* delegate)
    : BluetoothDetailedView(delegate),
      TrayDetailedView(detailed_view_delegate) {
  DCHECK(ash::features::IsBluetoothRevampEnabled());
  CreateTitleRow(IDS_ASH_STATUS_TRAY_BLUETOOTH);
  CreateScrollableList();
  AddScrollListSubHeader(kSystemMenuBluetoothPlusIcon,
                         IDS_ASH_STATUS_TRAY_BLUETOOTH_PAIR_NEW_DEVICE);
  disabled_view_ =
      AddChildViewAt(new BluetoothDisabledDetailedView, GetIndexOf(scroller()));
}

BluetoothDetailedViewImpl::~BluetoothDetailedViewImpl() = default;

void BluetoothDetailedViewImpl::SetBluetoothToggleState(bool enabled) {
  if (enabled == should_toggle_be_enabled_)
    return;
  should_toggle_be_enabled_ = enabled;
  disabled_view_->SetVisible(!should_toggle_be_enabled_);
  scroller()->SetVisible(should_toggle_be_enabled_);
  Layout();
}

views::View* BluetoothDetailedViewImpl::GetAsView() {
  return this;
}

BluetoothDeviceListItemView* BluetoothDetailedViewImpl::AddDeviceListItem() {
  return scroll_content()->AddChildView(
      new BluetoothDeviceListItemView(/*listener=*/this));
}

ash::TriView* BluetoothDetailedViewImpl::AddDeviceListSubHeader(
    const gfx::VectorIcon& icon,
    int text_id) {
  return AddScrollListSubHeader(icon, text_id);
}

void BluetoothDetailedViewImpl::NotifyDeviceListChanged() {
  scroller()->InvalidateLayout();
  Layout();
}

views::View* BluetoothDetailedViewImpl::device_list() {
  return scroll_content();
}

const char* BluetoothDetailedViewImpl::GetClassName() const {
  return "BluetoothDetailedViewImpl";
}

}  // namespace tray
}  // namespace ash
