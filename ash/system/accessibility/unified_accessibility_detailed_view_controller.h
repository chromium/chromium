// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_ACCESSIBILITY_UNIFIED_ACCESSIBILITY_DETAILED_VIEW_CONTROLLER_H_
#define ASH_SYSTEM_ACCESSIBILITY_UNIFIED_ACCESSIBILITY_DETAILED_VIEW_CONTROLLER_H_

#include <memory>

#include "ash/accessibility/accessibility_observer.h"
#include "ash/system/unified/detailed_view_controller.h"
#include "base/macros.h"

namespace ash {

namespace tray {
class AccessibilityDetailedView;
}  // namespace tray

class DetailedViewDelegate;
class UnifiedSystemTrayController;

// Controller of Accessibility detailed view in UnifiedSystemTray.
class UnifiedAccessibilityDetailedViewController
    : public DetailedViewController,
      public AccessibilityObserver {
 public:
  explicit UnifiedAccessibilityDetailedViewController(
      UnifiedSystemTrayController* tray_controller);
  ~UnifiedAccessibilityDetailedViewController() override;

  // DetailedViewControllerBase:
  views::View* CreateView() override;

  // AccessibilityObserver:
  void OnAccessibilityStatusChanged() override;

 private:
  const std::unique_ptr<DetailedViewDelegate> detailed_view_delegate_;

  tray::AccessibilityDetailedView* view_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(UnifiedAccessibilityDetailedViewController);
};

}  // namespace ash

#endif  // ASH_SYSTEM_ACCESSIBILITY_UNIFIED_ACCESSIBILITY_DETAILED_VIEW_CONTROLLER_H_
