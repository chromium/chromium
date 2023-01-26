// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/brightness/quick_settings_display_detailed_view_controller.h"

#include "ash/strings/grit/ash_strings.h"
#include "ash/system/brightness/display_detailed_view.h"
#include "ash/system/tray/detailed_view_delegate.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

QuickSettingsDisplayDetailedViewController::
    QuickSettingsDisplayDetailedViewController(
        UnifiedSystemTrayController* tray_controller)
    : detailed_view_delegate_(
          std::make_unique<DetailedViewDelegate>(tray_controller)),
      tray_controller_(tray_controller) {}

QuickSettingsDisplayDetailedViewController::
    ~QuickSettingsDisplayDetailedViewController() = default;

std::unique_ptr<views::View>
QuickSettingsDisplayDetailedViewController::CreateView() {
  return std::make_unique<DisplayDetailedView>(detailed_view_delegate_.get(),
                                               tray_controller_);
}

std::u16string QuickSettingsDisplayDetailedViewController::GetAccessibleName()
    const {
  return l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_DISPLAY);
}

}  // namespace ash
