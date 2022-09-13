// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/bluetooth/bluetooth_detailed_view_legacy.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <utility>

#include "ash/public/cpp/system_tray_client.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/color_util.h"
#include "ash/system/machine_learning/user_settings_event_logger.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/tray/hover_highlight_view.h"
#include "ash/system/tray/tray_info_label.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ash/system/tray/tray_toggle_button.h"
#include "ash/system/tray/tray_utils.h"
#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/ui/vector_icons/vector_icons.h"
#include "device/bluetooth/chromeos/bluetooth_utils.h"
#include "services/device/public/cpp/bluetooth/bluetooth_utils.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/toggle_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view_utils.h"

using device::mojom::BluetoothDeviceInfo;
using device::mojom::BluetoothSystem;

namespace ash {
namespace {

const int kDisabledPanelLabelBaselineY = 20;
const int kEnterpriseManagedIconSizeDip = 20;

// Returns corresponding device type icons for given Bluetooth device types and
// connection states.
const gfx::VectorIcon& GetBluetoothDeviceIcon(
    BluetoothDeviceInfo::DeviceType device_type,
    BluetoothDeviceInfo::ConnectionState connection_state) {
  switch (device_type) {
    case BluetoothDeviceInfo::DeviceType::kComputer:
      return ash::kSystemMenuComputerLegacyIcon;
    case BluetoothDeviceInfo::DeviceType::kPhone:
      return ash::kSystemMenuPhoneLegacyIcon;
    case BluetoothDeviceInfo::DeviceType::kAudio:
    case BluetoothDeviceInfo::DeviceType::kCarAudio:
      return ash::kSystemMenuHeadsetLegacyIcon;
    case BluetoothDeviceInfo::DeviceType::kVideo:
      return ash::kSystemMenuVideocamLegacyIcon;
    case BluetoothDeviceInfo::DeviceType::kJoystick:
    case BluetoothDeviceInfo::DeviceType::kGamepad:
      return ash::kSystemMenuGamepadLegacyIcon;
    case BluetoothDeviceInfo::DeviceType::kKeyboard:
    case BluetoothDeviceInfo::DeviceType::kKeyboardMouseCombo:
      return ash::kSystemMenuKeyboardLegacyIcon;
    case BluetoothDeviceInfo::DeviceType::kTablet:
      return ash::kSystemMenuTabletLegacyIcon;
    case BluetoothDeviceInfo::DeviceType::kMouse:
      return ash::kSystemMenuMouseLegacyIcon;
    case BluetoothDeviceInfo::DeviceType::kModem:
    case BluetoothDeviceInfo::DeviceType::kPeripheral:
      return ash::kSystemMenuBluetoothLegacyIcon;
    default:
      return connection_state ==
                     BluetoothDeviceInfo::ConnectionState::kConnected
                 ? ash::kSystemMenuBluetoothConnectedLegacyIcon
                 : ash::kSystemMenuBluetoothLegacyIcon;
  }
}

views::View* CreateDisabledPanel() {
  views::View* container = new views::View;
  auto box_layout = std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical);
  box_layout->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kCenter);
  container->SetLayoutManager(std::move(box_layout));

  auto* color_provider = AshColorProvider::Get();
  auto* image_view =
      container->AddChildView(std::make_unique<views::ImageView>());
  image_view->SetImage(gfx::CreateVectorIcon(
      kSystemMenuBluetoothDisabledIcon,
      ColorUtil::GetDisabledColor(color_provider->GetContentLayerColor(
          AshColorProvider::ContentLayerType::kIconColorPrimary))));
  image_view->SetVerticalAlignment(views::ImageView::Alignment::kTrailing);

  auto* label = container->AddChildView(std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_BLUETOOTH_DISABLED)));
  label->SetEnabledColor(
      ColorUtil::GetDisabledColor(color_provider->GetContentLayerColor(
          AshColorProvider::ContentLayerType::kTextColorPrimary)));
  TrayPopupUtils::SetLabelFontList(
      label, TrayPopupUtils::FontStyle::kDetailedViewLabel);
  label->SetBorder(views::CreateEmptyBorder(gfx::Insets::TLBR(
      kDisabledPanelLabelBaselineY - label->GetBaseline(), 0, 0, 0)));

  // Make top padding of the icon equal to the height of the label so that the
  // icon is vertically aligned to center of the container.
  image_view->SetBorder(views::CreateEmptyBorder(
      gfx::Insets::TLBR(label->GetPreferredSize().height(), 0, 0, 0)));
  return container;
}

