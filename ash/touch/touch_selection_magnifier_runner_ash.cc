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
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/selection_bound.h"
#include "ui/gfx/shadow_value.h"
#include "ui/gfx/skia_paint_util.h"

namespace ash {

namespace {

constexpr float kMagnifierScale = 1.25f;

constexpr int kMagnifierRadius = 20;

// Size of the magnified area, which excludes border and shadows.
constexpr gfx::Size kMagnifierSize{100, 40};

// Offset to apply to the magnifier bounds so that the magnifier is shown
// vertically above the caret (or selection endpoint). The offset specifies
// vertical displacement from the the top of the caret to the bottom of the
// magnified area. Note that it is negative since the bottom of the magnified
// area should be above the top of the caret.
constexpr int kMagnifierVerticalBoundsOffset = -8;

constexpr int kMagnifierBorderThickness = 1;

const gfx::ShadowValues kMagnifierShadowValues =
    gfx::ShadowValue::MakeChromeOSSystemUIShadowValues(3);

// The space outside the zoom layer needed for shadows.
const gfx::Outsets kMagnifierShadowOutsets =
    gfx::ShadowValue::GetMargin(kMagnifierShadowValues).ToOutsets();

// Bounds of the zoom layer in coordinates of its parent. These zoom layer
// bounds are fixed since we only update the bounds of the parent magnifier
// layer when the magnifier moves.
const gfx::Rect kZoomLayerBounds = gfx::Rect(kMagnifierShadowOutsets.left(),
                                             kMagnifierShadowOutsets.top(),
                                             kMagnifierSize.width(),
                                             kMagnifierSize.height());

// Size of the border layer, which includes space for the zoom layer and
// surrounding border and shadows.
const gfx::Size kBorderLayerSize =
    kMagnifierSize + kMagnifierShadowOutsets.size();

// Duration of the animation when updating magnifier bounds.
constexpr base::TimeDelta kMagnifierTransitionDuration = base::Milliseconds(50);

// Gets the bounds of the magnifier layer given an anchor point. The magnifier
// layer bounds should be horizontally centered above the anchor point (except
// possibly at the edges of the parent container) and include the magnifier
// border and shadows. `magnifier_anchor_point` and returned bounds are in
// coordinates of the magnifier's parent container.
gfx::Rect GetMagnifierLayerBounds(const gfx::Size& parent_container_size,
                                  const gfx::Point& magnifier_anchor_point) {
  const gfx::Point origin(
      magnifier_anchor_point.x() - kMagnifierSize.width() / 2,
      magnifier_anchor_point.y() - kMagnifierSize.height() +
          kMagnifierVerticalBoundsOffset);
  gfx::Rect magnifier_layer_bounds(origin, kMagnifierSize);
  magnifier_layer_bounds.Outset(kMagnifierShadowOutsets);
  // Adjust the magnifier layer to be completely within the parent container
  // while keeping the magnifier size fixed.
  magnifier_layer_bounds.AdjustToFit(gfx::Rect(parent_container_size));
  return magnifier_layer_bounds;
}

// Gets the zoom layer background offset needed to center `focus_center` in the
// magnified area. `magnifier_layer_bounds` and `focus_center` are in
// coordinates of the magnifier's parent container.
// TODO(b/275014115): Currently the magnifier doesn't show the very edge of the
// screen. Figure out correct background offset to fix this while keeping the
// magnified area completely inside the parent container.
gfx::Point GetZoomLayerBackgroundOffset(const gfx::Rect& magnifier_layer_bounds,
                                        const gfx::Point& focus_center) {
  return gfx::Point(0, magnifier_layer_bounds.y() +
                           kZoomLayerBounds.CenterPoint().y() -
                           focus_center.y());
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
    const gfx::SelectionBound& focus_bound) {
  DCHECK(context);
  DCHECK(!current_context_ || current_context_ == context);
  if (!current_context_) {
    current_context_ = context;
  }

  aura::Window* root_window = current_context_->GetRootWindow();
  DCHECK(root_window);
  aura::Window* parent_container =
      GetMagnifierParentContainerForRoot(root_window);

  bool created_new_magnifier_layer = false;
  if (!magnifier_layer_) {
    Observe(ColorUtil::GetColorProviderSourceForWindow(parent_container));
    // Create the magnifier layer, but don't add it to the parent container yet.
    // We will add it to the parent container after setting its bounds, so that
    // the magnifier doesn't appear initially in the wrong spot.
    CreateMagnifierLayer();
    created_new_magnifier_layer = true;
  }

  // Set up the animation for updating the magnifier bounds.
  ui::ScopedLayerAnimationSettings settings(magnifier_layer_->GetAnimator());
  if (created_new_magnifier_layer) {
    // Set the magnifier to appear immediately once its bounds are set.
    settings.SetTransitionDuration(base::Milliseconds(0));
    settings.SetTweenType(gfx::Tween::ZERO);
    settings.SetPreemptionStrategy(
        ui::LayerAnimator::IMMEDIATELY_SET_NEW_TARGET);
  } else {
    // Set the magnifier to move smoothly from its current bounds to the updated
    // bounds.
    settings.SetTransitionDuration(kMagnifierTransitionDuration);
    settings.SetTweenType(gfx::Tween::LINEAR);
    settings.SetPreemptionStrategy(
        ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
  }

  // Update magnifier bounds and background offset.
  gfx::Rect focus_rect = gfx::ToRoundedRect(
      gfx::BoundingRect(focus_bound.edge_start(), focus_bound.edge_end()));
  aura::Window::ConvertRectToTarget(context, parent_container, &focus_rect);
  const gfx::Rect magnifier_layer_bounds = GetMagnifierLayerBounds(
      parent_container->bounds().size(), focus_rect.top_center());
  magnifier_layer_->SetBounds(magnifier_layer_bounds);
  zoom_layer_->SetBackgroundOffset(GetZoomLayerBackgroundOffset(
      magnifier_layer_bounds, focus_rect.CenterPoint()));

  // Add magnifier layer to parent container if needed.
  if (created_new_magnifier_layer) {
    parent_container->layer()->Add(magnifier_layer_.get());
  } else {
    DCHECK_EQ(magnifier_layer_->parent(), parent_container->layer());
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

void TouchSelectionMagnifierRunnerAsh::CreateMagnifierLayer() {
  // Create the magnifier layer, which will parent the zoom layer and border
  // layer.
  magnifier_layer_ = std::make_unique<ui::Layer>(ui::LAYER_NOT_DRAWN);
  magnifier_layer_->SetFillsBoundsOpaquely(false);

  // Create the zoom layer, which will show the magnified area.
  zoom_layer_ = std::make_unique<ui::Layer>(ui::LAYER_SOLID_COLOR);
  zoom_layer_->SetBounds(kZoomLayerBounds);
  zoom_layer_->SetBackgroundZoom(kMagnifierScale, 0);
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
