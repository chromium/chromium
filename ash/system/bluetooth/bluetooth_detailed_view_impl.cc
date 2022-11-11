// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/bluetooth/bluetooth_detailed_view_impl.h"

#include <memory>
#include <utility>

#include "ash/public/cpp/system_tray_client.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/rounded_container.h"
#include "ash/system/bluetooth/bluetooth_device_list_item_view.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/tray/detailed_view_delegate.h"
#include "ash/system/tray/hover_highlight_view.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ash/system/tray/tri_view.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "device/bluetooth/chromeos/bluetooth_utils.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"

namespace ash {
namespace {

constexpr auto kPairNewDeviceIconMargins = gfx::Insets::TLBR(0, 2, 0, 0);

}  // namespace

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

  // Handle clicks on the "pair new device" row.
  if (view == pair_new_device_view_) {
    delegate()->OnPairNewDeviceRequested();
    return;
  }

  // Handle clicks on Bluetooth devices.
  DCHECK(views::IsViewClass<BluetoothDeviceListItemView>(view));
  delegate()->OnDeviceListItemSelected(
      static_cast<BluetoothDeviceListItemView*>(view)->device_properties());
}

void BluetoothDetailedViewImpl::CreateTitleSettingsButton() {
  DCHECK(!settings_button_);

  tri_view()->SetContainerVisible(TriView::Container::END, /*visible=*/true);

  settings_button_ = CreateSettingsButton(
      base::BindRepeating(&BluetoothDetailedViewImpl::OnSettingsClicked,
                          weak_factory_.GetWeakPtr()),
      IDS_ASH_STATUS_TRAY_BLUETOOTH_SETTINGS);
  settings_button_->SetState(TrayPopupUtils::CanOpenWebUISettings()
                                 ? views::Button::STATE_NORMAL
                                 : views::Button::STATE_DISABLED);
  tri_view()->AddView(TriView::Container::END, settings_button_);
}

void BluetoothDetailedViewImpl::CreateTopContainer() {
  // TODO(b/252872600): Implement.
}

void BluetoothDetailedViewImpl::CreateMainContainer() {
  DCHECK(!main_container_);
  main_container_ =
      scroll_content()->AddChildView(std::make_unique<RoundedContainer>(
          RoundedContainer::Behavior::kBottomRounded));

  // Add a row for "pair new device".
  pair_new_device_view_ = main_container_->AddChildView(
      std::make_unique<HoverHighlightView>(/*listener=*/this));

  // Create the "+" icon.
  auto icon = std::make_unique<views::ImageView>();
  icon->SetImage(ui::ImageModel::FromVectorIcon(kSystemMenuBluetoothPlusIcon,
                                                cros_tokens::kCrosSysPrimary));
  icon->SetProperty(views::kMarginsKey, kPairNewDeviceIconMargins);
  pair_new_device_icon_ = icon.get();
  pair_new_device_view_->AddViewAndLabel(
      std::move(icon),
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_BLUETOOTH_PAIR_NEW_DEVICE));

  views::Label* label = pair_new_device_view_->text_label();
  label->SetEnabledColorId(cros_tokens::kCrosSysPrimary);
  // TODO(b/252872600): Apply the correct font to the label.
  TrayPopupUtils::SetLabelFontList(
      label, TrayPopupUtils::FontStyle::kDetailedViewLabel);

  // The device list is a separate view because it cannot contain the "pair new
  // device" row.
  device_list_ = main_container_->AddChildView(std::make_unique<views::View>());
  device_list_->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
}

void BluetoothDetailedViewImpl::OnSettingsClicked() {
  CloseBubble();  // Deletes |this|.
  Shell::Get()->system_tray_model()->client()->ShowBluetoothSettings();
}

BEGIN_METADATA(BluetoothDetailedViewImpl, TrayDetailedView)
END_METADATA

}  // namespace ash
