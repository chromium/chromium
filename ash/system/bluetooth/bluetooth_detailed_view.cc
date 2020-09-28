// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/bluetooth/bluetooth_detailed_view.h"

#include <map>
#include <memory>
#include <utility>

#include "ash/public/cpp/system_tray_client.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/machine_learning/user_settings_event_logger.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/tray/hover_highlight_view.h"
#include "ash/system/tray/tray_info_label.h"
#include "ash/system/tray/tray_popup_item_style.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "base/strings/utf_string_conversions.h"
#include "services/device/public/cpp/bluetooth/bluetooth_utils.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/button/toggle_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/layout/box_layout.h"

using device::mojom::BluetoothDeviceInfo;
using device::mojom::BluetoothSystem;

namespace ash {
namespace tray {
namespace {

const int kDisabledPanelLabelBaselineY = 20;

// Returns corresponding device type icons for given Bluetooth device types and
// connection states.
const gfx::VectorIcon& GetBluetoothDeviceIcon(
    BluetoothDeviceInfo::DeviceType device_type,
    BluetoothDeviceInfo::ConnectionState connection_state) {
  switch (device_type) {
    case BluetoothDeviceInfo::DeviceType::kComputer:
      return ash::kSystemMenuComputerIcon;
    case BluetoothDeviceInfo::DeviceType::kPhone:
      return ash::kSystemMenuPhoneIcon;
    case BluetoothDeviceInfo::DeviceType::kAudio:
    case BluetoothDeviceInfo::DeviceType::kCarAudio:
      return ash::kSystemMenuHeadsetIcon;
    case BluetoothDeviceInfo::DeviceType::kVideo:
      return ash::kSystemMenuVideocamIcon;
    case BluetoothDeviceInfo::DeviceType::kJoystick:
    case BluetoothDeviceInfo::DeviceType::kGamepad:
      return ash::kSystemMenuGamepadIcon;
    case BluetoothDeviceInfo::DeviceType::kKeyboard:
    case BluetoothDeviceInfo::DeviceType::kKeyboardMouseCombo:
      return ash::kSystemMenuKeyboardIcon;
    case BluetoothDeviceInfo::DeviceType::kTablet:
      return ash::kSystemMenuTabletIcon;
    case BluetoothDeviceInfo::DeviceType::kMouse:
      return ash::kSystemMenuMouseIcon;
    case BluetoothDeviceInfo::DeviceType::kModem:
    case BluetoothDeviceInfo::DeviceType::kPeripheral:
      return ash::kSystemMenuBluetoothIcon;
    default:
      return connection_state ==
                     BluetoothDeviceInfo::ConnectionState::kConnected
                 ? ash::kSystemMenuBluetoothConnectedIcon
                 : ash::kSystemMenuBluetoothIcon;
  }
}

views::View* CreateDisabledPanel() {
  views::View* container = new views::View;
  auto box_layout = std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical);
  box_layout->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kCenter);
  container->SetLayoutManager(std::move(box_layout));

  TrayPopupItemStyle style(TrayPopupItemStyle::FontStyle::DETAILED_VIEW_LABEL);
  style.set_color_style(TrayPopupItemStyle::ColorStyle::DISABLED);

  views::ImageView* image_view = new views::ImageView;
  image_view->SetImage(gfx::CreateVectorIcon(kSystemMenuBluetoothDisabledIcon,
                                             style.GetIconColor()));
  image_view->SetVerticalAlignment(views::ImageView::Alignment::kTrailing);
  container->AddChildView(image_view);

  views::Label* label = new views::Label(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_BLUETOOTH_DISABLED));
  style.SetupLabel(label);
  label->SetBorder(views::CreateEmptyBorder(
      kDisabledPanelLabelBaselineY - label->GetBaseline(), 0, 0, 0));
  container->AddChildView(label);

  // Make top padding of the icon equal to the height of the label so that the
  // icon is vertically aligned to center of the container.
  image_view->SetBorder(
      views::CreateEmptyBorder(label->GetPreferredSize().height(), 0, 0, 0));
  return container;
}

