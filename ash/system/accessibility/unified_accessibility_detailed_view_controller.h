// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_ACCESSIBILITY_UNIFIED_ACCESSIBILITY_DETAILED_VIEW_CONTROLLER_H_
#define ASH_SYSTEM_ACCESSIBILITY_UNIFIED_ACCESSIBILITY_DETAILED_VIEW_CONTROLLER_H_

#include <memory>

#include "ash/accessibility/accessibility_observer.h"
#include "ash/system/unified/detailed_view_controller.h"
#include "base/memory/raw_ptr.h"

namespace ash {

class AccessibilityDetailedView;
class DetailedViewDelegate;
class UnifiedSystemTrayController;

// Controller of Accessibility detailed view in UnifiedSystemTray.
class UnifiedAccessibilityDetailedViewController
    : public DetailedViewController,
      public AccessibilityObserver {
 public:
  explicit UnifiedAccessibilityDetailedViewController(
      UnifiedSystemTrayController* tray_controller);

  UnifiedAccessibilityDetailedViewController(
      const UnifiedAccessibilityDetailedViewController&) = delete;
  UnifiedAccessibilityDetailedViewController& operator=(
      const UnifiedAccessibilityDetailedViewController&) = delete;

  ~UnifiedAccessibilityDetailedViewController() override;

  // DetailedViewController:
  std::unique_ptr<views::View> CreateView() override;
  std::u16string GetAccessibleName() const override;

  // AccessibilityObserver:
  void OnAccessibilityStatusChanged() override;

  AccessibilityDetailedView* accessibility_detailed_view_for_testing() {
    return view_;
  }

 private:
  const std::unique_ptr<DetailedViewDelegate> detailed_view_delegate_;

  raw_ptr<AccessibilityDetailedView, DanglingUntriaged> view_ = nullptr;
};

}  // namespace ash

#endif  // ASH_SYSTEM_ACCESSIBILITY_UNIFIED_ACCESSIBILITY_DETAILED_VIEW_CONTROLLER_H_
