// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/ui/arrow_container.h"

#include <cmath>

#include "ash/style/system_shadow.h"
#include "cc/paint/paint_flags.h"
#include "chrome/browser/ash/arc/input_overlay/constants.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/color/color_provider.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_delegate.h"
#include "ui/compositor/layer_type.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/skia_paint_util.h"
#include "ui/views/border.h"
#include "ui/views/highlight_border.h"
#include "ui/views/view_observer.h"
#include "ui/views/view_utils.h"

namespace arc::input_overlay {
namespace {

constexpr SkScalar kTriangleLength = 20.0f;
constexpr SkScalar kTriangleHeight = 14.0f;
// The straight distance from triangle rounded corner start to end.
constexpr SkScalar kTriangleRoundDistance = 4.0f;
constexpr SkScalar kCornerRadius = 16.0f;
constexpr SkScalar kHighlightBorderThickness = 2.0f;
constexpr SkScalar kHalfHightlightBorderThickness =
    kHighlightBorderThickness / 2.0f;

// Whole menu width with arrow.
constexpr int kMenuWidth = kButtonOptionsMenuWidth + kTriangleHeight;

// Draws the dialog shape path with round corner. It starts after the corner
// radius on line #0 and draws clockwise.
//
// `draw_triangle_on_left` draws the triangle wedge on the left side of the box
// instead of the right if set to true.
//
// `action_offset` draws the triangle wedge higher or lower if the position of
// the action is too close to the top or bottom of the window. An offset of
// zero draws the triangle wedge at the vertical center of the box.
//
// `origin_offset` of 0 means drawing the top and left edge on the x-axis and
// y-axis. Otherwise, draw the top and left edge on the y = origin_offset and x
// = origin_offset.
//  _0>__________
// |             |
// |             |
// |             |
// |              >
// |             |
// |             |
// |_____________|
//
SkPath BackgroundPath(SkScalar height,
                      SkScalar action_offset,
                      bool draw_triangle_on_left,
                      SkScalar origin_offset = 0,
                      SkScalar corner_radius = kCornerRadius) {
  SkPath path;
  const SkScalar short_length =
      SkIntToScalar(kMenuWidth) - kTriangleHeight - 2 * corner_radius;
  const SkScalar short_height = height - 2 * corner_radius;

  // Calculate values for drawing triangle rounded corner. Check b/324940844 for
  // calculation details.
  const SkScalar triangle_radius =
      kTriangleRoundDistance / 4 *
      std::sqrt(4 +
                std::pow(kTriangleLength, 2) / std::pow(kTriangleHeight, 2));
  const SkScalar dx =
      kTriangleHeight * kTriangleRoundDistance / kTriangleLength;
  const SkScalar dy = kTriangleRoundDistance / 2;

  // If the offset is greater than the limit or less than the negative
  // limit, set it respectively.
  const SkScalar limit = short_height / 2 - kTriangleLength / 2;
  if (action_offset > limit) {
    action_offset = limit;
  } else if (action_offset < -limit) {
    action_offset = -limit;
  }
  if (draw_triangle_on_left) {
    path.moveTo(corner_radius + kTriangleHeight + origin_offset, origin_offset);
  } else {
    path.moveTo(corner_radius + origin_offset, origin_offset);
  }
  // Top left after corner radius to top right corner radius.
  path.rLineTo(short_length, 0);
  path.rArcTo(corner_radius, corner_radius, 0, SkPath::kSmall_ArcSize,
              SkPathDirection::kCW, corner_radius, corner_radius);
  if (draw_triangle_on_left) {
    // Top right after corner radius to bottom right corner radius.
    path.rLineTo(0, short_height);
  } else {
    // Top right after corner radius to midway point.
    path.rLineTo(0, limit + action_offset);
    // Triangle shape.
    path.rLineTo(kTriangleHeight - dx, kTriangleLength / 2 - dy);
    // Draw triangle rounded corner.
    path.rArcTo(triangle_radius, triangle_radius, 0, SkPath::kSmall_ArcSize,
                SkPathDirection::kCW, 0, kTriangleRoundDistance);
    path.rLineTo(-kTriangleHeight + dx, kTriangleLength / 2 - dy);
    // After midway point to bottom right corner radius.
    path.rLineTo(0, limit - action_offset);
  }
  path.rArcTo(corner_radius, corner_radius, 0, SkPath::kSmall_ArcSize,
              SkPathDirection::kCW, -corner_radius, corner_radius);
  // Bottom right after corner radius to bottom left corner radius.
  path.rLineTo(-short_length, 0);
  path.rArcTo(corner_radius, corner_radius, 0, SkPath::kSmall_ArcSize,
              SkPathDirection::kCW, -corner_radius, -corner_radius);
  if (draw_triangle_on_left) {
    // bottom left after corner radius to midway point.
    path.rLineTo(0, -limit + action_offset);
    // Triangle shape.
    path.rLineTo(-kTriangleHeight + dx, -kTriangleLength / 2 + dy);
    // Draw triangle rounded corner.
    path.rArcTo(triangle_radius, triangle_radius, 0, SkPath::kSmall_ArcSize,
                SkPathDirection::kCW, 0, -kTriangleRoundDistance);
    path.rLineTo(kTriangleHeight - dx, -kTriangleLength / 2 + dy);
    // After midway point to bottom right corner radius.
    path.rLineTo(0, -limit - action_offset);
  } else {
    // Bottom left after corner radius to top left corner radius.
    path.rLineTo(0, -short_height);
  }
  path.rArcTo(corner_radius, corner_radius, 0, SkPath::kSmall_ArcSize,
              SkPathDirection::kCW, corner_radius, -corner_radius);
  // Path finish.
  path.close();
  return path;
}

gfx::ShadowValues GetShadowValues() {
  return gfx::ShadowValue::MakeChromeOSSystemUIShadowValues(
      ash::SystemShadow::GetElevationFromType(
          ash::SystemShadow::Type::kElevation12));
}

// Returns negative insets to expand the shadow layer bigger than the container.
gfx::Insets GetShadowInsets() {
  return gfx::ShadowValue::GetMargin(GetShadowValues());
}

}  // namespace

// ArrowContainer is not a regular shadow container, so it needs to draw the
// special shape of the shadow in the ShadowLayer.
class ArrowContainer::ShadowLayer : public ui::Layer,
                                    public ui::LayerDelegate,
                                    public views::ViewObserver {
 public:
  explicit ShadowLayer(ArrowContainer* owner)
      : ui::Layer(ui::LAYER_TEXTURED), owner_(owner) {
    // TODO(b/331837116): Check the shadow distance and blur again after the
    // system shadow is adjusted to keep them consistent.
    SetFillsBoundsOpaquely(false);
    set_delegate(this);
  }

  ShadowLayer(const ShadowLayer&) = delete;
  ShadowLayer& operator=(const ShadowLayer&) = delete;

  ~ShadowLayer() override = default;

  // ui::LayerDelegate:
  void OnPaintLayer(const ui::PaintContext& context) override {
    if (bounds().IsEmpty()) {
      return;
    }
    ui::PaintRecorder recorder(context, size());
    auto* canvas = recorder.canvas();

    const auto shadow_values = GetShadowValues();
    gfx::Insets shadow_insets = -gfx::ShadowValue::GetMargin(shadow_values);

    cc::PaintFlags flags;
    flags.setLooper(gfx::CreateShadowDrawLooper(shadow_values));
    flags.setColor(SK_ColorTRANSPARENT);
    flags.setAntiAlias(true);

    canvas->DrawPath(
        BackgroundPath(SkIntToScalar(owner_->size().height()),
                       SkIntToScalar(owner_->arrow_vertical_offset_),
                       owner_->arrow_on_left_, shadow_insets.top()),
        flags);
  }

  // ui::LayerDelegate:
  void OnDeviceScaleFactorChanged(float old_device_scale_factor,
                                  float new_device_scale_factor) override {}

  // views::ViewObserver:
  void OnViewBoundsChanged(views::View* observed_view) override {
    auto shadow_layer_bounds = observed_view->layer()->bounds();
    shadow_layer_bounds.Inset(GetShadowInsets());
    SetBounds(shadow_layer_bounds);
  }

  void OnViewLayoutInvalidated(views::View* observed_view) override {
    // When the `observed_view` is relayout without bounds change, it also needs
    // to redraw the shadow because the triangle arrow position may change.
    SchedulePaint(gfx::Rect(size()));
  }

  void OnViewRemovedFromWidget(views::View* observed_view) override {
    observed_view->RemoveObserver(this);
  }

 private:
  raw_ptr<ArrowContainer> owner_;
};

ArrowContainer::ArrowContainer() {
  UpdateBorder();

  // Add shadow.
  shadow_layer_ = std::make_unique<ShadowLayer>(this);
  AddLayerToRegion(shadow_layer_.get(), views::LayerRegion::kBelow);
  AddObserver(shadow_layer_.get());
}

ArrowContainer::~ArrowContainer() = default;

void ArrowContainer::SetArrowVerticalOffset(int offset) {
  if (arrow_vertical_offset_ != offset) {
    arrow_vertical_offset_ = offset;
    SchedulePaint();
  }
}

void ArrowContainer::SetArrowOnLeft(bool arrow_on_left) {
  if (arrow_on_left_ != arrow_on_left) {
    arrow_on_left_ = arrow_on_left;
    UpdateBorder();
    SchedulePaint();
  }
}

void ArrowContainer::UpdateBorder() {
  SetBorder(views::CreateEmptyBorder(
      arrow_on_left_ ? gfx::Insets::TLBR(kArrowContainerHorizontalBorderInset,
                                         kArrowContainerHorizontalBorderInset +
                                             kTriangleHeight,
                                         kArrowContainerHorizontalBorderInset,
                                         kArrowContainerHorizontalBorderInset)
                     : gfx::Insets::TLBR(kArrowContainerHorizontalBorderInset,
                                         kArrowContainerHorizontalBorderInset,
                                         kArrowContainerHorizontalBorderInset,
                                         kArrowContainerHorizontalBorderInset +
                                             kTriangleHeight)));
}

void ArrowContainer::OnPaintBackground(gfx::Canvas* canvas) {
  cc::PaintFlags flags;
  // Draw the shape.
  flags.setAntiAlias(true);
  flags.setStyle(cc::PaintFlags::kFill_Style);
  ui::ColorProvider* color_provider = GetColorProvider();
  flags.setColor(
      color_provider->GetColor(cros_tokens::kCrosSysSystemBaseElevatedOpaque));

  const int height = GetHeightForWidth(kMenuWidth);
  canvas->DrawPath(
      BackgroundPath(SkIntToScalar(height),
                     SkIntToScalar(arrow_vertical_offset_), arrow_on_left_),
      flags);

  // Start to draw the highlight border.
  flags.setStyle(cc::PaintFlags::kStroke_Style);
  flags.setStrokeWidth(kHalfHightlightBorderThickness);
  auto* as_view = views::AsViewClass<views::View>(this);
  // Draw outside border.
  flags.setColor(views::HighlightBorder::GetBorderColor(
      *as_view, views::HighlightBorder::Type::kHighlightBorderOnShadow));
  canvas->DrawPath(
      BackgroundPath(SkIntToScalar(height),
                     SkIntToScalar(arrow_vertical_offset_), arrow_on_left_),
      flags);
  // Draw inside highlight.
  flags.setColor(views::HighlightBorder::GetHighlightColor(
      *as_view, views::HighlightBorder::Type::kHighlightBorderOnShadow));
  canvas->DrawPath(
      BackgroundPath(
          SkIntToScalar(height - kHighlightBorderThickness),
          SkIntToScalar(arrow_vertical_offset_), arrow_on_left_,
          /*origin_offset=*/kHalfHightlightBorderThickness,
          /*corner_radius=*/kCornerRadius - kHalfHightlightBorderThickness),
      flags);
}

gfx::Size ArrowContainer::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  return gfx::Size(kMenuWidth, GetLayoutManager()->GetPreferredHeightForWidth(
                                   this, kMenuWidth));
}

BEGIN_METADATA(ArrowContainer)
END_METADATA

}  // namespace arc::input_overlay