void LogUserBluetoothEvent(const BluetoothAddress& device_address) {
  ml::UserSettingsEventLogger* logger = ml::UserSettingsEventLogger::Get();
  if (logger) {
    logger->LogBluetoothUkmEvent(device_address);
  }
}

}  // namespace

BluetoothDetailedView::BluetoothDetailedView(DetailedViewDelegate* delegate,
                                             LoginStatus login)
    : TrayDetailedView(delegate),
      login_(login),
      toggle_(nullptr),
      settings_(nullptr),
      disabled_panel_(nullptr) {
  CreateItems();
}

BluetoothDetailedView::~BluetoothDetailedView() = default;

void BluetoothDetailedView::ShowLoadingIndicator() {
  // Setting a value of -1 gives progress_bar an infinite-loading behavior.
  ShowProgress(-1, true);
}

void BluetoothDetailedView::HideLoadingIndicator() {
  ShowProgress(0, false);
}

void BluetoothDetailedView::ShowBluetoothDisabledPanel() {
  device_map_.clear();
  scroll_content()->RemoveAllChildViews(true);

  DCHECK(scroller());
  if (!disabled_panel_) {
    disabled_panel_ = CreateDisabledPanel();
    // Insert |disabled_panel_| before the scroller, since the scroller will
    // have unnecessary bottom border when it is not the last child.
    AddChildViewAt(disabled_panel_, GetIndexOf(scroller()));
    // |disabled_panel_| need to fill the remaining space below the title row
    // so that the inner contents of |disabled_panel_| are placed properly.
    box_layout()->SetFlexForView(disabled_panel_, 1);
  }

  disabled_panel_->SetVisible(true);
  scroller()->SetVisible(false);

  Layout();
}

void BluetoothDetailedView::HideBluetoothDisabledPanel() {
  DCHECK(scroller());
  if (disabled_panel_)
    disabled_panel_->SetVisible(false);
  scroller()->SetVisible(true);

  Layout();
}

bool BluetoothDetailedView::IsDeviceScrollListEmpty() const {
  return device_map_.empty();
}

void BluetoothDetailedView::UpdateDeviceScrollList(
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

  base::Optional<BluetoothAddress> focused_device_address =
      GetFocusedDeviceAddress();

  device_map_.clear();
  scroll_content()->RemoveAllChildViews(true);

  // Add paired devices and their section header to the list.
  bool has_paired_devices = !connected_devices.empty() ||
                            !connecting_devices.empty() ||
                            !paired_not_connected_devices.empty();
  if (has_paired_devices) {
    AddScrollListSubHeader(IDS_ASH_STATUS_TRAY_BLUETOOTH_PAIRED_DEVICES);
    AppendSameTypeDevicesToScrollList(connected_devices, true, true);
    AppendSameTypeDevicesToScrollList(connecting_devices, true, false);
    AppendSameTypeDevicesToScrollList(paired_not_connected_devices, false,
                                      false);
  }

  // Add unpaired devices to the list. If at least one paired device is
  // present, also add a section header above the unpaired devices.
  if (!discovered_not_paired_devices.empty()) {
    if (has_paired_devices)
      AddScrollListSubHeader(IDS_ASH_STATUS_TRAY_BLUETOOTH_UNPAIRED_DEVICES);
    AppendSameTypeDevicesToScrollList(discovered_not_paired_devices, false,
                                      false);
  }

  // Show user Bluetooth state if there is no bluetooth devices in list.
  if (device_map_.empty()) {
    scroll_content()->AddChildView(new TrayInfoLabel(
        nullptr /* delegate */, IDS_ASH_STATUS_TRAY_BLUETOOTH_DISCOVERING));
  }

  // Focus the device which was focused before the device-list update.
  if (focused_device_address)
    FocusDeviceByAddress(focused_device_address.value());

  scroll_content()->InvalidateLayout();

  Layout();
}

void BluetoothDetailedView::SetToggleIsOn(bool is_on) {
  if (toggle_)
    toggle_->AnimateIsOn(is_on);
}

const char* BluetoothDetailedView::GetClassName() const {
  return "BluetoothDetailedView";
}

void BluetoothDetailedView::CreateItems() {
  CreateScrollableList();
  CreateTitleRow(IDS_ASH_STATUS_TRAY_BLUETOOTH);
}

