// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/tray_bubble_wrapper.h"

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/constants/ash_features.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/system/message_center/ash_message_popup_collection.h"
#include "ash/system/tray/tray_background_view.h"
#include "ash/system/tray/tray_bubble_view.h"
#include "ash/system/tray/tray_event_filter.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ui/aura/window.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/transient_window_manager.h"
#include "ui/wm/core/window_util.h"
#include "ui/wm/public/activation_client.h"

namespace ash {

TrayBubbleWrapper::TrayBubbleWrapper(TrayBackgroundView* tray,
                                     bool event_handling)
    : tray_(tray), event_handling_(event_handling) {}

TrayBubbleWrapper::~TrayBubbleWrapper() {
  if (event_handling_) {
    Shell::Get()->activation_client()->RemoveObserver(this);
    tray_->tray_event_filter()->RemoveBubble(this);
  }
  if (bubble_widget_) {
    auto* transient_manager = ::wm::TransientWindowManager::GetOrCreate(
        bubble_widget_->GetNativeWindow());
    if (transient_manager) {
      for (auto* window : transient_manager->transient_children())
        transient_manager->RemoveTransientChild(window);
    }
    bubble_widget_->RemoveObserver(this);
    bubble_widget_->Close();
  }
  CHECK(!TrayBubbleBase::IsInObserverList());
}

void TrayBubbleWrapper::ShowBubble(
    std::unique_ptr<TrayBubbleView> bubble_view) {
  // We must ensure `ShowBubble` is only called when there is no existing
  // `bubble_widget_`.
  DCHECK(!bubble_widget_);

  bubble_view_ = bubble_view.get();
  bubble_widget_ =
      views::BubbleDialogDelegateView::CreateBubble(std::move(bubble_view));
  bubble_widget_->AddObserver(this);

  TrayBackgroundView::InitializeBubbleAnimations(bubble_widget_);
  bubble_view_->InitializeAndShowBubble();

  if (!Shell::Get()->tablet_mode_controller()->InTabletMode())
    Shell::Get()->app_list_controller()->DismissAppList();

  if (event_handling_) {
    tray_->tray_event_filter()->AddBubble(this);
    Shell::Get()->activation_client()->AddObserver(this);
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

void TrayBubbleWrapper::OnWidgetDestroying(views::Widget* widget) {
  CHECK_EQ(bubble_widget_, widget);
  bubble_widget_->RemoveObserver(this);
  bubble_widget_ = nullptr;

  // Although the bubble is already closed, the next mouse release event
  // will invoke PerformAction which reopens the bubble again. To prevent the
  // reopen, the mouse capture of |tray_| has to be released.
  // See crbug.com/177075
  tray_->GetWidget()->GetNativeWindow()->ReleaseCapture();

  tray_->HideBubbleWithView(bubble_view_);  // May destroy |bubble_view_|
}

void TrayBubbleWrapper::OnWindowActivated(ActivationReason reason,
                                          aura::Window* gained_active,
                                          aura::Window* lost_active) {
  if (!gained_active)
    return;

  // Check for the CloseBubble() lock.
  if (!TrayBackgroundView::ShouldCloseBubbleOnWindowActivated())
    return;

  views::Widget* bubble_widget = bubble_view()->GetWidget();
  auto* gained_active_widget =
      views::Widget::GetWidgetForNativeView(gained_active);

  // Don't close the bubble if a transient child is gaining or losing
  // activation.
  if (bubble_widget == gained_active_widget ||
      ::wm::HasTransientAncestor(gained_active,
                                 bubble_widget->GetNativeWindow()) ||
      (lost_active && ::wm::HasTransientAncestor(
                          lost_active, bubble_widget->GetNativeWindow()))) {
    return;
  }

  // If the activated window is a popup notification, interacting with it should
  // not close the bubble.
  if (features::IsQsRevampEnabled() &&
      RootWindowController::ForWindow(gained_active)
          ->shelf()
          ->GetStatusAreaWidget()
          ->unified_system_tray()
          ->GetMessagePopupCollection()
          ->IsWidgetAPopupNotification(gained_active_widget)) {
    return;
  }

  tray_->CloseBubble();
}

}  // namespace ash
