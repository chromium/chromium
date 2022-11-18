// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TRAY_TRAY_BUBBLE_WRAPPER_H_
#define ASH_SYSTEM_TRAY_TRAY_BUBBLE_WRAPPER_H_

#include "ash/ash_export.h"
#include "ash/system/tray/tray_bubble_base.h"
#include "ui/views/widget/widget_observer.h"
#include "ui/wm/public/activation_change_observer.h"

namespace ash {

class TrayBackgroundView;
class TrayBubbleView;

// Creates and manages the Widget and EventFilter components of a bubble.
class ASH_EXPORT TrayBubbleWrapper : public TrayBubbleBase,
                                     public ::wm::ActivationChangeObserver {
 public:
  // `event_handling` When set to false disables the tray's event filtering
  // and also ignores the activation events. Eche window is an example of a use
  // case in which we do not want the keyboard events (both inside and outside
  // of the bubble) be filtered and also we do not want activaion of other
  // windows closes the bubble.
  explicit TrayBubbleWrapper(TrayBackgroundView* tray,
                             bool event_handling = true);

  TrayBubbleWrapper(const TrayBubbleWrapper&) = delete;
  TrayBubbleWrapper& operator=(const TrayBubbleWrapper&) = delete;

  ~TrayBubbleWrapper() override;

  void ShowBubble(std::unique_ptr<TrayBubbleView> bubble_view);

  // TrayBubbleBase overrides:
  TrayBackgroundView* GetTray() const override;
  TrayBubbleView* GetBubbleView() const override;
  views::Widget* GetBubbleWidget() const override;

  // views::WidgetObserver overrides:
  void OnWidgetDestroying(views::Widget* widget) override;
  void OnWidgetBoundsChanged(views::Widget* widget,
                             const gfx::Rect& new_bounds) override;

  // ::wm::ActivationChangeObserver overrides:
  void OnWindowActivated(ActivationReason reason,
                         aura::Window* gained_active,
                         aura::Window* lost_active) override;

  TrayBackgroundView* tray() { return tray_; }
  TrayBubbleView* bubble_view() { return bubble_view_; }
  views::Widget* bubble_widget() { return bubble_widget_; }

 private:
  TrayBackgroundView* tray_;
  views::Widget* bubble_widget_ = nullptr;

  // Owned by `bubble_widget_`
  TrayBubbleView* bubble_view_ = nullptr;

  // When set to false disables the tray's event filtering
  // and also ignores the activation events. Eche window is an example of a use
  // case in which we do not want the keyboard events (both inside and outside
  // of the bubble) be filtered and also we do not want activaion of other
  // windows closes the bubble.
  const bool event_handling_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_TRAY_TRAY_BUBBLE_WRAPPER_H_