void BluetoothDetailedView::AppendSameTypeDevicesToScrollList(
    const BluetoothDeviceList& list,
    bool highlight,
    bool checked) {
  for (const auto& device : list) {
    const gfx::VectorIcon& icon =
        GetBluetoothDeviceIcon(device->device_type, device->connection_state);
    HoverHighlightView* container = AddScrollListItem(
        icon, device::GetBluetoothDeviceNameForDisplay(device));
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
                           ? base::make_optional<uint8_t>(
                                 device->battery_info->battery_percentage)
                           : base::nullopt);
        break;
    }
    device_map_[container] = device->address;
  }
}

bool BluetoothDetailedView::FoundDevice(
    const BluetoothAddress& device_address,
    const BluetoothDeviceList& device_list) const {
  for (const auto& device : device_list) {
    if (device->address == device_address)
      return true;
  }
  return false;
}

void BluetoothDetailedView::UpdateClickedDevice(
    const BluetoothAddress& device_address,
    views::View* item_container) {
  if (FoundDevice(device_address, paired_not_connected_devices_)) {
    HoverHighlightView* container =
        static_cast<HoverHighlightView*>(item_container);
    SetupConnectingScrollListItem(container);
    scroll_content()->SizeToPreferredSize();
    scroller()->Layout();
  }
}

void BluetoothDetailedView::ShowSettings() {
  if (TrayPopupUtils::CanOpenWebUISettings()) {
    CloseBubble();  // Deletes |this|.
    Shell::Get()->system_tray_model()->client()->ShowBluetoothSettings();
  }
}

base::Optional<BluetoothAddress>
BluetoothDetailedView::GetFocusedDeviceAddress() const {
  for (const auto& view_and_address : device_map_) {
    if (view_and_address.first->HasFocus())
      return view_and_address.second;
  }
  return base::nullopt;
}

void BluetoothDetailedView::FocusDeviceByAddress(
    const BluetoothAddress& address) const {
  for (auto& view_and_address : device_map_) {
    if (view_and_address.second == address) {
      view_and_address.first->RequestFocus();
      return;
    }
  }
}

void BluetoothDetailedView::HandleViewClicked(views::View* view) {
  TrayBluetoothHelper* helper = Shell::Get()->tray_bluetooth_helper();
  if (helper->GetBluetoothState() != BluetoothSystem::State::kPoweredOn)
    return;

  std::map<views::View*, BluetoothAddress>::iterator find;
  find = device_map_.find(view);
  if (find == device_map_.end())
    return;

  const BluetoothAddress& device_address = find->second;
  if (FoundDevice(device_address, connecting_devices_))
    return;

  UpdateClickedDevice(device_address, view);
  LogUserBluetoothEvent(device_address);
  helper->ConnectToBluetoothDevice(device_address);
}

void BluetoothDetailedView::HandleButtonPressed(views::Button* sender,
                                                const ui::Event& event) {
  if (sender == toggle_) {
    Shell::Get()->tray_bluetooth_helper()->SetBluetoothEnabled(
        toggle_->GetIsOn());
  } else {
    DCHECK_EQ(settings_, sender);
    ShowSettings();
  }
}

void BluetoothDetailedView::CreateExtraTitleRowButtons() {
  if (login_ == LoginStatus::LOCKED)
    return;

  DCHECK(!toggle_);
  DCHECK(!settings_);

  tri_view()->SetContainerVisible(TriView::Container::END, true);

  toggle_ =
      TrayPopupUtils::CreateToggleButton(this, IDS_ASH_STATUS_TRAY_BLUETOOTH);
  toggle_->SetIsOn(Shell::Get()->tray_bluetooth_helper()->GetBluetoothState() ==
                   BluetoothSystem::State::kPoweredOn);
  tri_view()->AddView(TriView::Container::END, toggle_);

  settings_ = CreateSettingsButton(IDS_ASH_STATUS_TRAY_BLUETOOTH_SETTINGS);
  tri_view()->AddView(TriView::Container::END, settings_);
}

}  // namespace tray
}  // namespace ash
