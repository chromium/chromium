// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/bluetooth/tray_bluetooth.h"

#include <map>
#include <memory>
#include <set>
#include <string>

#include "ash/metrics/user_metrics_recorder.h"
#include "ash/public/cpp/ash_view_ids.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/bluetooth/bluetooth_detailed_view.h"
#include "ash/system/bluetooth/tray_bluetooth_helper.h"
#include "ash/system/tray/system_tray.h"
#include "ash/system/tray/system_tray_item_detailed_view_delegate.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_detailed_view.h"
#include "ash/system/tray/tray_item_more.h"
#include "ash/system/tray/tray_popup_item_style.h"
#include "ash/system/tray/tri_view.h"
#include "device/bluetooth/bluetooth_common.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/progress_bar.h"
#include "ui/views/controls/separator.h"

using device::mojom::BluetoothSystem;

namespace ash {
namespace tray {

class BluetoothDefaultView : public TrayItemMore {
 public:
  explicit BluetoothDefaultView(SystemTrayItem* owner) : TrayItemMore(owner) {
    set_id(VIEW_ID_BLUETOOTH_DEFAULT_VIEW);
  }
  ~BluetoothDefaultView() override = default;

  void Update() {
    TrayBluetoothHelper* helper = Shell::Get()->tray_bluetooth_helper();
    bool powered_on = true;

    switch (helper->GetBluetoothState()) {
      case BluetoothSystem::State::kUnsupported:
      case BluetoothSystem::State::kUnavailable:
        SetVisible(false);
        break;
      case BluetoothSystem::State::kPoweredOff:
        powered_on = false;
        FALLTHROUGH;
      case BluetoothSystem::State::kTransitioning:
      case BluetoothSystem::State::kPoweredOn:
        const base::string16 label = l10n_util::GetStringUTF16(
            powered_on ? IDS_ASH_STATUS_TRAY_BLUETOOTH_ENABLED
                       : IDS_ASH_STATUS_TRAY_BLUETOOTH_DISABLED);
        SetLabel(label);
        SetAccessibleName(label);
        SetVisible(true);
        break;
    }
    UpdateStyle();
  }

 protected:
  // TrayItemMore:
  std::unique_ptr<TrayPopupItemStyle> HandleCreateStyle() const override {
    TrayBluetoothHelper* helper = Shell::Get()->tray_bluetooth_helper();
    std::unique_ptr<TrayPopupItemStyle> style =
        TrayItemMore::HandleCreateStyle();
    TrayPopupItemStyle::ColorStyle color_style;
    switch (helper->GetBluetoothState()) {
      case BluetoothSystem::State::kUnsupported:
      case BluetoothSystem::State::kUnavailable:
        color_style = TrayPopupItemStyle::ColorStyle::INACTIVE;
        break;
      case BluetoothSystem::State::kPoweredOff:
      case BluetoothSystem::State::kTransitioning:
        color_style = TrayPopupItemStyle::ColorStyle::DISABLED;
        break;
      case BluetoothSystem::State::kPoweredOn:
        color_style = TrayPopupItemStyle::ColorStyle::ACTIVE;
        break;
    }
    style->set_color_style(color_style);

    return style;
  }

  void UpdateStyle() override {
    TrayItemMore::UpdateStyle();
    std::unique_ptr<TrayPopupItemStyle> style = CreateStyle();
    SetImage(gfx::CreateVectorIcon(GetCurrentIcon(), style->GetIconColor()));
  }

 private:
  const gfx::VectorIcon& GetCurrentIcon() {
    TrayBluetoothHelper* helper = Shell::Get()->tray_bluetooth_helper();
    if (helper->GetBluetoothState() != BluetoothSystem::State::kPoweredOn)
      return kSystemMenuBluetoothDisabledIcon;

    bool has_connected_device = false;
    BluetoothDeviceList list = helper->GetAvailableBluetoothDevices();
    for (const auto& device : list) {
      if (device.connected) {
        has_connected_device = true;
        break;
      }
    }
    return has_connected_device ? kSystemMenuBluetoothConnectedIcon
                                : kSystemMenuBluetoothIcon;
  }

  DISALLOW_COPY_AND_ASSIGN(BluetoothDefaultView);
};

}  // namespace tray

TrayBluetooth::TrayBluetooth(SystemTray* system_tray)
    : SystemTrayItem(system_tray, SystemTrayItemUmaType::UMA_BLUETOOTH),
      default_(nullptr),
      detailed_(nullptr),
      detailed_view_delegate_(
          std::make_unique<SystemTrayItemDetailedViewDelegate>(this)) {
  Shell::Get()->system_tray_notifier()->AddBluetoothObserver(this);
}

TrayBluetooth::~TrayBluetooth() {
  Shell::Get()->system_tray_notifier()->RemoveBluetoothObserver(this);
}

views::View* TrayBluetooth::CreateDefaultView(LoginStatus status) {
  CHECK(default_ == nullptr);
  SessionController* session_controller = Shell::Get()->session_controller();
  default_ = new tray::BluetoothDefaultView(this);
  if (!session_controller->IsActiveUserSessionStarted()) {
    // Bluetooth power setting is always mutable in login screen before any
    // user logs in. The changes will affect local state preferences.
    default_->SetEnabled(true);
  } else {
    // The bluetooth setting should be mutable only if:
    // * the active user is the primary user, and
    // * the session is not in lock screen
    // The changes will affect the primary user's preferences.
    default_->SetEnabled(session_controller->IsUserPrimary() &&
                         status != LoginStatus::LOCKED);
  }
  default_->Update();
  return default_;
}

views::View* TrayBluetooth::CreateDetailedView(LoginStatus status) {
  if (!Shell::Get()->tray_bluetooth_helper()->IsBluetoothStateAvailable())
    return nullptr;

  Shell::Get()->metrics()->RecordUserMetricsAction(
      UMA_STATUS_AREA_DETAILED_BLUETOOTH_VIEW);
  CHECK(detailed_ == nullptr);
  detailed_ =
      new tray::BluetoothDetailedView(detailed_view_delegate_.get(), status);
  detailed_->Update();
  return detailed_;
}

void TrayBluetooth::OnDefaultViewDestroyed() {
  default_ = nullptr;
}

void TrayBluetooth::OnDetailedViewDestroyed() {
  detailed_ = nullptr;
}

void TrayBluetooth::UpdateAfterLoginStatusChange(LoginStatus status) {}

void TrayBluetooth::OnBluetoothRefresh() {
  if (default_)
    default_->Update();
  else if (detailed_)
    detailed_->Update();
}

void TrayBluetooth::OnBluetoothDiscoveringChanged() {
  if (!detailed_)
    return;
  detailed_->Update();
}

}  // namespace ash
