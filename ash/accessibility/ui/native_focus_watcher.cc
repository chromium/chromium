// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accessibility/ui/native_focus_watcher.h"

#include "ash/wm/window_util.h"
namespace ash {

NativeFocusWatcher::NativeFocusWatcher() = default;

NativeFocusWatcher::~NativeFocusWatcher() {
  SetEnabled(false);
  CHECK(!IsInObserverList());
}

void NativeFocusWatcher::SetEnabled(bool enabled) {
  if (enabled_ == enabled) {
    return;
  }
  enabled_ = enabled;
  if (enabled_) {
    views::WidgetFocusManager::GetInstance()->AddFocusChangeListener(this);
    aura::Window* active_window = window_util::GetActiveWindow();
    if (active_window) {
      SetWidget(views::Widget::GetWidgetForNativeWindow(active_window));
    }
  } else {
    views::WidgetFocusManager::GetInstance()->RemoveFocusChangeListener(this);
    SetWidget(nullptr);
  }
}

void NativeFocusWatcher::AddObserver(NativeFocusObserver* observer) {
  observers_.AddObserver(observer);
}

void NativeFocusWatcher::RemoveObserver(NativeFocusObserver* observer) {
  observers_.RemoveObserver(observer);
}

void NativeFocusWatcher::UpdateFocusedView() {
  views::View* view = NULL;
  if (widget_ && widget_->GetFocusManager()) {
    view = widget_->GetFocusManager()->GetFocusedView();
  }

  // No focus ring if no focused view or the focused view covers the whole
  // widget content area (such as RenderWidgetHostWidgetAura).
  if (!view || view->ConvertRectToWidget(view->bounds()) ==
                   widget_->GetContentsView()->bounds()) {
    observers_.Notify(&NativeFocusObserver::OnNativeFocusCleared);
    return;
  }

  gfx::Rect view_bounds = view->GetContentsBounds();

  // Workarounds that attempts to pick a better bounds.
  if (view->GetClassName() == views::LabelButton::kViewClassName) {
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

  observers_.Notify(&NativeFocusObserver::OnNativeFocusChanged, view_bounds);
}

void NativeFocusWatcher::SetWidget(views::Widget* widget) {
  if (widget_) {
    widget_->RemoveObserver(this);
    if (widget_->GetFocusManager()) {
      widget_->GetFocusManager()->RemoveFocusChangeListener(this);
    }
  }

  widget_ = widget;

  if (widget_) {
    widget_->AddObserver(this);
    if (widget_->GetFocusManager()) {
      widget_->GetFocusManager()->AddFocusChangeListener(this);
    }
  }

  UpdateFocusedView();
}

void NativeFocusWatcher::OnWidgetDestroying(views::Widget* widget) {
  DCHECK_EQ(widget_, widget);
  SetWidget(NULL);
}

void NativeFocusWatcher::OnWidgetBoundsChanged(views::Widget* widget,
                                               const gfx::Rect& new_bounds) {
  DCHECK_EQ(widget_, widget);
  UpdateFocusedView();
}

void NativeFocusWatcher::OnNativeFocusChanged(gfx::NativeView focused_now) {
  views::Widget* widget =
      focused_now ? views::Widget::GetWidgetForNativeWindow(focused_now) : NULL;
  SetWidget(widget);
}

void NativeFocusWatcher::OnDidChangeFocus(views::View* focused_before,
                                          views::View* focused_now) {
  DCHECK_EQ(focused_now, widget_->GetFocusManager()->GetFocusedView());
  UpdateFocusedView();
}

}  // namespace ash
