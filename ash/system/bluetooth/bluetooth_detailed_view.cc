// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/bluetooth/bluetooth_detailed_view.h"

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/tray/hover_highlight_view.h"
#include "ash/system/tray/tray_info_label.h"
#include "ash/system/tray/tray_popup_item_style.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/button/toggle_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/layout/box_layout.h"

using device::mojom::BluetoothSystem;

namespace ash {
namespace tray {
namespace {

const int kUpdateFrequencyMs = 1000;
const int kDisabledPanelLabelBaselineY = 20;

// Updates bluetooth device |device| in the |list|. If it is new, append to the
// end of the |list|; otherwise, keep it at the same place, but update the data
// with new device info provided by |device|.
void UpdateBluetoothDeviceListHelper(BluetoothDeviceList* list,
                                     const BluetoothDeviceInfo& device) {
  for (BluetoothDeviceList::iterator it = list->begin(); it != list->end();
       ++it) {
    if ((*it).address == device.address) {
      *it = device;
      return;
    }
  }

  list->push_back(device);
}

// Removes the obsolete BluetoothDevices from |list|, if they are not in the
// |new_device_address_list|.
void RemoveObsoleteBluetoothDevicesFromList(
    BluetoothDeviceList* device_list,
    const std::set<std::string>& new_device_address_list) {
  for (BluetoothDeviceList::iterator it = device_list->begin();
       it != device_list->end(); ++it) {
    if (!new_device_address_list.count((*it).address)) {
      it = device_list->erase(it);
      if (it == device_list->end())
        return;
    }
  }
}

// Returns corresponding device type icons for given Bluetooth device types and
// connection states.
const gfx::VectorIcon& GetBluetoothDeviceIcon(
    device::BluetoothDeviceType device_type,
    bool connected) {
  switch (device_type) {
    case device::BluetoothDeviceType::COMPUTER:
      return ash::kSystemMenuComputerIcon;
    case device::BluetoothDeviceType::PHONE:
      return ash::kSystemMenuPhoneIcon;
    case device::BluetoothDeviceType::AUDIO:
    case device::BluetoothDeviceType::CAR_AUDIO:
      return ash::kSystemMenuHeadsetIcon;
    case device::BluetoothDeviceType::VIDEO:
      return ash::kSystemMenuVideocamIcon;
    case device::BluetoothDeviceType::JOYSTICK:
    case device::BluetoothDeviceType::GAMEPAD:
      return ash::kSystemMenuGamepadIcon;
    case device::BluetoothDeviceType::KEYBOARD:
    case device::BluetoothDeviceType::KEYBOARD_MOUSE_COMBO:
      return ash::kSystemMenuKeyboardIcon;
    case device::BluetoothDeviceType::TABLET:
      return ash::kSystemMenuTabletIcon;
    case device::BluetoothDeviceType::MOUSE:
      return ash::kSystemMenuMouseIcon;
    case device::BluetoothDeviceType::MODEM:
    case device::BluetoothDeviceType::PERIPHERAL:
      return ash::kSystemMenuBluetoothIcon;
    default:
      return connected ? ash::kSystemMenuBluetoothConnectedIcon
                       : ash::kSystemMenuBluetoothIcon;
  }
}

views::View* CreateDisabledPanel() {
  views::View* container = new views::View;
  auto box_layout =
      std::make_unique<views::BoxLayout>(views::BoxLayout::kVertical);
  box_layout->set_main_axis_alignment(
      views::BoxLayout::MAIN_AXIS_ALIGNMENT_CENTER);
  container->SetLayoutManager(std::move(box_layout));

  TrayPopupItemStyle style(TrayPopupItemStyle::FontStyle::DETAILED_VIEW_LABEL);
  style.set_color_style(TrayPopupItemStyle::ColorStyle::DISABLED);

  views::ImageView* image_view = new views::ImageView;
  image_view->SetImage(gfx::CreateVectorIcon(kSystemMenuBluetoothDisabledIcon,
                                             style.GetIconColor()));
  image_view->SetVerticalAlignment(views::ImageView::TRAILING);
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

BluetoothDetailedView::~BluetoothDetailedView() {
  // Stop discovering bluetooth devices when exiting BT detailed view.
  BluetoothStopDiscovering();
}

void BluetoothDetailedView::Update() {
  // Update immediately for initial device list and
  // when bluetooth is disabled.
  if (device_map_.empty() ||
      Shell::Get()->tray_bluetooth_helper()->GetBluetoothState() !=
          BluetoothSystem::State::kPoweredOn) {
    timer_.Stop();
    DoUpdate();
    return;
  }

  // Return here since an update is already queued.
  if (timer_.IsRunning())
    return;

  // Update the detailed view after kUpdateFrequencyMs.
  timer_.Start(FROM_HERE, base::TimeDelta::FromMilliseconds(kUpdateFrequencyMs),
               this, &BluetoothDetailedView::DoUpdate);
}

void BluetoothDetailedView::CreateItems() {
  CreateScrollableList();
  CreateTitleRow(IDS_ASH_STATUS_TRAY_BLUETOOTH);
}

void BluetoothDetailedView::BluetoothStartDiscovering() {
  TrayBluetoothHelper* helper = Shell::Get()->tray_bluetooth_helper();
  if (helper->HasBluetoothDiscoverySession()) {
    ShowLoadingIndicator();
    return;
  }
  HideLoadingIndicator();
  if (helper->GetBluetoothState() == BluetoothSystem::State::kPoweredOn)
    helper->StartBluetoothDiscovering();
}

void BluetoothDetailedView::BluetoothStopDiscovering() {
  TrayBluetoothHelper* helper = Shell::Get()->tray_bluetooth_helper();
  if (helper && helper->HasBluetoothDiscoverySession()) {
    helper->StopBluetoothDiscovering();
    HideLoadingIndicator();
  }
}

void BluetoothDetailedView::UpdateBluetoothDeviceList() {
  std::set<std::string> new_connecting_devices;
  std::set<std::string> new_connected_devices;
  std::set<std::string> new_paired_not_connected_devices;
  std::set<std::string> new_discovered_not_paired_devices;

  BluetoothDeviceList list =
      Shell::Get()->tray_bluetooth_helper()->GetAvailableBluetoothDevices();
  for (const auto& device : list) {
    if (device.connecting) {
      new_connecting_devices.insert(device.address);
      UpdateBluetoothDeviceListHelper(&connecting_devices_, device);
    } else if (device.connected && device.paired) {
      new_connected_devices.insert(device.address);
      UpdateBluetoothDeviceListHelper(&connected_devices_, device);
    } else if (device.paired) {
      new_paired_not_connected_devices.insert(device.address);
      UpdateBluetoothDeviceListHelper(&paired_not_connected_devices_, device);
    } else {
      new_discovered_not_paired_devices.insert(device.address);
      UpdateBluetoothDeviceListHelper(&discovered_not_paired_devices_, device);
    }
  }
  RemoveObsoleteBluetoothDevicesFromList(&connecting_devices_,
                                         new_connecting_devices);
  RemoveObsoleteBluetoothDevicesFromList(&connected_devices_,
                                         new_connected_devices);
  RemoveObsoleteBluetoothDevicesFromList(&paired_not_connected_devices_,
                                         new_paired_not_connected_devices);
  RemoveObsoleteBluetoothDevicesFromList(&discovered_not_paired_devices_,
                                         new_discovered_not_paired_devices);
}

void BluetoothDetailedView::UpdateHeaderEntry() {
  const bool is_bluetooth_enabled =
      Shell::Get()->tray_bluetooth_helper()->GetBluetoothState() ==
      BluetoothSystem::State::kPoweredOn;
  if (toggle_)
    toggle_->SetIsOn(is_bluetooth_enabled, true);
}

void BluetoothDetailedView::UpdateDeviceScrollList() {
  std::string focused_device_address = GetFocusedDeviceAddress();

  device_map_.clear();
  scroll_content()->RemoveAllChildViews(true);

  TrayBluetoothHelper* helper = Shell::Get()->tray_bluetooth_helper();
  const BluetoothSystem::State bluetooth_state = helper->GetBluetoothState();

  switch (bluetooth_state) {
    case BluetoothSystem::State::kUnsupported:
      // Bluetooth is always supported on Chrome OS.
      NOTREACHED();
      return;
    case BluetoothSystem::State::kUnavailable:
    case BluetoothSystem::State::kPoweredOff:
    case BluetoothSystem::State::kTransitioning:
      // If Bluetooth is disabled, show a panel which only indicates that it is
      // disabled, instead of the scroller with Bluetooth devices.
      ShowDisabledPanel();
      return;
    case BluetoothSystem::State::kPoweredOn:
      HideDisabledPanel();
      break;
  }

  // Add paired devices and their section header to the list.
  bool has_paired_devices = !connected_devices_.empty() ||
                            !connecting_devices_.empty() ||
                            !paired_not_connected_devices_.empty();
  if (has_paired_devices) {
    AddScrollListSubHeader(IDS_ASH_STATUS_TRAY_BLUETOOTH_PAIRED_DEVICES);
    AppendSameTypeDevicesToScrollList(connected_devices_, true, true);
    AppendSameTypeDevicesToScrollList(connecting_devices_, true, false);
    AppendSameTypeDevicesToScrollList(paired_not_connected_devices_, false,
                                      false);
  }

  // Add unpaired devices to the list. If at least one paired device is
  // present, also add a section header above the unpaired devices.
  if (!discovered_not_paired_devices_.empty()) {
    if (has_paired_devices)
      AddScrollListSubHeader(IDS_ASH_STATUS_TRAY_BLUETOOTH_UNPAIRED_DEVICES);
    AppendSameTypeDevicesToScrollList(discovered_not_paired_devices_, false,
                                      false);
  }

  // Show user Bluetooth state if there is no bluetooth devices in list.
  if (device_map_.empty()) {
    scroll_content()->AddChildView(new TrayInfoLabel(
        nullptr /* delegate */, IDS_ASH_STATUS_TRAY_BLUETOOTH_DISCOVERING));
  }

  // Focus the device which was focused before the device-list update.
  if (!focused_device_address.empty())
    FocusDeviceByAddress(focused_device_address);

  scroll_content()->InvalidateLayout();
}

void BluetoothDetailedView::AppendSameTypeDevicesToScrollList(
    const BluetoothDeviceList& list,
    bool highlight,
    bool checked) {
  for (const auto& device : list) {
    const gfx::VectorIcon& icon =
        GetBluetoothDeviceIcon(device.device_type, device.connected);
    HoverHighlightView* container =
        AddScrollListItem(icon, device.display_name);
    if (device.connected)
      SetupConnectedScrollListItem(container);
    else if (device.connecting)
      SetupConnectingScrollListItem(container);
    device_map_[container] = device.address;
  }
}

bool BluetoothDetailedView::FoundDevice(
    const std::string& device_address,
    const BluetoothDeviceList& device_list) const {
  for (const auto& device : device_list) {
    if (device.address == device_address)
      return true;
  }
  return false;
}

void BluetoothDetailedView::UpdateClickedDevice(
    const std::string& device_address,
    views::View* item_container) {
  if (FoundDevice(device_address, paired_not_connected_devices_)) {
    HoverHighlightView* container =
        static_cast<HoverHighlightView*>(item_container);
    SetupConnectingScrollListItem(container);
    scroll_content()->SizeToPreferredSize();
    scroller()->Layout();
  }
}

void BluetoothDetailedView::HandleViewClicked(views::View* view) {
  TrayBluetoothHelper* helper = Shell::Get()->tray_bluetooth_helper();
  if (helper->GetBluetoothState() != BluetoothSystem::State::kPoweredOn)
    return;

  std::map<views::View*, std::string>::iterator find;
  find = device_map_.find(view);
  if (find == device_map_.end())
    return;

  const std::string device_address = find->second;
  if (FoundDevice(device_address, connecting_devices_))
    return;

  UpdateClickedDevice(device_address, view);
  helper->ConnectToBluetoothDevice(device_address);
}

void BluetoothDetailedView::HandleButtonPressed(views::Button* sender,
                                                const ui::Event& event) {
  if (sender == toggle_) {
    Shell::Get()->tray_bluetooth_helper()->SetBluetoothEnabled(
        toggle_->is_on());
  } else if (sender == settings_) {
    ShowSettings();
  } else {
    NOTREACHED();
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
                       BluetoothSystem::State::kPoweredOn,
                   false /* animate */);
  tri_view()->AddView(TriView::Container::END, toggle_);

  settings_ = CreateSettingsButton(IDS_ASH_STATUS_TRAY_BLUETOOTH_SETTINGS);
  tri_view()->AddView(TriView::Container::END, settings_);
}

void BluetoothDetailedView::ShowSettings() {
  if (TrayPopupUtils::CanOpenWebUISettings()) {
    Shell::Get()->system_tray_model()->client_ptr()->ShowBluetoothSettings();
    CloseBubble();
  }
}

void BluetoothDetailedView::ShowLoadingIndicator() {
  // Setting a value of -1 gives progress_bar an infinite-loading behavior.
  ShowProgress(-1, true);
}

void BluetoothDetailedView::HideLoadingIndicator() {
  ShowProgress(0, false);
}

void BluetoothDetailedView::ShowDisabledPanel() {
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
}

void BluetoothDetailedView::HideDisabledPanel() {
  DCHECK(scroller());
  if (disabled_panel_)
    disabled_panel_->SetVisible(false);
  scroller()->SetVisible(true);
}

std::string BluetoothDetailedView::GetFocusedDeviceAddress() const {
  for (auto& view_and_address : device_map_) {
    if (view_and_address.first->HasFocus())
      return view_and_address.second;
  }
  return std::string();
}

void BluetoothDetailedView::FocusDeviceByAddress(
    const std::string& address) const {
  for (auto& view_and_address : device_map_) {
    if (view_and_address.second == address) {
      view_and_address.first->RequestFocus();
      return;
    }
  }
}

void BluetoothDetailedView::DoUpdate() {
  BluetoothStartDiscovering();
  UpdateBluetoothDeviceList();

  // Update UI.
  UpdateDeviceScrollList();
  UpdateHeaderEntry();
  Layout();
}

}  // namespace tray
}  // namespace ash
