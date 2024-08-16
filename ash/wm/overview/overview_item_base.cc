// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/overview_item_base.h"

#include <vector>

#include "ash/public/cpp/window_properties.h"
#include "ash/shell.h"
#include "ash/style/rounded_label_widget.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/desks/templates/saved_desk_animations.h"
#include "ash/wm/drag_window_controller.h"
#include "ash/wm/overview/overview_constants.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_grid_event_handler.h"
#include "ash/wm/overview/overview_group_item.h"
#include "ash/wm/overview/overview_item.h"
#include "ash/wm/overview/overview_session.h"
#include "ash/wm/overview/overview_utils.h"
#include "ash/wm/overview/scoped_overview_animation_settings.h"
#include "ash/wm/snap_group/snap_group.h"
#include "ash/wm/snap_group/snap_group_controller.h"
#include "ash/wm/splitview/split_view_utils.h"
#include "ash/wm/window_properties.h"
#include "ash/wm/window_util.h"
#include "ash/wm/wm_constants.h"
#include "base/memory/raw_ptr.h"

namespace ash {

OverviewItemBase::OverviewItemBase(OverviewSession* overview_session,
                                   OverviewGrid* overview_grid,
                                   aura::Window* root_window)
    : root_window_(root_window),
      overview_session_(overview_session),
      overview_grid_(overview_grid) {}

OverviewItemBase::~OverviewItemBase() = default;

// static
std::unique_ptr<OverviewItemBase> OverviewItemBase::Create(
    aura::Window* window,
    OverviewSession* overview_session,
    OverviewGrid* overview_grid) {
  SnapGroupController* snap_group_controller = SnapGroupController::Get();
  if (snap_group_controller) {
    if (SnapGroup* snap_group =
            snap_group_controller->GetSnapGroupForGivenWindow(window)) {
      return std::make_unique<OverviewGroupItem>(
          std::vector<raw_ptr<aura::Window, VectorExperimental>>{
              snap_group->GetPhysicallyLeftOrTopWindow(),
              snap_group->GetPhysicallyRightOrBottomWindow()},
          overview_session, overview_grid);
    }
  }

  return std::make_unique<OverviewItem>(window, overview_session, overview_grid,
                                        /*destruction_delegate=*/overview_grid,
                                        /*event_handler_delegate=*/nullptr,
                                        /*eligible_for_shadow_config=*/true);
}

bool OverviewItemBase::IsDragItem() const {
  // `overview_session_` may be null in tests.
  // TODO(https://b/299391958): `overview_session_` should not be null even in
  // tests.
  return overview_session_ &&
         overview_session_->GetCurrentDraggedOverviewItem() == this;
}

void OverviewItemBase::SetVisibleDuringItemDragging(bool visible,
                                                    bool animate) {
  SetWindowsVisibleDuringItemDragging(GetWindowsForHomeGesture(), visible,
                                      animate);
}

void OverviewItemBase::RefreshShadowVisuals(bool shadow_visible) {
  const bool should_have_shadow = ShouldHaveShadow();
  if (should_have_shadow != !!shadow_) {
    if (should_have_shadow) {
      CreateShadow();
    } else {
      shadow_.reset();
    }
  }

  // On destruction, `shadow_` is cleaned up before `transform_window_`, which
  // may call this function, so early exit if `shadow_` is nullptr.
  if (!shadow_) {
    return;
  }

  const gfx::RectF shadow_bounds_in_screen = target_bounds_;
  auto* shadow_layer = shadow_->GetLayer();

  // Shadow is normally turned off during animations and reapplied when on
  // animation complete.
  if (!shadow_visible || shadow_bounds_in_screen.IsEmpty()) {
    shadow_layer->SetVisible(false);
    return;
  }

  shadow_layer->SetVisible(true);

  gfx::Rect shadow_content_bounds(
      gfx::ToRoundedRect(shadow_bounds_in_screen).size());
  shadow_->SetContentBounds(shadow_content_bounds);
  shadow_->SetRoundedCornerRadius(
      window_util::GetMiniWindowRoundedCornerRadius());
}

void OverviewItemBase::UpdateShadowTypeForDrag(bool is_dragging) {
  if (shadow_) {
    shadow_->SetType(is_dragging ? kDraggedShadowType : kDefaultShadowType);
  }
}

void OverviewItemBase::HandleGestureEventForTabletModeLayout(
    ui::GestureEvent* event,
    OverviewItemBase* event_source_item) {
  const gfx::PointF location = event->details().bounding_box_f().CenterPoint();
  OverviewGridEventHandler* grid_event_handler =
      overview_grid()->grid_event_handler();
  const bool is_drag_item = IsDragItem();
  switch (event->type()) {
    case ui::EventType::kScrollFlingStart:
      if (is_drag_item) {
        HandleFlingStartEvent(location, event->details().velocity_x(),
                              event->details().velocity_y());
      } else {
        grid_event_handler->OnGestureEvent(event);
      }
      break;
    case ui::EventType::kGestureScrollBegin:
      if (std::abs(event->details().scroll_y_hint()) >
          std::abs(event->details().scroll_x_hint())) {
        HandlePressEvent(location, /*from_touch_gesture=*/true,
                         event_source_item);
      } else {
        grid_event_handler->OnGestureEvent(event);
      }
      break;
    case ui::EventType::kGestureScrollUpdate:
      if (is_drag_item) {
        HandleDragEvent(location);
      } else {
        grid_event_handler->OnGestureEvent(event);
      }
      break;
    case ui::EventType::kGestureScrollEnd:
      if (is_drag_item) {
        HandleReleaseEvent(location);
      } else {
        grid_event_handler->OnGestureEvent(event);
      }
      break;
    case ui::EventType::kGestureLongPress:
      HandlePressEvent(location, /*from_touch_gesture=*/true,
                       event_source_item);
      HandleLongPressEvent(location);
      break;
    case ui::EventType::kGestureTap:
      HandleTapEvent(location, event_source_item);
      break;
    case ui::EventType::kGestureEnd:
      HandleGestureEndEvent();
      break;
    default:
      grid_event_handler->OnGestureEvent(event);
      break;
  }
}

void OverviewItemBase::HandleMouseEvent(const ui::MouseEvent& event,
                                        OverviewItemBase* event_source_item) {
  if (!overview_session_->CanProcessEvent(this, /*from_touch_gesture=*/false)) {
    return;
  }

  // `event.target()` will be null if we use search+space on this item with
  // chromevox on. Accessibility API will synthesize a mouse event in that case
  // without a target. We just use the centerpoint of the item so that
  // search+space will select the item, leaving overview.
  const gfx::PointF screen_location =
      event.target() ? event.target()->GetScreenLocationF(event)
                     : gfx::PointF(GetWindowsUnionScreenBounds().CenterPoint());
  switch (event.type()) {
    case ui::EventType::kMousePressed:
      HandlePressEvent(screen_location, /*from_touch_gesture=*/false,
                       event_source_item);
      break;
    case ui::EventType::kMouseReleased:
      HandleReleaseEvent(screen_location);
      break;
    case ui::EventType::kMouseDragged:
      HandleDragEvent(screen_location);
      break;
    default:
      NOTREACHED();
  }
}

void OverviewItemBase::HandleGestureEvent(ui::GestureEvent* event,
                                          OverviewItemBase* event_source_item) {
  if (!overview_session_->CanProcessEvent(this, /*from_touch_gesture=*/true)) {
    event->StopPropagation();
    event->SetHandled();
    return;
  }

  if (ShouldUseTabletModeGridLayout()) {
    HandleGestureEventForTabletModeLayout(event, event_source_item);
    return;
  }

  const gfx::PointF location = event->details().bounding_box_f().CenterPoint();
  switch (event->type()) {
    case ui::EventType::kGestureTapDown:
      HandlePressEvent(location, /*from_touch_gesture=*/true,
                       event_source_item);
      break;
    case ui::EventType::kGestureScrollUpdate:
      HandleDragEvent(location);
      break;
    case ui::EventType::kScrollFlingStart:
      HandleFlingStartEvent(location, event->details().velocity_x(),
                            event->details().velocity_y());
      break;
    case ui::EventType::kGestureScrollEnd:
      HandleReleaseEvent(location);
      break;
    case ui::EventType::kGestureLongPress:
      HandleLongPressEvent(location);
      break;
    case ui::EventType::kGestureTap:
      HandleTapEvent(location, event_source_item);
      break;
    case ui::EventType::kGestureEnd:
      HandleGestureEndEvent();
      break;
    default:
      break;
  }
}

void OverviewItemBase::SetOpacity(float opacity) {
  item_widget_->SetOpacity(opacity);
  if (cannot_snap_widget_) {
    cannot_snap_widget_->SetOpacity(opacity);
  }
}

aura::Window::Windows OverviewItemBase::GetWindowsForHomeGesture() {
  aura::Window::Windows windows = {item_widget_->GetNativeWindow()};

  if (cannot_snap_widget_) {
    windows.push_back(cannot_snap_widget_->GetNativeWindow());
  }

  return windows;
}

void OverviewItemBase::HideForSavedDeskLibrary(bool animate) {
  // Temporarily hide this window in overview, so that dark/light theme change
  // does not reset the layer visible. If `animate` is false, the callback will
  // not run in `PerformFadeOutLayer`. Thus, here we make sure the window is
  // also hidden in that case.
  DCHECK(item_widget_);
  hide_window_in_overview_callback_.Reset(base::BindOnce(
      &OverviewItemBase::HideItemWidgetWindow, weak_ptr_factory_.GetWeakPtr()));
  PerformFadeOutLayer(item_widget_->GetLayer(), animate,
                      hide_window_in_overview_callback_.callback());
  if (!animate) {
    // Cancel the callback if we are going to run it directly.
    hide_window_in_overview_callback_.Cancel();
    HideItemWidgetWindow();
  }

  item_widget_event_blocker_ =
      std::make_unique<aura::ScopedWindowEventTargetingBlocker>(
          item_widget_->GetNativeWindow());

  // TODO(http://b/339108996): Determine how to inform users when a group item
  // cannot be snapped.
  HideCannotSnapWarning(animate);
}

void OverviewItemBase::RevertHideForSavedDeskLibrary(bool animate) {
  // This might run before `HideForSavedDeskLibrary()`, thus cancel the
  // callback to prevent such case.
  hide_window_in_overview_callback_.Cancel();

  // Restore and show the window back to overview.
  ShowItemWidgetWindow();

  // `item_widget_` may be null during shutdown if the window is minimized.
  if (item_widget_) {
    PerformFadeInLayer(item_widget_->GetLayer(), animate);
  }

  item_widget_event_blocker_.reset();

  // TODO(http://b/339108996): Determine how to inform users when a group item
  // cannot be snapped.
  UpdateCannotSnapWarningVisibility(animate);
}

void OverviewItemBase::UpdateMirrorsForDragging(bool is_touch_dragging) {
  CHECK_GT(Shell::GetAllRootWindows().size(), 1u);

  if (!item_mirror_for_dragging_) {
    item_mirror_for_dragging_ = std::make_unique<DragWindowController>(
        item_widget_->GetNativeWindow(), is_touch_dragging);
  }

  item_mirror_for_dragging_->Update();
}

// Resets the mirrors needed for multi display dragging.
void OverviewItemBase::DestroyMirrorsForDragging() {
  item_mirror_for_dragging_.reset();
}

views::Widget::InitParams OverviewItemBase::CreateOverviewItemWidgetParams(
    aura::Window* parent_window,
    const std::string& widget_name,
    bool accept_event) const {
  views::Widget::InitParams params(
      views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
      views::Widget::InitParams::TYPE_POPUP);
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.name = widget_name;
  params.accept_events = accept_event;
  params.parent = parent_window;
  params.init_properties_container.SetProperty(kHideInDeskMiniViewKey, true);
  params.init_properties_container.SetProperty(kOverviewUiKey, true);
  return params;
}

void OverviewItemBase::CreateShadow() {
  shadow_ = SystemShadow::CreateShadowOnNinePatchLayer(
      kDefaultShadowType, SystemShadow::LayerRecreatedCallback());
  auto* shadow_layer = shadow_->GetLayer();
  auto* widget_layer = item_widget_->GetLayer();
  widget_layer->Add(shadow_layer);
  widget_layer->StackAtBottom(shadow_layer);
  shadow_->ObserveColorProviderSource(item_widget_.get());
}

void OverviewItemBase::HandleDragEvent(const gfx::PointF& location_in_screen) {
  if (IsDragItem()) {
    overview_session_->Drag(this, location_in_screen);
  }
}

void OverviewItemBase::HideItemWidgetWindow() {
  ScopedOverviewHideWindows* hide_windows =
      overview_session_->hide_windows_for_saved_desks_grid();
  DCHECK(hide_windows);

  // Hide the overview item window.
  if (item_widget_ &&
      !hide_windows->HasWindow(item_widget_->GetNativeWindow())) {
    hide_windows->AddWindow(item_widget_->GetNativeWindow());
  }
}

void OverviewItemBase::ShowItemWidgetWindow() {
  ScopedOverviewHideWindows* hide_windows =
      overview_session_->hide_windows_for_saved_desks_grid();
  DCHECK(hide_windows);

  // Show the overview item window.
  if (item_widget_ &&
      hide_windows->HasWindow(item_widget_->GetNativeWindow())) {
    hide_windows->RemoveWindow(item_widget_->GetNativeWindow(),
                               /*show_window=*/true);
  }
}

void OverviewItemBase::HandlePressEvent(const gfx::PointF& location_in_screen,
                                        bool from_touch_gesture,
                                        OverviewItemBase* event_source_item) {
  // No need to start the drag again if already in a drag. This can happen if we
  // switch fingers midway through a drag.
  if (!IsDragItem()) {
    StartDrag();
    overview_session_->InitiateDrag(this, location_in_screen,
                                    /*is_touch_dragging=*/from_touch_gesture,
                                    event_source_item);
  }
}

void OverviewItemBase::HandleReleaseEvent(
    const gfx::PointF& location_in_screen) {
  if (IsDragItem()) {
    overview_session_->CompleteDrag(this, location_in_screen);
  }
}

void OverviewItemBase::HandleLongPressEvent(
    const gfx::PointF& location_in_screen) {
  if (IsDragItem() && (IsEligibleForDraggingToSnapInOverview(this) ||
                       (desks_util::ShouldDesksBarBeCreated() &&
                        overview_grid_->IsDesksBarViewActive()))) {
    overview_session_->StartNormalDragMode(location_in_screen);
  }
}

void OverviewItemBase::HandleFlingStartEvent(
    const gfx::PointF& location_in_screen,
    float velocity_x,
    float velocity_y) {
  overview_session_->Fling(this, location_in_screen, velocity_x, velocity_y);
}

void OverviewItemBase::HandleTapEvent(const gfx::PointF& location_in_screen,
                                      OverviewItemBase* event_source_item) {
  if (IsDragItem()) {
    overview_session_->ActivateDraggedWindow();
    return;
  }

  overview_session_->SelectWindow(event_source_item);
}

void OverviewItemBase::HandleGestureEndEvent() {
  if (IsDragItem()) {
    // Gesture end events come from a long press getting canceled. Long press
    // alters the stacking order, so on gesture end, make sure we restore the
    // stacking order on the next reposition.
    set_should_restack_on_animation_end(true);
    overview_session_->ResetDraggedWindowGesture();
  }
}

}  // namespace ash
