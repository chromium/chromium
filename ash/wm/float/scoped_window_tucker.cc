// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/float/scoped_window_tucker.h"

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/style/color_util.h"
#include "ash/style/dark_light_mode_controller_impl.h"
#include "ash/wm/float/float_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_window_state.h"
#include "base/time/time.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/geometry/transform_util.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/controls/button/button.h"
#include "ui/wm/public/activation_client.h"

namespace ash {

namespace {

constexpr int kTuckHandleWidth = 20;
constexpr int kTuckHandleHeight = 92;

// The distance from the edge of the tucked window to the edge of the screen
// during the bounce.
constexpr int kTuckOffscreenPaddingDp = 20;

// The duration for the tucked window to slide offscreen during the bounce.
constexpr base::TimeDelta kTuckWindowBounceStartDuration =
    base::Milliseconds(400);

// The duration for the tucked window to bounce back to the edge of the screen.
constexpr base::TimeDelta kTuckWindowBounceEndDuration =
    base::Milliseconds(533);

}  // namespace

// -----------------------------------------------------------------------------
// ScopedWindowTucker::TuckHandle:

// Represents a tuck handle that untucks floated windows from offscreen.
class ScopedWindowTucker::TuckHandle : public views::Button {
 public:
  TuckHandle(base::RepeatingClosure callback, bool left)
      : views::Button(callback), left_(left) {
    SetFlipCanvasOnPaintForRTLUI(false);
    SetFocusBehavior(FocusBehavior::NEVER);
  }
  TuckHandle(const TuckHandle&) = delete;
  TuckHandle& operator=(const TuckHandle&) = delete;
  ~TuckHandle() override = default;

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

    // We draw two icons on top of each other because we need separate
    // themeing on different parts which is not supported by `VectorIcon`.
    const SkColor container_color = ColorUtil::GetSecondToneColor(
        DarkLightModeControllerImpl::Get()->IsDarkModeEnabled()
            ? SK_ColorWHITE
            : SK_ColorBLACK);
    const gfx::ImageSkia& tuck_container = gfx::CreateVectorIcon(
        kTuckHandleContainerIcon, kTuckHandleWidth, container_color);
    canvas->DrawImageInt(tuck_container, 0, 0);

    const gfx::ImageSkia& tuck_icon = gfx::CreateVectorIcon(
        kTuckHandleChevronIcon, kTuckHandleWidth, SK_ColorWHITE);
    canvas->DrawImageInt(tuck_icon, 0, 0);
  }

 private:
  // Whether the tuck handle is on the left or right edge of the screen. A
  // left tuck handle will have the chevron arrow pointing right and vice
  // versa.
  const bool left_;
};

// -----------------------------------------------------------------------------

ScopedWindowTucker::ScopedWindowTucker(aura::Window* window, bool left)
    : window_(window) {
  DCHECK(window_);

  views::Widget::InitParams params(views::Widget::InitParams::TYPE_POPUP);
  params.activatable = views::Widget::InitParams::Activatable::kYes;
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.parent =
      window_->GetRootWindow()->GetChildById(kShellWindowId_FloatContainer);
  params.init_properties_container.SetProperty(kHideInOverviewKey, true);
  params.init_properties_container.SetProperty(kForceVisibleInMiniViewKey,
                                               false);
  params.name = "TuckHandleWidget";
  tuck_handle_widget_->Init(std::move(params));

  tuck_handle_widget_->SetContentsView(std::make_unique<TuckHandle>(
      base::BindRepeating(&ScopedWindowTucker::UntuckWindow,
                          base::Unretained(this)),
      left));
  tuck_handle_widget_->Show();

  Shell::Get()->activation_client()->AddObserver(this);
}

ScopedWindowTucker::~ScopedWindowTucker() {
  Shell::Get()->activation_client()->RemoveObserver(this);
}

void ScopedWindowTucker::AnimateTuck(bool left) {
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
  const gfx::Point tuck_handle_origin =
      left ? final_bounds.right_center() -
                 gfx::Vector2d(0, kTuckHandleHeight / 2)
           : final_bounds.left_center() -
                 gfx::Vector2d(kTuckHandleWidth, kTuckHandleHeight / 2);
  const gfx::Rect tuck_handle_bounds(
      tuck_handle_origin, gfx::Size(kTuckHandleWidth, kTuckHandleHeight));
  tuck_handle->SetBounds(tuck_handle_bounds);
  tuck_handle->layer()->SetOpacity(1.f);

  // Set the window back to its initial floated bounds.
  const gfx::Transform initial_transform = gfx::TransformBetweenRects(
      gfx::RectF(final_bounds), gfx::RectF(initial_bounds));

  // Set the transform during the bounce.
  const gfx::Transform offset_transform = gfx::Transform::MakeTranslation(
      left ? -kTuckOffscreenPaddingDp : kTuckOffscreenPaddingDp, 0);

  views::AnimationBuilder()
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
}

void ScopedWindowTucker::OnWindowActivated(ActivationReason reason,
                                           aura::Window* gained_active,
                                           aura::Window* lost_active) {
  // Note that `UntuckWindow()` destroys `this`.
  if (gained_active == window_)
    UntuckWindow();
}

void ScopedWindowTucker::UntuckWindow() {
  Shell::Get()->float_controller()->MaybeUntuckFloatedWindowForTablet(window_);
}

}  // namespace ash