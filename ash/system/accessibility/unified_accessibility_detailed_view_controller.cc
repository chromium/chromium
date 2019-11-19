// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/accessibility/unified_accessibility_detailed_view_controller.h"

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/accessibility/tray_accessibility.h"
#include "ash/system/tray/detailed_view_delegate.h"

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

views::View* UnifiedAccessibilityDetailedViewController::CreateView() {
  DCHECK(!view_);
  view_ = new tray::AccessibilityDetailedView(detailed_view_delegate_.get());
  return view_;
}

void UnifiedAccessibilityDetailedViewController::
    OnAccessibilityStatusChanged() {
  view_->OnAccessibilityStatusChanged();
}

}  // namespace ash
