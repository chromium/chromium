// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/drag_handle.h"

#include <optional>
#include <string>

#include "ash/accessibility/accessibility_controller.h"
#include "ash/constants/ash_features.h"
#include "ash/controls/contextual_tooltip.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shelf/shelf_navigation_widget.h"
#include "ash/shelf/shelf_observer.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/system/status_area_widget.h"
#include "ash/wm/overview/overview_controller.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/timer/timer.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {

namespace {

// Vertical padding to make the drag handle easier to tap.
constexpr int kVerticalClickboxPadding = 15;

// Drag handle translation distance for the first part of nudge animation.
constexpr int kInAppToHomeNudgeVerticalMarginRise = -4;

// Drag handle translation distance for the second part of nudge animation.
constexpr int kInAppToHomeVerticalMarginDrop = 10;

// Drag handle contextual nudge text box translation distance for the nudge
// animation at  the end.
constexpr int kInAppToHomeNudgeVerticalMarginDrop = 8;

// Animation time for each translation of drag handle to show contextual nudge.
constexpr base::TimeDelta kInAppToHomeAnimationTime = base::Milliseconds(300);

// Animation time to return drag handle to original position after hiding
// contextual nudge.
constexpr base::TimeDelta kInAppToHomeHideAnimationDuration =
    base::Milliseconds(600);

// Animation time to return drag handle to original position after the user taps
// to hide the contextual nudge.
constexpr base::TimeDelta kInAppToHomeHideOnTapAnimationDuration =
    base::Milliseconds(100);

// Delay between animating drag handle and tooltip opacity.
constexpr base::TimeDelta kInAppToHomeNudgeOpacityDelay =
    base::Milliseconds(500);

// Fade in time for drag handle nudge tooltip.
constexpr base::TimeDelta kInAppToHomeNudgeOpacityAnimationDuration =
    base::Milliseconds(200);

// Delay before animating the drag handle and showing the drag handle nudge.
constexpr base::TimeDelta kShowNudgeDelay = base::Seconds(2);

// This class is deleted after OnImplicitAnimationsCompleted() is called.
class HideNudgeObserver : public ui::ImplicitAnimationObserver {
 public:
  explicit HideNudgeObserver(ContextualNudge* drag_handle_nudge)
      : drag_handle_nudge_(drag_handle_nudge) {}
  ~HideNudgeObserver() override = default;

  // ui::ImplicitAnimationObserver:
  void OnImplicitAnimationsCompleted() override {
    drag_handle_nudge_->GetWidget()->CloseWithReason(
        views::Widget::ClosedReason::kUnspecified);
    delete this;
  }

 private:
  const raw_ptr<ContextualNudge> drag_handle_nudge_;
};

}  // namespace

DragHandle::DragHandle(float drag_handle_corner_radius, Shelf* shelf)
    : views::Button(base::BindRepeating(&DragHandle::ButtonPressed,
                                        base::Unretained(this))),
      shelf_(shelf) {
  SetPaintToLayer(ui::LAYER_SOLID_COLOR);
  layer()->SetRoundedCornerRadius(
      {drag_handle_corner_radius, drag_handle_corner_radius,
       drag_handle_corner_radius, drag_handle_corner_radius});
  SetSize(ShelfConfig::Get()->DragHandleSize());
  SetEventTargeter(std::make_unique<views::ViewTargeter>(this));
  SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);
  shell_observation_.Observe(Shell::Get());

  Shell::Get()->accessibility_controller()->AddObserver(this);
  shelf_->AddObserver(this);
  GetViewAccessibility().SetRole(ax::mojom::Role::kPopUpButton);
  OnAccessibilityStatusChanged();
  UpdateAccessibleName();
  UpdateExpandedCollapsedAccessibleState();
}

DragHandle::~DragHandle() {
  StopObservingImplicitAnimations();

  Shell::Get()->accessibility_controller()->RemoveObserver(this);
  shelf_->RemoveObserver(this);
}

bool DragHandle::DoesIntersectRect(const views::View* target,
                                   const gfx::Rect& rect) const {
  DCHECK_EQ(target, this);
  gfx::Rect drag_handle_bounds = target->GetLocalBounds();
  drag_handle_bounds.set_y(drag_handle_bounds.y() - kVerticalClickboxPadding);
  drag_handle_bounds.set_height(drag_handle_bounds.height() +
                                2 * kVerticalClickboxPadding);
  return drag_handle_bounds.Intersects(rect);
}

