// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/bluetooth/bluetooth_detailed_view_impl.h"

#include <memory>
#include <utility>

#include "ash/ash_element_identifiers.h"
#include "ash/bubble/bubble_utils.h"
#include "ash/public/cpp/resources/grit/ash_public_unscaled_resources.h"
#include "ash/public/cpp/system_tray_client.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/rounded_container.h"
#include "ash/style/typography.h"
#include "ash/system/bluetooth/bluetooth_device_list_item_view.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/tray/detailed_view_delegate.h"
#include "ash/system/tray/hover_highlight_view.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ash/system/tray/tray_toggle_button.h"
#include "ash/system/tray/tri_view.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "chromeos/ash/services/bluetooth_config/public/cpp/cros_bluetooth_config_util.h"
#include "chromeos/ash/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom.h"
#include "chromeos/constants/chromeos_features.h"
#include "device/bluetooth/chromeos/bluetooth_utils.h"
#include "ui/accessibility/ax_enums.mojom-shared.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/toggle_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"

namespace ash {
namespace {

using bluetooth_config::IsBluetoothEnabled;
using bluetooth_config::IsBluetoothEnabledOrEnabling;
using bluetooth_config::mojom::BluetoothSystemState;

constexpr auto kToggleRowTriViewInsets = gfx::Insets::VH(8, 24);
constexpr auto kMainContainerMargins = gfx::Insets::TLBR(2, 0, 0, 0);
constexpr auto kPairNewDeviceIconMargins = gfx::Insets::TLBR(0, 2, 0, 0);
constexpr auto kSubHeaderInsets = gfx::Insets::TLBR(10, 24, 10, 16);

}  // namespace

BluetoothDetailedViewImpl::BluetoothDetailedViewImpl(
    DetailedViewDelegate* detailed_view_delegate,
    BluetoothDetailedView::Delegate* delegate)
    : BluetoothDetailedView(delegate),
      TrayDetailedView(detailed_view_delegate) {
  CreateTitleRow(IDS_ASH_STATUS_TRAY_BLUETOOTH);
  CreateScrollableList();
  CreateTopContainer();
  CreateMainContainer();
  if (chromeos::features::IsBluetoothWifiQSPodRefreshEnabled()) {
    CreateZeroStateView(std::make_unique<ZeroStateView>(
        /*image_id=*/IDR_TRAY_BLUETOOTH_UNAVAILABLE_STATE_IMAGE,
        /*title_id=*/IDS_ASH_STATUS_TRAY_BLUETOOTH_UNAVAILABLE_STATE_TITLE,
        /*subtitle_id=*/
        IDS_ASH_STATUS_TRAY_BLUETOOTH_UNAVAILABLE_STATE_SUBTITLE));
  }
  UpdateBluetoothEnabledState(BluetoothSystemState::kDisabled);
  device::RecordUiSurfaceDisplayed(
      device::BluetoothUiSurface::kBluetoothQuickSettings);
}

BluetoothDetailedViewImpl::~BluetoothDetailedViewImpl() = default;

views::View* BluetoothDetailedViewImpl::GetAsView() {
  return this;
}

void BluetoothDetailedViewImpl::UpdateBluetoothEnabledState(
    const BluetoothSystemState system_state) {
  if (chromeos::features::IsBluetoothWifiQSPodRefreshEnabled() &&
      system_state == BluetoothSystemState::kUnavailable) {
    SetZeroStateViewVisibility(true);
    return;
  }
  if (chromeos::features::IsBluetoothWifiQSPodRefreshEnabled()) {
    SetZeroStateViewVisibility(false);
  }

  bool is_enabled_or_enabling = IsBluetoothEnabledOrEnabling(system_state);

  // Use square corners on the bottom edge when Bluetooth is enabled.
  top_container_->SetBehavior(is_enabled_or_enabling
                                  ? RoundedContainer::Behavior::kTopRounded
                                  : RoundedContainer::Behavior::kAllRounded);
  main_container_->SetVisible(is_enabled_or_enabling);

  // Update the top container Bluetooth icon.
  toggle_icon_->SetImage(ui::ImageModel::FromVectorIcon(
      is_enabled_or_enabling ? kSystemMenuBluetoothIcon
                             : kSystemMenuBluetoothDisabledIcon,
      cros_tokens::kCrosSysOnSurface));

  // Update the top container on/off label.
  toggle_row_->text_label()->SetText(l10n_util::GetStringUTF16(
      is_enabled_or_enabling ? IDS_ASH_STATUS_TRAY_BLUETOOTH_ENABLED_SHORT
                             : IDS_ASH_STATUS_TRAY_BLUETOOTH_DISABLED_SHORT));

  // Update the toggle row and button tooltips. The entire row is clickable.
  std::u16string tooltip_template =
      is_enabled_or_enabling
          ? l10n_util::GetStringUTF16(
                IDS_ASH_STATUS_TRAY_BLUETOOTH_ENABLED_TOOLTIP)
          : l10n_util::GetStringUTF16(
                IDS_ASH_STATUS_TRAY_BLUETOOTH_DISABLED_TOOLTIP);
  std::u16string tooltip_text = l10n_util::GetStringFUTF16(
      IDS_ASH_STATUS_TRAY_BLUETOOTH_TOGGLE_TOOLTIP, tooltip_template);
  toggle_row_->SetTooltipText(tooltip_text);
  toggle_button_->SetTooltipText(tooltip_text);
  toggle_button_->GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_BLUETOOTH));

  // Ensure the toggle button is in sync with the current Bluetooth state.
  if (toggle_button_->GetIsOn() != is_enabled_or_enabling) {
    toggle_button_->SetIsOn(is_enabled_or_enabling);
  }

  // Only show "Pair new device" button if bluetooth is enabled.
  // see (b/297044914)
  pair_new_device_view_->SetVisible(IsBluetoothEnabled(system_state));

  InvalidateLayout();
}

