// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/workspace/phantom_window_controller.h"

#include <math.h>

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/style/scoped_light_mode_as_default.h"
#include "ash/root_window_controller.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/default_color_constants.h"
#include "ash/style/default_colors.h"
#include "ash/wm/window_util.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/display/screen.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/shadow_controller.h"
#include "ui/wm/core/shadow_types.h"

namespace ash {
namespace {

// The duration of the show animation.
const int kAnimationDurationMs = 200;

// The size of the phantom window at the beginning of the show animation in
// relation to the size of the phantom window at the end of the animation.
const float kStartBoundsRatio = 0.85f;

// The elevation of the shadow for the phantom window should match that of an
// active window.
// The shadow ninebox requires a minimum size to work well. See
// ui/compositor_extra/shadow.cc
constexpr int kMinWidthWithShadow = 2 * ::wm::kShadowElevationActiveWindow;
constexpr int kMinHeightWithShadow = 4 * ::wm::kShadowElevationActiveWindow;

// The top margin of the cue from the top of work area.
constexpr int kMaximizeCueTopMargin = 32;
constexpr int kMaximizeCueHeight = 36;
constexpr int kMaximizeCueHorizontalInsets = 16;
constexpr int kMaximizeCueVerticalInsets = 8;
constexpr int kMaximizeCueBackgroundBlur = 80;

constexpr int kPhantomWindowCornerRadius = 4;
constexpr gfx::Insets kPhantomWindowInsets(8);

constexpr int kHighlightBorderThickness = 1;

// The move down factor of y-position for entrance animation of maximize cue.
constexpr float kMaximizeCueEntraceAnimationYPositionMoveDownFactor = 1.5f;

// The duration of the maximize cue entrance animation.
constexpr base::TimeDelta kMaximizeCueEntranceAnimationDurationMs =
    base::Milliseconds(200);
// The duration of the maximize cue exit animation.
constexpr base::TimeDelta kMaximizeCueExitAnimationDurationMs =
    base::Milliseconds(100);
// The delay of the maximize cue entrance and exit animation.
constexpr base::TimeDelta kMaximizeCueAnimationDelayMs =
    base::Milliseconds(100);

// A rounded rectangle border that has inner (highlight) and outer color.
class HighlightBorder : public views::Border {
 public:
  explicit HighlightBorder(int corner_radius);

  HighlightBorder(const HighlightBorder&) = delete;
  HighlightBorder& operator=(const HighlightBorder&) = delete;

  // views::Border:
  void Paint(const views::View& view, gfx::Canvas* canvas) override;
  gfx::Insets GetInsets() const override;
  gfx::Size GetMinimumSize() const override;

