// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_IME_UNIFIED_IME_DETAILED_VIEW_CONTROLLER_H_
#define ASH_SYSTEM_IME_UNIFIED_IME_DETAILED_VIEW_CONTROLLER_H_

#include <memory>

#include "ash/accessibility/accessibility_observer.h"
#include "ash/system/ime/ime_observer.h"
#include "ash/system/unified/detailed_view_controller.h"
#include "ash/system/virtual_keyboard/virtual_keyboard_observer.h"

namespace ash {

class DetailedViewDelegate;
class IMEDetailedView;
class UnifiedSystemTrayController;

// Controller of IME detailed view in UnifiedSystemTray.
class UnifiedIMEDetailedViewController : public DetailedViewController,
                                         public VirtualKeyboardObserver,
                                         public AccessibilityObserver,
                                         public IMEObserver {
 public:
  explicit UnifiedIMEDetailedViewController(
      UnifiedSystemTrayController* tray_controller);

  UnifiedIMEDetailedViewController(const UnifiedIMEDetailedViewController&) =
      delete;
  UnifiedIMEDetailedViewController& operator=(
      const UnifiedIMEDetailedViewController&) = delete;

  ~UnifiedIMEDetailedViewController() override;

  // DetailedViewControllerBase:
  views::View* CreateView() override;
  std::u16string GetAccessibleName() const override;

  // VirtualKeyboardObserver:
  void OnKeyboardSuppressionChanged(bool suppressed) override;

  // AccessibilityObserver:
  void OnAccessibilityStatusChanged() override;

  // IMEObserver:
  void OnIMERefresh() override;
  void OnIMEMenuActivationChanged(bool is_active) override;

 private:
  void Update();

  bool ShouldShowKeyboardToggle() const;

  const std::unique_ptr<DetailedViewDelegate> detailed_view_delegate_;

  IMEDetailedView* view_ = nullptr;

  bool keyboard_suppressed_ = false;
};

}  // namespace ash

#endif  // ASH_SYSTEM_IME_UNIFIED_IME_DETAILED_VIEW_CONTROLLER_H_
