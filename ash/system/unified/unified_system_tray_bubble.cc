// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/unified_system_tray_bubble.h"

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/bubble/bubble_constants.h"
#include "ash/constants/ash_features.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/system/message_center/unified_message_center_bubble.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/time/calendar_metrics.h"
#include "ash/system/tray/tray_background_view.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_event_filter.h"
#include "ash/system/tray/tray_utils.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "ash/system/unified/unified_system_tray_view.h"
#include "ash/wm/container_finder.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/metrics/histogram_macros.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/events/event.h"
#include "ui/wm/core/window_util.h"
#include "ui/wm/public/activation_client.h"

namespace ash {
namespace {

constexpr int kDetailedViewHeight = 464;

}

UnifiedSystemTrayBubble::UnifiedSystemTrayBubble(UnifiedSystemTray* tray)
    : controller_(std::make_unique<UnifiedSystemTrayController>(tray->model(),
                                                                this,
                                                                tray)),
      tray_(tray) {
  time_opened_ = base::TimeTicks::Now();

  TrayBubbleView::InitParams init_params;
  init_params.shelf_alignment = tray_->shelf()->alignment();
  init_params.preferred_width =
      features::IsQsRevampEnabled() ? kRevampedTrayMenuWidth : kTrayMenuWidth;
  init_params.delegate = tray->GetWeakPtr();
  init_params.parent_window = tray->GetBubbleWindowContainer();
  init_params.anchor_view = nullptr;
  init_params.anchor_mode = TrayBubbleView::AnchorMode::kRect;
  init_params.anchor_rect = tray->shelf()->GetSystemTrayAnchorRect();
  init_params.insets = GetTrayBubbleInsets();
  init_params.close_on_deactivate = false;
  init_params.reroute_event_handler = true;
  init_params.translucent = true;

  bubble_view_ = new TrayBubbleView(init_params);

  // Max height calculated from the maximum available height of the screen.
  int max_height = CalculateMaxTrayBubbleHeight();

  if (features::IsQsRevampEnabled()) {
    auto quick_settings_view = controller_->CreateQuickSettingsView(max_height);
    bubble_view_->SetMaxHeight(max_height);
    quick_settings_view_ =
        bubble_view_->AddChildView(std::move(quick_settings_view));
    time_to_click_recorder_ = std::make_unique<TimeToClickRecorder>(
        /*delegate=*/this, /*target_view=*/quick_settings_view_);
  } else {
    DCHECK(!features::IsQsRevampEnabled());
    auto unified_view = controller_->CreateUnifiedQuickSettingsView();
    unified_view->SetMaxHeight(max_height);
    bubble_view_->SetMaxHeight(max_height);
    controller_->ResetToCollapsedIfRequired();
    unified_view_ = bubble_view_->AddChildView(std::move(unified_view));
    time_to_click_recorder_ = std::make_unique<TimeToClickRecorder>(
        /*delegate=*/this, /*target_view=*/unified_view_);
  }

  bubble_widget_ = views::BubbleDialogDelegateView::CreateBubble(bubble_view_);
  bubble_widget_->AddObserver(this);

  TrayBackgroundView::InitializeBubbleAnimations(bubble_widget_);
  bubble_view_->InitializeAndShowBubble();

  // Notify accessibility features that the status tray has opened.
  NotifyAccessibilityEvent(ax::mojom::Event::kShow, true);

  // Explicitly close the app list in clamshell mode.
  if (!Shell::Get()->tablet_mode_controller()->InTabletMode()) {
    Shell::Get()->app_list_controller()->DismissAppList();
  }
}

UnifiedSystemTrayBubble::~UnifiedSystemTrayBubble() {
  if (controller_->showing_calendar_view()) {
    tray_->NotifyLeavingCalendarView();
  }

  Shell::Get()->activation_client()->RemoveObserver(this);
  if (Shell::Get()->tablet_mode_controller()) {
    Shell::Get()->tablet_mode_controller()->RemoveObserver(this);
  }
  tray_->tray_event_filter()->RemoveBubble(this);
  tray_->shelf()->RemoveObserver(this);

  // Unified view children depend on `controller_` which is about to go away.
  // Remove child views synchronously to ensure they don't try to access
  // `controller_` after `this` goes out of scope.
  bubble_view_->RemoveAllChildViews();
  bubble_view_->ResetDelegate();

  if (bubble_widget_) {
    bubble_widget_->RemoveObserver(this);
    bubble_widget_->Close();
  }

  CHECK(!TrayBubbleBase::IsInObserverList());
}

void UnifiedSystemTrayBubble::InitializeObservers() {
  tray_->tray_event_filter()->AddBubble(this);
  tray_->shelf()->AddObserver(this);
  Shell::Get()->tablet_mode_controller()->AddObserver(this);
  Shell::Get()->activation_client()->AddObserver(this);
}

gfx::Rect UnifiedSystemTrayBubble::GetBoundsInScreen() const {
  DCHECK(bubble_view_);
  return bubble_view_->GetBoundsInScreen();
}

bool UnifiedSystemTrayBubble::IsBubbleActive() const {
  return bubble_widget_ && bubble_widget_->IsActive();
}

void UnifiedSystemTrayBubble::EnsureCollapsed() {
  if (!bubble_widget_ || quick_settings_view_) {
    return;
  }

  DCHECK(unified_view_);
  DCHECK(controller_);
  controller_->EnsureCollapsed();
}

void UnifiedSystemTrayBubble::EnsureExpanded() {
  if (!bubble_widget_) {
    return;
  }

  DCHECK(unified_view_ || quick_settings_view_);
  DCHECK(controller_);
  controller_->EnsureExpanded();
}

void UnifiedSystemTrayBubble::CollapseWithoutAnimating() {
  if (!bubble_widget_ || quick_settings_view_) {
    return;
  }

  DCHECK(unified_view_);
  DCHECK(controller_);

  controller_->CollapseWithoutAnimating();
}

void UnifiedSystemTrayBubble::CollapseMessageCenter() {
  if (quick_settings_view_) {
    return;
  }
  tray_->CollapseMessageCenter();
}

void UnifiedSystemTrayBubble::ExpandMessageCenter() {
  if (quick_settings_view_) {
    return;
  }
  tray_->ExpandMessageCenter();
}

void UnifiedSystemTrayBubble::ShowAudioDetailedView() {
  if (!bubble_widget_) {
    return;
  }

  DCHECK(unified_view_ || quick_settings_view_);
  DCHECK(controller_);
  controller_->ShowAudioDetailedView();
}

void UnifiedSystemTrayBubble::ShowDisplayDetailedView() {
  if (!bubble_widget_) {
    return;
  }

  DCHECK(unified_view_ || quick_settings_view_);
  DCHECK(controller_);
  controller_->ShowDisplayDetailedView();
}

void UnifiedSystemTrayBubble::ShowCalendarView(
    calendar_metrics::CalendarViewShowSource show_source,
    calendar_metrics::CalendarEventSource event_source) {
  if (!bubble_widget_) {
    return;
  }

  if (event_source == calendar_metrics::CalendarEventSource::kKeyboard) {
    auto weak_this = weak_factory_.GetWeakPtr();
    bubble_view_->SetCanActivate(true);
    bubble_widget_->Activate();
    // Calling `bubble_widget_->Activate()` can cause `this` to be deleted. We
    // should not continue if that happens.
    if (!weak_this) {
      return;
    }
  }

  DCHECK(unified_view_ || quick_settings_view_);
  DCHECK(controller_);
  controller_->ShowCalendarView(show_source, event_source);
}

void UnifiedSystemTrayBubble::ShowNetworkDetailedView(bool force) {
  if (!bubble_widget_) {
    return;
  }

  DCHECK(unified_view_ || quick_settings_view_);
  DCHECK(controller_);
  controller_->ShowNetworkDetailedView(force);
}

void UnifiedSystemTrayBubble::UpdateBubble() {
  if (!bubble_widget_) {
    return;
  }
  DCHECK(bubble_view_);

  bubble_view_->UpdateBubble();
}

TrayBackgroundView* UnifiedSystemTrayBubble::GetTray() const {
  return tray_;
}

TrayBubbleView* UnifiedSystemTrayBubble::GetBubbleView() const {
  return bubble_view_;
}

views::Widget* UnifiedSystemTrayBubble::GetBubbleWidget() const {
  return bubble_widget_;
}

int UnifiedSystemTrayBubble::GetCurrentTrayHeight() const {
  if (features::IsQsRevampEnabled()) {
    return quick_settings_view_->GetCurrentHeight();
  }

  return unified_view_->GetCurrentHeight();
}

bool UnifiedSystemTrayBubble::FocusOut(bool reverse) {
  if (quick_settings_view_) {
    return false;
  }
  return tray_->FocusMessageCenter(reverse);
}

void UnifiedSystemTrayBubble::FocusEntered(bool reverse) {
  if (features::IsQsRevampEnabled()) {
    return;
  }

  unified_view_->FocusEntered(reverse);
}

void UnifiedSystemTrayBubble::OnMessageCenterActivated() {
  if (quick_settings_view_) {
    return;
  }
  // When the message center is activated, we no longer need to reroute key
  // events to this bubble. Otherwise, we interfere with notifications that may
  // require key input like inline replies. See crbug.com/1040738.
  bubble_view_->StopReroutingEvents();
}

void UnifiedSystemTrayBubble::OnDisplayConfigurationChanged() {
  UpdateBubbleBounds();
}

void UnifiedSystemTrayBubble::OnWidgetDestroying(views::Widget* widget) {
  CHECK_EQ(bubble_widget_, widget);
  bubble_widget_->RemoveObserver(this);
  bubble_widget_ = nullptr;

  // `tray_->CloseBubble()` will delete `this`.
  tray_->CloseBubble();
}

void UnifiedSystemTrayBubble::OnWindowActivated(ActivationReason reason,
                                                aura::Window* gained_active,
                                                aura::Window* lost_active) {
  if (!gained_active || !bubble_widget_) {
    return;
  }

  // Check for the CloseBubble() lock.
  if (!TrayBackgroundView::ShouldCloseBubbleOnWindowActivated()) {
    return;
  }

  // Don't close the bubble if a transient child is gaining or losing
  // activation.
  if (bubble_widget_ == views::Widget::GetWidgetForNativeView(gained_active) ||
      ::wm::HasTransientAncestor(gained_active,
                                 bubble_widget_->GetNativeWindow()) ||
      (lost_active && ::wm::HasTransientAncestor(
                          lost_active, bubble_widget_->GetNativeWindow()))) {
    return;
  }

  // Don't close the bubble if the message center is gaining activation.
  if (tray_->IsMessageCenterBubbleShown()) {
    views::Widget* message_center_widget =
        tray_->message_center_bubble()->GetBubbleWidget();
    if (message_center_widget ==
        views::Widget::GetWidgetForNativeView(gained_active)) {
      return;
    }

    // If the message center is not visible, ignore activation changes.
    // Otherwise, this may cause a crash when closing the dialog via
    // accelerator. See crbug.com/1041174.
    if (!message_center_widget->IsVisible()) {
      return;
    }
  }

  tray_->CloseBubble();
}

void UnifiedSystemTrayBubble::RecordTimeToClick() {
  if (!time_opened_) {
    return;
  }

  tray_->MaybeRecordFirstInteraction(
      UnifiedSystemTray::FirstInteractionType::kQuickSettings);

  UMA_HISTOGRAM_TIMES("ChromeOS.SystemTray.TimeToClick2",
                      base::TimeTicks::Now() - time_opened_.value());

  time_opened_.reset();
}

void UnifiedSystemTrayBubble::OnTabletPhysicalStateChanged() {
  tray_->CloseBubble();
}

void UnifiedSystemTrayBubble::OnAutoHideStateChanged(
    ShelfAutoHideState new_state) {
  UpdateBubbleBounds();
}

void UnifiedSystemTrayBubble::UpdateBubbleHeight(bool is_showing_detiled_view) {
  DCHECK(features::IsQsRevampEnabled());
  bubble_view_->SetShouldUseFixedHeight(is_showing_detiled_view);
  UpdateBubbleBounds();
}

void UnifiedSystemTrayBubble::UpdateBubbleBounds() {
  int max_height = CalculateMaxTrayBubbleHeight();
  if (bubble_view_->ShouldUseFixedHeight()) {
    DCHECK(features::IsQsRevampEnabled());
    max_height = std::min(max_height, kDetailedViewHeight);
  }
  if (features::IsQsRevampEnabled()) {
    quick_settings_view_->SetMaxHeight(max_height);
  } else {
    unified_view_->SetMaxHeight(max_height);
  }
  bubble_view_->SetMaxHeight(max_height);
  bubble_view_->ChangeAnchorAlignment(tray_->shelf()->alignment());
  bubble_view_->ChangeAnchorRect(tray_->shelf()->GetSystemTrayAnchorRect());
  if (quick_settings_view_) {
    return;
  }
  if (tray_->IsMessageCenterBubbleShown()) {
    tray_->message_center_bubble()->UpdatePosition();
  }
}

void UnifiedSystemTrayBubble::NotifyAccessibilityEvent(ax::mojom::Event event,
                                                       bool send_native_event) {
  bubble_view_->NotifyAccessibilityEvent(event, send_native_event);
}

bool UnifiedSystemTrayBubble::ShowingAudioDetailedView() const {
  return bubble_widget_ && controller_->showing_audio_detailed_view();
}

bool UnifiedSystemTrayBubble::ShowingDisplayDetailedView() const {
  return bubble_widget_ && controller_->showing_display_detailed_view();
}

bool UnifiedSystemTrayBubble::ShowingCalendarView() const {
  return bubble_widget_ && controller_->showing_calendar_view();
}

}  // namespace ash
