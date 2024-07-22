// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/scoped_window_tucker.h"

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/shell.h"
#include "ash/wm/tablet_mode/tablet_mode_window_state.h"
#include "base/metrics/user_metrics.h"
#include "ui/aura/window_targeter.h"
#include "ui/compositor/layer_animator.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/transform_util.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/wm/core/scoped_animation_disabler.h"
#include "ui/wm/core/window_util.h"
#include "ui/wm/public/activation_client.h"

namespace ash {

namespace {

// The tuck handle can be tapped slightly outside its bounds.
constexpr gfx::Insets kTuckHandleExtraTapInset = gfx::Insets::VH(-8, -16);

// The distance from the edge of the tucked window to the edge of the screen
// during the bounce.
constexpr int kTuckOffscreenPaddingDp = 20;

// The duration for the tucked window to slide offscreen during the bounce.
constexpr base::TimeDelta kTuckWindowBounceStartDuration =
    base::Milliseconds(400);

// The duration for the tucked window to bounce back to the edge of the
// screen.
constexpr base::TimeDelta kTuckWindowBounceEndDuration =
    base::Milliseconds(533);

constexpr base::TimeDelta kUntuckWindowAnimationDuration =
    base::Milliseconds(400);

constexpr base::TimeDelta kSlideHandleForOverviewDuration =
    base::Milliseconds(200);

}  // namespace

ScopedWindowTucker::Delegate::Delegate() {}
ScopedWindowTucker::Delegate::~Delegate() {}

// Represents a tuck handle that untucks floated windows from offscreen.
ScopedWindowTucker::TuckHandleView::TuckHandleView(
    base::WeakPtr<Delegate> delegate,
    base::RepeatingClosure callback,
    bool left)
    : views::Button(callback),
      scoped_window_tucker_delegate_(delegate),
      left_(left) {
  SetFlipCanvasOnPaintForRTLUI(false);
  SetFocusBehavior(FocusBehavior::NEVER);
  SetEventTargeter(std::make_unique<views::ViewTargeter>(this));
}

ScopedWindowTucker::TuckHandleView::~TuckHandleView() {}

void ScopedWindowTucker::TuckHandleView::OnThemeChanged() {
  views::View::OnThemeChanged();
  SchedulePaint();
}

void ScopedWindowTucker::TuckHandleView::PaintButtonContents(
    gfx::Canvas* canvas) {
  if (scoped_window_tucker_delegate_) {
    scoped_window_tucker_delegate_->PaintTuckHandle(canvas, width(), left_);
  }
}

void ScopedWindowTucker::TuckHandleView::OnGestureEvent(
    ui::GestureEvent* event) {
  if (event->type() != ui::EventType::kGestureScrollBegin) {
    views::Button::OnGestureEvent(event);
    return;
  }
  const ui::GestureEventDetails details = event->details();
  const float detail_x = details.scroll_x_hint(),
              detail_y = details.scroll_y_hint();

  // Ignore vertical gestures.
  if (std::fabs(detail_x) <= std::fabs(detail_y)) {
    return;
  }

  // Handle like a normal button press for events on the tuck handle that are
  // obvious inward gestures.
  if ((left_ && detail_x > 0) || (!left_ && detail_x < 0)) {
    NotifyClick(*event);
    event->SetHandled();
    event->StopPropagation();
  }
}

bool ScopedWindowTucker::TuckHandleView::DoesIntersectRect(
    const views::View* target,
    const gfx::Rect& rect) const {
  return true;
}

ScopedWindowTucker::ScopedWindowTucker(std::unique_ptr<Delegate> delegate,
                                       aura::Window* window,
                                       bool left)
    : delegate_(std::move(delegate)),
      window_(window),
      left_(left),
      event_blocker_(window) {
  InitializeTuckHandleWidget();
  window_observation_.Observe(window);
}

ScopedWindowTucker::~ScopedWindowTucker() {
  Shell::Get()->activation_client()->RemoveObserver(this);
  if (!window_->IsVisible()) {
    window_->Show();
    return;
  }
  wm::ActivateWindow(window_);
}

void ScopedWindowTucker::AnimateTuck() {
  const gfx::Rect initial_bounds(window_->bounds());

  // Sets the destination tucked bounds after the animation.
  delegate_->UpdateWindowPosition(window_, left_);
  const gfx::Rect final_bounds(window_->bounds());

  // Align the tuck handle with the window.
  aura::Window* tuck_handle = tuck_handle_widget_->GetNativeWindow();
  tuck_handle->SetBounds(delegate_->GetTuckHandleBounds(left_, final_bounds));

  // Set the window back to its initial floated bounds.
  const gfx::Transform initial_transform = gfx::TransformBetweenRects(
      gfx::RectF(final_bounds), gfx::RectF(initial_bounds));

  // Set the transform during the bounce.
  const gfx::Transform offset_transform = gfx::Transform::MakeTranslation(
      left_ ? -kTuckOffscreenPaddingDp : kTuckOffscreenPaddingDp, 0);

  views::AnimationBuilder()
      .OnEnded(base::BindOnce(&ScopedWindowTucker::OnAnimateTuckEnded,
                              weak_factory_.GetWeakPtr()))
      .SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .Once()
      .SetDuration(base::TimeDelta())
      .SetTransform(window_, initial_transform)
      .SetTransform(tuck_handle, initial_transform)
      .Then()
      .SetDuration(kTuckWindowBounceStartDuration)
      .SetTransform(window_, offset_transform, gfx::Tween::ACCEL_30_DECEL_20_85)
      .SetTransform(tuck_handle, offset_transform,
                    gfx::Tween::ACCEL_30_DECEL_20_85)
      .Then()
      .SetDuration(kTuckWindowBounceEndDuration)
      .SetTransform(window_, gfx::Transform(), gfx::Tween::ACCEL_20_DECEL_100)
      .SetTransform(tuck_handle, gfx::Transform(),
                    gfx::Tween::ACCEL_20_DECEL_100);

  base::RecordAction(base::UserMetricsAction(kTuckUserAction));
}

void ScopedWindowTucker::AnimateUntuck(base::OnceClosure callback) {
  wm::ScopedAnimationDisabler disable(window_);
  window_->Show();

  const gfx::RectF initial_bounds(window_->bounds());

  delegate_->UpdateWindowPosition(window_, left_);
  const gfx::Rect final_bounds(window_->bounds());
  const gfx::Transform transform =
      gfx::TransformBetweenRects(gfx::RectF(final_bounds), initial_bounds);
  aura::Window* tuck_handle = tuck_handle_widget_->GetNativeWindow();
  tuck_handle->SetBounds(delegate_->GetTuckHandleBounds(left_, final_bounds));

  views::AnimationBuilder()
      // TODO(sammiequon|sophiewen): Should we handle the case where the
      // animation gets aborted?
      .OnEnded(std::move(callback))
      .SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .Once()
      .SetDuration(base::TimeDelta())
      .SetTransform(window_, transform)
      .SetTransform(tuck_handle, transform)
      .Then()
      .SetDuration(kUntuckWindowAnimationDuration)
      .SetTransform(window_, gfx::Transform(), gfx::Tween::ACCEL_5_70_DECEL_90)
      .SetTransform(tuck_handle, gfx::Transform(),
                    gfx::Tween::ACCEL_5_70_DECEL_90);

  base::RecordAction(base::UserMetricsAction(kUntuckUserAction));
}

void ScopedWindowTucker::UntuckWindow() {
  delegate_->UntuckWindow(window_);
}

void ScopedWindowTucker::OnAnimateTuckEnded() {
  delegate_->OnAnimateTuckEnded(window_);
}

void ScopedWindowTucker::OnWindowActivated(ActivationReason reason,
                                           aura::Window* gained_active,
                                           aura::Window* lost_active) {
  // Note that `UntuckWindow()` destroys `this`.
  if (gained_active == window_) {
    delegate_->UntuckWindow(window_);
  }
}

void ScopedWindowTucker::OnOverviewModeStarting() {
  OnOverviewModeChanged(/*in_overview=*/true);
}

void ScopedWindowTucker::OnOverviewModeEndingAnimationComplete(bool canceled) {
  OnOverviewModeChanged(/*in_overview=*/false);
}

void ScopedWindowTucker::OnWindowBoundsChanged(
    aura::Window* window,
    const gfx::Rect& old_bounds,
    const gfx::Rect& new_bounds,
    ui::PropertyChangeReason reason) {
  aura::Window* tuck_handle = tuck_handle_widget_->GetNativeWindow();
  tuck_handle->SetBounds(delegate_->GetTuckHandleBounds(left_, new_bounds));
}

void ScopedWindowTucker::InitializeTuckHandleWidget() {
  views::Widget::InitParams params(
      views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_POPUP);
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.parent =
      window()->GetRootWindow()->GetChildById(delegate_->ParentContainerId());
  params.init_properties_container.SetProperty(kHideInOverviewKey, true);
  params.init_properties_container.SetProperty(kForceVisibleInMiniViewKey,
                                               false);
  params.name = "TuckHandleWidget";
  tuck_handle_widget_->Init(std::move(params));

  tuck_handle_widget_->SetContentsView(std::make_unique<TuckHandleView>(
      delegate_->GetWeakPtr(),
      base::BindRepeating(&ScopedWindowTucker::UntuckWindow,
                          base::Unretained(this)),
      left_));
  tuck_handle_widget_->Show();

  auto targeter = std::make_unique<aura::WindowTargeter>();
  targeter->SetInsets(kTuckHandleExtraTapInset);
  tuck_handle_widget_->GetNativeWindow()->SetEventTargeter(std::move(targeter));

  // Activate the most recent window that is not minimized and not the
  // tucked `window_`, if it exists. If no such window exists in tablet
  // mode, activate the app list.
  auto mru_windows =
      Shell::Get()->mru_window_tracker()->BuildMruWindowList(kActiveDesk);
  auto app_window_it =
      base::ranges::find_if(mru_windows, [this](aura::Window* w) {
        CHECK(WindowState::Get(w));
        return w != window() && !WindowState::Get(w)->IsMinimized();
      });
  aura::Window* window_to_activate = nullptr;
  if (app_window_it == mru_windows.end()) {
    if (display::Screen::GetScreen()->InTabletMode()) {
      window_to_activate = Shell::Get()->app_list_controller()->GetWindow();
    }
  } else {
    window_to_activate = *app_window_it;
  }
  if (window_to_activate) {
    wm::ActivateWindow(window_to_activate);
  }

  Shell::Get()->activation_client()->AddObserver(this);
  overview_observer_.Observe(Shell::Get()->overview_controller());
}

void ScopedWindowTucker::OnOverviewModeChanged(bool in_overview) {
  // Slide the tuck handle offscreen if entering overview mode, or back onscreen
  // if exiting overview mode.
  aura::Window* tuck_handle = tuck_handle_widget_->GetNativeWindow();
  const gfx::Rect bounds = tuck_handle->bounds();
  gfx::Rect target_bounds =
      delegate_->GetTuckHandleBounds(left_, window_->GetTargetBounds());
  if (in_overview) {
    const int x_offset = left_ ? -kTuckHandleWidth : kTuckHandleWidth;
    target_bounds.Offset(x_offset, 0);
  }

  if (target_bounds == bounds) {
    return;
  }

  tuck_handle->SetBounds(target_bounds);
  const gfx::Transform transform =
      gfx::TransformBetweenRects(gfx::RectF(target_bounds), gfx::RectF(bounds));

  views::AnimationBuilder()
      .SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .Once()
      .SetDuration(base::TimeDelta())
      .SetTransform(tuck_handle, transform)
      .Then()
      .SetDuration(kSlideHandleForOverviewDuration)
      .SetTransform(tuck_handle, gfx::Transform(),
                    gfx::Tween::ACCEL_20_DECEL_100);
}

}  // namespace ash
