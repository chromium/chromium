// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/bluetooth/bluetooth_detailed_view_legacy.h"

#include <memory>
#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/system_tray_client.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/icon_button.h"
#include "ash/system/bluetooth/bluetooth_device_list_item_view.h"
#include "ash/system/bluetooth/bluetooth_disabled_detailed_view.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/tray/detailed_view_delegate.h"
#include "ash/system/tray/hover_highlight_view.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ash/system/tray/tray_toggle_button.h"
#include "ash/system/tray/tri_view.h"
#include "base/check.h"
#include "base/memory/ptr_util.h"
#include "chromeos/ash/services/bluetooth_config/public/cpp/cros_bluetooth_config_util.h"
#include "chromeos/ash/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom.h"
#include "device/bluetooth/chromeos/bluetooth_utils.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"

namespace ash {

using bluetooth_config::IsBluetoothEnabled;
using bluetooth_config::IsBluetoothEnabledOrEnabling;
using bluetooth_config::mojom::BluetoothSystemState;

BluetoothDetailedViewLegacy::BluetoothDetailedViewLegacy(
    DetailedViewDelegate* detailed_view_delegate,
    BluetoothDetailedView::Delegate* delegate)
    : BluetoothDetailedView(delegate),
      TrayDetailedView(detailed_view_delegate) {
  DCHECK(!features::IsQsRevampEnabled());
  CreateTitleRow(IDS_ASH_STATUS_TRAY_BLUETOOTH);
  CreateTitleRowButtons();
  CreateScrollableList();
  CreateDisabledView();
  CreatePairNewDeviceView();
  UpdateBluetoothEnabledState(BluetoothSystemState::kDisabled);
  device::RecordUiSurfaceDisplayed(
      device::BluetoothUiSurface::kBluetoothQuickSettings);
}

BluetoothDetailedViewLegacy::~BluetoothDetailedViewLegacy() = default;

views::View* BluetoothDetailedViewLegacy::GetAsView() {
  return this;
}

void BluetoothDetailedViewLegacy::UpdateBluetoothEnabledState(
    const BluetoothSystemState system_state) {
  bool is_enabled_or_enabling = IsBluetoothEnabledOrEnabling(system_state);
  disabled_view_->SetVisible(!is_enabled_or_enabling);
  scroller()->SetVisible(is_enabled_or_enabling);

  // Only show "Pair new device" button if bluetooth is enabled.
  // see (b/297044914)
  pair_new_device_view_->SetVisible(IsBluetoothEnabled(system_state));

  const std::u16string toggle_tooltip =
      is_enabled_or_enabling
          ? l10n_util::GetStringUTF16(
                IDS_ASH_STATUS_TRAY_BLUETOOTH_ENABLED_TOOLTIP)
          : l10n_util::GetStringUTF16(
                IDS_ASH_STATUS_TRAY_BLUETOOTH_DISABLED_TOOLTIP);
  toggle_button_->SetTooltipText(l10n_util::GetStringFUTF16(
      IDS_ASH_STATUS_TRAY_BLUETOOTH_TOGGLE_TOOLTIP, toggle_tooltip));

  // The toggle should already have animated to |is_enabled_or_enabling| unless
  // it has become out of sync with the Bluetooth state, so just set it to the
  // correct state.
  if (toggle_button_->GetIsOn() != is_enabled_or_enabling) {
    toggle_button_->SetIsOn(is_enabled_or_enabling);
  }

  Layout();
}

BluetoothDeviceListItemView* BluetoothDetailedViewLegacy::AddDeviceListItem() {
  return scroll_content()->AddChildView(
      new BluetoothDeviceListItemView(/*listener=*/this));
}

views::View* BluetoothDetailedViewLegacy::AddDeviceListSubHeader(
    const gfx::VectorIcon& icon,
    int text_id) {
  return AddScrollListSubHeader(scroll_content(), icon, text_id);
}

void BluetoothDetailedViewLegacy::NotifyDeviceListChanged() {
  scroll_content()->InvalidateLayout();
  Layout();
}

views::View* BluetoothDetailedViewLegacy::device_list() {
  return scroll_content();
}

void BluetoothDetailedViewLegacy::HandleViewClicked(views::View* view) {
  // We only handle clicks on the "pair new device" view and on the individual
  // device views. When |view| is a child of |pair_new_device_view_| we know the
  // "pair new device" button was clicked, otherwise it must have been an
  // individual device view.
  if (pair_new_device_view_->GetIndexOf(view).has_value()) {
    delegate()->OnPairNewDeviceRequested();
    return;
  }
  delegate()->OnDeviceListItemSelected(
      static_cast<BluetoothDeviceListItemView*>(view)->device_properties());
}

const char* BluetoothDetailedViewLegacy::GetClassName() const {
  return "BluetoothDetailedViewLegacy";
}

void BluetoothDetailedViewLegacy::CreateDisabledView() {
  DCHECK(!disabled_view_);

  // Check that the views::ScrollView has already been created so that we can
  // insert the disabled view before it to avoid the unnecessary bottom border
  // spacing of views::ScrollView when it is not the last child.
  DCHECK(scroller());

  disabled_view_ = AddChildViewAt(new BluetoothDisabledDetailedView,
                                  GetIndexOf(scroller()).value());
  disabled_view_->SetID(
      static_cast<int>(BluetoothDetailedViewChildId::kDisabledView));

  // Make |disabled_panel_| fill the entire space below the title row so that
  // the inner contents can be placed correctly.
  box_layout()->SetFlexForView(disabled_view_, 1);
}

void BluetoothDetailedViewLegacy::CreatePairNewDeviceView() {
  DCHECK(!pair_new_device_view_);

  // Check that the views::ScrollView has already been created so that we can
  // insert the "pair new device" before it.
  DCHECK(scroller());

  pair_new_device_view_ =
      AddChildViewAt(new views::View(), GetIndexOf(scroller()).value());
  pair_new_device_view_->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  pair_new_device_view_->SetID(
      static_cast<int>(BluetoothDetailedViewChildId::kPairNewDeviceView));

  std::unique_ptr<HoverHighlightView> hover_highlight_view =
      std::make_unique<HoverHighlightView>(/*listener=*/this);
  hover_highlight_view->SetID(static_cast<int>(
      BluetoothDetailedViewChildId::kPairNewDeviceClickableView));

  std::unique_ptr<ash::IconButton> button = std::make_unique<ash::IconButton>(
      views::Button::PressedCallback(), IconButton::Type::kMedium,
      &kSystemMenuPlusIcon, IDS_ASH_STATUS_TRAY_BLUETOOTH_PAIR_NEW_DEVICE);
  button->SetCanProcessEventsWithinSubtree(/*can_process=*/false);
  button->SetFocusBehavior(
      /*focus_behavior=*/views::View::FocusBehavior::NEVER);

  hover_highlight_view->AddViewAndLabel(
      std::move(button),
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_BLUETOOTH_PAIR_NEW_DEVICE));

