// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/unified_system_tray_bubble.h"

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/bubble/bubble_constants.h"
#include "ash/constants/ash_features.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/system/notification_center/ash_message_popup_collection.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/time/calendar_metrics.h"
#include "ash/system/tray/tray_background_view.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_event_filter.h"
#include "ash/system/tray/tray_utils.h"
#include "ash/system/unified/quick_settings_metrics_util.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "ash/wm/container_finder.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/debug/crash_logging.h"
#include "base/metrics/histogram_macros.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/display/screen.h"
#include "ui/events/event.h"

namespace ash {

namespace {
constexpr int kDetailedViewHeight = 464;
}  // namespace

UnifiedSystemTrayBubble::UnifiedSystemTrayBubble(UnifiedSystemTray* tray)
    : controller_(std::make_unique<UnifiedSystemTrayController>(tray->model(),
                                                                this,
                                                                tray)),
      unified_system_tray_(tray) {
  time_opened_ = base::TimeTicks::Now();

  TrayBubbleView::InitParams init_params =
      CreateInitParamsForTrayBubble(tray, /*anchor_to_shelf_corner=*/true);
  init_params.preferred_width = kWideTrayMenuWidth;
  init_params.close_on_deactivate = false;

  bubble_view_ = new TrayBubbleView(init_params);

  // Max height calculated from the maximum available height of the screen.
  int max_height = CalculateMaxTrayBubbleHeight(
      unified_system_tray_->GetBubbleWindowContainer());

  auto quick_settings_view = controller_->CreateQuickSettingsView(max_height);
  bubble_view_->SetMaxHeight(max_height);
  quick_settings_view_ =
      bubble_view_->AddChildView(std::move(quick_settings_view));
  time_to_click_recorder_ = std::make_unique<TimeToClickRecorder>(
      /*delegate=*/this, /*target_view=*/quick_settings_view_);

  bubble_widget_ = views::BubbleDialogDelegateView::CreateBubble(bubble_view_);
  bubble_widget_->AddObserver(this);

  TrayBackgroundView::InitializeBubbleAnimations(bubble_widget_);
  bubble_view_->InitializeAndShowBubble();

  // Notify accessibility features that the status tray has opened.
  NotifyAccessibilityEvent(ax::mojom::Event::kShow, true);

  // Explicitly close the app list in clamshell mode.
  if (!display::Screen::GetScreen()->InTabletMode()) {
    Shell::Get()->app_list_controller()->DismissAppList();
  }
}

UnifiedSystemTrayBubble::~UnifiedSystemTrayBubble() {
  // Record the number of quick settings pages.
  auto page_count = unified_system_tray_controller()
                        ->model()
                        ->pagination_model()
                        ->total_pages();
  DCHECK_GT(page_count, 0);
  quick_settings_metrics_util::RecordQsPageCountOnClose(page_count);

  if (controller_->showing_calendar_view()) {
    unified_system_tray_->NotifyLeavingCalendarView();
  }

  if (Shell::Get()->tablet_mode_controller()) {
    Shell::Get()->tablet_mode_controller()->RemoveObserver(this);
  }
  unified_system_tray_->shelf()->RemoveObserver(this);

  // Unified view children depend on `controller_` which is about to go away.
  // Remove child views synchronously to ensure they don't try to access
  // `controller_` after `this` goes out of scope.
  if (bubble_view_) {
    controller_->ShutDownDetailedViewController();
    bubble_view_->RemoveAllChildViews();
    quick_settings_view_ = nullptr;
    bubble_view_->ResetDelegate();
    bubble_view_ = nullptr;
  }

  if (bubble_widget_) {
    bubble_widget_->RemoveObserver(this);
    bubble_widget_->Close();
    bubble_widget_ = nullptr;
  }

  CHECK(!TrayBubbleBase::IsInObserverList());
}

void UnifiedSystemTrayBubble::InitializeObservers() {
  unified_system_tray_->shelf()->AddObserver(this);
  Shell::Get()->tablet_mode_controller()->AddObserver(this);

  CHECK(bubble_widget_);
  CHECK(bubble_view_);
  tray_event_filter_ = std::make_unique<TrayEventFilter>(
      bubble_widget_, bubble_view_, /*tray_button=*/unified_system_tray_);
}

gfx::Rect UnifiedSystemTrayBubble::GetBoundsInScreen() const {
  DCHECK(bubble_view_);
  return bubble_view_->GetBoundsInScreen();
}

bool UnifiedSystemTrayBubble::IsBubbleActive() const {
  return bubble_widget_ && bubble_widget_->IsActive();
}

void UnifiedSystemTrayBubble::ShowAudioDetailedView() {
  if (!bubble_widget_) {
    return;
  }

  DCHECK(quick_settings_view_);
  DCHECK(controller_);
  controller_->ShowAudioDetailedView();
}

void UnifiedSystemTrayBubble::ShowDisplayDetailedView() {
  if (!bubble_widget_) {
    return;
  }

  DCHECK(quick_settings_view_);
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

  DCHECK(quick_settings_view_);
  DCHECK(controller_);
  controller_->ShowCalendarView(show_source, event_source);
}

void UnifiedSystemTrayBubble::ShowNetworkDetailedView() {
  if (!bubble_widget_) {
    return;
  }

  DCHECK(quick_settings_view_);
  DCHECK(controller_);
  controller_->ShowNetworkDetailedView();
}

void UnifiedSystemTrayBubble::UpdateBubble() {
  if (!bubble_widget_) {
    return;
  }
  DCHECK(bubble_view_);

  bubble_view_->UpdateBubble();
}

TrayBackgroundView* UnifiedSystemTrayBubble::GetTray() const {
  return unified_system_tray_;
}

TrayBubbleView* UnifiedSystemTrayBubble::GetBubbleView() const {
  return bubble_view_;
}

views::Widget* UnifiedSystemTrayBubble::GetBubbleWidget() const {
  return bubble_widget_;
}

int UnifiedSystemTrayBubble::GetCurrentTrayHeight() const {
  return quick_settings_view_->GetCurrentHeight();
}

void UnifiedSystemTrayBubble::OnDidApplyDisplayChanges() {
  UpdateBubbleBounds();
}

void UnifiedSystemTrayBubble::OnWidgetDestroying(views::Widget* widget) {
  CHECK_EQ(bubble_widget_, widget);
  bubble_widget_->RemoveObserver(this);
  bubble_widget_ = nullptr;

  controller_->ShutDownDetailedViewController();
  bubble_view_->RemoveAllChildViews();
  quick_settings_view_ = nullptr;
  bubble_view_->ResetDelegate();
  bubble_view_ = nullptr;

  // `unified_system_tray_->CloseBubble()` will delete `this`.
  unified_system_tray_->CloseBubble();
}

void UnifiedSystemTrayBubble::RecordTimeToClick() {
  if (!time_opened_) {
    return;
  }

  unified_system_tray_->MaybeRecordFirstInteraction(
      UnifiedSystemTray::FirstInteractionType::kQuickSettings);

  UMA_HISTOGRAM_TIMES("ChromeOS.SystemTray.TimeToClick2",
                      base::TimeTicks::Now() - time_opened_.value());

  time_opened_.reset();
}

void UnifiedSystemTrayBubble::OnTabletPhysicalStateChanged() {
  // Deletes this.
  unified_system_tray_->CloseBubble();
}

void UnifiedSystemTrayBubble::OnAutoHideStateChanged(
    ShelfAutoHideState new_state) {
  UpdateBubbleBounds();
}

void UnifiedSystemTrayBubble::UpdateBubbleHeight(bool is_showing_detiled_view) {
  if (!bubble_view_) {
    return;
  }
  bubble_view_->SetShouldUseFixedHeight(is_showing_detiled_view);
  UpdateBubbleBounds();
}

void UnifiedSystemTrayBubble::UpdateBubbleBounds() {
  // USTB_UBB stands for `UnifiedSystemTrayBubble::UpdateBubbleBounds`. Here
  // using the short version since the log method has a character count limit
  // of 40.
  SCOPED_CRASH_KEY_BOOL("USTB_UBB", "bubble_view_", !!bubble_view_);
  SCOPED_CRASH_KEY_BOOL("USTB_UBB", "unified_system_tray_",
                        !!unified_system_tray_);
  SCOPED_CRASH_KEY_BOOL(
      "USTB_UBB", "unified_system_tray_->shelf()",
      !!unified_system_tray_ && !!unified_system_tray_->shelf());
  SCOPED_CRASH_KEY_BOOL("USTB_UBB", "bubble_widget_", !!bubble_widget_);
  SCOPED_CRASH_KEY_BOOL("USTB_UBB", "bubble_widget_->IsClosed()",
                        !!bubble_widget_ && !!bubble_widget_->IsClosed());

  // `bubble_view_` or `Shelf` may be null, see https://b/293264371,
  if (!bubble_view_ || !quick_settings_view_) {
    return;
  }
  if (!unified_system_tray_->shelf()) {
    return;
  }

  int max_height = CalculateMaxTrayBubbleHeight(
      unified_system_tray_->GetBubbleWindowContainer());
  if (bubble_view_->ShouldUseFixedHeight()) {
    const int qs_current_height = quick_settings_view_->height();
    max_height =
        std::min(max_height, std::max(qs_current_height, kDetailedViewHeight));
  }

  // Setting the max height can result in the popup baseline being updated,
  // closing this bubble.
  quick_settings_view_->SetMaxHeight(max_height);

  if (!bubble_view_) {
    // Updating the maximum height can result in popup baseline changing. If
    // there is not enough room for popups, the bubble will be closed, and this
    // `bubble_view_` will not exist. This is a corner case, and we should
    // probably not close the bubble in this case.  See https://b/302172146.
    return;
  }
  bubble_view_->SetMaxHeight(max_height);
  bubble_view_->ChangeAnchorAlignment(
      unified_system_tray_->shelf()->alignment());
  bubble_view_->ChangeAnchorRect(
      unified_system_tray_->shelf()->GetSystemTrayAnchorRect());
}

void UnifiedSystemTrayBubble::NotifyAccessibilityEvent(ax::mojom::Event event,
                                                       bool send_native_event) {
  if (!bubble_view_) {
    return;
  }
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
