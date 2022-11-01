// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/occlusion.h"

#include "base/check_op.h"
#include "cc/base/math_util.h"
#include "ui/gfx/geometry/rect.h"

namespace cc {

Occlusion::Occlusion() = default;

Occlusion::Occlusion(const gfx::Transform& draw_transform,
                     const SimpleEnclosedRegion& occlusion_from_outside_target,
                     const SimpleEnclosedRegion& occlusion_from_inside_target)
    : draw_transform_(draw_transform),
      occlusion_from_outside_target_(occlusion_from_outside_target),
      occlusion_from_inside_target_(occlusion_from_inside_target) {
}

Occlusion Occlusion::GetOcclusionWithGivenDrawTransform(
    const gfx::Transform& transform) const {
  return Occlusion(
      transform, occlusion_from_outside_target_, occlusion_from_inside_target_);
}

bool Occlusion::HasOcclusion() const {
  return !occlusion_from_inside_target_.IsEmpty() ||
         !occlusion_from_outside_target_.IsEmpty();
}

bool Occlusion::IsOccluded(const gfx::Rect& content_rect) const {
  if (content_rect.IsEmpty())
    return true;

  if (!HasOcclusion())
    return false;

  gfx::Rect unoccluded_rect_in_target_surface =
      GetUnoccludedRectInTargetSurface(content_rect);
  return unoccluded_rect_in_target_surface.IsEmpty();
}

gfx::Rect Occlusion::GetUnoccludedContentRect(
    const gfx::Rect& content_rect) const {
  if (content_rect.IsEmpty())
    return content_rect;

  if (!HasOcclusion())
    return content_rect;

  gfx::Rect unoccluded_rect_in_target_surface =
      GetUnoccludedRectInTargetSurface(content_rect);
  if (unoccluded_rect_in_target_surface.IsEmpty())
    return gfx::Rect();

  gfx::Transform inverse_draw_transform;
  bool ok = draw_transform_.GetInverse(&inverse_draw_transform);
  // TODO(ajuma): Skip drawing layers with uninvertible draw transforms, and
  // change this to a DCHECK. crbug.com/517170
  if (!ok)
    return content_rect;

  gfx::Rect unoccluded_rect = MathUtil::ProjectEnclosingClippedRect(
      inverse_draw_transform, unoccluded_rect_in_target_surface);
  unoccluded_rect.Intersect(content_rect);

  return unoccluded_rect;
}

gfx::Rect Occlusion::GetUnoccludedRectInTargetSurface(
    const gfx::Rect& content_rect) const {
  // Take the ToEnclosingRect at each step, as we want to contain any unoccluded
  // partial pixels in the resulting Rect.
  gfx::Rect unoccluded_rect_in_target_surface =
      MathUtil::MapEnclosingClippedRect(draw_transform_, content_rect);
  DCHECK_LE(occlusion_from_inside_target_.GetRegionComplexity(), 1u);
  DCHECK_LE(occlusion_from_outside_target_.GetRegionComplexity(), 1u);
  // These subtract operations are more lossy than if we did both operations at
  // once.
  unoccluded_rect_in_target_surface.Subtract(
      occlusion_from_inside_target_.bounds());
  unoccluded_rect_in_target_surface.Subtract(
      occlusion_from_outside_target_.bounds());

  return unoccluded_rect_in_target_surface;
}

bool Occlusion::IsEqual(const Occlusion& other) const {
  return draw_transform_ == other.draw_transform_ &&
         occlusion_from_inside_target_ == other.occlusion_from_inside_target_ &&
         occlusion_from_outside_target_ == other.occlusion_from_outside_target_;
}

std::string Occlusion::ToString() const {
  return draw_transform_.ToString() + "outside(" +
         occlusion_from_outside_target_.ToString() + ") inside(" +
         occlusion_from_inside_target_.ToString() + ")";
}

}  // namespace cc
