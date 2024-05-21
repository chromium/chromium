// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/workspace/phantom_window_controller.h"

#include <math.h>

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/root_window_controller.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/ash_color_provider.h"
#include "ash/wm/window_util.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/presentation_time_recorder.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/display/screen.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/transform_util.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/highlight_border.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/coordinate_conversion.h"
#include "ui/wm/core/shadow_controller.h"
#include "ui/wm/core/shadow_types.h"

namespace ash {
namespace {

// The size of the phantom window at the beginning of the show animation in
// relation to the size of the phantom window at the end of the animation.
constexpr float kScrimStartBoundsRatio = 0.75f;

// The duration of the phantom scrim entrance animation for size transform.
constexpr base::TimeDelta kScrimEntranceSizeAnimationDurationMs =
    base::Milliseconds(150);
// The duration of the phantom scrim entrance animation for opacity.
constexpr base::TimeDelta kScrimEntranceOpacityAnimationDurationMs =
    base::Milliseconds(50);

// The elevation of the shadow for the phantom window should match that of an
// active window.
// The shadow ninebox requires a minimum size to work well. See
// ui/compositor_extra/shadow.cc
constexpr int kMinWidthWithShadow = 2 * wm::kShadowElevationActiveWindow;
constexpr int kMinHeightWithShadow = 4 * wm::kShadowElevationActiveWindow;

// The top margin of the cue from the top of work area.
constexpr int kMaximizeCueTopMargin = 32;
constexpr int kMaximizeCueHeight = 36;
constexpr int kMaximizeCueHorizontalInsets = 16;
constexpr int kMaximizeCueVerticalInsets = 8;

constexpr int kPhantomWindowCornerRadius = 12;
constexpr gfx::Insets kPhantomWindowInsets(8);

// The move up factor of starting y-position from the target position for
// entrance animation of maximize cue.
constexpr float kMaximizeCueEntraceAnimationYPositionMoveUpFactor = 0.5f;

// The duration of the maximize cue entrance animation.
constexpr base::TimeDelta kMaximizeCueEntranceAnimationDurationMs =
    base::Milliseconds(150);
// The duration of the maximize cue exit animation.
constexpr base::TimeDelta kMaximizeCueExitAnimationDurationMs =
    base::Milliseconds(100);
// The delay of the maximize cue entrance and exit animation.
constexpr base::TimeDelta kMaximizeCueAnimationDelayMs =
    base::Milliseconds(100);

constexpr char kShowPresentationHistogramName[] =
    "Ash.PhantomWindowController.Show.PresentationTime";

}  // namespace

// PhantomWindowController ----------------------------------------------------

PhantomWindowController::PhantomWindowController(aura::Window* window)
    : window_(window) {}

PhantomWindowController::~PhantomWindowController() = default;

void PhantomWindowController::Show(const gfx::Rect& window_bounds_in_screen) {
  gfx::Rect target_bounds_in_screen = window_bounds_in_screen;
  target_bounds_in_screen.Inset(kPhantomWindowInsets);
  if (target_bounds_in_screen == target_bounds_in_screen_)
    return;
  target_bounds_in_screen_ = target_bounds_in_screen;

  // Computes starting size with a shrinking ratio |kScrimStartBoundsRatio|
  // from the actual size |target_bounds_in_screen| used for entrace animation
  // in `ShowPhantomWidget()`.
  gfx::Rect start_bounds_in_screen = target_bounds_in_screen_;
  int start_width = std::max(kMinWidthWithShadow,
                             static_cast<int>(start_bounds_in_screen.width() *
                                              kScrimStartBoundsRatio));
  int start_height = std::max(kMinHeightWithShadow,
                              static_cast<int>(start_bounds_in_screen.height() *
                                               kScrimStartBoundsRatio));
  start_bounds_in_screen.Inset(gfx::Insets::VH(
      floor((start_bounds_in_screen.height() - start_height) / 2.0f),
      floor((start_bounds_in_screen.width() - start_width) / 2.0f)));

  aura::Window* root =
      window_util::GetRootWindowMatching(target_bounds_in_screen_);
  auto presentation_time_recorder = CreatePresentationTimeHistogramRecorder(
      root->layer()->GetCompositor(), kShowPresentationHistogramName);
  presentation_time_recorder->RequestNext();

  // Create a phantom widget with starting size so `ShowPhantomWidget()` can
  // animate from that current size to |target_bounds_in_screen|.
  phantom_widget_ = CreatePhantomWidget(root, start_bounds_in_screen);
  ShowPhantomWidget();
}

void PhantomWindowController::HideMaximizeCue() {
  DCHECK(maximize_cue_widget_);
  ui::Layer* widget_layer = maximize_cue_widget_->GetLayer();

  views::AnimationBuilder()
      .SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .Once()
      .SetDuration(kMaximizeCueAnimationDelayMs)
      .Then()
      .SetDuration(kMaximizeCueExitAnimationDurationMs)
      .SetOpacity(widget_layer, 0, gfx::Tween::LINEAR);
}

void PhantomWindowController::ShowMaximizeCue() {
  if (!maximize_cue_widget_) {
    maximize_cue_widget_ = CreateMaximizeCue(
        window_util::GetRootWindowMatching(target_bounds_in_screen_));
    maximize_cue_widget_->Show();
  }
  maximize_cue_widget_->SetOpacity(0.f);

  // Starts entrance animation with fade in and moving the cue from 50%
  // higher y position to the actual y position.
  const gfx::Rect target_bounds =
      maximize_cue_widget_->GetWindowBoundsInScreen();
  ui::Layer* widget_layer = maximize_cue_widget_->GetLayer();
  widget_layer->SetTransform(gfx::Transform::MakeTranslation(
      0,
      -target_bounds.y() * kMaximizeCueEntraceAnimationYPositionMoveUpFactor));

  views::AnimationBuilder()
      .SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .Once()
      .SetDuration(kMaximizeCueAnimationDelayMs)
      .Then()
      .SetDuration(kMaximizeCueEntranceAnimationDurationMs)
      .SetTransform(widget_layer, gfx::Transform(),
                    gfx::Tween::ACCEL_LIN_DECEL_100)
      .SetOpacity(widget_layer, 1.f, gfx::Tween::LINEAR);
}

void PhantomWindowController::TransformPhantomWidgetFromSnapTopToMaximize(
    const gfx::Rect& maximize_window_bounds_in_screen) {
  gfx::Rect target_bounds_in_screen = maximize_window_bounds_in_screen;
  target_bounds_in_screen.Inset(kPhantomWindowInsets);
  target_bounds_in_screen_ = target_bounds_in_screen;

  // Set the bounds of the widget to `target_bounds_in_screen_`, then apply a
  // transform so that the widget is in the same position as
  // `old_bounds_in_screen`. Then animate to the identity transform, the target
  // position will be `target_bounds_in_screen_`.
  const gfx::Rect old_bounds_in_screen =
      phantom_widget_->GetWindowBoundsInScreen();
  phantom_widget_->SetBounds(target_bounds_in_screen_);
  phantom_widget_->GetLayer()->SetTransform(gfx::TransformBetweenRects(
      gfx::RectF(target_bounds_in_screen_), gfx::RectF(old_bounds_in_screen)));

  views::AnimationBuilder()
      .SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .Once()
      .SetDuration(kScrimEntranceSizeAnimationDurationMs)
      .SetTransform(phantom_widget_->GetLayer(), gfx::Transform(),
                    gfx::Tween::ACCEL_20_DECEL_100);
}

gfx::Rect PhantomWindowController::GetTargetWindowBounds() const {
  gfx::Rect target_window_bounds = target_bounds_in_screen_;
  target_window_bounds.Inset(-kPhantomWindowInsets);
  return target_window_bounds;
}

views::Widget* PhantomWindowController::GetMaximizeCueForTesting() const {
  return maximize_cue_widget_.get();
}

const gfx::Rect& PhantomWindowController::GetTargetBoundsInScreenForTesting()
    const {
  return target_bounds_in_screen_;
}

std::unique_ptr<views::Widget> PhantomWindowController::CreatePhantomWidget(
    aura::Window* root_window,
    const gfx::Rect& bounds_in_screen) {
  auto phantom_widget = std::make_unique<views::Widget>();
  views::Widget::InitParams params(
      views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
      views::Widget::InitParams::TYPE_POPUP);
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.name = "PhantomWindow";
  params.shadow_type = views::Widget::InitParams::ShadowType::kNone;
  params.parent = root_window->GetChildById(kShellWindowId_ShelfContainer);

  phantom_widget->set_focus_on_creation(false);
  phantom_widget->Init(std::move(params));
  phantom_widget->SetVisibilityChangedAnimationsEnabled(false);
  aura::Window* phantom_widget_window = phantom_widget->GetNativeWindow();
  phantom_widget_window->SetId(kShellWindowId_PhantomWindow);
  phantom_widget->SetBounds(bounds_in_screen);
  // Ensure the phantom and its shadow do not cover the shelf.
  phantom_widget_window->parent()->StackChildAtBottom(phantom_widget_window);

  phantom_widget->SetContentsView(
      views::Builder<views::View>()
          .SetBackground(views::CreateThemedRoundedRectBackground(
              kColorAshPhantomWindowBackgroundColor,
              kPhantomWindowCornerRadius))
          .SetBorder(std::make_unique<views::HighlightBorder>(
              kPhantomWindowCornerRadius,
              views::HighlightBorder::Type::kHighlightBorderNoShadow))
          .Build());
  return phantom_widget;
}

std::unique_ptr<views::Widget> PhantomWindowController::CreateMaximizeCue(
    aura::Window* root_window) {
  DCHECK(phantom_widget_);

  auto maximize_cue_widget = std::make_unique<views::Widget>();
  views::Widget::InitParams params(
      views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
      views::Widget::InitParams::TYPE_POPUP);
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.name = "MaximizeCueWidget";
  params.shadow_type = views::Widget::InitParams::ShadowType::kNone;
  params.parent = root_window->GetChildById(kShellWindowId_OverlayContainer);

  maximize_cue_widget->set_focus_on_creation(false);
  maximize_cue_widget->Init(std::move(params));

  ui::Layer* layer = maximize_cue_widget->GetLayer();
  layer->SetBackgroundBlur(ColorProvider::kBackgroundBlurSigma);
  layer->SetBackdropFilterQuality(ColorProvider::kBackgroundBlurQuality);
  layer->SetRoundedCornerRadius(gfx::RoundedCornersF(kMaximizeCueHeight / 2.f));

  aura::Window* maximize_cue_widget_window =
      maximize_cue_widget->GetNativeWindow();
  maximize_cue_widget_window->parent()->StackChildAtTop(
      maximize_cue_widget_window);

  // The cue has one view and a child label.
  auto* color_provider = AshColorProvider::Get();
  views::View* maximize_cue_view = maximize_cue_widget->SetContentsView(
      views::Builder<views::BoxLayoutView>()
          .SetOrientation(views::BoxLayout::Orientation::kHorizontal)
          .SetInsideBorderInsets(gfx::Insets::VH(kMaximizeCueVerticalInsets,
                                                 kMaximizeCueHorizontalInsets))
          .SetBackground(views::CreateThemedRoundedRectBackground(
              kColorAshShieldAndBase20, kPhantomWindowCornerRadius))
          .SetBorder(std::make_unique<views::HighlightBorder>(
              kPhantomWindowCornerRadius,
              views::HighlightBorder::Type::kHighlightBorderNoShadow))
          .AddChildren(
              views::Builder<views::Label>()
                  .SetText(l10n_util::GetStringUTF16(
                      IDS_ASH_SPLIT_VIEW_HOLD_TO_MAXIMIZE))
                  .SetEnabledColor(color_provider->GetContentLayerColor(
                      AshColorProvider::ContentLayerType::kTextColorPrimary))
                  .SetAutoColorReadabilityEnabled(false)
                  .SetFontList(views::Label::GetDefaultFontList().Derive(
                      2, gfx::Font::FontStyle::NORMAL,
                      gfx::Font::Weight::NORMAL)))
          .Build());

  const display::Display& display =
      display::Screen::GetScreen()->GetDisplayNearestWindow(root_window);
  const gfx::Rect work_area = display.work_area();
  const int maximize_cue_width = maximize_cue_view->GetPreferredSize().width();
  const int maximize_cue_y = work_area.y() + kMaximizeCueTopMargin;
  const gfx::Rect phantom_bounds = phantom_widget_->GetWindowBoundsInScreen();
  const gfx::Rect maximize_cue_bounds(
      phantom_bounds.CenterPoint().x() - maximize_cue_width / 2, maximize_cue_y,
      maximize_cue_width, kMaximizeCueHeight);
  maximize_cue_widget->SetBounds(maximize_cue_bounds);
  return maximize_cue_widget;
}

void PhantomWindowController::ShowPhantomWidget() {
  phantom_widget_->Show();
  phantom_widget_->SetOpacity(0);
  ui::Layer* widget_layer = phantom_widget_->GetLayer();

  // Layer uses bounds in parent so convert from screen bounds.
  gfx::Rect bounds = target_bounds_in_screen_;
  wm::ConvertRectFromScreen(phantom_widget_->GetNativeWindow()->parent(),
                            &bounds);

  views::AnimationBuilder()
      .SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .Once()
      .SetDuration(kScrimEntranceSizeAnimationDurationMs)
      .SetBounds(widget_layer, bounds, gfx::Tween::ACCEL_20_DECEL_100)
      .At(base::Seconds(0))
      .SetDuration(kScrimEntranceOpacityAnimationDurationMs)
      .SetOpacity(widget_layer, 1, gfx::Tween::LINEAR);
}

}  // namespace ash
