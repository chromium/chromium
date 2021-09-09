// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/bluetooth/bluetooth_detailed_view_impl.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/bluetooth/bluetooth_device_list_item_view.h"
#include "ash/system/bluetooth/bluetooth_disabled_detailed_view.h"
#include "ash/system/tray/hover_highlight_view.h"
#include "ash/system/tray/tray_toggle_button.h"
#include "ash/system/tray/tri_view.h"
#include "base/check.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/layout/box_layout.h"
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
  CreateTitleRowButtons();
  CreateScrollableList();
  AddScrollListSubHeader(kSystemMenuBluetoothPlusIcon,
                         IDS_ASH_STATUS_TRAY_BLUETOOTH_PAIR_NEW_DEVICE);
  CreateDisabledView();
  UpdateBluetoothEnabledState(/*enabled=*/false);
}

BluetoothDetailedViewImpl::~BluetoothDetailedViewImpl() = default;

views::View* BluetoothDetailedViewImpl::GetAsView() {
  return this;
}

void BluetoothDetailedViewImpl::UpdateBluetoothEnabledState(bool enabled) {
  disabled_view_->SetVisible(!enabled);
  scroller()->SetVisible(enabled);

  const std::u16string toggle_tooltip =
      enabled ? l10n_util::GetStringUTF16(
                    IDS_ASH_STATUS_TRAY_BLUETOOTH_ENABLED_TOOLTIP)
              : l10n_util::GetStringUTF16(
                    IDS_ASH_STATUS_TRAY_BLUETOOTH_DISABLED_TOOLTIP);
  toggle_->SetTooltipText(l10n_util::GetStringFUTF16(
      IDS_ASH_STATUS_TRAY_BLUETOOTH_TOGGLE_TOOLTIP, toggle_tooltip));

  // The toggle should already have animated to |enabled| unless it has become
  // out of sync with the Bluetooth state, so just set it to the correct state.
  if (toggle_->GetIsOn() != enabled)
    toggle_->SetIsOn(enabled);

  Layout();
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

void BluetoothDetailedViewImpl::CreateTitleRowButtons() {
  // TODO(crbug.com/1010321): Once we are able to determine whether we are
  // authorized to turn Bluetooth on or off using the API we should update this
  // to return early if not.

  DCHECK(!toggle_);

  tri_view()->SetContainerVisible(TriView::Container::END, /*visible=*/true);

  std::unique_ptr<TrayToggleButton> toggle = std::make_unique<TrayToggleButton>(
      base::BindRepeating(&BluetoothDetailedViewImpl::OnToggleClicked,
                          weak_factory_.GetWeakPtr()),
      IDS_ASH_STATUS_TRAY_BLUETOOTH);
  toggle->SetID(static_cast<int>(BluetoothDetailedViewChildId::kToggleButton));

  toggle_ = toggle.get();

  tri_view()->AddView(TriView::Container::END, toggle.release());
}

void BluetoothDetailedViewImpl::CreateDisabledView() {
  DCHECK(!disabled_view_);

  // Check that the views::ScrollView has already been created and insert the
  // disabled view before it to avoid the unnecessary bottom border spacing of
  // views::ScrollView when it is not the last child.
  DCHECK(scroller());

  disabled_view_ =
      AddChildViewAt(new BluetoothDisabledDetailedView, GetIndexOf(scroller()));
  disabled_view_->SetID(
      static_cast<int>(BluetoothDetailedViewChildId::kDisabledView));

  // Make |disabled_panel_| fill the entire space below the title row so that
  // the inner contents can be placed correctly.
  box_layout()->SetFlexForView(disabled_view_, 1);
}

void BluetoothDetailedViewImpl::OnToggleClicked() {
  const bool toggle_state = toggle_->GetIsOn();
  delegate()->OnToggleClicked(toggle_state);

  // Avoid the situation where there is a delay between the toggle becoming
  // enabled/disabled and Bluetooth becoming enabled/disabled by forcing the
  // view state to match the toggle state.
  UpdateBluetoothEnabledState(toggle_state);
}

}  // namespace tray
}  // namespace ash