bool DragHandle::MaybeShowDragHandleNudge() {
  // Stop observing overview state if nudge show timer has fired.
  if (!show_drag_handle_nudge_timer_.IsRunning())
    overview_observation_.Reset();

  if (!features::IsHideShelfControlsInTabletModeEnabled()) {
    return false;
  }

  // Do not show drag handle nudge if it is already shown or drag handle is not
  // visible.
  if (gesture_nudge_target_visibility() ||
      window_drag_from_shelf_in_progress_ || !GetVisible() ||
      SplitViewController::Get(shelf_->shelf_widget()->GetNativeWindow())
          ->InSplitViewMode()) {
    return false;
  }
  show_nudge_animation_in_progress_ = true;
  auto_hide_lock_ = std::make_unique<Shelf::ScopedAutoHideLock>(shelf_);

  StopDragHandleNudgeShowTimer();
  ShowDragHandleNudge();
  return true;
}

void DragHandle::ShowDragHandleNudge() {
  DCHECK(!gesture_nudge_target_visibility_);
  PrefService* pref =
      Shell::Get()->session_controller()->GetLastActiveUserPrefService();
  base::TimeDelta nudge_duration = contextual_tooltip::GetNudgeTimeout(
      pref, contextual_tooltip::TooltipType::kInAppToHome);
  AnimateDragHandleShow();
  ShowDragHandleTooltip();
  gesture_nudge_target_visibility_ = true;
  split_view_observation_.Observe(
      SplitViewController::Get(shelf_->shelf_widget()->GetNativeWindow()));

  if (!nudge_duration.is_zero()) {
    hide_drag_handle_nudge_timer_.Start(
        FROM_HERE, nudge_duration,
        base::BindOnce(&DragHandle::HideDragHandleNudge, base::Unretained(this),
                       contextual_tooltip::DismissNudgeReason::kTimeout,
                       /*animate=*/true));
  }
  contextual_tooltip::HandleNudgeShown(
      pref, contextual_tooltip::TooltipType::kInAppToHome);
}

void DragHandle::ScheduleShowDragHandleNudge() {
  if (gesture_nudge_target_visibility_ ||
      show_drag_handle_nudge_timer_.IsRunning() ||
      window_drag_from_shelf_in_progress_ ||
      Shell::Get()->overview_controller()->InOverviewSession()) {
    return;
  }

  // Observe overview controller to detect overview session start - this should
  // cancel the scheduled nudge show.
  overview_observation_.Observe(Shell::Get()->overview_controller());

  show_drag_handle_nudge_timer_.Start(
      FROM_HERE, kShowNudgeDelay,
      base::BindOnce(base::IgnoreResult(&DragHandle::MaybeShowDragHandleNudge),
                     base::Unretained(this)));
}

void DragHandle::HideDragHandleNudge(
    contextual_tooltip::DismissNudgeReason reason,
    bool animate) {
  StopDragHandleNudgeShowTimer();
  if (!gesture_nudge_target_visibility())
    return;

  split_view_observation_.Reset();
  hide_drag_handle_nudge_timer_.Stop();

  if (reason == contextual_tooltip::DismissNudgeReason::kPerformedGesture) {
    contextual_tooltip::HandleGesturePerformed(
        Shell::Get()->session_controller()->GetLastActiveUserPrefService(),
        contextual_tooltip::TooltipType::kInAppToHome);
  }

  HideDragHandleNudgeHelper(
      /*hidden_by_tap=*/reason == contextual_tooltip::DismissNudgeReason::kTap,
      animate);
  gesture_nudge_target_visibility_ = false;
}

void DragHandle::SetWindowDragFromShelfInProgress(bool gesture_in_progress) {
  if (window_drag_from_shelf_in_progress_ == gesture_in_progress)
    return;

  window_drag_from_shelf_in_progress_ = gesture_in_progress;

  // If the contextual nudge is not yet shown, make sure that any scheduled
  // nudge show request is canceled.
  if (!gesture_nudge_target_visibility()) {
    StopDragHandleNudgeShowTimer();
    return;
  }

  // If the drag handle nudge is shown when the gesture to home or overview
  // starts, keep it around until the gesture completes.
  if (window_drag_from_shelf_in_progress_) {
    hide_drag_handle_nudge_timer_.Stop();
  } else {
    HideDragHandleNudge(
        contextual_tooltip::DismissNudgeReason::kPerformedGesture,
        /*animate=*/true);
  }
}

