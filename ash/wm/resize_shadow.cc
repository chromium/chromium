// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/resize_shadow.h"

#include <algorithm>
#include <optional>

#include "base/check.h"
#include "base/check_op.h"
#include "base/export_template.h"
#include "cc/paint/paint_flags.h"
#include "ui/decoration/decoration_util.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/outsets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/rounded_corners_f.h"

namespace ash {

namespace {

// Calculates outward margins for the active resize edges of `hit` with
// `thickness`.
gfx::Outsets CalculateOutsets(int hit, int thickness) {
  const bool show_top = hit == HTTOPLEFT || hit == HTTOP || hit == HTTOPRIGHT;
  const bool show_left =
      hit == HTTOPLEFT || hit == HTLEFT || hit == HTBOTTOMLEFT;
  const bool show_bottom =
      hit == HTBOTTOMLEFT || hit == HTBOTTOM || hit == HTBOTTOMRIGHT;
  const bool show_right =
      hit == HTTOPRIGHT || hit == HTRIGHT || hit == HTBOTTOMRIGHT;

  return gfx::Outsets::TLBR(show_top ? thickness : 0, show_left ? thickness : 0,
                            show_bottom ? thickness : 0,
                            show_right ? thickness : 0);
}

}  // namespace

// static
gfx::Outsets ResizeShadowGenerator::GetMargins(const ResizeShadowValue& value) {
  return value.outsets();
}

// static
gfx::Insets ResizeShadowGenerator::GetNineboxApertureInsets(
    const ResizeShadowValue& value,
    const gfx::RoundedCornersF& rounded_corners) {
  // Each corner patch must extend beneath both the outward shadow margin and
  // the window's rounded corner, and must also be at least
  // `corner_radius` so the shadow's own rounded corners stay outside the
  // stretchable 1x1 center aperture.
  const gfx::Insets corner_insets =
      ui::decoration::GetInsetsForRoundedCorners(rounded_corners);
  const gfx::Outsets& outsets = value.outsets();
  const int radius = value.corner_radius();
  return gfx::Insets::TLBR(
      std::max(outsets.top() + corner_insets.top(), radius),
      std::max(outsets.left() + corner_insets.left(), radius),
      std::max(outsets.bottom() + corner_insets.bottom(), radius),
      std::max(outsets.right() + corner_insets.right(), radius));
}

// static
void ResizeShadowGenerator::Draw(gfx::Canvas* canvas,
                                 const ResizeShadowValue& value,
                                 const gfx::RoundedCornersF& rounded_corners,
                                 const gfx::Rect& content_rect) {
  gfx::Rect shadow_rect = content_rect;
  shadow_rect.Outset(value.outsets());

  cc::PaintFlags paint;
  paint.setAntiAlias(true);
  paint.setColor(value.color());
  canvas->DrawRoundRect(gfx::RectF(shadow_rect), value.corner_radius(), paint);
}

DEFINE_SAFE_CAST_TARGET(ResizeShadow)

ResizeShadow::ResizeShadow(const InitParams& params, ResizeShadowType type)
    : params_(params), type_(type) {
  CHECK_GT(params_.thickness, 0);
}

ResizeShadow::~ResizeShadow() = default;

void ResizeShadow::SetColor(SkColor color) {
  if (color_ == color) {
    return;
  }
  color_ = color;
  NotifyDecorationChanged();
}

std::optional<ResizeShadowValue> ResizeShadow::GetSpec(
    const gfx::Rect& content_bounds,
    const gfx::RoundedCornersF& rounded_corners) const {
  CHECK(!content_bounds.IsEmpty());
  if (outsets_.IsEmpty()) {
    return std::nullopt;
  }

  const ResizeShadowValue spec(outsets_, params_.shadow_corner_radius, color_);
  gfx::Rect layer_bounds = content_bounds;
  layer_bounds.Outset(outsets_);
  const gfx::Insets aperture_insets =
      ResizeShadowGenerator::GetNineboxApertureInsets(spec, rounded_corners);
  if (aperture_insets.width() > layer_bounds.width() ||
      aperture_insets.height() > layer_bounds.height()) {
    return std::nullopt;
  }
  return spec;
}

std::optional<ui::decoration::DecorationSource::Details>
ResizeShadow::GetDetails(const gfx::Rect& content_bounds,
                         const gfx::RoundedCornersF& rounded_corners) {
  const std::optional<ResizeShadowValue> spec =
      GetSpec(content_bounds, rounded_corners);
  if (!spec.has_value()) {
    return std::nullopt;
  }

  const ResizeShadowDetails& details =
      ResizeShadowDetails::Get(rounded_corners, *spec);

  gfx::Rect occlusion_rect(content_bounds.size());
  occlusion_rect.Inset(
      ui::decoration::GetInsetsForRoundedCorners(rounded_corners));

  return Details{
      .appearance =
          {
              .nine_patch_image = details.nine_patch_image,
              .aperture_insets = details.aperture_insets,
              .margins = details.margins,
          },
      .occlusion_rect = occlusion_rect,
  };
}

void ResizeShadow::ShowForHitTest(int hit_test) {
  last_hit_test_ = hit_test;
  const gfx::Outsets new_outsets =
      params_.hit_test_enabled
          ? CalculateOutsets(last_hit_test_, params_.thickness)
          : gfx::Outsets(params_.thickness);
  visible_ = !new_outsets.IsEmpty();
  if (visible_ && outsets_ != new_outsets) {
    outsets_ = new_outsets;
    NotifyDecorationChanged();
  }
}

void ResizeShadow::Hide() {
  last_hit_test_ = HTNOWHERE;
  visible_ = false;
}

}  // namespace ash

namespace ui::decoration {

template class EXPORT_TEMPLATE_DEFINE(
    ASH_EXPORT) internal::DecorationCache<ash::ResizeShadowValue,
                                          ash::ResizeShadowGenerator>;

}  // namespace ui::decoration
