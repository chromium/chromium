// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/accessibility/unified_accessibility_detailed_view_controller.h"

#include "ash/accessibility/accessibility_controller.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/accessibility/accessibility_detailed_view.h"
#include "ash/system/tray/detailed_view_delegate.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

UnifiedAccessibilityDetailedViewController::
    UnifiedAccessibilityDetailedViewController(
        UnifiedSystemTrayController* tray_controller)
    : detailed_view_delegate_(
          std::make_unique<DetailedViewDelegate>(tray_controller)) {
  Shell::Get()->accessibility_controller()->AddObserver(this);
}

UnifiedAccessibilityDetailedViewController::
    ~UnifiedAccessibilityDetailedViewController() {
  Shell::Get()->accessibility_controller()->RemoveObserver(this);
}

std::unique_ptr<views::View>
UnifiedAccessibilityDetailedViewController::CreateView() {
  DCHECK(!view_);
  auto view = std::make_unique<AccessibilityDetailedView>(
      detailed_view_delegate_.get());
  view_ = view.get();
  return view;
}

std::u16string UnifiedAccessibilityDetailedViewController::GetAccessibleName()
    const {
  return l10n_util::GetStringUTF16(
      IDS_ASH_QUICK_SETTINGS_BUBBLE_A11Y_SETTINGS_ACCESSIBLE_DESCRIPTION);
}

void UnifiedAccessibilityDetailedViewController::
    OnAccessibilityStatusChanged() {
  view_->OnAccessibilityStatusChanged();
}

}  // namespace ash
