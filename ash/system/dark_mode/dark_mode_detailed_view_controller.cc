// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/dark_mode/dark_mode_detailed_view_controller.h"

#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/dark_mode/dark_mode_detailed_view.h"
#include "ash/system/tray/detailed_view_delegate.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

DarkModeDetailedViewController::DarkModeDetailedViewController(
    UnifiedSystemTrayController* tray_controller)
    : detailed_view_delegate_(
          std::make_unique<DetailedViewDelegate>(tray_controller)) {
  AshColorProvider::Get()->AddObserver(this);
}

DarkModeDetailedViewController::~DarkModeDetailedViewController() {
  AshColorProvider::Get()->RemoveObserver(this);
}

views::View* DarkModeDetailedViewController::CreateView() {
  DCHECK(!view_);
  view_ = new DarkModeDetailedView(detailed_view_delegate_.get());
  return view_;
}

base::string16 DarkModeDetailedViewController::GetAccessibleName() const {
  return l10n_util::GetStringUTF16(
      IDS_ASH_QUICK_SETTINGS_BUBBLE_DARK_THEME_SETTINGS_ACCESSIBLE_DESCRIPTION);
}

void DarkModeDetailedViewController::OnColorModeChanged(
    bool dark_mode_enabled) {
  view_->UpdateToggleButton(dark_mode_enabled);
}

void DarkModeDetailedViewController::OnColorModeThemed(bool is_themed) {
  view_->UpdateCheckedButton(is_themed);
}

}  // namespace ash
