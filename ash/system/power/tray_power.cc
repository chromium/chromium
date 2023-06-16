// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/power/tray_power.h"

#include <utility>

#include "ash/accessibility/accessibility_delegate.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/system/power/battery_notification.h"
#include "ash/system/power/dual_role_notification.h"
#include "ash/system/time/time_view.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_item_view.h"
#include "ash/system/tray/tray_utils.h"
#include "base/command_line.h"
#include "base/metrics/histogram.h"
#include "base/time/time.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/chromeos/devicetype_utils.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/image/image_skia_source.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/view.h"

using message_center::MessageCenter;
using message_center::Notification;

namespace ash {

PowerTrayView::PowerTrayView(Shelf* shelf) : TrayItemView(shelf) {
  CreateImageView();

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

  // Ensure battery isn't too tall in side shelf.
  return gfx::Size(standard_size.width(), kUnifiedTrayIconSize);
}

void PowerTrayView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  // A valid role must be set prior to setting the name.
  node_data->role = ax::mojom::Role::kImage;
  node_data->SetNameChecked(GetAccessibleName());
}

views::View* PowerTrayView::GetTooltipHandlerForPoint(const gfx::Point& point) {
  return GetLocalBounds().Contains(point) ? this : nullptr;
}

std::u16string PowerTrayView::GetTooltipText(const gfx::Point& p) const {
  return tooltip_;
}

const char* PowerTrayView::GetClassName() const {
  return "PowerTrayView";
}

void PowerTrayView::OnThemeChanged() {
  TrayItemView::OnThemeChanged();
  UpdateStatus();
  UpdateImage(/*icon_color_changed=*/true);
}

void PowerTrayView::HandleLocaleChange() {
  UpdateStatus();
}

void PowerTrayView::UpdateLabelOrImageViewColor(bool active) {
  if (!chromeos::features::IsJellyEnabled()) {
    return;
  }
  TrayItemView::UpdateLabelOrImageViewColor(active);

  const SkColor icon_fg_color = GetColorProvider()->GetColor(
      active ? cros_tokens::kCrosSysSystemOnPrimaryContainer
             : cros_tokens::kCrosSysOnSurface);
  const PowerStatus::BatteryImageInfo& info =
      PowerStatus::Get()->GetBatteryImageInfo();
  image_view()->SetImage(PowerStatus::GetBatteryImage(
      info, kUnifiedTrayBatteryIconSize, icon_fg_color));
}

void PowerTrayView::OnPowerStatusChanged() {
  UpdateStatus();
}

void PowerTrayView::UpdateStatus() {
  UpdateImage(/*icon_color_changed=*/false);
  SetVisible(PowerStatus::Get()->IsBatteryPresent());
  SetAccessibleName(PowerStatus::Get()->GetAccessibleNameString(true));
  tooltip_ = PowerStatus::Get()->GetInlinedStatusString();
  // Currently ChromeVox only reads the inner view when touching the icon.
  // As a result this node's accessible node data will not be read.
  image_view()->SetAccessibleName(GetAccessibleName());
}

void PowerTrayView::UpdateImage(bool icon_color_changed) {
  const PowerStatus::BatteryImageInfo& info =
      PowerStatus::Get()->GetBatteryImageInfo();
  // Only change the image when the info changes or the icon color has
  // changed. http://crbug.com/589348
  if (info_ && info_->ApproximatelyEqual(info) && !icon_color_changed)
    return;
  info_ = info;

  if (!chromeos::features::IsJellyEnabled()) {
    // Note: The icon color changes when the UI is in OOBE mode.
    const SkColor icon_fg_color =
        GetColorProvider()->GetColor(kColorAshIconColorPrimary);
    image_view()->SetImage(PowerStatus::GetBatteryImage(
        info, kUnifiedTrayBatteryIconSize, icon_fg_color));
    return;
  }
  UpdateLabelOrImageViewColor(is_active());
}

}  // namespace ash
