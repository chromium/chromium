// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/power/tray_power.h"

#include <utility>

#include "ash/accessibility/accessibility_delegate.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/system/power/battery_notification.h"
#include "ash/system/power/dual_role_notification.h"
#include "ash/system/power/power_status.h"
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
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/chromeos/devicetype_utils.h"
#include "ui/chromeos/styles/cros_styles.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/image/image_skia_source.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/view.h"

using message_center::MessageCenter;
using message_center::Notification;

namespace ash {

PowerTrayView::PowerTrayView(Shelf* shelf) : TrayItemView(shelf) {
  CreateImageView();

  previous_battery_saver_state_ = PowerStatus::Get()->IsBatterySaverActive();
  PowerStatus::Get()->AddObserver(this);

  GetViewAccessibility().SetRole(ax::mojom::Role::kImage);
  UpdateAccessibleName();
}

PowerTrayView::~PowerTrayView() {
  PowerStatus::Get()->RemoveObserver(this);
}

gfx::Size PowerTrayView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  // The battery icon is a lot thinner than other icons, hence the special
  // logic.
  gfx::Size standard_size =
      TrayItemView::CalculatePreferredSize(available_size);
  if (IsHorizontalAlignment())
    return gfx::Size(kUnifiedTrayBatteryWidth, standard_size.height());

  // Ensure battery isn't too tall in side shelf.
  return gfx::Size(standard_size.width(), kUnifiedTrayIconSize);
}

views::View* PowerTrayView::GetTooltipHandlerForPoint(const gfx::Point& point) {
  return GetLocalBounds().Contains(point) ? this : nullptr;
}

std::u16string PowerTrayView::GetTooltipText(const gfx::Point& p) const {
  return tooltip_;
}

void PowerTrayView::OnThemeChanged() {
  TrayItemView::OnThemeChanged();
  UpdateStatus(false);
  UpdateImage(/*icon_color_changed=*/true);
}

void PowerTrayView::HandleLocaleChange() {
  UpdateStatus(false);
}

void PowerTrayView::UpdateLabelOrImageViewColor(bool active) {
  TrayItemView::UpdateLabelOrImageViewColor(active);

  cros_tokens::CrosSysColorIds icon_fg_token = cros_tokens::kCrosSysOnSurface;
  if (active) {
    icon_fg_token = cros_tokens::kCrosSysSystemOnPrimaryContainer;
  } else if (features::IsBatterySaverAvailable() &&
             PowerStatus::Get()->IsBatterySaverActive()) {
    icon_fg_token = cros_tokens::kCrosSysOnWarningContainer;
  }
  const SkColor icon_fg_color = GetColorProvider()->GetColor(icon_fg_token);

  PowerStatus::BatteryImageInfo info =
      PowerStatus::Get()->GenerateBatteryImageInfo(icon_fg_color);

  image_view()->SetImage(PowerStatus::GetBatteryImage(
      info, kUnifiedTrayBatteryIconSize, GetColorProvider()));
}

void PowerTrayView::OnPowerStatusChanged() {
  const bool bsm_active = PowerStatus::Get()->IsBatterySaverActive();
  UpdateStatus(bsm_active != previous_battery_saver_state_);
  previous_battery_saver_state_ = bsm_active;
}

void PowerTrayView::UpdateStatus(bool icon_color_changed) {
  UpdateImage(icon_color_changed);
  SetVisible(PowerStatus::Get()->IsBatteryPresent());
  UpdateAccessibleName();
  tooltip_ = PowerStatus::Get()->GetInlinedStatusString();
  // Currently ChromeVox only reads the inner view when touching the icon.
  // As a result this node's accessible node data will not be read.
  // TODO(crbug.com/325137417): This line should not be needed. Investigate to
  // confirm and remove.
  image_view()->GetViewAccessibility().SetName(
      GetViewAccessibility().GetCachedName());
}

void PowerTrayView::UpdateImage(bool icon_color_changed) {
  SkColor prev_foreground_color = SK_ColorWHITE,
          prev_badge_color = SK_ColorWHITE;
  if (info_) {
    prev_foreground_color = info_->battery_color_preferences.foreground_color;
    prev_badge_color =
        info_->battery_color_preferences.badge_color.value_or(SK_ColorWHITE);
  }

  PowerStatus::BatteryImageInfo info =
      PowerStatus::Get()->GenerateBatteryImageInfo(prev_foreground_color,
                                                   prev_badge_color);

  // Only change the image when the info changes or the icon color has
  // changed. http://crbug.com/589348
  if (info_ && info_->ApproximatelyEqual(info) && !icon_color_changed)
    return;
  info_ = info;

  UpdateLabelOrImageViewColor(is_active());
}

void PowerTrayView::UpdateAccessibleName() {
  std::u16string accessible_name =
      PowerStatus::Get()->GetAccessibleNameString(/* full_description*/ true);
  if (!accessible_name.empty()) {
    GetViewAccessibility().SetName(accessible_name);
  } else {
    GetViewAccessibility().SetName(
        std::u16string(), ax::mojom::NameFrom::kAttributeExplicitlyEmpty);
  }
}

BEGIN_METADATA(PowerTrayView)
END_METADATA

}  // namespace ash
