// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_ACCESSIBILITY_FLOATING_ACCESSIBILITY_DETAILED_CONTROLLER_H_
#define ASH_SYSTEM_ACCESSIBILITY_FLOATING_ACCESSIBILITY_DETAILED_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/system/tray/detailed_view_delegate.h"
#include "ash/system/tray/tray_bubble_view.h"
#include "base/memory/raw_ptr.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/wm/public/activation_change_observer.h"

namespace ash {

class AccessibilityDetailedView;

// Controller for the detailed view of accessibility floating menu.
class ASH_EXPORT FloatingAccessibilityDetailedController
    : public TrayBubbleView::Delegate,
      public DetailedViewDelegate,
      public ::wm::ActivationChangeObserver {
 public:
  class Delegate {
   public:
    virtual void OnDetailedMenuClosed() {}
    virtual views::Widget* GetBubbleWidget() = 0;
    virtual ~Delegate() = default;
  };

  explicit FloatingAccessibilityDetailedController(Delegate* delegate);
  ~FloatingAccessibilityDetailedController() override;

  void Show(gfx::Rect anchor_rect, views::BubbleBorder::Arrow alignment);
  void UpdateAnchorRect(gfx::Rect anchor_rect,
                        views::BubbleBorder::Arrow alignment);
  // DetailedViewDelegate:
  void CloseBubble() override;
  void TransitionToMainView(bool restore_focus) override;
  std::u16string GetAccessibleNameForBubble() override;

  void OnAccessibilityStatusChanged();

 private:
  friend class FloatingAccessibilityControllerTest;
  class DetailedBubbleView;

  // DetailedViewDelegate:
  views::Button* CreateBackButton(
      views::Button::PressedCallback callback) override;
  views::Button* CreateHelpButton(
      views::Button::PressedCallback callback) override;
  // TrayBubbleView::Delegate:
  void BubbleViewDestroyed() override;
  void HideBubble(const TrayBubbleView* bubble_view) override;

  // ::wm::ActivationChangeObserver:
  void OnWindowActivated(ActivationReason reason,
                         aura::Window* gained_active,
                         aura::Window* lost_active) override;

  raw_ptr<DetailedBubbleView> bubble_view_ = nullptr;
  raw_ptr<views::Widget> bubble_widget_ = nullptr;
  raw_ptr<AccessibilityDetailedView> detailed_view_ = nullptr;

  const raw_ptr<Delegate> delegate_;  // Owns us.
};

}  // namespace ash

#endif  // ASH_SYSTEM_ACCESSIBILITY_FLOATING_ACCESSIBILITY_DETAILED_CONTROLLER_H_