void DragHandle::OnGestureEvent(ui::GestureEvent* event) {
  if (!features::IsHideShelfControlsInTabletModeEnabled() ||
      !gesture_nudge_target_visibility_) {
    return;
  }

  if (event->type() == ui::EventType::kGestureTap) {
    HandleTapOnNudge();
    event->StopPropagation();
  }
}

gfx::Rect DragHandle::GetAnchorBoundsInScreen() const {
  gfx::Rect anchor_bounds = ConvertRectToWidget(GetLocalBounds());
  // Ignore any transform set on the drag handle - drag handle is used as an
  // anchor for contextual nudges, and their bounds are set relative to the
  // handle bounds without transform (for example, for in-app to home nudge both
  // drag handle and the nudge will have non-indentity, identical transforms).
  gfx::PointF origin(anchor_bounds.origin());
  gfx::PointF origin_in_screen =
      layer()->transform().InverseMapPoint(origin).value_or(origin);

  // If the parent widget has a transform set, it should be ignored as well (the
  // transform is set during shelf widget animations, and will animate to
  // identity transform), so the nudge bounds are set relative to the target
  // shelf bounds.
  aura::Window* const widget_window = GetWidget()->GetNativeWindow();
  origin_in_screen +=
      gfx::Vector2dF(widget_window->bounds().origin().OffsetFromOrigin());
  wm::ConvertPointToScreen(widget_window->parent(), &origin_in_screen);

  anchor_bounds.set_origin(gfx::ToRoundedPoint(origin_in_screen));
  return anchor_bounds;
}

void DragHandle::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  // TODO(b/262424972): Remove unwanted ", window" string from the announcement.
  Button::GetAccessibleNodeData(node_data);

  switch (shelf_->shelf_layout_manager()->hotseat_state()) {
    case HotseatState::kNone:
    case HotseatState::kShownClamshell:
    case HotseatState::kShownHomeLauncher:
      break;
    case HotseatState::kHidden:
      // When the hotseat is kHidden, the focus traversal should go to the
      // status area as the next focus and the navigation area as the previous
      // focus.
      GetViewAccessibility().SetNextFocus(shelf_->GetStatusAreaWidget());
      GetViewAccessibility().SetPreviousFocus(
          shelf_->shelf_widget()->navigation_widget());
      break;
    case HotseatState::kExtended:
      // When the hotseat is kExtended, the focus traversal should go to the
      // hotseat as both the next and previous focus.
      GetViewAccessibility().SetNextFocus(shelf_->hotseat_widget());
      GetViewAccessibility().SetPreviousFocus(shelf_->hotseat_widget());
      break;
  }
}

void DragHandle::OnThemeChanged() {
  views::Button::OnThemeChanged();
  layer()->SetColor(
      GetColorProvider()->GetColor(cros_tokens::kCrosSysOnSurface));
}

void DragHandle::OnOverviewModeStarting() {
  StopDragHandleNudgeShowTimer();
}

void DragHandle::OnShellDestroying() {
  shell_observation_.Reset();
  // Removes the overview controller observer.
  StopDragHandleNudgeShowTimer();
  hide_drag_handle_nudge_timer_.Stop();
}

void DragHandle::OnSplitViewStateChanged(
    SplitViewController::State previous_state,
    SplitViewController::State state) {
  if (SplitViewController::Get(shelf_->shelf_widget()->GetNativeWindow())
          ->InSplitViewMode()) {
    HideDragHandleNudge(contextual_tooltip::DismissNudgeReason::kOther,
                        /*animate=*/true);
  }
}

void DragHandle::OnHotseatStateChanged(HotseatState old_state,
                                       HotseatState new_state) {
  // Reset |force_show_hotseat_resetter_| when it is no longer extended.
  if (force_show_hotseat_resetter_ && new_state != HotseatState::kExtended) {
    shelf_->hotseat_widget()->set_manually_extended(false);
    force_show_hotseat_resetter_.RunAndReset();
  }

  UpdateAccessibleName();
  UpdateExpandedCollapsedAccessibleState();
}

void DragHandle::OnAccessibilityStatusChanged() {
  // Only enable the button if shelf controls are shown for accessibility.
  views::View::SetEnabled(
      ShelfConfig::Get()->ShelfControlsForcedShownForAccessibility());
}