  TrayPopupUtils::SetLabelFontList(hover_highlight_view->text_label(),
                                   TrayPopupUtils::FontStyle::kSubHeader);

  views::View* separator = pair_new_device_view_->AddChildView(
      TrayPopupUtils::CreateListSubHeaderSeparator());
  const gfx::Insets padding = separator->GetInsets();

  // The hover highlight view does not have top padding by default, unlike the
  // following separator. To keep the icon and label centered, and to make the
  // entirety of the view "highlightable", remove the padding from the separator
  // and add it to both the top and bottom of the hover highlight view.
  separator->SetBorder(/*b=*/nullptr);
  hover_highlight_view->SetBorder(views::CreateEmptyBorder(
      gfx::Insets::TLBR(padding.top(), 0, padding.top(), 0)));

  pair_new_device_view_->AddChildViewAt(hover_highlight_view.release(), 0);
}

void BluetoothDetailedViewLegacy::CreateTitleRowButtons() {
  DCHECK(!settings_button_);
  DCHECK(!toggle_button_);

  tri_view()->SetContainerVisible(TriView::Container::END, /*visible=*/true);

  std::unique_ptr<TrayToggleButton> toggle = std::make_unique<TrayToggleButton>(
      base::BindRepeating(&BluetoothDetailedViewLegacy::OnToggleClicked,
                          weak_factory_.GetWeakPtr()),
      IDS_ASH_STATUS_TRAY_BLUETOOTH);
  toggle->SetID(static_cast<int>(BluetoothDetailedViewChildId::kToggleButton));
  toggle_button_ =
      tri_view()->AddView(TriView::Container::END, std::move(toggle));

  std::unique_ptr<views::Button> settings =
      base::WrapUnique(CreateSettingsButton(
          base::BindRepeating(&BluetoothDetailedViewLegacy::OnSettingsClicked,
                              weak_factory_.GetWeakPtr()),
          IDS_ASH_STATUS_TRAY_BLUETOOTH_SETTINGS));
  settings->SetID(
      static_cast<int>(BluetoothDetailedViewChildId::kSettingsButton));
  settings_button_ =
      tri_view()->AddView(TriView::Container::END, std::move(settings));
}

void BluetoothDetailedViewLegacy::OnSettingsClicked() {
  if (!TrayPopupUtils::CanOpenWebUISettings())
    return;
  CloseBubble();  // Deletes |this|.
  Shell::Get()->system_tray_model()->client()->ShowBluetoothSettings();
}

void BluetoothDetailedViewLegacy::OnToggleClicked() {
  const bool toggle_state = toggle_button_->GetIsOn();
  delegate()->OnToggleClicked(toggle_state);

  const BluetoothSystemState new_system_state =
      toggle_state ? BluetoothSystemState::kEnabling
                   : BluetoothSystemState::kDisabling;

  // Avoid the situation where there is a delay between the toggle becoming
  // enabled/disabled and Bluetooth becoming enabled/disabled by forcing the
  // view state to match the toggle state.
  UpdateBluetoothEnabledState(new_system_state);
}

}  // namespace ash
