// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/power/tray_power.h"

#include <utility>

#include "ash/accessibility/accessibility_delegate.h"
#include "ash/public/cpp/ash_switches.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/power/battery_notification.h"
#include "ash/system/power/dual_role_notification.h"
#include "ash/system/time/time_view.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_item_view.h"
#include "ash/system/tray/tray_utils.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/time/time.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/chromeos/devicetype_utils.h"
#include "ui/gfx/image/image_skia_source.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/view.h"

using message_center::MessageCenter;
using message_center::Notification;

namespace ash {

namespace tray {

PowerTrayView::PowerTrayView(Shelf* shelf) : TrayItemView(shelf) {
  CreateImageView();
  UpdateImage();
  UpdateStatus();
  PowerStatus::Get()->AddObserver(this);
}

PowerTrayView::~PowerTrayView() {
  PowerStatus::Get()->RemoveObserver(this);
}

gfx::Size PowerTrayView::CalculatePreferredSize() const {
  // The battery icon is a lot thinner than other icons, hence the special
  // logic.
  gfx::Size standard_size = TrayItemView::CalculatePreferredSize();
  if (IsHorizontalAlignment())
    return gfx::Size(kUnifiedTrayBatteryWidth, standard_size.height());
  return standard_size;
}

void PowerTrayView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->SetName(accessible_name_);
  node_data->role = ax::mojom::Role::kButton;
}

views::View* PowerTrayView::GetTooltipHandlerForPoint(const gfx::Point& point) {
  return GetLocalBounds().Contains(point) ? this : nullptr;
}

base::string16 PowerTrayView::GetTooltipText(const gfx::Point& p) const {
  return tooltip_;
}

const char* PowerTrayView::GetClassName() const {
  return "PowerTrayView";
}

void PowerTrayView::OnPowerStatusChanged() {
  UpdateStatus();
}

void PowerTrayView::OnSessionStateChanged(session_manager::SessionState state) {
  UpdateImage();
}

void PowerTrayView::UpdateStatus() {
  UpdateImage();
  SetVisible(PowerStatus::Get()->IsBatteryPresent());
  accessible_name_ = PowerStatus::Get()->GetAccessibleNameString(true);
  tooltip_ = PowerStatus::Get()->GetInlinedStatusString();
  // Currently ChromeVox only reads the inner view when touching the icon.
  // As a result this node's accessible node data will not be read.
  image_view()->SetAccessibleName(accessible_name_);
}

void PowerTrayView::UpdateImage() {
  const PowerStatus::BatteryImageInfo& info =
      PowerStatus::Get()->GetBatteryImageInfo();
  session_manager::SessionState session_state =
      Shell::Get()->session_controller()->GetSessionState();

  // Only change the image when the info changes and the icon color has not
  // changed. http://crbug.com/589348
  if (info_ && info_->ApproximatelyEqual(info) &&
      icon_session_state_color_ == session_state)
    return;
  info_ = info;
  icon_session_state_color_ = session_state;

  // Note: The icon color (both fg and bg) changes when the UI in in OOBE mode.
  const SkColor icon_fg_color = TrayIconColor(session_state);
  const SkColor icon_bg_color =
      AshColorProvider::GetSecondToneColor(icon_fg_color);
  image_view()->SetImage(PowerStatus::GetBatteryImage(
      info, kUnifiedTrayIconSize, icon_bg_color, icon_fg_color));
}

}  // namespace tray
}  // namespace ash
