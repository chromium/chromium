// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/message_center/ash_message_popup_collection.h"

#include "ash/constants/ash_features.h"
#include "ash/focus_cycler.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/root_window_controller.h"
#include "ash/shelf/hotseat_widget.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/system/message_center/fullscreen_notification_blocker.h"
#include "ash/system/message_center/message_center_constants.h"
#include "ash/system/message_center/message_view_factory.h"
#include "ash/system/message_center/metrics_utils.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_utils.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/work_area_insets.h"
#include "base/i18n/rtl.h"
#include "base/metrics/histogram_functions.h"
#include "ui/compositor/compositor.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/message_center/public/cpp/message_center_constants.h"
#include "ui/message_center/views/message_popup_view.h"
#include "ui/wm/core/shadow_types.h"

namespace ash {

namespace {

const int kToastMarginX = 7;

void ReportPopupAnimationSmoothness(int smoothness) {
  base::UmaHistogramPercentage("Ash.NotificationPopup.AnimationSmoothness",
                               smoothness);
}

}  // namespace

const char AshMessagePopupCollection::kMessagePopupWidgetName[] =
    "ash/message_center/MessagePopup";

AshMessagePopupCollection::AshMessagePopupCollection(Shelf* shelf)
    : screen_(nullptr), shelf_(shelf), tray_bubble_height_(0) {
  // The order for notifications will be reversed when
  // IsNotificationsRefreshEnabled.
  if (!features::IsNotificationsRefreshEnabled())
    set_inverse();
  shelf_->AddObserver(this);
  Shell::Get()->tablet_mode_controller()->AddObserver(this);
}

AshMessagePopupCollection::~AshMessagePopupCollection() {
  Shell::Get()->tablet_mode_controller()->RemoveObserver(this);
  shelf_->RemoveObserver(this);
  for (views::Widget* widget : tracked_widgets_)
    widget->RemoveObserver(this);
  CHECK(!views::WidgetObserver::IsInObserverList());
}

void AshMessagePopupCollection::StartObserving(
    display::Screen* screen,
    const display::Display& display) {
  screen_ = screen;
  work_area_ = display.work_area();
  display_observer_.emplace(this);
  if (tray_bubble_height_ > 0)
    UpdateWorkArea();
}

void AshMessagePopupCollection::SetTrayBubbleHeight(int height) {
  const int old_tray_bubble_height = tray_bubble_height_;

  tray_bubble_height_ = height;

  // If the shelf is shown during auto-hide state, the distance from the edge
  // should be reduced by the height of shelf's shown height.
  if (shelf_->GetVisibilityState() == SHELF_AUTO_HIDE &&
      shelf_->GetAutoHideState() == SHELF_AUTO_HIDE_SHOWN) {
    tray_bubble_height_ -= ShelfConfig::Get()->shelf_size();
  }

  if (tray_bubble_height_ > 0)
    tray_bubble_height_ += message_center::kMarginBetweenPopups;
  else
    tray_bubble_height_ = 0;

  if (old_tray_bubble_height != tray_bubble_height_)
    ResetBounds();
}

int AshMessagePopupCollection::GetToastOriginX(
    const gfx::Rect& toast_bounds) const {
  // In Ash, RTL UI language mirrors the whole ash layout, so the toast
  // widgets should be at the bottom-left instead of bottom right.
  if (base::i18n::IsRTL())
    return work_area_.x() + kToastMarginX;

  if (IsFromLeft())
    return work_area_.x() + kToastMarginX;
  return work_area_.right() - kToastMarginX - toast_bounds.width();
}

int AshMessagePopupCollection::GetBaseline() const {
  gfx::Insets tray_bubble_insets = GetTrayBubbleInsets();
  int hotseat_height =
      shelf_->hotseat_widget()->state() == HotseatState::kExtended
          ? shelf_->hotseat_widget()->GetHotseatSize()
          : 0;
  return work_area_.bottom() - tray_bubble_insets.bottom() -
         tray_bubble_height_ - hotseat_height;
}

gfx::Rect AshMessagePopupCollection::GetWorkArea() const {
  gfx::Rect work_area_without_tray_bubble = work_area_;
  work_area_without_tray_bubble.set_height(
      work_area_without_tray_bubble.height() - tray_bubble_height_);
  return work_area_without_tray_bubble;
}

bool AshMessagePopupCollection::IsTopDown() const {
  return false;
}

bool AshMessagePopupCollection::IsFromLeft() const {
  return GetAlignment() == ShelfAlignment::kLeft;
}

bool AshMessagePopupCollection::RecomputeAlignment(
    const display::Display& display) {
  // Nothing needs to be done.
  return false;
}

void AshMessagePopupCollection::ConfigureWidgetInitParamsForContainer(
    views::Widget* widget,
    views::Widget::InitParams* init_params) {
  init_params->shadow_type = views::Widget::InitParams::ShadowType::kDrop;
  init_params->shadow_elevation = ::wm::kShadowElevationInactiveWindow;
  // On ash, popups go in the status container.
  init_params->parent = shelf_->GetWindow()->GetRootWindow()->GetChildById(
      kShellWindowId_ShelfContainer);

  // Make the widget activatable so it can receive focus when cycling through
  // windows (i.e. pressing ctrl + forward/back).
  init_params->activatable = views::Widget::InitParams::Activatable::kYes;
  init_params->name = kMessagePopupWidgetName;
  init_params->corner_radius = kMessagePopupCornerRadius;
  Shell::Get()->focus_cycler()->AddWidget(widget);
  widget->AddObserver(this);
  tracked_widgets_.insert(widget);
}

bool AshMessagePopupCollection::IsPrimaryDisplayForNotification() const {
  return screen_ &&
         GetCurrentDisplay().id() == screen_->GetPrimaryDisplay().id();
}

bool AshMessagePopupCollection::BlockForMixedFullscreen(
    const message_center::Notification& notification) const {
  return FullscreenNotificationBlocker::BlockForMixedFullscreen(
      notification, RootWindowController::ForWindow(shelf_->GetWindow())
                        ->IsInFullscreenMode());
}

void AshMessagePopupCollection::NotifyPopupAdded(
    message_center::MessagePopupView* popup) {
  MessagePopupCollection::NotifyPopupAdded(popup);
  popup->message_view()->AddObserver(this);
  metrics_utils::LogPopupShown(popup->message_view()->notification_id());
  last_pop_up_added_ = popup;
}

void AshMessagePopupCollection::NotifyPopupClosed(
    message_center::MessagePopupView* popup) {
  metrics_utils::LogPopupClosed(popup);
  MessagePopupCollection::NotifyPopupClosed(popup);
  popup->message_view()->RemoveObserver(this);
  if (last_pop_up_added_ == popup)
    last_pop_up_added_ = nullptr;
}

void AshMessagePopupCollection::AnimationStarted() {
  if (popups_animating_ == 0 && last_pop_up_added_) {
    // Since all the popup widgets use the same compositor, we only need to set
    // this when the first popup shows in the animation sequence.
    animation_tracker_.emplace(last_pop_up_added_->GetWidget()
                                   ->GetCompositor()
                                   ->RequestNewThroughputTracker());
    animation_tracker_->Start(metrics_util::ForSmoothness(
        base::BindRepeating(&ReportPopupAnimationSmoothness)));
  }
  ++popups_animating_;
}

void AshMessagePopupCollection::AnimationFinished() {
  --popups_animating_;
  // Stop when all animations are finished.
  if (animation_tracker_ && popups_animating_ == 0) {
    animation_tracker_->Stop();
    animation_tracker_.reset();
  }
}

message_center::MessagePopupView* AshMessagePopupCollection::CreatePopup(
    const message_center::Notification& notification) {
  bool a11_feedback_on_init =
      notification.rich_notification_data()
          .should_make_spoken_feedback_for_popup_updates;
  return new message_center::MessagePopupView(
      MessageViewFactory::Create(notification, /*shown_in_popup=*/true)
          .release(),
      this, a11_feedback_on_init);
}

void AshMessagePopupCollection::OnTabletModeStarted() {
  // Reset bounds so pop-up baseline is updated.
  ResetBounds();
}

void AshMessagePopupCollection::OnTabletModeEnded() {
  // Reset bounds so pop-up baseline is updated.
  ResetBounds();
}

void AshMessagePopupCollection::OnSlideOut(const std::string& notification_id) {
  metrics_utils::LogClosedByUser(notification_id, /*is_swipe=*/true,
                                 /*is_popup=*/true);
}

void AshMessagePopupCollection::OnCloseButtonPressed(
    const std::string& notification_id) {
  metrics_utils::LogClosedByUser(notification_id, /*is_swipe=*/false,
                                 /*is_popup=*/true);
}

void AshMessagePopupCollection::OnSettingsButtonPressed(
    const std::string& notification_id) {
  metrics_utils::LogSettingsShown(notification_id, /*is_slide_controls=*/false,
                                  /*is_popup=*/true);
}

void AshMessagePopupCollection::OnSnoozeButtonPressed(
    const std::string& notification_id) {
  metrics_utils::LogSnoozed(notification_id, /*is_slide_controls=*/false,
                            /*is_popup=*/true);
}

ShelfAlignment AshMessagePopupCollection::GetAlignment() const {
  return shelf_->alignment();
}

display::Display AshMessagePopupCollection::GetCurrentDisplay() const {
  return display::Screen::GetScreen()->GetDisplayNearestWindow(
      shelf_->GetWindow());
}

void AshMessagePopupCollection::UpdateWorkArea() {
  gfx::Rect new_work_area =
      WorkAreaInsets::ForWindow(shelf_->GetWindow()->GetRootWindow())
          ->user_work_area_bounds();
  if (work_area_ == new_work_area)
    return;

  work_area_ = new_work_area;
  ResetBounds();
}

///////////////////////////////////////////////////////////////////////////////
// ShelfObserver:

void AshMessagePopupCollection::OnShelfWorkAreaInsetsChanged() {
  UpdateWorkArea();
}

void AshMessagePopupCollection::OnHotseatStateChanged(HotseatState old_state,
                                                      HotseatState new_state) {
  ResetBounds();
}

///////////////////////////////////////////////////////////////////////////////
// display::DisplayObserver:

void AshMessagePopupCollection::OnDisplayMetricsChanged(
    const display::Display& display,
    uint32_t metrics) {
  if (GetCurrentDisplay().id() == display.id())
    UpdateWorkArea();
}

///////////////////////////////////////////////////////////////////////////////
// views::WidgetObserver:

void AshMessagePopupCollection::OnWidgetClosing(views::Widget* widget) {
  Shell::Get()->focus_cycler()->RemoveWidget(widget);
  widget->RemoveObserver(this);
  tracked_widgets_.erase(widget);
}

void AshMessagePopupCollection::OnWidgetActivationChanged(views::Widget* widget,
                                                          bool active) {
  // Note: Each pop-up is contained in it's own widget and we need to manually
  // focus the contained MessageView when the widget is activated through the
  // FocusCycler.
  if (active && Shell::Get()->focus_cycler()->widget_activating() == widget)
    widget->GetFocusManager()->SetFocusedView(widget->GetContentsView());
}

}  // namespace ash