void DragHandle::ButtonPressed() {
  if (shelf_->shelf_layout_manager()->hotseat_state() ==
      HotseatState::kHidden) {
    force_show_hotseat_resetter_ =
        shelf_->shelf_widget()->ForceShowHotseatInTabletMode();
  } else if (force_show_hotseat_resetter_) {
    // Hide hotseat only if it's been brought up by tapping the drag handle.
    shelf_->hotseat_widget()->set_manually_extended(false);
    force_show_hotseat_resetter_.RunAndReset();
  }

  // The accessibility focus order depends on the hotseat state, and pressing
  // the drag handle changes the hotseat state. So, send an accessibility
  // notification in order to recompute the focus order.
  UpdateAccessibleName();
  UpdateExpandedCollapsedAccessibleState();
}

void DragHandle::OnImplicitAnimationsCompleted() {
  show_nudge_animation_in_progress_ = false;
  auto_hide_lock_.reset();
}

void DragHandle::ShowDragHandleTooltip() {
  DCHECK(!drag_handle_nudge_);
  drag_handle_nudge_ = new ContextualNudge(
      this, nullptr /*parent_window*/, ContextualNudge::Position::kTop,
      gfx::Insets(), l10n_util::GetStringUTF16(IDS_ASH_DRAG_HANDLE_NUDGE),
      base::BindRepeating(&DragHandle::HandleTapOnNudge,
                          weak_factory_.GetWeakPtr()));
  drag_handle_nudge_->GetWidget()->Show();
  drag_handle_nudge_->label()->layer()->SetOpacity(0.0f);

  {
    // Layer transform should be animated after a delay so the animator must
    // first schedules a pause for transform animation.
    ui::LayerAnimator* transform_animator =
        drag_handle_nudge_->GetWidget()->GetLayer()->GetAnimator();
    transform_animator->SchedulePauseForProperties(
        kInAppToHomeAnimationTime, ui::LayerAnimationElement::TRANSFORM);

    // Enqueue transform animation to start after pause.
    ui::ScopedLayerAnimationSettings transform_animation_settings(
        transform_animator);
    transform_animation_settings.SetTweenType(gfx::Tween::FAST_OUT_LINEAR_IN);
    transform_animation_settings.SetTransitionDuration(
        kInAppToHomeAnimationTime);
    transform_animation_settings.SetPreemptionStrategy(
        ui::LayerAnimator::ENQUEUE_NEW_ANIMATION);

    // gfx::Transform translate;
    gfx::Transform translate;
    translate.Translate(0, kInAppToHomeNudgeVerticalMarginDrop);
    drag_handle_nudge_->GetWidget()->GetLayer()->SetTransform(translate);
  }

  {
    // Layer opacity should be animated after a delay so the animator must first
    // schedules a pause for opacity animation.
    ui::LayerAnimator* opacity_animator =
        drag_handle_nudge_->label()->layer()->GetAnimator();
    opacity_animator->SchedulePauseForProperties(
        kInAppToHomeNudgeOpacityDelay, ui::LayerAnimationElement::OPACITY);

    // Enqueue opacity animation to start after pause.
    ui::ScopedLayerAnimationSettings opacity_animation_settings(
        opacity_animator);
    opacity_animation_settings.SetPreemptionStrategy(
        ui::LayerAnimator::ENQUEUE_NEW_ANIMATION);
    opacity_animation_settings.SetTweenType(gfx::Tween::LINEAR);
    opacity_animation_settings.SetTransitionDuration(
        kInAppToHomeNudgeOpacityAnimationDuration);
    opacity_animation_settings.AddObserver(this);
    drag_handle_nudge_->label()->layer()->SetOpacity(1.0f);
  }
}

