// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/tray_bubble_wrapper.h"
#include <memory>

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/tray/tray_background_view.h"
#include "ash/system/tray/tray_bubble_view.h"
#include "ash/system/tray/tray_event_filter.h"
#include "ui/aura/window.h"
#include "ui/display/screen.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/transient_window_manager.h"

namespace ash {

TrayBubbleWrapper::TrayBubbleWrapper(TrayBackgroundView* tray,
                                     bool event_handling)
    : tray_(tray), event_handling_(event_handling) {}

TrayBubbleWrapper::~TrayBubbleWrapper() {
  if (bubble_widget_) {
    // A bubble might have transcient child open (i.e. the network info bubble
    // in the network detailed view of QS). Thus, we need to remove all those
    // transient children before destruction.
    auto* transient_manager = ::wm::TransientWindowManager::GetOrCreate(
        bubble_widget_->GetNativeWindow());
    while (transient_manager &&
           !transient_manager->transient_children().empty()) {
      transient_manager->RemoveTransientChild(
          transient_manager->transient_children().back());
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

  // We need to explicitly dismiss app list bubble here due to b/1186479.
  if (!display::Screen::GetScreen()->InTabletMode()) {
    Shell::Get()->app_list_controller()->DismissAppList();
  }

  if (event_handling_) {
    tray_event_filter_ = std::make_unique<TrayEventFilter>(
        bubble_widget_, bubble_view_, /*tray_button=*/tray_);
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

  tray_->HideBubbleWithView(bubble_view_);  // May destroy |bubble_view_|
}

}  // namespace ash
