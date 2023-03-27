// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/touch/touch_selection_magnifier_runner_ash.h"

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/style/color_util.h"
#include "third_party/skia/include/core/SkDrawLooper.h"
#include "ui/aura/window.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/color/color_provider_source_observer.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/shadow_value.h"
#include "ui/gfx/skia_paint_util.h"

namespace ash {

namespace {

constexpr int kMagnifierRadius = 20;

constexpr int kMagnifierBorderThickness = 1;

const gfx::ShadowValues kMagnifierShadowValues =
    gfx::ShadowValue::MakeChromeOSSystemUIShadowValues(3);

// The space outside the zoom layer needed for shadows.
const gfx::Outsets kMagnifierShadowOutsets =
    gfx::ShadowValue::GetMargin(kMagnifierShadowValues).ToOutsets();

// Bounds of the zoom layer in coordinates of its parent. These zoom layer
// bounds are fixed since we only update the bounds of the parent magnifier
// layer when the magnifier moves.
const gfx::Rect kZoomLayerBounds =
    gfx::Rect(kMagnifierShadowOutsets.left(),
              kMagnifierShadowOutsets.top(),
              TouchSelectionMagnifierRunnerAsh::kMagnifierSize.width(),
              TouchSelectionMagnifierRunnerAsh::kMagnifierSize.height());

// Size of the border layer, which includes space for the zoom layer and
// surrounding border and shadows.
const gfx::Size kBorderLayerSize =
    TouchSelectionMagnifierRunnerAsh::kMagnifierSize +
    kMagnifierShadowOutsets.size();

// Duration of the animation when updating magnifier bounds.
constexpr base::TimeDelta kMagnifierTransitionDuration = base::Milliseconds(50);

// Gets the bounds of the magnifier layer for showing the specified point of
// interest. These bounds include the magnifier border and shadows.
// `point_of_interest` and returned bounds are in coordinates of the magnifier's
// parent container.
gfx::Rect GetMagnifierLayerBounds(const gfx::Point& point_of_interest) {
  const gfx::Size& size = TouchSelectionMagnifierRunnerAsh::kMagnifierSize;
  const gfx::Point origin(
      point_of_interest.x() - size.width() / 2,
      point_of_interest.y() - size.height() / 2 +
          TouchSelectionMagnifierRunnerAsh::kMagnifierVerticalOffset);
  gfx::Rect magnifier_layer_bounds(origin, size);
  magnifier_layer_bounds.Outset(kMagnifierShadowOutsets);
  return magnifier_layer_bounds;
}

// Gets the border color using `color_provider_source`. Defaults to black if
// `color_provider_source` is nullptr.
SkColor GetBorderColor(const ui::ColorProviderSource* color_provider_source) {
  return color_provider_source
             ? color_provider_source->GetColorProvider()->GetColor(
                   cros_tokens::kCrosSysSeparator)
             : SkColorSetARGB(51, 0, 0, 0);
}

// Returns the child container in `root` that should parent the magnifier layer.
aura::Window* GetMagnifierParentContainerForRoot(aura::Window* root) {
  return root->GetChildById(kShellWindowId_ImeWindowParentContainer);
}

}  // namespace

// Delegate for drawing the magnifier border and shadows onto the border layer.
class TouchSelectionMagnifierRunnerAsh::BorderRenderer
    : public ui::LayerDelegate {
 public:
  explicit BorderRenderer(SkColor border_color) : border_color_(border_color) {}

  BorderRenderer(const BorderRenderer&) = delete;
  BorderRenderer& operator=(const BorderRenderer&) = delete;
  ~BorderRenderer() override = default;

  void set_border_color(SkColor border_color) { border_color_ = border_color; }

  // ui::LayerDelegate:
  void OnPaintLayer(const ui::PaintContext& context) override {
    ui::PaintRecorder recorder(context, kBorderLayerSize);

    // Draw shadows onto the border layer. These shadows should surround the
    // magnified area, so we draw them around the zoom layer bounds.
    cc::PaintFlags shadow_flags;
    shadow_flags.setAntiAlias(true);
    shadow_flags.setColor(SK_ColorTRANSPARENT);
    shadow_flags.setLooper(gfx::CreateShadowDrawLooper(kMagnifierShadowValues));
    recorder.canvas()->DrawRoundRect(kZoomLayerBounds, kMagnifierRadius,
                                     shadow_flags);

    // Since the border layer is stacked above the zoom layer (to prevent the
    // magnifier border and shadows from being magnified), we now need to clear
    // the parts of the shadow covering the zoom layer.
    cc::PaintFlags mask_flags;
    mask_flags.setAntiAlias(true);
    mask_flags.setBlendMode(SkBlendMode::kClear);
    mask_flags.setStyle(cc::PaintFlags::kFill_Style);
    recorder.canvas()->DrawRoundRect(kZoomLayerBounds, kMagnifierRadius,
                                     mask_flags);

    // Draw the magnifier border onto the border layer, using the zoom layer
    // bounds so that the border surrounds the magnified area.
    cc::PaintFlags border_flags;
    border_flags.setAntiAlias(true);
    border_flags.setStyle(cc::PaintFlags::kStroke_Style);
    border_flags.setStrokeWidth(kMagnifierBorderThickness);
    border_flags.setColor(border_color_);
    recorder.canvas()->DrawRoundRect(kZoomLayerBounds, kMagnifierRadius,
                                     border_flags);
  }

  void OnDeviceScaleFactorChanged(float old_device_scale_factor,
                                  float new_device_scale_factor) override {}

 private:
  SkColor border_color_;
};

TouchSelectionMagnifierRunnerAsh::TouchSelectionMagnifierRunnerAsh() = default;

TouchSelectionMagnifierRunnerAsh::~TouchSelectionMagnifierRunnerAsh() = default;

void TouchSelectionMagnifierRunnerAsh::ShowMagnifier(
    aura::Window* context,
    const gfx::PointF& position) {
  DCHECK(context);
  DCHECK(!current_context_ || current_context_ == context);
  if (!current_context_) {
    current_context_ = context;
  }

  aura::Window* root_window = current_context_->GetRootWindow();
  DCHECK(root_window);
  aura::Window* parent_container =
      GetMagnifierParentContainerForRoot(root_window);
  gfx::PointF position_in_parent(position);
  aura::Window::ConvertPointToTarget(context, parent_container,
                                     &position_in_parent);

  if (!magnifier_layer_) {
    CreateMagnifierLayer(parent_container, position_in_parent);
  } else {
    ui::ScopedLayerAnimationSettings settings(magnifier_layer_->GetAnimator());
    settings.SetTransitionDuration(kMagnifierTransitionDuration);
    settings.SetTweenType(gfx::Tween::LINEAR);
    settings.SetPreemptionStrategy(
        ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
    magnifier_layer_->SetBounds(
        GetMagnifierLayerBounds(gfx::ToRoundedPoint(position_in_parent)));
  }
}

void TouchSelectionMagnifierRunnerAsh::CloseMagnifier() {
  current_context_ = nullptr;
  magnifier_layer_ = nullptr;
  zoom_layer_ = nullptr;
  border_layer_ = nullptr;
  border_renderer_ = nullptr;
  Observe(nullptr);
}

bool TouchSelectionMagnifierRunnerAsh::IsRunning() const {
  return current_context_ != nullptr;
}

void TouchSelectionMagnifierRunnerAsh::OnColorProviderChanged() {
  if (border_renderer_) {
    DCHECK(border_layer_);
    border_renderer_->set_border_color(
        GetBorderColor(GetColorProviderSource()));
    border_layer_->SchedulePaint(gfx::Rect(border_layer_->size()));
  }
}

const aura::Window*
TouchSelectionMagnifierRunnerAsh::GetCurrentContextForTesting() const {
  return current_context_;
}

const ui::Layer* TouchSelectionMagnifierRunnerAsh::GetMagnifierLayerForTesting()
    const {
  return magnifier_layer_.get();
}

const ui::Layer* TouchSelectionMagnifierRunnerAsh::GetZoomLayerForTesting()
    const {
  return zoom_layer_.get();
}

void TouchSelectionMagnifierRunnerAsh::CreateMagnifierLayer(
    aura::Window* parent_container,
    const gfx::PointF& position_in_parent) {
  Observe(ColorUtil::GetColorProviderSourceForWindow(parent_container));
  ui::Layer* parent_layer = parent_container->layer();

  // Create the magnifier layer, which will parent the zoom layer and border
  // layer.
  magnifier_layer_ = std::make_unique<ui::Layer>(ui::LAYER_NOT_DRAWN);
  magnifier_layer_->SetBounds(
      GetMagnifierLayerBounds(gfx::ToRoundedPoint(position_in_parent)));
  magnifier_layer_->SetFillsBoundsOpaquely(false);
  parent_layer->Add(magnifier_layer_.get());

  // Create the zoom layer, which will show the magnified area.
  zoom_layer_ = std::make_unique<ui::Layer>(ui::LAYER_SOLID_COLOR);
  zoom_layer_->SetBounds(kZoomLayerBounds);
  zoom_layer_->SetBackgroundZoom(kMagnifierScale, 0);
  zoom_layer_->SetBackgroundOffset(gfx::Point(0, kMagnifierVerticalOffset));
  zoom_layer_->SetFillsBoundsOpaquely(false);
  zoom_layer_->SetRoundedCornerRadius(gfx::RoundedCornersF{kMagnifierRadius});
  magnifier_layer_->Add(zoom_layer_.get());

  // Create the border layer. This is stacked above the zoom layer so that the
  // magnifier border and shadows aren't shown in the magnified area drawn by
  // the zoom layer.
  border_layer_ = std::make_unique<ui::Layer>();
  border_layer_->SetBounds(gfx::Rect(kBorderLayerSize));
  border_renderer_ = std::make_unique<BorderRenderer>(
      GetBorderColor(GetColorProviderSource()));
  border_layer_->set_delegate(border_renderer_.get());
  border_layer_->SetFillsBoundsOpaquely(false);
  magnifier_layer_->Add(border_layer_.get());
}

}  // namespace ash