void LogUserBluetoothEvent(const BluetoothAddress& device_address) {
  ml::UserSettingsEventLogger* logger = ml::UserSettingsEventLogger::Get();
  if (logger) {
    logger->LogBluetoothUkmEvent(device_address);
  }
}

HoverHighlightView* GetScrollListItemForDevice(
    const std::unordered_map<HoverHighlightView*, BluetoothAddress>& device_map,
    const BluetoothAddress& address) {
  for (const auto& view_and_address : device_map) {
    if (view_and_address.second == address)
      return view_and_address.first;
  }
  return nullptr;
}

}  // namespace

BluetoothDetailedViewLegacy::BluetoothDetailedViewLegacy(
    DetailedViewDelegate* delegate,
    LoginStatus login)
    : TrayDetailedView(delegate), login_(login) {
  CreateItems();
  device::RecordUiSurfaceDisplayed(
      device::BluetoothUiSurface::kBluetoothQuickSettings);
}

BluetoothDetailedViewLegacy::~BluetoothDetailedViewLegacy() = default;

void BluetoothDetailedViewLegacy::ShowLoadingIndicator() {
  // Setting a value of -1 gives progress_bar an infinite-loading behavior.
  ShowProgress(-1, true);
}

void BluetoothDetailedViewLegacy::HideLoadingIndicator() {
  ShowProgress(0, false);
}

void BluetoothDetailedViewLegacy::ShowBluetoothDisabledPanel() {
  device_map_.clear();
  paired_devices_heading_ = nullptr;
  unpaired_devices_heading_ = nullptr;
  bluetooth_discovering_label_ = nullptr;
  scroll_content()->RemoveAllChildViews();

  DCHECK(scroller());
  if (!disabled_panel_) {
    disabled_panel_ = CreateDisabledPanel();
    // Insert |disabled_panel_| before the scroller, since the scroller will
    // have unnecessary bottom border when it is not the last child.
    AddChildViewAt(disabled_panel_, GetIndexOf(scroller()).value());
    // |disabled_panel_| need to fill the remaining space below the title row
    // so that the inner contents of |disabled_panel_| are placed properly.
    box_layout()->SetFlexForView(disabled_panel_, 1);
  }

  disabled_panel_->SetVisible(true);
  scroller()->SetVisible(false);

  Layout();
}

void BluetoothDetailedViewLegacy::HideBluetoothDisabledPanel() {
  DCHECK(scroller());
  if (disabled_panel_)
    disabled_panel_->SetVisible(false);
  scroller()->SetVisible(true);

  Layout();
}

bool BluetoothDetailedViewLegacy::IsDeviceScrollListEmpty() const {
  return device_map_.empty();
}

