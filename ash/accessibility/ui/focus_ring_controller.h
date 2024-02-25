// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCESSIBILITY_UI_FOCUS_RING_CONTROLLER_H_
#define ASH_ACCESSIBILITY_UI_FOCUS_RING_CONTROLLER_H_

#include <memory>

#include "ash/accessibility/ui/focus_ring_layer.h"
#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/focus/widget_focus_manager.h"
#include "ui/views/widget/widget_observer.h"

namespace views {
class View;
class Widget;
}  // namespace views

namespace ash {

// FocusRingController manages the focus ring around the focused view. It
// follows widget focus change and update the focus ring layer when the focused
// view of the widget changes.
class ASH_EXPORT FocusRingController : public AccessibilityLayerDelegate,
                                       public views::WidgetObserver,
                                       public views::WidgetFocusChangeListener,
                                       public views::FocusChangeListener {
 public:
  FocusRingController();

  FocusRingController(const FocusRingController&) = delete;
  FocusRingController& operator=(const FocusRingController&) = delete;

  ~FocusRingController() override;

  // Turns on/off the focus ring.
  void SetVisible(bool visible);

 private:
  // AccessibilityLayerDelegate.
  void OnDeviceScaleFactorChanged() override;

  // Sets the focused |widget|.
  void SetWidget(views::Widget* widget);

  // Updates the focus ring to the focused view of |widget_|. If |widget_| is
  // NULL or has no focused view, removes the focus ring. Otherwise, draws it.
  void UpdateFocusRing();

  // views::WidgetObserver overrides:
  void OnWidgetDestroying(views::Widget* widget) override;
  void OnWidgetBoundsChanged(views::Widget* widget,
                             const gfx::Rect& new_bounds) override;

  // views::WidgetFocusChangeListener overrides:
  void OnNativeFocusChanged(gfx::NativeView focused_now) override;

  // views::FocusChangeListener overrides:
  void OnWillChangeFocus(views::View* focused_before,
                         views::View* focused_now) override;
  void OnDidChangeFocus(views::View* focused_before,
                        views::View* focused_now) override;

  bool visible_ = false;

  raw_ptr<views::Widget> widget_ = nullptr;
  std::unique_ptr<FocusRingLayer> focus_ring_layer_;
};

}  // namespace ash

#endif  // ASH_ACCESSIBILITY_UI_FOCUS_RING_CONTROLLER_H_
