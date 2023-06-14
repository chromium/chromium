// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/touch/touch_selection_magnifier_runner_ash.h"

#include "third_party/skia/include/core/SkDrawLooper.h"
#include "ui/aura/window.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_provider_manager.h"
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
#include "ui/native_theme/native_theme.h"

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

// Gets the bounds of the content that will be magnified, relative to the parent
// (`parent_bounds` should be the parent's bounds in its own coordinate space,
// e.g. {0,0,w,h}). The magnified bounds will be in the same coordinate space as
// `parent_bounds` and are adjusted to be contained within them.
gfx::Rect GetMagnifiedBounds(const gfx::Rect& parent_bounds,
                             const gfx::Point& focus_center) {
  gfx::SizeF magnified_size(kMagnifierSize.width() / kMagnifierScale,
                            kMagnifierSize.height() / kMagnifierScale);
  gfx::PointF origin(focus_center.x() - magnified_size.width() / 2,
                     focus_center.y() - magnified_size.height() / 2);

  gfx::RectF magnified_bounds(origin, magnified_size);
  magnified_bounds.AdjustToFit(gfx::RectF(parent_bounds));

  // Transform the adjusted magnified_bounds to the layer's scale. It's okay if
  // these bounds go outside the container, since they will be offset and then
  // fit to the parent.
  magnified_size = {kMagnifierScale * magnified_bounds.width(),
                    kMagnifierScale * magnified_bounds.height()};
  origin = {magnified_bounds.CenterPoint().x() - magnified_size.width() / 2,
            magnified_bounds.CenterPoint().y() - magnified_size.height() / 2};
  return gfx::ToEnclosingRect(gfx::RectF(origin, magnified_size));
}

std::pair<gfx::Rect, gfx::Point> GetMagnifierLayerBoundsAndOffset(
    const gfx::Size& parent_size,
    const gfx::Rect& focus_rect) {
  // The parent-relative bounding box of the parent container, which is the
  // coordinate space that the magnifier layer's bounds need to be in.
  const gfx::Rect parent_bounds(gfx::Point(0, 0), parent_size);
  // `magnified_bounds` holds the bounds of the content that will be magnified,
  // but that contains the `focus_center`, making it so the user's finger blocks
  // it if the final magnified content were shown in place.
  gfx::Rect magnified_bounds =
      GetMagnifiedBounds(parent_bounds, focus_rect.CenterPoint());
  // To avoid being blocked, offset the bounds (and the background so it
  // remains visually consistent) along the Y axis. This must be clamped to
  // `parent_bounds` so that it's not drawn off the top edge of the screen.
  gfx::Rect layer_bounds = magnified_bounds;
  layer_bounds.Offset(0, kMagnifierVerticalBoundsOffset -
                             magnified_bounds.height() / 2 -
                             focus_rect.height() / 2);

  layer_bounds.Outset(kMagnifierShadowOutsets);
  layer_bounds.AdjustToFit(parent_bounds);

  // `zoom_layer_center` is the center of the zoom layer relative to the
  // magnifier layer's parent. Since the magnifier layer has non-uniform outsets
  // for the shadows, its center (layer_bounds.CenterPoint()) is not exactly
  // the same as the center of the zoom layer.
  gfx::Point zoom_layer_center =
      kZoomLayerBounds.CenterPoint() + layer_bounds.OffsetFromOrigin();
  gfx::Point offset = gfx::PointAtOffsetFromOrigin(
      zoom_layer_center - magnified_bounds.CenterPoint());
  return {layer_bounds, offset};
}

// Gets the color to use for the border based on the default native theme.
SkColor GetBorderColor() {
  auto* native_theme = ui::NativeTheme::GetInstanceForNativeUi();
  return ui::ColorProviderManager::Get()
      .GetColorProviderFor(native_theme->GetColorProviderKey(nullptr))
      ->GetColor(cros_tokens::kCrosSysSeparator);
}

}  // namespace

// Delegate for drawing the magnifier border and shadows onto the border layer.
class TouchSelectionMagnifierRunnerAsh::BorderRenderer
    : public ui::LayerDelegate {
 public:
  BorderRenderer() = default;
  BorderRenderer(const BorderRenderer&) = delete;
  BorderRenderer& operator=(const BorderRenderer&) = delete;
  ~BorderRenderer() override = default;

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
    border_flags.setColor(GetBorderColor());
    recorder.canvas()->DrawRoundRect(kZoomLayerBounds, kMagnifierRadius,
                                     border_flags);
  }

  void OnDeviceScaleFactorChanged(float old_device_scale_factor,
                                  float new_device_scale_factor) override {}
};

TouchSelectionMagnifierRunnerAsh::TouchSelectionMagnifierRunnerAsh(
    int parent_container_id)
    : parent_container_id_(parent_container_id) {}

TouchSelectionMagnifierRunnerAsh::~TouchSelectionMagnifierRunnerAsh() = default;

void TouchSelectionMagnifierRunnerAsh::ShowMagnifier(
    aura::Window* context,
    const gfx::SelectionBound& focus_bound) {
  DCHECK(context);
  DCHECK(!current_context_ || current_context_ == context);
  if (!current_context_) {
    current_context_ = context;
  }

  bool created_new_magnifier_layer = false;
  if (!magnifier_layer_) {
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
  aura::Window* parent_container = GetParentContainer();
  aura::Window::ConvertRectToTarget(context, parent_container, &focus_rect);

  auto [magnifier_layer_bounds, background_offset] =
      GetMagnifierLayerBoundsAndOffset(parent_container->bounds().size(),
                                       focus_rect);

  zoom_layer_->SetBackgroundOffset(background_offset);
  magnifier_layer_->SetBounds(magnifier_layer_bounds);

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
}

bool TouchSelectionMagnifierRunnerAsh::IsRunning() const {
  return current_context_ != nullptr;
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
  border_renderer_ = std::make_unique<BorderRenderer>();
  border_layer_->set_delegate(border_renderer_.get());
  border_layer_->SetFillsBoundsOpaquely(false);
  magnifier_layer_->Add(border_layer_.get());
}

aura::Window* TouchSelectionMagnifierRunnerAsh::GetParentContainer() const {
  DCHECK(current_context_);
  aura::Window* root_window = current_context_->GetRootWindow();
  DCHECK(root_window);
  aura::Window* parent_container =
      root_window->GetChildById(parent_container_id_);
  DCHECK(parent_container);
  return parent_container;
}

}  // namespace ash