BluetoothDeviceListItemView* BluetoothDetailedViewImpl::AddDeviceListItem() {
  return device_list_->AddChildView(
      std::make_unique<BluetoothDeviceListItemView>(/*listener=*/this));
}

views::View* BluetoothDetailedViewImpl::AddDeviceListSubHeader(
    const gfx::VectorIcon& icon,
    int text_id) {
  auto header = std::make_unique<views::BoxLayoutView>();
  header->SetInsideBorderInsets(kSubHeaderInsets);
  std::unique_ptr<views::Label> label = bubble_utils::CreateLabel(
      TypographyToken::kCrosBody2, l10n_util::GetStringUTF16(text_id),
      cros_tokens::kCrosSysOnSurfaceVariant);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label->SetSubpixelRenderingEnabled(false);
  header->AddChildView(std::move(label));

  return device_list_->AddChildView(std::move(header));
}

void BluetoothDetailedViewImpl::NotifyDeviceListChanged() {
  device_list_->InvalidateLayout();
  DeprecatedLayoutImmediately();
}

views::View* BluetoothDetailedViewImpl::device_list() {
  return device_list_;
}

void BluetoothDetailedViewImpl::HandleViewClicked(views::View* view) {
  // Handle clicks on the on/off toggle row.
  if (view == toggle_row_) {
    // The toggle button has the old state, so switch to the opposite state.
    ToggleBluetoothState(!toggle_button_->GetIsOn());
    return;
  }

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

void BluetoothDetailedViewImpl::CreateExtraTitleRowButtons() {
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
  top_container_ =
      scroll_content()->AddChildView(std::make_unique<RoundedContainer>());
  // Ensure the HoverHighlightView ink drop fills the whole container.
  top_container_->SetBorderInsets(gfx::Insets());

  toggle_row_ = top_container_->AddChildView(
      std::make_unique<HoverHighlightView>(/*listener=*/this));
  toggle_row_->SetFocusBehavior(FocusBehavior::NEVER);

  // The icon image and label text depend on whether Bluetooth is enabled. They
  // are set in UpdateBluetoothEnabledState().
  auto icon = std::make_unique<views::ImageView>();
  toggle_icon_ = icon.get();
  toggle_row_->AddViewAndLabel(std::move(icon), u"");
  toggle_row_->text_label()->SetEnabledColorId(cros_tokens::kCrosSysOnSurface);
  TypographyProvider::Get()->StyleLabel(ash::TypographyToken::kCrosButton1,
                                        *toggle_row_->text_label());

  auto toggle = std::make_unique<Switch>(base::BindRepeating(
      &BluetoothDetailedViewImpl::OnToggleClicked, weak_factory_.GetWeakPtr()));
  toggle_button_ = toggle.get();
  toggle_button_->SetProperty(views::kElementIdentifierKey,
                              kBluetoothDetailedViewToggleElementId);
  toggle_row_->AddRightView(toggle.release());

  // Allow the row to be taller than a typical tray menu item.
  toggle_row_->SetExpandable(true);
  toggle_row_->tri_view()->SetInsets(kToggleRowTriViewInsets);

  // ChromeVox users will just use the `toggle_button_` to toggle.
  toggle_icon_->GetViewAccessibility().SetIsIgnored(true);
  toggle_row_->text_label()->GetViewAccessibility().SetIsIgnored(true);
}

void BluetoothDetailedViewImpl::CreateMainContainer() {
  DCHECK(!main_container_);
  main_container_ =
      scroll_content()->AddChildView(std::make_unique<RoundedContainer>(
          RoundedContainer::Behavior::kBottomRounded));

  // Add a small empty space, like a separator, between the containers.
  main_container_->SetProperty(views::kMarginsKey, kMainContainerMargins);

  // Add a row for "pair new device".
  pair_new_device_view_ = main_container_->AddChildView(
      std::make_unique<HoverHighlightView>(/*listener=*/this));
  pair_new_device_view_->SetProperty(
      views::kElementIdentifierKey,
      kBluetoothDetailedViewPairNewDeviceElementId);

  // Create the "+" icon.
  auto icon = std::make_unique<views::ImageView>();
  icon->SetImage(ui::ImageModel::FromVectorIcon(kSystemMenuPlusIcon,
                                                cros_tokens::kCrosSysPrimary));
  icon->SetProperty(views::kMarginsKey, kPairNewDeviceIconMargins);
  pair_new_device_icon_ = icon.get();
  pair_new_device_view_->AddViewAndLabel(
      std::move(icon),
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_BLUETOOTH_PAIR_NEW_DEVICE));

  views::Label* label = pair_new_device_view_->text_label();
  label->SetEnabledColorId(cros_tokens::kCrosSysPrimary);
  ash::TypographyProvider::Get()->StyleLabel(ash::TypographyToken::kCrosButton2,
                                             *label);

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

void BluetoothDetailedViewImpl::OnToggleClicked() {
  // The toggle button already has the new state after a click.
  ToggleBluetoothState(toggle_button_->GetIsOn());
}

void BluetoothDetailedViewImpl::ToggleBluetoothState(bool new_state) {
  delegate()->OnToggleClicked(new_state);

  const BluetoothSystemState new_system_state =
      new_state ? BluetoothSystemState::kEnabling
                : BluetoothSystemState::kDisabling;

  // Avoid the situation where there is a delay between the toggle becoming
  // enabled/disabled and Bluetooth becoming enabled/disabled by forcing the
  // view state to match the toggle state.
  UpdateBluetoothEnabledState(new_system_state);
}

BEGIN_METADATA(BluetoothDetailedViewImpl)
END_METADATA

}  // namespace ash