void BluetoothDetailedViewLegacy::UpdateDeviceScrollList(
    const BluetoothDeviceList& connected_devices,
    const BluetoothDeviceList& connecting_devices,
    const BluetoothDeviceList& paired_not_connected_devices,
    const BluetoothDeviceList& discovered_not_paired_devices) {
  connecting_devices_.clear();
  for (const auto& device : connecting_devices)
    connecting_devices_.push_back(device->Clone());

  paired_not_connected_devices_.clear();
  for (const auto& device : paired_not_connected_devices)
    paired_not_connected_devices_.push_back(device->Clone());

  // Keep track of previous device_map_ so that existing scroll list
  // item views can be re-used. This is required for a11y so that
  // keyboard focus and screen-reader call outs are not disrupted
  // by frequent device list updates.
  std::unordered_map<HoverHighlightView*, BluetoothAddress> old_device_map =
      device_map_;
  device_map_.clear();

  // Add paired devices and their section header to the list.
  bool has_paired_devices = !connected_devices.empty() ||
                            !connecting_devices.empty() ||
                            !paired_not_connected_devices.empty();
  size_t index = 0;
  if (has_paired_devices) {
    paired_devices_heading_ =
        AddSubHeading(IDS_ASH_STATUS_TRAY_BLUETOOTH_PAIRED_DEVICES,
                      paired_devices_heading_, index++);
    index = AddSameTypeDevicesToScrollList(connected_devices, old_device_map,
                                           index, true, true);
    index = AddSameTypeDevicesToScrollList(connecting_devices, old_device_map,
                                           index, true, false);
    index = AddSameTypeDevicesToScrollList(paired_not_connected_devices,
                                           old_device_map, index, false, false);
  } else if (paired_devices_heading_) {
    scroll_content()->RemoveChildViewT(paired_devices_heading_);
    paired_devices_heading_ = nullptr;
  }

  // Add unpaired devices to the list. If at least one paired device is
  // present, also add a section header above the unpaired devices.
  if (!discovered_not_paired_devices.empty()) {
    if (has_paired_devices) {
      unpaired_devices_heading_ =
          AddSubHeading(IDS_ASH_STATUS_TRAY_BLUETOOTH_UNPAIRED_DEVICES,
                        unpaired_devices_heading_, index++);
    }
    index = AddSameTypeDevicesToScrollList(discovered_not_paired_devices,
                                           old_device_map, index, false, false);
  }

  if (unpaired_devices_heading_ &&
      (discovered_not_paired_devices.empty() || !has_paired_devices)) {
    scroll_content()->RemoveChildViewT(unpaired_devices_heading_);
    unpaired_devices_heading_ = nullptr;
  }

  // Show user Bluetooth state if there is no bluetooth devices in list.
  if (device_map_.empty()) {
    if (!bluetooth_discovering_label_) {
      bluetooth_discovering_label_ =
          new TrayInfoLabel(IDS_ASH_STATUS_TRAY_BLUETOOTH_DISCOVERING);
      scroll_content()->AddChildViewAt(bluetooth_discovering_label_, index++);
    } else {
      scroll_content()->ReorderChildView(bluetooth_discovering_label_, index++);
    }
  } else if (bluetooth_discovering_label_) {
    scroll_content()->RemoveChildViewT(bluetooth_discovering_label_);
    bluetooth_discovering_label_ = nullptr;
  }

  // Remove views for devices from old_device_map that are not in device_map_.
  for (auto& view_and_address : old_device_map) {
    if (device_map_.find(view_and_address.first) == device_map_.end()) {
      scroll_content()->RemoveChildViewT(view_and_address.first);
    }
  }

  scroll_content()->InvalidateLayout();

  Layout();
}

void BluetoothDetailedViewLegacy::SetToggleIsOn(bool is_on) {
  if (toggle_)
    toggle_->AnimateIsOn(is_on);
}

const char* BluetoothDetailedViewLegacy::GetClassName() const {
  return "BluetoothDetailedViewLegacy";
}

void BluetoothDetailedViewLegacy::CreateItems() {
  CreateScrollableList();
  scroll_content()->SetID(kScrollContentID);
  CreateTitleRow(IDS_ASH_STATUS_TRAY_BLUETOOTH);
}

TriView* BluetoothDetailedViewLegacy::AddSubHeading(int text_id,
                                                    TriView* sub_heading_view,
                                                    size_t child_index) {
  if (!sub_heading_view) {
    sub_heading_view = AddScrollListSubHeader(text_id);
  }
  scroll_content()->ReorderChildView(sub_heading_view, child_index);
  return sub_heading_view;
}

size_t BluetoothDetailedViewLegacy::AddSameTypeDevicesToScrollList(
    const BluetoothDeviceList& list,
    const std::unordered_map<HoverHighlightView*, BluetoothAddress>&
        old_device_list,
    size_t child_index,
    bool highlight,
    bool checked) {
  for (const auto& device : list) {
    const gfx::VectorIcon& icon =
        GetBluetoothDeviceIcon(device->device_type, device->connection_state);
    std::u16string device_name =
        device::GetBluetoothDeviceNameForDisplay(device);
    HoverHighlightView* container =
        GetScrollListItemForDevice(old_device_list, device->address);
    if (!container) {
      container = AddScrollListItem(icon, device_name);
    } else {
      container->text_label()->SetText(device_name);

      DCHECK(views::IsViewClass<views::ImageView>(container->left_view()));
      views::ImageView* left_icon =
          static_cast<views::ImageView*>(container->left_view());
      left_icon->SetImage(gfx::CreateVectorIcon(
          icon, AshColorProvider::Get()->GetContentLayerColor(
                    AshColorProvider::ContentLayerType::kIconColorPrimary)));
    }

    if (device->is_blocked_by_policy) {
      if (container->right_view()) {
        container->SetRightViewVisible(true);
      } else {
        gfx::ImageSkia enterprise_managed_icon = CreateVectorIcon(
            chromeos::kEnterpriseIcon, kEnterpriseManagedIconSizeDip,
            gfx::kGoogleGrey100);
        container->AddRightIcon(enterprise_managed_icon,
                                enterprise_managed_icon.width());
      }
    } else if (container->right_view()) {
      container->SetRightViewVisible(false);
    }

    container->SetAccessibleName(
        device::GetBluetoothDeviceLabelForAccessibility(device));
    switch (device->connection_state) {
      case BluetoothDeviceInfo::ConnectionState::kNotConnected:
        break;
      case BluetoothDeviceInfo::ConnectionState::kConnecting:
        SetupConnectingScrollListItem(container);
        break;
      case BluetoothDeviceInfo::ConnectionState::kConnected:
        SetupConnectedScrollListItem(
            container, device->battery_info
                           ? absl::make_optional<uint8_t>(
                                 device->battery_info->battery_percentage)
                           : absl::nullopt);
        break;
    }
    scroll_content()->ReorderChildView(container, child_index++);
    device_map_[container] = device->address;
  }
  return child_index;
}

