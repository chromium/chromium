// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/media/unified_media_controls_detailed_view_controller.h"

#include "ash/public/cpp/media_notification_provider.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/media/unified_media_controls_detailed_view.h"
#include "ash/system/tray/detailed_view_delegate.h"
#include "ash/system/tray/tray_constants.h"
#include "base/metrics/histogram_functions.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

// static
bool UnifiedMediaControlsDetailedViewController::detailed_view_has_shown_ =
    false;

UnifiedMediaControlsDetailedViewController::
    UnifiedMediaControlsDetailedViewController(
        UnifiedSystemTrayController* tray_controller)
    : detailed_view_delegate_(
          std::make_unique<DetailedViewDelegate>(tray_controller)) {}

UnifiedMediaControlsDetailedViewController::
    ~UnifiedMediaControlsDetailedViewController() {
  if (!MediaNotificationProvider::Get())
    return;

  MediaNotificationProvider::Get()->OnBubbleClosing();
}

views::View* UnifiedMediaControlsDetailedViewController::CreateView() {
  DCHECK(MediaNotificationProvider::Get());

  base::UmaHistogramBoolean(
      "Media.CrosGlobalMediaControls.RepeatUsageInQuickSetting",
      detailed_view_has_shown_);
  detailed_view_has_shown_ = true;

  return new UnifiedMediaControlsDetailedView(
      detailed_view_delegate_.get(),
      MediaNotificationProvider::Get()->GetMediaNotificationListView(
          kMenuSeparatorWidth));
}

base::string16 UnifiedMediaControlsDetailedViewController::GetAccessibleName()
    const {
  return l10n_util::GetStringUTF16(
      IDS_ASH_QUICK_SETTINGS_BUBBLE_MEDIA_CONTROLS_ACCESSIBLE_DESCRIPTION);
}

}  // namespace ash
