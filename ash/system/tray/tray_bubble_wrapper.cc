// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/tray_bubble_wrapper.h"

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "ash/system/tray/tray_background_view.h"
#include "ash/system/tray/tray_bubble_view.h"
#include "ash/system/tray/tray_event_filter.h"
#include "ash/wm/container_finder.h"
#include "ui/aura/window.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/transient_window_manager.h"
#include "ui/wm/core/window_util.h"
#include "ui/wm/public/activation_client.h"

namespace ash {

TrayBubbleWrapper::TrayBubbleWrapper(TrayBackgroundView* tray,
                                     TrayBubbleView* bubble_view,
                                     bool is_persistent)
    : tray_(tray), bubble_view_(bubble_view), is_persistent_(is_persistent) {
  bubble_widget_ = views::BubbleDialogDelegateView::CreateBubble(bubble_view_);
  bubble_widget_->AddObserver(this);

  TrayBackgroundView::InitializeBubbleAnimations(bubble_widget_);
  tray_->UpdateBubbleViewArrow(bubble_view_);
  bubble_view_->InitializeAndShowBubble();

  tray->tray_event_filter()->AddBubble(this);

  bubble_widget_->GetNativeWindow()->GetRootWindow()->AddObserver(this);

  if (!is_persistent_)
    Shell::Get()->activation_client()->AddObserver(this);
}

TrayBubbleWrapper::~TrayBubbleWrapper() {
  if (!is_persistent_)
    Shell::Get()->activation_client()->RemoveObserver(this);

  tray_->tray_event_filter()->RemoveBubble(this);
  if (bubble_widget_) {
    auto* transient_manager = ::wm::TransientWindowManager::GetOrCreate(
        bubble_widget_->GetNativeWindow());
    if (transient_manager) {
      for (auto* window : transient_manager->transient_children())
        transient_manager->RemoveTransientChild(window);
    }
    bubble_widget_->GetNativeWindow()->GetRootWindow()->RemoveObserver(this);
    bubble_widget_->RemoveObserver(this);
    bubble_widget_->Close();
  }
}

TrayBackgroundView* TrayBubbleWrapper::GetTray() const {
  return tray_;
}

TrayBubbleView* TrayBubbleWrapper::GetBubbleView() const {
  return bubble_view_;
}

views::Widget* TrayBubbleWrapper::GetBubbleWidget() const {
  return bubble_widget_;
}

void TrayBubbleWrapper::OnWidgetClosing(views::Widget* widget) {
  CHECK_EQ(bubble_widget_, widget);
  // Remove this from the observer list before the widget is closed and detached
  // from the root window.
  bubble_widget_->GetNativeWindow()->GetRootWindow()->RemoveObserver(this);
}

void TrayBubbleWrapper::OnWidgetDestroying(views::Widget* widget) {
  CHECK_EQ(bubble_widget_, widget);
  bubble_widget_->RemoveObserver(this);
  bubble_widget_ = NULL;

  // Although the bubble is already closed, the next mouse release event
  // will invoke PerformAction which reopens the bubble again. To prevent the
  // reopen, the mouse capture of |tray_| has to be released.
  // See crbug.com/177075
  tray_->GetWidget()->GetNativeWindow()->ReleaseCapture();

  tray_->HideBubbleWithView(bubble_view_);  // May destroy |bubble_view_|
}

void TrayBubbleWrapper::OnWidgetBoundsChanged(views::Widget* widget,
                                              const gfx::Rect& new_bounds) {
  DCHECK_EQ(bubble_widget_, widget);
  tray_->BubbleResized(bubble_view_);
}

void TrayBubbleWrapper::OnWindowBoundsChanged(aura::Window* window,
                                              const gfx::Rect& old_bounds,
                                              const gfx::Rect& new_bounds,
                                              ui::PropertyChangeReason reason) {
  tray_->UpdateAfterRootWindowBoundsChange(old_bounds, new_bounds);
}

void TrayBubbleWrapper::OnWindowActivated(ActivationReason reason,
                                          aura::Window* gained_active,
                                          aura::Window* lost_active) {
  if (!gained_active)
    return;

  int container_id = GetContainerForWindow(gained_active)->id();
  int lost_container_id =
      lost_active != nullptr ? GetContainerForWindow(lost_active)->id() : -1;

  // Don't close the bubble if a popup notification is activated.
  //
  // When the settings button in a notification popup is clicked,
  // the notification is activated and hidden almost at the same time.
  // In such case, the notification is deactivated without OnWindowActivated for
  // the activation being called.
  // We also have to ignore such case by checking |lost_container_id|.
  if (container_id == kShellWindowId_StatusContainer ||
      lost_container_id == kShellWindowId_StatusContainer ||
      container_id == kShellWindowId_SettingBubbleContainer) {
    return;
  }

  views::Widget* bubble_widget = bubble_view()->GetWidget();
  // Don't close the bubble if a transient child is gaining or losing
  // activation.
  if (bubble_widget == views::Widget::GetWidgetForNativeView(gained_active) ||
      ::wm::HasTransientAncestor(gained_active,
                                 bubble_widget->GetNativeWindow()) ||
      (lost_active && ::wm::HasTransientAncestor(
                          lost_active, bubble_widget->GetNativeWindow()))) {
    return;
  }

  tray_->CloseBubble();
}

}  // namespace ash
