// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accessibility/ui/focus_ring_controller.h"

#include <memory>
#include <string_view>

#include "ash/accessibility/ui/focus_ring_layer.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "ash/system/tray/tray_background_view.h"
#include "ash/wm/window_util.h"
#include "ui/aura/window.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ash {

FocusRingController::FocusRingController() = default;

FocusRingController::~FocusRingController() {
  SetVisible(false);
  CHECK(!IsInObserverList());
}

void FocusRingController::SetVisible(bool visible) {
  if (visible_ == visible)
    return;

  visible_ = visible;

  if (visible_) {
    views::WidgetFocusManager::GetInstance()->AddFocusChangeListener(this);
    aura::Window* active_window = window_util::GetActiveWindow();
    if (active_window)
      SetWidget(views::Widget::GetWidgetForNativeWindow(active_window));
  } else {
    views::WidgetFocusManager::GetInstance()->RemoveFocusChangeListener(this);
    SetWidget(nullptr);
  }
}

void FocusRingController::UpdateFocusRing() {
  views::View* view = NULL;
  if (widget_ && widget_->GetFocusManager())
    view = widget_->GetFocusManager()->GetFocusedView();

  // No focus ring if no focused view or the focused view covers the whole
  // widget content area (such as RenderWidgetHostWidgetAura).
  if (!view || view->ConvertRectToWidget(view->bounds()) ==
                   widget_->GetContentsView()->bounds()) {
    focus_ring_layer_.reset();
    return;
  }

  gfx::Rect view_bounds = view->GetContentsBounds();

  // Workarounds that attempts to pick a better bounds.
  if (std::string_view(view->GetClassName()) ==
      std::string_view(views::LabelButton::kViewClassName)) {
    view_bounds = view->GetLocalBounds();
    view_bounds.Inset(2);
  }

  // Convert view bounds to widget/window coordinates.
  view_bounds = view->ConvertRectToWidget(view_bounds);

  // Translate window coordinates to root window coordinates.
  DCHECK(view->GetWidget());
  aura::Window* window = view->GetWidget()->GetNativeWindow();
  aura::Window* root_window = window->GetRootWindow();
  gfx::Point origin = view_bounds.origin();
  aura::Window::ConvertPointToTarget(window, root_window, &origin);
  view_bounds.set_origin(origin);

  // Update the focus ring layer.
  if (!focus_ring_layer_)
    focus_ring_layer_ = std::make_unique<FocusRingLayer>(this);
  aura::Window* container = Shell::GetContainer(
      root_window, kShellWindowId_AccessibilityBubbleContainer);
  focus_ring_layer_->Set(container, view_bounds, /*stack_at_top=*/true);
}

void FocusRingController::OnDeviceScaleFactorChanged() {
  UpdateFocusRing();
}

void FocusRingController::SetWidget(views::Widget* widget) {
  if (widget_) {
    widget_->RemoveObserver(this);
    if (widget_->GetFocusManager())
      widget_->GetFocusManager()->RemoveFocusChangeListener(this);
  }

  widget_ = widget;

  if (widget_) {
    widget_->AddObserver(this);
    if (widget_->GetFocusManager())
      widget_->GetFocusManager()->AddFocusChangeListener(this);
  }

  UpdateFocusRing();
}

void FocusRingController::OnWidgetDestroying(views::Widget* widget) {
  DCHECK_EQ(widget_, widget);
  SetWidget(NULL);
}

void FocusRingController::OnWidgetBoundsChanged(views::Widget* widget,
                                                const gfx::Rect& new_bounds) {
  DCHECK_EQ(widget_, widget);
  UpdateFocusRing();
}

void FocusRingController::OnNativeFocusChanged(gfx::NativeView focused_now) {
  views::Widget* widget =
      focused_now ? views::Widget::GetWidgetForNativeWindow(focused_now) : NULL;
  SetWidget(widget);
}

void FocusRingController::OnWillChangeFocus(views::View* focused_before,
                                            views::View* focused_now) {}

void FocusRingController::OnDidChangeFocus(views::View* focused_before,
                                           views::View* focused_now) {
  DCHECK_EQ(focused_now, widget_->GetFocusManager()->GetFocusedView());
  UpdateFocusRing();
}

}  // namespace ash
