// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/notification_center/ash_message_popup_collection.h"

#include <cstddef>
#include <cstdint>
#include <memory>

#include "ash/constants/ash_constants.h"
#include "ash/constants/ash_features.h"
#include "ash/focus_cycler.h"
#include "ash/public/cpp/message_center/arc_notification_constants.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/root_window_controller.h"
#include "ash/shelf/hotseat_widget.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/notification_center/fullscreen_notification_blocker.h"
#include "ash/system/notification_center/message_center_constants.h"
#include "ash/system/notification_center/message_center_utils.h"
#include "ash/system/notification_center/message_view_factory.h"
#include "ash/system/notification_center/metrics_utils.h"
#include "ash/system/notification_center/notification_style_utils.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/status_area_widget_delegate.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ash/system/tray/tray_background_view.h"
#include "ash/system/tray/tray_bubble_view.h"
#include "ash/system/tray/tray_utils.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/wm/window_properties.h"
#include "ash/wm/work_area_insets.h"
#include "base/auto_reset.h"
#include "base/check.h"
#include "base/i18n/rtl.h"
#include "base/metrics/histogram_functions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/compositor.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/display/tablet_state.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/message_center_constants.h"
#include "ui/message_center/public/cpp/notification_types.h"
#include "ui/message_center/views/message_popup_collection.h"
#include "ui/message_center/views/message_popup_view.h"
#include "ui/message_center/views/message_view.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/shadow_types.h"