 private:
  const int corner_radius_;
  const SkColor inner_color_;
  const SkColor outer_color_;
};

// TODO(crbug/1224694): Remove |inner_color_| and |outer_color| and call
// `GetControlsLayerColor()` for colors directly in `Paint()` once we
// officially launch Dark/Light mode and no longer use default mode.
HighlightBorder::HighlightBorder(int corner_radius)
    : corner_radius_(corner_radius),
      inner_color_(AshColorProvider::Get()->GetControlsLayerColor(
          AshColorProvider::ControlsLayerType::kHighlightBorderHighlightColor)),
      outer_color_(AshColorProvider::Get()->GetControlsLayerColor(
          AshColorProvider::ControlsLayerType::kHighlightBorderBorderColor)) {}

void HighlightBorder::Paint(const views::View& view, gfx::Canvas* canvas) {
  cc::PaintFlags flags;
  flags.setStrokeWidth(kHighlightBorderThickness);
  flags.setColor(outer_color_);
  flags.setStyle(cc::PaintFlags::kStroke_Style);
  flags.setAntiAlias(true);

  const float half_thickness = kHighlightBorderThickness / 2.0f;
  gfx::RectF outer_border_bounds(view.GetLocalBounds());
  outer_border_bounds.Inset(half_thickness, half_thickness);
  canvas->DrawRoundRect(outer_border_bounds, corner_radius_, flags);

  gfx::RectF inner_border_bounds(view.GetLocalBounds());
  inner_border_bounds.Inset(gfx::Insets(kHighlightBorderThickness));
  inner_border_bounds.Inset(half_thickness, half_thickness);
  flags.setColor(inner_color_);
  canvas->DrawRoundRect(inner_border_bounds, corner_radius_, flags);
}

gfx::Insets HighlightBorder::GetInsets() const {
  return gfx::Insets(2 * kHighlightBorderThickness);
}

gfx::Size HighlightBorder::GetMinimumSize() const {
  return gfx::Size(kHighlightBorderThickness * 4,
                   kHighlightBorderThickness * 4);
}

}  // namespace

// PhantomWindowController ----------------------------------------------------

PhantomWindowController::PhantomWindowController(aura::Window* window)
    : window_(window) {}

PhantomWindowController::~PhantomWindowController() = default;

void PhantomWindowController::Show(const gfx::Rect& bounds_in_screen) {
  if (bounds_in_screen == target_bounds_in_screen_)
    return;
  target_bounds_in_screen_ = bounds_in_screen;

  gfx::Rect start_bounds_in_screen = target_bounds_in_screen_;
  int start_width = std::max(
      kMinWidthWithShadow,
      static_cast<int>(start_bounds_in_screen.width() * kStartBoundsRatio));
  int start_height = std::max(
      kMinHeightWithShadow,
      static_cast<int>(start_bounds_in_screen.height() * kStartBoundsRatio));
  start_bounds_in_screen.Inset(
      floor((start_bounds_in_screen.width() - start_width) / 2.0f),
      floor((start_bounds_in_screen.height() - start_height) / 2.0f));
  phantom_widget_ = CreatePhantomWidget(
      window_util::GetRootWindowMatching(target_bounds_in_screen_),
      start_bounds_in_screen);
}

void PhantomWindowController::HideMaximizeCue() {
  // `WorkspaceWindowResizer::ShowMaximizePhantom()` calls this function once
  // the dwell timer completes, but maximize phantom shown for landscape
  // display does not have |maximize_cue_widget_|.
  if (!maximize_cue_widget_)
    return;
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
  maximize_cue_widget_->SetOpacity(0);

  // Starts entrance animation with fade in and moving the cue from 50%
  // lower y position to the actual y position.
  const gfx::Rect target_bounds =
      maximize_cue_widget_->GetNativeView()->bounds();
  const gfx::Rect starting_bounds = gfx::Rect(
      target_bounds.x(),
      target_bounds.y() * kMaximizeCueEntraceAnimationYPositionMoveDownFactor,
      target_bounds.width(), target_bounds.height());
  maximize_cue_widget_->SetBounds(starting_bounds);

  ui::Layer* widget_layer = maximize_cue_widget_->GetLayer();

  views::AnimationBuilder()
      .SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .Once()
      .SetDuration(kMaximizeCueAnimationDelayMs)
      .Then()
      .SetDuration(kMaximizeCueEntranceAnimationDurationMs)
      .SetBounds(widget_layer, target_bounds, gfx::Tween::ACCEL_LIN_DECEL_100)
      .SetOpacity(widget_layer, 1, gfx::Tween::LINEAR);
}

std::unique_ptr<views::Widget> PhantomWindowController::CreatePhantomWidget(
    aura::Window* root_window,
    const gfx::Rect& bounds_in_screen) {
  std::unique_ptr<views::Widget> phantom_widget(new views::Widget);
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_POPUP);
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  // PhantomWindowController is used by FrameMaximizeButton to highlight the
  // launcher button. Put the phantom in the same window as the launcher so that
  // the phantom is visible.
  params.z_order = ui::ZOrderLevel::kFloatingUIElement;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.name = "PhantomWindow";
  params.layer_type = ui::LAYER_SOLID_COLOR;
  params.shadow_type = views::Widget::InitParams::ShadowType::kNone;
  params.parent = root_window->GetChildById(kShellWindowId_ShelfContainer);
  phantom_widget->set_focus_on_creation(false);
  phantom_widget->Init(std::move(params));
  phantom_widget->SetVisibilityChangedAnimationsEnabled(false);
  aura::Window* phantom_widget_window = phantom_widget->GetNativeWindow();
  phantom_widget_window->SetId(kShellWindowId_PhantomWindow);
  phantom_widget->SetBounds(bounds_in_screen);
  // TODO(sky): I suspect this is never true, verify that.
  if (phantom_widget_window->parent() == window_->parent()) {
    phantom_widget_window->parent()->StackChildAbove(phantom_widget_window,
                                                     window_);
  } else {
    // Ensure the phantom and its shadow do not cover the shelf.
    phantom_widget_window->parent()->StackChildAtBottom(phantom_widget_window);
  }
  ui::Layer* widget_layer = phantom_widget_window->layer();

  views::View* phantom_view =
      phantom_widget->SetContentsView(std::make_unique<views::View>());
  phantom_view->SetPaintToLayer();
  phantom_view->layer()->SetFillsBoundsOpaquely(false);
  phantom_view->SetBackground(views::CreateRoundedRectBackground(
      DeprecatedGetShieldLayerColor(
          AshColorProvider::ShieldLayerType::kShield20,
          kSplitviewPhantomWindowColor),
      kPhantomWindowCornerRadius));

