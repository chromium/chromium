// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TRAY_TRAY_BUBBLE_WRAPPER_H_
#define ASH_SYSTEM_TRAY_TRAY_BUBBLE_WRAPPER_H_

#include "ash/ash_export.h"
#include "ash/system/tray/tray_bubble_base.h"
#include "base/macros.h"
#include "ui/aura/window_observer.h"
#include "ui/views/widget/widget_observer.h"
#include "ui/wm/public/activation_change_observer.h"

namespace ash {

class TrayBackgroundView;
class TrayBubbleView;

// Creates and manages the Widget and EventFilter components of a bubble.
// TODO(tetsui): Remove this and use TrayBubbleBase for all bubbles.
class ASH_EXPORT TrayBubbleWrapper : public TrayBubbleBase,
                                     public views::WidgetObserver,
                                     public aura::WindowObserver,
                                     public ::wm::ActivationChangeObserver {
 public:
  TrayBubbleWrapper(TrayBackgroundView* tray,
                    TrayBubbleView* bubble_view,
                    bool is_persistent);
  ~TrayBubbleWrapper() override;

  // TrayBubbleBase overrides:
  TrayBackgroundView* GetTray() const override;
  TrayBubbleView* GetBubbleView() const override;
  views::Widget* GetBubbleWidget() const override;

  // views::WidgetObserver overrides:
  void OnWidgetClosing(views::Widget* widget) override;
  void OnWidgetDestroying(views::Widget* widget) override;
  void OnWidgetBoundsChanged(views::Widget* widget,
                             const gfx::Rect& new_bounds) override;

  // aura::WindowObserver:
  void OnWindowBoundsChanged(aura::Window* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds,
                             ui::PropertyChangeReason reason) override;

  // ::wm::ActivationChangeObserver overrides:
  void OnWindowActivated(ActivationReason reason,
                         aura::Window* gained_active,
                         aura::Window* lost_active) override;

  TrayBackgroundView* tray() { return tray_; }
  TrayBubbleView* bubble_view() { return bubble_view_; }
  views::Widget* bubble_widget() { return bubble_widget_; }

 private:
  TrayBackgroundView* tray_;
  TrayBubbleView* bubble_view_;  // unowned
  views::Widget* bubble_widget_;
  bool is_persistent_;

  DISALLOW_COPY_AND_ASSIGN(TrayBubbleWrapper);
};

}  // namespace ash

#endif  // ASH_SYSTEM_TRAY_TRAY_BUBBLE_WRAPPER_H_
