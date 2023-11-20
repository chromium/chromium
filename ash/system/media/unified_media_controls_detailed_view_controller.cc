// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/media/unified_media_controls_detailed_view_controller.h"

#include <memory>

#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/media/media_notification_provider.h"
#include "ash/system/media/unified_media_controls_detailed_view.h"
#include "ash/system/tray/detailed_view_delegate.h"
#include "ash/system/tray/tray_constants.h"
#include "base/metrics/histogram_functions.h"
#include "components/global_media_controls/public/constants.h"
#include "components/media_message_center/notification_theme.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

// static
bool UnifiedMediaControlsDetailedViewController::detailed_view_has_shown_ =
    false;

UnifiedMediaControlsDetailedViewController::
    UnifiedMediaControlsDetailedViewController(
        UnifiedSystemTrayController* tray_controller,
        global_media_controls::GlobalMediaControlsEntryPoint entry_point,
        const std::string& show_devices_for_item_id)
    : detailed_view_delegate_(
          std::make_unique<DetailedViewDelegate>(tray_controller)),
      entry_point_(entry_point),
      show_devices_for_item_id_(show_devices_for_item_id) {}

UnifiedMediaControlsDetailedViewController::
    ~UnifiedMediaControlsDetailedViewController() {
  if (!MediaNotificationProvider::Get())
    return;

  MediaNotificationProvider::Get()->OnBubbleClosing();
}

std::unique_ptr<views::View>
UnifiedMediaControlsDetailedViewController::CreateView() {
  DCHECK(MediaNotificationProvider::Get());

  media_message_center::NotificationTheme theme;
  theme.primary_text_color = AshColorProvider::Get()->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kTextColorPrimary);
  theme.secondary_text_color = AshColorProvider::Get()->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kTextColorSecondary);
  theme.enabled_icon_color = AshColorProvider::Get()->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kIconColorPrimary);
  theme.disabled_icon_color = AshColorProvider::Get()->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kIconColorSecondary);
  theme.separator_color = AshColorProvider::Get()->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kSeparatorColor);
  theme.background_color = AshColorProvider::Get()->GetControlsLayerColor(
      AshColorProvider::ControlsLayerType::kControlBackgroundColorInactive);
  MediaNotificationProvider::Get()->SetColorTheme(theme);

  base::UmaHistogramBoolean(
      "Media.CrosGlobalMediaControls.RepeatUsageInQuickSetting",
      detailed_view_has_shown_);
  detailed_view_has_shown_ = true;
  return std::make_unique<UnifiedMediaControlsDetailedView>(
      detailed_view_delegate_.get(),
      MediaNotificationProvider::Get()->GetMediaNotificationListView(
          kMenuSeparatorWidth, /*should_clip_height=*/false, entry_point_,
          show_devices_for_item_id_));
}

std::u16string UnifiedMediaControlsDetailedViewController::GetAccessibleName()
    const {
  return l10n_util::GetStringUTF16(
      IDS_ASH_QUICK_SETTINGS_BUBBLE_MEDIA_CONTROLS_SUB_MENU_ACCESSIBLE_DESCRIPTION);
}

}  // namespace ash