void DragHandle::HideDragHandleNudgeHelper(bool hidden_by_tap, bool animate) {
  if (!animate) {
    ScheduleDragHandleTranslationAnimation(
        0, base::TimeDelta(), gfx::Tween::ZERO,
        ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
    views::Widget* nudge_widget = drag_handle_nudge_->GetWidget();
    drag_handle_nudge_ = nullptr;
    nudge_widget->CloseWithReason(views::Widget::ClosedReason::kUnspecified);
    return;
  }
  ScheduleDragHandleTranslationAnimation(
      0,
      hidden_by_tap ? kInAppToHomeHideOnTapAnimationDuration
                    : kInAppToHomeHideAnimationDuration,
      hidden_by_tap ? gfx::Tween::FAST_OUT_LINEAR_IN
                    : gfx::Tween::FAST_OUT_SLOW_IN,
      ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);

  if (drag_handle_nudge_) {
    ui::LayerAnimator* opacity_animator =
        drag_handle_nudge_->label()->layer()->GetAnimator();
    ui::ScopedLayerAnimationSettings opacity_animation_settings(
        opacity_animator);
    opacity_animation_settings.SetPreemptionStrategy(
        ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
    opacity_animation_settings.SetTweenType(gfx::Tween::LINEAR);
    opacity_animation_settings.SetTransitionDuration(
        hidden_by_tap ? kInAppToHomeHideOnTapAnimationDuration
                      : kInAppToHomeNudgeOpacityAnimationDuration);

    // Register an animation observer to close the tooltip widget once the label
    // opacity is animated to 0 as the widget will no longer be needed after
    // this point.
    opacity_animation_settings.AddObserver(
        new HideNudgeObserver(drag_handle_nudge_));
    drag_handle_nudge_->label()->layer()->SetOpacity(0.0f);

    drag_handle_nudge_ = nullptr;
  }
}

void DragHandle::AnimateDragHandleShow() {
  // Drag handle is animated in two steps that run in sequence. The first step
  // uses |IMMEDIATELY_ANIMATE_TO_NEW_TARGET| to preempt any in-progress
  // animations while the second step uses |ENQUEUE_NEW_ANIMATION| so it runs
  // after the first animation.
  ScheduleDragHandleTranslationAnimation(
      kInAppToHomeNudgeVerticalMarginRise, kInAppToHomeAnimationTime,
      gfx::Tween::FAST_OUT_SLOW_IN,
      ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
  ScheduleDragHandleTranslationAnimation(
      kInAppToHomeVerticalMarginDrop, kInAppToHomeAnimationTime,
      gfx::Tween::FAST_OUT_LINEAR_IN, ui::LayerAnimator::ENQUEUE_NEW_ANIMATION);
}

void DragHandle::ScheduleDragHandleTranslationAnimation(
    int vertical_offset,
    base::TimeDelta animation_time,
    gfx::Tween::Type tween_type,
    ui::LayerAnimator::PreemptionStrategy strategy) {
  ui::ScopedLayerAnimationSettings animation(layer()->GetAnimator());
  animation.SetTweenType(tween_type);
  animation.SetTransitionDuration(animation_time);
  animation.SetPreemptionStrategy(strategy);

  gfx::Transform translate;
  translate.Translate(0, vertical_offset);
  SetTransform(translate);
}

void DragHandle::HandleTapOnNudge() {
  if (!drag_handle_nudge_)
    return;
  HideDragHandleNudge(contextual_tooltip::DismissNudgeReason::kTap,
                      /*animate=*/true);
}

void DragHandle::StopDragHandleNudgeShowTimer() {
  show_drag_handle_nudge_timer_.Stop();
  overview_observation_.Reset();
}

void DragHandle::UpdateExpandedCollapsedAccessibleState() const {
  if (!shelf_ || !shelf_->shelf_layout_manager()) {
    return;
  }

  if (shelf_->shelf_layout_manager()->hotseat_state() ==
      HotseatState::kExtended) {
    GetViewAccessibility().SetIsExpanded();
  } else if (shelf_->shelf_layout_manager()->hotseat_state() ==
             HotseatState::kHidden) {
    GetViewAccessibility().SetIsCollapsed();
  } else {
    GetViewAccessibility().RemoveExpandCollapseState();
  }
}

void DragHandle::UpdateAccessibleName() {
  if (!shelf_ || !shelf_->shelf_layout_manager()) {
    GetViewAccessibility().SetName(
        std::string(), ax::mojom::NameFrom::kAttributeExplicitlyEmpty);
    return;
  }

  std::u16string accessible_name = std::u16string();
  switch (shelf_->shelf_layout_manager()->hotseat_state()) {
    case HotseatState::kNone:
    case HotseatState::kShownClamshell:
    case HotseatState::kShownHomeLauncher:
      break;
    case HotseatState::kHidden:
      accessible_name = l10n_util::GetStringUTF16(
          IDS_ASH_DRAG_HANDLE_HOTSEAT_ACCESSIBLE_NAME);
      break;
    case HotseatState::kExtended:
      // The name should be empty when the hotseat is extended but we cannot
      // hide it.
      if (force_show_hotseat_resetter_) {
        accessible_name = l10n_util::GetStringUTF16(
            IDS_ASH_DRAG_HANDLE_HOTSEAT_ACCESSIBLE_NAME);
      }
      break;
  }

  if (accessible_name.empty()) {
    GetViewAccessibility().SetName(
        std::string(), ax::mojom::NameFrom::kAttributeExplicitlyEmpty);
  } else {
    GetViewAccessibility().SetName(accessible_name);
  }
}

BEGIN_METADATA(DragHandle)
END_METADATA

}  // namespace ash
