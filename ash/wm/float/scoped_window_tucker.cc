// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/float/scoped_window_tucker.h"

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/scoped_animation_disabler.h"
#include "ash/shell.h"
#include "ash/style/color_util.h"
#include "ash/style/dark_light_mode_controller_impl.h"
#include "ash/wm/float/float_controller.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/tablet_mode/tablet_mode_window_state.h"
#include "base/metrics/user_metrics.h"
#include "base/time/time.h"
#include "ui/aura/window_targeter.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/geometry/transform_util.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/view_targeter_delegate.h"
#include "ui/wm/core/window_util.h"
#include "ui/wm/public/activation_client.h"

namespace ash {

namespace {

constexpr int kTuckHandleWidth = 20;
constexpr int kTuckHandleHeight = 92;

// The tuck handle can be tapped slightly outside its bounds.
constexpr gfx::Insets kTuckHandleExtraTapInset = gfx::Insets::VH(-8, -16);

// The distance from the edge of the tucked window to the edge of the screen
// during the bounce.
constexpr int kTuckOffscreenPaddingDp = 20;

// The duration for the tucked window to slide offscreen during the bounce.
constexpr base::TimeDelta kTuckWindowBounceStartDuration =
    base::Milliseconds(400);

// The duration for the tucked window to bounce back to the edge of the screen.
constexpr base::TimeDelta kTuckWindowBounceEndDuration =
    base::Milliseconds(533);

constexpr base::TimeDelta kUntuckWindowAnimationDuration =
    base::Milliseconds(400);

constexpr base::TimeDelta kSlideHandleForOverviewDuration =
    base::Milliseconds(200);

// Returns the tuck handle bounds aligned with `window_bounds`.
const gfx::Rect GetTuckHandleBounds(bool left, const gfx::Rect& window_bounds) {
  const gfx::Point tuck_handle_origin =
      left ? window_bounds.right_center() -
                 gfx::Vector2d(0, kTuckHandleHeight / 2)
           : window_bounds.left_center() -
                 gfx::Vector2d(kTuckHandleWidth, kTuckHandleHeight / 2);
  return gfx::Rect(tuck_handle_origin,
                   gfx::Size(kTuckHandleWidth, kTuckHandleHeight));
}

}  // namespace

// -----------------------------------------------------------------------------
// TuckHandleView:

// Represents a tuck handle that untucks floated windows from offscreen.
class TuckHandleView : public views::Button,
                       public views::ViewTargeterDelegate {
 public:
  METADATA_HEADER(TuckHandleView);

  TuckHandleView(base::RepeatingClosure callback, bool left)
      : views::Button(callback), left_(left) {
    SetFlipCanvasOnPaintForRTLUI(false);
    SetFocusBehavior(FocusBehavior::NEVER);
    SetEventTargeter(std::make_unique<views::ViewTargeter>(this));
  }
  TuckHandleView(const TuckHandleView&) = delete;
  TuckHandleView& operator=(const TuckHandleView&) = delete;
  ~TuckHandleView() override = default;

  // views::Button:
  void OnThemeChanged() override {
    views::View::OnThemeChanged();
    SchedulePaint();
  }

  void PaintButtonContents(gfx::Canvas* canvas) override {
    // Flip the canvas horizontally for `left` tuck handle.
    if (left_) {
      canvas->Translate(gfx::Vector2d(width(), 0));
      canvas->Scale(-1, 1);
    }

    // We draw three icons on top of each other because we need separate
    // themeing on different parts which is not supported by `VectorIcon`.
    const bool dark_mode =
        DarkLightModeControllerImpl::Get()->IsDarkModeEnabled();

    // Paint the container bottom layer with default 80% opacity.
    SkColor color = dark_mode ? gfx::kGoogleGrey500 : gfx::kGoogleGrey600;
    const SkColor bottom_color =
        SkColorSetA(color, std::round(SkColorGetA(color) * 0.8f));

    const gfx::ImageSkia& tuck_container_bottom = gfx::CreateVectorIcon(
        kTuckHandleContainerBottomIcon, kTuckHandleWidth, bottom_color);
    canvas->DrawImageInt(tuck_container_bottom, 0, 0);

    // Paint the container top layer. This is mostly transparent, with 12%
    // opacity.
    color = dark_mode ? gfx::kGoogleGrey200 : gfx::kGoogleGrey600;
    const SkColor top_color =
        SkColorSetA(color, std::round(SkColorGetA(color) * 0.12f));
    const gfx::ImageSkia& tuck_container_top = gfx::CreateVectorIcon(
        kTuckHandleContainerTopIcon, kTuckHandleWidth, top_color);
    canvas->DrawImageInt(tuck_container_top, 0, 0);

    const gfx::ImageSkia& tuck_icon = gfx::CreateVectorIcon(
        kTuckHandleChevronIcon, kTuckHandleWidth, SK_ColorWHITE);
    canvas->DrawImageInt(tuck_icon, 0, 0);
  }

  void OnGestureEvent(ui::GestureEvent* event) override {
    if (event->type() != ui::ET_GESTURE_SCROLL_BEGIN) {
      views::Button::OnGestureEvent(event);
      return;
    }
    const ui::GestureEventDetails details = event->details();
    const float detail_x = details.scroll_x_hint(),
                detail_y = details.scroll_y_hint();

    // Ignore vertical gestures.
    if (std::fabs(detail_x) <= std::fabs(detail_y))
      return;

    // Handle like a normal button press for events on the tuck handle that are
    // obvious inward gestures.
    if ((left_ && detail_x > 0) || (!left_ && detail_x < 0)) {
      NotifyClick(*event);
      event->SetHandled();
      event->StopPropagation();
    }
  }

  // views::ViewTargeterDelegate:
  bool DoesIntersectRect(const views::View* target,
                         const gfx::Rect& rect) const override {
    return true;
  }

 private:
  // Whether the tuck handle is on the left or right edge of the screen. A
  // left tuck handle will have the chevron arrow pointing right and vice
  // versa.
  const bool left_;
};

BEGIN_METADATA(TuckHandleView, views::Button)
END_METADATA

// -----------------------------------------------------------------------------

ScopedWindowTucker::ScopedWindowTucker(aura::Window* window, bool left)
    : window_(window), left_(left), event_blocker_(window) {
  DCHECK(window_);

  views::Widget::InitParams params(views::Widget::InitParams::TYPE_POPUP);
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.parent =
      window_->GetRootWindow()->GetChildById(kShellWindowId_FloatContainer);
  params.init_properties_container.SetProperty(kHideInOverviewKey, true);
  params.init_properties_container.SetProperty(kForceVisibleInMiniViewKey,
                                               false);
  params.name = "TuckHandleWidget";
  tuck_handle_widget_->Init(std::move(params));

  tuck_handle_widget_->SetContentsView(std::make_unique<TuckHandleView>(
      base::BindRepeating(&ScopedWindowTucker::UntuckWindow,
                          base::Unretained(this)),
      left));
  tuck_handle_widget_->Show();

  auto targeter = std::make_unique<aura::WindowTargeter>();
  targeter->SetInsets(kTuckHandleExtraTapInset);
  tuck_handle_widget_->GetNativeWindow()->SetEventTargeter(std::move(targeter));

  // Activate the most recent window that is not minimized and not the tucked
  // `window_`, otherwise activate the app list.
  auto mru_windows =
      Shell::Get()->mru_window_tracker()->BuildMruWindowList(kActiveDesk);
  auto app_window_it =
      base::ranges::find_if(mru_windows, [this](aura::Window* w) {
        DCHECK(WindowState::Get(w));
        return w != window_ && !WindowState::Get(w)->IsMinimized();
      });
  aura::Window* window_to_activate =
      app_window_it == mru_windows.end()
          ? Shell::Get()->app_list_controller()->GetWindow()
          : *app_window_it;
  DCHECK(window_to_activate);
  wm::ActivateWindow(window_to_activate);

  Shell::Get()->activation_client()->AddObserver(this);
  overview_observer_.Observe(Shell::Get()->overview_controller());
}

ScopedWindowTucker::~ScopedWindowTucker() {
  Shell::Get()->activation_client()->RemoveObserver(this);
  wm::ActivateWindow(window_);
}

void ScopedWindowTucker::AnimateTuck() {
  const gfx::Rect initial_bounds(window_->bounds());

  // Sets the destination tucked bounds after the animation.
  // `TabletModeWindowState::UpdatePosition` calls
  // `GetPreferredFloatWindowTabletBounds` which checks if a window is tucked
  // and returns the tucked bounds accordingly.
  TabletModeWindowState::UpdateWindowPosition(
      WindowState::Get(window_), WindowState::BoundsChangeAnimationType::kNone);
  const gfx::Rect final_bounds(window_->bounds());

  // Align the tuck handle with the window.
  aura::Window* tuck_handle = tuck_handle_widget_->GetNativeWindow();
  tuck_handle->SetBounds(GetTuckHandleBounds(left_, final_bounds));

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
  ScopedAnimationDisabler disable(window_);
  window_->Show();

  const gfx::RectF initial_bounds(window_->bounds());

  TabletModeWindowState::UpdateWindowPosition(
      WindowState::Get(window_), WindowState::BoundsChangeAnimationType::kNone);

  const gfx::Rect final_bounds(window_->bounds());
  const gfx::Transform transform =
      gfx::TransformBetweenRects(gfx::RectF(final_bounds), initial_bounds);
  aura::Window* tuck_handle = tuck_handle_widget_->GetNativeWindow();
  tuck_handle->SetBounds(GetTuckHandleBounds(left_, final_bounds));

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

void ScopedWindowTucker::OnWindowActivated(ActivationReason reason,
                                           aura::Window* gained_active,
                                           aura::Window* lost_active) {
  // Note that `UntuckWindow()` destroys `this`.
  if (gained_active == window_)
    UntuckWindow();
}

void ScopedWindowTucker::OnOverviewModeStarting() {
  OnOverviewModeChanged(/*in_overview=*/true);
}

void ScopedWindowTucker::OnOverviewModeEndingAnimationComplete(bool canceled) {
  OnOverviewModeChanged(/*in_overview=*/false);
}

void ScopedWindowTucker::OnOverviewModeChanged(bool in_overview) {
  // Slide the tuck handle offscreen if entering overview mode, or back onscreen
  // if exiting overview mode.
  aura::Window* tuck_handle = tuck_handle_widget_->GetNativeWindow();
  const gfx::Rect bounds = tuck_handle->bounds();
  gfx::Rect target_bounds =
      GetTuckHandleBounds(left_, window_->GetTargetBounds());
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

void ScopedWindowTucker::OnAnimateTuckEnded() {
  ScopedAnimationDisabler disable(window_);
  window_->Hide();
}

void ScopedWindowTucker::UntuckWindow() {
  Shell::Get()->float_controller()->MaybeUntuckFloatedWindowForTablet(window_);
}

}  // namespace ash