bool BluetoothDetailedViewLegacy::FoundDevice(
    const BluetoothAddress& device_address,
    const BluetoothDeviceList& device_list) const {
  for (const auto& device : device_list) {
    if (device->address == device_address)
      return true;
  }
  return false;
}

void BluetoothDetailedViewLegacy::UpdateClickedDevice(
    const BluetoothAddress& device_address,
    HoverHighlightView* item_container) {
  if (FoundDevice(device_address, paired_not_connected_devices_)) {
    SetupConnectingScrollListItem(item_container);
    scroll_content()->SizeToPreferredSize();
    scroller()->Layout();
  }
}

void BluetoothDetailedViewLegacy::ToggleButtonPressed() {
  Shell::Get()->tray_bluetooth_helper()->SetBluetoothEnabled(
      toggle_->GetIsOn());
}

void BluetoothDetailedViewLegacy::ShowSettings() {
  if (TrayPopupUtils::CanOpenWebUISettings()) {
    CloseBubble();  // Deletes |this|.
    Shell::Get()->system_tray_model()->client()->ShowBluetoothSettings();
  }
}

absl::optional<BluetoothAddress>
BluetoothDetailedViewLegacy::GetFocusedDeviceAddress() const {
  for (const auto& view_and_address : device_map_) {
    if (view_and_address.first->HasFocus())
      return view_and_address.second;
  }
  return absl::nullopt;
}

void BluetoothDetailedViewLegacy::FocusDeviceByAddress(
    const BluetoothAddress& address) const {
  for (auto& view_and_address : device_map_) {
    if (view_and_address.second == address) {
      view_and_address.first->RequestFocus();
      return;
    }
  }
}

void BluetoothDetailedViewLegacy::HandleViewClicked(views::View* view) {
  TrayBluetoothHelper* helper = Shell::Get()->tray_bluetooth_helper();
  if (helper->GetBluetoothState() != BluetoothSystem::State::kPoweredOn)
    return;

  HoverHighlightView* container = static_cast<HoverHighlightView*>(view);
  std::unordered_map<HoverHighlightView*, BluetoothAddress>::iterator find;
  find = device_map_.find(container);
  if (find == device_map_.end())
    return;

  const BluetoothAddress& device_address = find->second;
  if (FoundDevice(device_address, connecting_devices_))
    return;

  UpdateClickedDevice(device_address, container);
  LogUserBluetoothEvent(device_address);
  helper->ConnectToBluetoothDevice(device_address);
}

void BluetoothDetailedViewLegacy::CreateExtraTitleRowButtons() {
  if (login_ == LoginStatus::LOCKED)
    return;

  DCHECK(!toggle_);
  DCHECK(!settings_);

  tri_view()->SetContainerVisible(TriView::Container::END, true);

  toggle_ = new TrayToggleButton(
      base::BindRepeating(&BluetoothDetailedViewLegacy::ToggleButtonPressed,
                          base::Unretained(this)),
      IDS_ASH_STATUS_TRAY_BLUETOOTH);
  toggle_->SetIsOn(Shell::Get()->tray_bluetooth_helper()->GetBluetoothState() ==
                   BluetoothSystem::State::kPoweredOn);
  tri_view()->AddView(TriView::Container::END, toggle_);

  settings_ = CreateSettingsButton(
      base::BindRepeating(&BluetoothDetailedViewLegacy::ShowSettings,
                          base::Unretained(this)),
      IDS_ASH_STATUS_TRAY_BLUETOOTH_SETTINGS);
  tri_view()->AddView(TriView::Container::END, settings_);
}

}  // namespace ash
