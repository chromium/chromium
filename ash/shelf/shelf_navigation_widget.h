// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_SHELF_NAVIGATION_WIDGET_H_
#define ASH_SHELF_SHELF_NAVIGATION_WIDGET_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/public/cpp/tablet_mode_observer.h"
#include "ash/shell_observer.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/views/accessible_pane_view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace aura {
class Window;
}

namespace views {
class BoundsAnimator;
}

namespace ash {
class BackButton;
class HomeButton;
class Shelf;
class ShelfView;

// The shelf navigation widget holds the home button and (when in tablet mode)
// the back button.
class ASH_EXPORT ShelfNavigationWidget : public views::Widget,
                                         public TabletModeObserver,
                                         public ShellObserver,
                                         public ui::ImplicitAnimationObserver,
                                         public ShelfConfig::Observer {
 public:
  ShelfNavigationWidget(Shelf* shelf, ShelfView* shelf_view);
  ~ShelfNavigationWidget() override;

  // Initializes the widget, sets its contents view and basic properties.
  void Initialize(aura::Window* container);

  // Returns the size that this widget would like to have depending on whether
  // tablet mode is on.
  gfx::Size GetIdealSize() const;

  // views::Widget:
  void OnMouseEvent(ui::MouseEvent* event) override;
  bool OnNativeWidgetActivationChanged(bool active) override;
  void OnGestureEvent(ui::GestureEvent* event) override;

  // Getters for the back and home buttons.
  BackButton* GetBackButton() const;
  HomeButton* GetHomeButton() const;

  // Places the keyboard focus on either the first or the last child of this
  // widget.
  void FocusFirstOrLastFocusableChild(bool last);

  // Sets whether the last focusable child (instead of the first) should be
  // focused when activating this widget.
  void SetDefaultLastFocusableChild(bool default_last_focusable_child);

  // TabletModeObserver:
  void OnTabletModeStarted() override;
  void OnTabletModeEnded() override;

  // ShellObserver:
  void OnShelfAlignmentChanged(aura::Window* root_window) override;

  // ui::ImplicitAnimationObserver:
  void OnImplicitAnimationsCompleted() override;

  // ShelfConfig::Observer:
  void OnShelfConfigUpdated() override;

  views::BoundsAnimator* get_bounds_animator_for_testing() {
    return bounds_animator_.get();
  }

 private:
  class Delegate;

  // Updates this widget's layout according to current constraints: tablet
  // mode and shelf orientation.
  void UpdateLayout();

  Shelf* shelf_ = nullptr;
  Delegate* delegate_ = nullptr;
  std::unique_ptr<views::BoundsAnimator> bounds_animator_;

  DISALLOW_COPY_AND_ASSIGN(ShelfNavigationWidget);
};

}  // namespace ash

#endif  // ASH_SHELF_SHELF_NAVIGATION_WIDGET_H_
