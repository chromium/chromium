// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/bluetooth/bluetooth_detailed_view_impl.h"

#include <memory>
#include <utility>

#include "ash/strings/grit/ash_strings.h"
#include "ash/style/rounded_container.h"
#include "ash/system/bluetooth/bluetooth_device_list_item_view.h"
#include "ash/system/tray/detailed_view_delegate.h"
#include "ash/system/tray/tri_view.h"
#include "base/check.h"
#include "device/bluetooth/chromeos/bluetooth_utils.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"
#include "ui/views/view_utils.h"

namespace ash {

BluetoothDetailedViewImpl::BluetoothDetailedViewImpl(
    DetailedViewDelegate* detailed_view_delegate,
    BluetoothDetailedView::Delegate* delegate)
    : BluetoothDetailedView(delegate),
      TrayDetailedView(detailed_view_delegate) {
  CreateTitleRow(IDS_ASH_STATUS_TRAY_BLUETOOTH);
  CreateTitleSettingsButton();
  CreateScrollableList();
  CreateTopContainer();
  CreateMainContainer();
  UpdateBluetoothEnabledState(/*enabled=*/false);
  device::RecordUiSurfaceDisplayed(
      device::BluetoothUiSurface::kBluetoothQuickSettings);
}

BluetoothDetailedViewImpl::~BluetoothDetailedViewImpl() = default;

views::View* BluetoothDetailedViewImpl::GetAsView() {
  return this;
}

void BluetoothDetailedViewImpl::UpdateBluetoothEnabledState(bool enabled) {
  main_container_->SetVisible(enabled);
  InvalidateLayout();
}

BluetoothDeviceListItemView* BluetoothDetailedViewImpl::AddDeviceListItem() {
  return device_list_->AddChildView(
      std::make_unique<BluetoothDeviceListItemView>(/*listener=*/this));
}

TriView* BluetoothDetailedViewImpl::AddDeviceListSubHeader(
    const gfx::VectorIcon& icon,
    int text_id) {
  // TODO(b/252872600): Update styling to match spec.
  return AddScrollListSubHeader(device_list_, icon, text_id);
}

void BluetoothDetailedViewImpl::NotifyDeviceListChanged() {
  device_list_->InvalidateLayout();
  Layout();
}

views::View* BluetoothDetailedViewImpl::device_list() {
  return device_list_;
}

void BluetoothDetailedViewImpl::HandleViewClicked(views::View* view) {
  // TODO(b/252872600): Handle on/off toggle.
  // TODO(b/252872600): Handle "pair new device" row.

  DCHECK(views::IsViewClass<BluetoothDeviceListItemView>(view));
  delegate()->OnDeviceListItemSelected(
      static_cast<BluetoothDeviceListItemView*>(view)->device_properties());
}

void BluetoothDetailedViewImpl::CreateTitleSettingsButton() {
  // TODO(b/252872600): Implement.
}

void BluetoothDetailedViewImpl::CreateTopContainer() {
  // TODO(b/252872600): Implement.
}

void BluetoothDetailedViewImpl::CreateMainContainer() {
  DCHECK(!main_container_);
  main_container_ =
      scroll_content()->AddChildView(std::make_unique<RoundedContainer>(
          RoundedContainer::Behavior::kBottomRounded));

  // TODO(b/252872600): Add "pair new device" row.

  // The device list is a separate view because it cannot contain the "pair new
  // device" row.
  device_list_ = main_container_->AddChildView(std::make_unique<views::View>());
  device_list_->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
}

BEGIN_METADATA(BluetoothDetailedViewImpl, TrayDetailedView)
END_METADATA

}  // namespace ash