namespace ash {

namespace {

const int kPopupMarginX = 8;

void ReportPopupAnimationSmoothness(int smoothness) {
  base::UmaHistogramPercentage("Ash.NotificationPopup.AnimationSmoothness",
                               smoothness);
}

}  // namespace

const char AshMessagePopupCollection::kMessagePopupWidgetName[] =
    "ash/message_center/MessagePopup";

///////////////////////////////////////////////////////////////////////////////
// NotifierCollisionHandler:

AshMessagePopupCollection::NotifierCollisionHandler::NotifierCollisionHandler(
    AshMessagePopupCollection* popup_collection)
    : popup_collection_(popup_collection) {
  Shell::Get()->system_tray_notifier()->AddSystemTrayObserver(this);
  popup_collection_->shelf_->AddObserver(this);
}

AshMessagePopupCollection::NotifierCollisionHandler::
    ~NotifierCollisionHandler() {
  popup_collection_->shelf_->RemoveObserver(this);
  Shell::Get()->system_tray_notifier()->RemoveSystemTrayObserver(this);
}

void AshMessagePopupCollection::NotifierCollisionHandler::
    OnPopupCollectionHeightChanged() {
  if (!features::IsNotifierCollisionEnabled()) {
    return;
  }

  // Ignore changes happen to the popup collection height when bubble changes is
  // being handled. This is to avoid crashes (b/305781721) when we handle both
  // the bubble and the collection height changes at the same time.
  if (is_handling_bubble_change_) {
    return;
  }

  // Do nothing if there's no open corner anchored shelf pod bubble.
  auto* status_area =
      StatusAreaWidget::ForWindow(popup_collection_->shelf_->GetWindow());
  auto* shelf_pod_bubble =
      status_area ? status_area->open_shelf_pod_bubble() : nullptr;
  if (!shelf_pod_bubble || !shelf_pod_bubble->IsAnchoredToShelfCorner()) {
    return;
  }

  // If the popups do not fit in the available space, close the bubble.
  if (popup_collection_->popup_collection_bounds().height() >
      popup_collection_->GetBaseline()) {
    shelf_pod_bubble->CloseBubbleView();
    popup_collection_->MoveDownPopups();

    // Reset bounds so popup baseline is updated.
    popup_collection_->ResetBounds();
  } else {
    // Record metrics if the bubble stays open.
    RecordOnTopOfSurfacesPopupCount();
  }
}

int AshMessagePopupCollection::NotifierCollisionHandler::
    CalculateBaselineOffset() {
  // Baseline pre-notifier collision does not consider corner anchored shelf pod
  // bubbles or slider bubbles to set its offset.
  if (!features::IsNotifierCollisionEnabled()) {
    surface_type_ = NotifierCollisionSurfaceType::kExtendedHotseat;
    return CalculateExtendedHotseatOffset();
  }

  auto* status_area =
      StatusAreaWidget::ForWindow(popup_collection_->shelf_->GetWindow());
  auto* current_open_shelf_pod_bubble =
      status_area ? status_area->open_shelf_pod_bubble() : nullptr;

  if (current_open_shelf_pod_bubble &&
      current_open_shelf_pod_bubble->IsAnchoredToShelfCorner()) {
    // Offset is calculated based on the height of the corner anchored shelf pod
    // bubble, if one is open.
    baseline_offset_ = current_open_shelf_pod_bubble->height() +
                       message_center::kMarginBetweenPopups;
    surface_type_ = NotifierCollisionSurfaceType::kShelfPodBubble;
  } else {
    int slider_offset = CalculateSliderOffset();
    int hotseat_offset = CalculateExtendedHotseatOffset();

    // If no corner anchored shelf pod bubble is open, the offset is calculated
    // based on the visibility of slider bubbles and the extended hotseat.
    baseline_offset_ = slider_offset + hotseat_offset;

    if (slider_offset != 0 && hotseat_offset != 0) {
      surface_type_ =
          NotifierCollisionSurfaceType::kSliderBubbleAndExtendedHotseat;
    } else if (slider_offset != 0) {
      surface_type_ = NotifierCollisionSurfaceType::kSliderBubble;
    } else if (hotseat_offset != 0) {
      surface_type_ = NotifierCollisionSurfaceType::kExtendedHotseat;
    } else {
      surface_type_ = NotifierCollisionSurfaceType::kNone;
    }
  }

  return baseline_offset_;
}

void AshMessagePopupCollection::NotifierCollisionHandler::
    OnStatusAreaAnchoredBubbleVisibilityChanged(TrayBubbleView* tray_bubble,
                                                bool visible) {
  HandleBubbleVisibilityOrBoundsChanged();
}

void AshMessagePopupCollection::NotifierCollisionHandler::
    OnTrayBubbleBoundsChanged(TrayBubbleView* tray_bubble) {
  HandleBubbleVisibilityOrBoundsChanged();
}

void AshMessagePopupCollection::NotifierCollisionHandler::
    HandleBubbleVisibilityOrBoundsChanged() {
  if (!features::IsNotifierCollisionEnabled()) {
    return;
  }

  // This is to make sure that we don't close the bubble through
  // `OnPopupCollectionHeightChanged()` to avoid crashes (b/305781721).
  base::AutoReset<bool> reset(&is_handling_bubble_change_, true);

  int previous_baseline_offset = baseline_offset_;

  // If the popup collection does not fit in the available space when opening a
  // bubble or updating its height, close all popups.
  if (popup_collection_->popup_collection_bounds().height() >
      popup_collection_->GetBaseline()) {
    popup_collection_->CloseAllPopupsNow();
  }

  // Reset bounds so popup baseline is updated.
  popup_collection_->ResetBounds();

  if (baseline_offset_ != previous_baseline_offset && baseline_offset_ != 0) {
    RecordOnTopOfSurfacesPopupCount();
    RecordSurfaceType();
  }
}

int AshMessagePopupCollection::NotifierCollisionHandler::
    CalculateExtendedHotseatOffset() const {
  auto* hotseat_widget = popup_collection_->shelf_->hotseat_widget();

  // `hotseat_widget` might be null since it dtor-ed before this class.
  return (hotseat_widget && hotseat_widget->state() == HotseatState::kExtended)
             ? hotseat_widget->GetHotseatSize()
             : 0;
}

int AshMessagePopupCollection::NotifierCollisionHandler::CalculateSliderOffset()
    const {
  auto* root_window_controller =
      RootWindowController::ForWindow(popup_collection_->shelf_->GetWindow());

  if (!root_window_controller ||
      !root_window_controller->GetStatusAreaWidget()) {
    return 0;
  }

  auto* unified_system_tray =
      root_window_controller->GetStatusAreaWidget()->unified_system_tray();

  return (unified_system_tray && unified_system_tray->IsSliderBubbleShown() &&
          unified_system_tray->GetSliderView())
             ? unified_system_tray->GetSliderView()->height() +
                   message_center::kMarginBetweenPopups
             : 0;
}

void AshMessagePopupCollection::NotifierCollisionHandler::
    RecordOnTopOfSurfacesPopupCount() {
  size_t popup_count = popup_collection_->popup_items().size();
  if (popup_count != 0) {
    base::UmaHistogramCounts100(
        "Ash.NotificationPopup.OnTopOfSurfacesPopupCount", popup_count);
  }
}

void AshMessagePopupCollection::NotifierCollisionHandler::RecordSurfaceType() {
  if (popup_collection_->popup_items().size() != 0) {
    base::UmaHistogramEnumeration("Ash.NotificationPopup.OnTopOfSurfacesType",
                                  surface_type_);
  }
}

void AshMessagePopupCollection::NotifierCollisionHandler::
    OnDisplayTabletStateChanged(display::TabletState state) {
  if (display::IsTabletStateChanging(state)) {
    // Do nothing when the tablet state is still in the process of transition.
    return;
  }

  // Reset bounds so pop-up baseline is updated.
  popup_collection_->ResetBounds();
}

void AshMessagePopupCollection::NotifierCollisionHandler::
    OnBackgroundTypeChanged(ShelfBackgroundType background_type,
                            AnimationChangeType change_type) {
  popup_collection_->ResetBounds();
}

void AshMessagePopupCollection::NotifierCollisionHandler::
    OnShelfWorkAreaInsetsChanged() {
  popup_collection_->UpdateWorkArea();
}

void AshMessagePopupCollection::NotifierCollisionHandler::OnHotseatStateChanged(
    HotseatState old_state,
    HotseatState new_state) {
  // We only need to take care of `HotseatState::kExtended` state.
  if (old_state != HotseatState::kExtended &&
      new_state != HotseatState::kExtended) {
    return;
  }
  popup_collection_->ResetBounds();
  RecordSurfaceType();
}

///////////////////////////////////////////////////////////////////////////////
// AshMessagePopupCollection:

AshMessagePopupCollection::AshMessagePopupCollection(display::Screen* screen,
                                                     Shelf* shelf)
    : screen_(screen), shelf_(shelf) {
  notifier_collision_handler_ =
      std::make_unique<NotifierCollisionHandler>(this);
  StartObserving(screen_,
                 screen_->GetDisplayNearestWindow(
                     shelf_->GetStatusAreaWidget()->GetNativeWindow()));
}

AshMessagePopupCollection::~AshMessagePopupCollection() {
  for (views::Widget* widget : tracked_widgets_)
    widget->RemoveObserver(this);
  CHECK(!views::WidgetObserver::IsInObserverList());

  // Should destruct `notifier_collision_handler_` before all other instances of
  // this class since the handler depends on some of them.
  notifier_collision_handler_.reset();
}

void AshMessagePopupCollection::StartObserving(
    display::Screen* screen,
    const display::Display& display) {
  screen_ = screen;
  work_area_ = display.work_area();
  display_observer_.emplace(this);
}

int AshMessagePopupCollection::GetPopupOriginX(
    const gfx::Rect& popup_bounds) const {
  // Popups should always follow the status area and will usually show on the
  // bottom-right of the screen. They will show at the bottom-left whenever the
  // shelf is left-aligned or for RTL when the shelf is not right aligned.
  return ((base::i18n::IsRTL() && GetAlignment() != ShelfAlignment::kRight) ||
          IsFromLeft())
             ? work_area_.x() + kPopupMarginX
             : work_area_.right() - kPopupMarginX - popup_bounds.width();
}

int AshMessagePopupCollection::GetBaseline() const {
  gfx::Insets tray_bubble_insets = GetTrayBubbleInsets(shelf_->GetWindow());
  int notifier_collision_offset =
      notifier_collision_handler_
          ? notifier_collision_handler_->CalculateBaselineOffset()
          : 0;

  // Decrease baseline by `kShelfDisplayOffset` to compensate for the adjustment
  // of edges in `Shelf::GetSystemTrayAnchorRect()`.
  return work_area_.bottom() - tray_bubble_insets.bottom() -
         notifier_collision_offset - kShelfDisplayOffset;
}

gfx::Rect AshMessagePopupCollection::GetWorkArea() const {
  return work_area_;
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
  // On ash, popups go in `SettingBubbleContainer` together with other tray
  // bubbles, so the most recent element on screen will appear in front.
  init_params->parent = shelf_->GetWindow()->GetRootWindow()->GetChildById(
      kShellWindowId_SettingBubbleContainer);

  // Make the widget activatable so it can receive focus when cycling through
  // windows (i.e. pressing ctrl + forward/back).
  init_params->activatable = views::Widget::InitParams::Activatable::kYes;
  init_params->name = kMessagePopupWidgetName;
  init_params->corner_radius = kMessagePopupCornerRadius;
  init_params->init_properties_container.SetProperty(
      kStayInOverviewOnActivationKey, true);
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

void AshMessagePopupCollection::NotifySilentNotification(
    const std::string& notification_id) {
  // Have any active screen reader announce the incoming silent notification.
  const views::View* status_area_widget_delegate =
      shelf_->GetStatusAreaWidget()->status_area_widget_delegate();
  CHECK(status_area_widget_delegate);
  status_area_widget_delegate->GetViewAccessibility().AnnounceText(
      l10n_util::GetStringFUTF16Int(
          IDS_ASH_MESSAGE_CENTER_SILENT_NOTIFICATION_ANNOUNCEMENT,
          (int)message_center::MessageCenter::Get()->NotificationCount()));
}

void AshMessagePopupCollection::NotifyPopupCollectionHeightChanged() {
  if (!notifier_collision_handler_) {
    return;
  }

  notifier_collision_handler_->OnPopupCollectionHeightChanged();
}

void AshMessagePopupCollection::AnimationStarted() {
  if (popups_animating_ == 0 && last_pop_up_added_) {
    // Since all the popup widgets use the same compositor, we only need to set
    // this when the first popup shows in the animation sequence.
    animation_tracker_.emplace(last_pop_up_added_->GetWidget()
                                   ->GetCompositor()
                                   ->RequestNewThroughputTracker());
    animation_tracker_->Start(metrics_util::ForSmoothnessV3(
        base::BindRepeating(&ReportPopupAnimationSmoothness)));
  }
  ++popups_animating_;
}

void AshMessagePopupCollection::AnimationFinished() {
  --popups_animating_;
  if (!popups_animating_) {
    // Stop tracking when all animations are finished.
    if (animation_tracker_) {
      animation_tracker_->Stop();
      animation_tracker_.reset();
    }

    if (animation_idle_closure_) {
      std::move(animation_idle_closure_).Run();
    }
  }
}

message_center::MessagePopupView* AshMessagePopupCollection::CreatePopup(
    const message_center::Notification& notification) {
  bool a11_feedback_on_init =
      notification.rich_notification_data()
          .should_make_spoken_feedback_for_popup_updates;
  auto* popup_view = new message_center::MessagePopupView(
      MessageViewFactory::Create(notification, /*shown_in_popup=*/true)
          .release(),
      this, a11_feedback_on_init);

  // Custom arc notifications handle their own styling and background.
  if (notification.custom_view_type() != kArcNotificationCustomViewType) {
    notification_style_utils::StyleNotificationPopup(
        popup_view->message_view());
  }
  return popup_view;
}

void AshMessagePopupCollection::ClosePopupItem(PopupItem& item) {
  // We lock closing tray bubble here to prevent a bubble close when popup item
  // is removed (b/291988617).
  auto lock = TrayBackgroundView::DisableCloseBubbleOnWindowActivated();

  message_center::MessagePopupCollection::ClosePopupItem(item);
}

bool AshMessagePopupCollection::IsWidgetAPopupNotification(
    views::Widget* widget) {
  for (views::Widget* popup_widget : tracked_widgets_) {
    if (widget == popup_widget) {
      return true;
    }
  }
  return false;
}

void AshMessagePopupCollection::SetAnimationIdleClosureForTest(
    base::OnceClosure closure) {
  DCHECK(closure);
  DCHECK(!animation_idle_closure_);
  animation_idle_closure_ = std::move(closure);
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