  ScopedLightModeAsDefault scoped_light_mode_as_default;
  phantom_view->SetBorder(
      std::make_unique<HighlightBorder>(kPhantomWindowCornerRadius));
  phantom_widget->Show();

  // Fade the window in.
  widget_layer->SetOpacity(0);
  ui::ScopedLayerAnimationSettings scoped_setter(widget_layer->GetAnimator());
  scoped_setter.SetTransitionDuration(base::Milliseconds(kAnimationDurationMs));
  scoped_setter.SetTweenType(gfx::Tween::EASE_IN);
  scoped_setter.SetPreemptionStrategy(
      ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
  widget_layer->SetOpacity(1);
  gfx::Rect phantom_widget_bounds = target_bounds_in_screen_;
  phantom_widget_bounds.Inset(kPhantomWindowInsets);
  phantom_widget->SetBounds(phantom_widget_bounds);

  return phantom_widget;
}

std::unique_ptr<views::Widget> PhantomWindowController::CreateMaximizeCue(
    aura::Window* root_window) {
  DCHECK(phantom_widget_);

  std::unique_ptr<views::Widget> maximize_cue_widget(new views::Widget);
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_POPUP);
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  // Put the maximize cue in the same window as the floating UI.
  params.z_order = ui::ZOrderLevel::kFloatingUIElement;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.name = "MaximizeCueWidget";
  params.shadow_type = views::Widget::InitParams::ShadowType::kNone;
  params.parent = root_window->GetChildById(kShellWindowId_OverlayContainer);
  maximize_cue_widget->set_focus_on_creation(false);
  maximize_cue_widget->Init(std::move(params));

  aura::Window* maximize_cue_widget_window =
      maximize_cue_widget->GetNativeWindow();
  maximize_cue_widget_window->parent()->StackChildAtTop(
      maximize_cue_widget_window);

  views::View* maximize_cue =
      maximize_cue_widget->SetContentsView(std::make_unique<views::View>());
  maximize_cue->SetPaintToLayer();
  maximize_cue->layer()->SetFillsBoundsOpaquely(false);

  auto* color_provider = AshColorProvider::Get();
  maximize_cue->SetBackground(views::CreateRoundedRectBackground(
      color_provider->GetBaseLayerColor(
          AshColorProvider::BaseLayerType::kOpaque),
      kMaximizeCueHeight / 2));
  maximize_cue->layer()->SetBackgroundBlur(
      static_cast<float>(kMaximizeCueBackgroundBlur));
  const gfx::RoundedCornersF radii(kMaximizeCueHeight / 2);
  maximize_cue->layer()->SetRoundedCornerRadius(radii);
  maximize_cue->SetBorder(
      std::make_unique<HighlightBorder>(kMaximizeCueHeight / 2));

  // Set layout of cue view and add a label to the view.
  maximize_cue->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal,
      gfx::Insets(kMaximizeCueVerticalInsets, kMaximizeCueHorizontalInsets)));
  views::Label* maximize_cue_label =
      maximize_cue->AddChildView(std::make_unique<views::Label>(
          l10n_util::GetStringUTF16(IDS_ASH_SPLIT_VIEW_HOLD_TO_MAXIMIZE)));
  maximize_cue_label->SetPaintToLayer();
  maximize_cue_label->layer()->SetFillsBoundsOpaquely(false);
  maximize_cue_label->SetEnabledColor(color_provider->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kTextColorPrimary));
  maximize_cue_label->SetVisible(true);

  maximize_cue_label->SetAutoColorReadabilityEnabled(false);
  maximize_cue_label->SetFontList(views::Label::GetDefaultFontList().Derive(
      2, gfx::Font::FontStyle::NORMAL, gfx::Font::Weight::NORMAL));

  const display::Display& display =
      display::Screen::GetScreen()->GetDisplayNearestWindow(root_window);
  const gfx::Rect work_area = display.work_area();
  const int maximize_cue_width = maximize_cue->GetPreferredSize().width();
  const int maximize_cue_y = work_area.y() + kMaximizeCueTopMargin;
  const gfx::Rect phantom_bounds = phantom_widget_->GetNativeView()->bounds();
  const gfx::Rect maximize_cue_bounds =
      gfx::Rect(phantom_bounds.CenterPoint().x() - maximize_cue_width / 2,
                maximize_cue_y, maximize_cue_width, kMaximizeCueHeight);

  maximize_cue_widget->SetBounds(maximize_cue_bounds);
  return maximize_cue_widget;
}

}  // namespace ash
