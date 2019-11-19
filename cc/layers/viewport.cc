// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/viewport.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "cc/input/browser_controls_offset_manager.h"
#include "cc/trees/layer_tree_host_impl.h"
#include "cc/trees/layer_tree_impl.h"
#include "cc/trees/scroll_node.h"
#include "ui/gfx/geometry/vector2d_conversions.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace cc {

// static
std::unique_ptr<Viewport> Viewport::Create(LayerTreeHostImpl* host_impl) {
  return base::WrapUnique(new Viewport(host_impl));
}

Viewport::Viewport(LayerTreeHostImpl* host_impl)
    : host_impl_(host_impl)
    , pinch_zoom_active_(false) {
  DCHECK(host_impl_);
}

void Viewport::Pan(const gfx::Vector2dF& delta) {
  gfx::Vector2dF pending_delta = delta;
  float page_scale = host_impl_->active_tree()->current_page_scale_factor();
  pending_delta.Scale(1 / page_scale);
  scroll_tree().ScrollBy(InnerScrollNode(), pending_delta,
                         host_impl_->active_tree());
}

Viewport::ScrollResult Viewport::ScrollBy(const gfx::Vector2dF& physical_delta,
                                          const gfx::Point& viewport_point,
                                          bool is_direct_manipulation,
                                          bool affect_browser_controls,
                                          bool scroll_outer_viewport) {
  if (!OuterScrollNode())
    return ScrollResult();

  gfx::Vector2dF scroll_node_delta = physical_delta;

  if (affect_browser_controls &&
      ShouldBrowserControlsConsumeScroll(physical_delta))
    scroll_node_delta -= ScrollBrowserControls(physical_delta);

  gfx::Vector2dF pending_scroll_node_delta = scroll_node_delta;

  // Attempt to scroll inner viewport first.
  pending_scroll_node_delta -= host_impl_->ScrollSingleNode(
      InnerScrollNode(), pending_scroll_node_delta, viewport_point,
      is_direct_manipulation, &scroll_tree());

  // Now attempt to scroll the outer viewport.
  if (scroll_outer_viewport) {
    pending_scroll_node_delta -= host_impl_->ScrollSingleNode(
        OuterScrollNode(), pending_scroll_node_delta, viewport_point,
        is_direct_manipulation, &scroll_tree());
  }

  ScrollResult result;
  result.consumed_delta =
      physical_delta - AdjustOverscroll(pending_scroll_node_delta);
  result.content_scrolled_delta = scroll_node_delta - pending_scroll_node_delta;
  return result;
}

bool Viewport::CanScroll(const ScrollState& scroll_state) const {
  auto* outer_node = OuterScrollNode();

  if (!outer_node)
    return false;

  bool result = false;
  if (auto* inner_node = InnerScrollNode())
    result |= host_impl_->CanConsumeDelta(*inner_node, scroll_state);

  result |= host_impl_->CanConsumeDelta(*outer_node, scroll_state);

  return result;
}

void Viewport::ScrollByInnerFirst(const gfx::Vector2dF& delta) {
  gfx::Vector2dF unused_delta = scroll_tree().ScrollBy(
      InnerScrollNode(), delta, host_impl_->active_tree());

  auto* outer_node = OuterScrollNode();
  if (!unused_delta.IsZero() && outer_node) {
    scroll_tree().ScrollBy(outer_node, unused_delta, host_impl_->active_tree());
  }
}

bool Viewport::ShouldAnimateViewport(const gfx::Vector2dF& viewport_delta,
                                     const gfx::Vector2dF& pending_delta) {
  float max_dim_viewport_delta =
      std::max(std::abs(viewport_delta.x()), std::abs(viewport_delta.y()));
  float max_dim_pending_delta =
      std::max(std::abs(pending_delta.x()), std::abs(pending_delta.y()));
  return max_dim_viewport_delta > max_dim_pending_delta;
}

gfx::Vector2dF Viewport::ScrollAnimated(const gfx::Vector2dF& delta,
                                        base::TimeDelta delayed_by) {
  auto* outer_node = OuterScrollNode();
  if (!outer_node)
    return gfx::Vector2dF(0, 0);

  float scale_factor = host_impl_->active_tree()->current_page_scale_factor();
  gfx::Vector2dF scaled_delta = delta;
  scaled_delta.Scale(1.f / scale_factor);

  ScrollNode* inner_node = InnerScrollNode();
  gfx::Vector2dF inner_delta =
      host_impl_->ComputeScrollDelta(*inner_node, delta);

  gfx::Vector2dF pending_delta = scaled_delta - inner_delta;
  pending_delta.Scale(scale_factor);

  gfx::Vector2dF outer_delta =
      host_impl_->ComputeScrollDelta(*outer_node, pending_delta);

  if (inner_delta.IsZero() && outer_delta.IsZero())
    return gfx::Vector2dF(0, 0);

  // Animate the viewport to which the majority of scroll delta will be applied.
  // The animation system only supports running one scroll offset animation.
  // TODO(ymalik): Fix the visible jump seen by instant scrolling one of the
  // viewports.
  bool will_animate = false;
  if (ShouldAnimateViewport(inner_delta, outer_delta)) {
    scroll_tree().ScrollBy(outer_node, outer_delta, host_impl_->active_tree());
    will_animate =
        host_impl_->ScrollAnimationCreate(inner_node, inner_delta, delayed_by);
  } else {
    scroll_tree().ScrollBy(inner_node, inner_delta, host_impl_->active_tree());
    will_animate =
        host_impl_->ScrollAnimationCreate(outer_node, outer_delta, delayed_by);
  }
  if (will_animate) {
    // Consume entire scroll delta as long as we are starting an animation.
    return delta;
  }

  pending_delta = scaled_delta - inner_delta - outer_delta;
  pending_delta.Scale(scale_factor);
  return pending_delta;
}

void Viewport::SnapPinchAnchorIfWithinMargin(const gfx::Point& anchor) {
  gfx::SizeF viewport_size = gfx::SizeF(InnerScrollNode()->container_bounds);

  if (anchor.x() < kPinchZoomSnapMarginDips)
    pinch_anchor_adjustment_.set_x(-anchor.x());
  else if (anchor.x() > viewport_size.width() - kPinchZoomSnapMarginDips)
    pinch_anchor_adjustment_.set_x(viewport_size.width() - anchor.x());

  if (anchor.y() < kPinchZoomSnapMarginDips)
    pinch_anchor_adjustment_.set_y(-anchor.y());
  else if (anchor.y() > viewport_size.height() - kPinchZoomSnapMarginDips)
    pinch_anchor_adjustment_.set_y(viewport_size.height() - anchor.y());
}

void Viewport::PinchUpdate(float magnify_delta, const gfx::Point& anchor) {
  if (!pinch_zoom_active_) {
    // If this is the first pinch update and the pinch is within a margin-
    // length of the screen edge, offset all updates by the amount so that we
    // effectively snap the pinch zoom to the edge of the screen. This makes it
    // easy to zoom in on position: fixed elements.
    SnapPinchAnchorIfWithinMargin(anchor);

    pinch_zoom_active_ = true;
  }

  LayerTreeImpl* active_tree = host_impl_->active_tree();

  // Keep the center-of-pinch anchor specified by (x, y) in a stable
  // position over the course of the magnify.
  gfx::Point adjusted_anchor = anchor + pinch_anchor_adjustment_;
  float page_scale = active_tree->current_page_scale_factor();
  gfx::PointF previous_scale_anchor =
      gfx::ScalePoint(gfx::PointF(adjusted_anchor), 1.f / page_scale);
  active_tree->SetPageScaleOnActiveTree(page_scale * magnify_delta);
  page_scale = active_tree->current_page_scale_factor();
  gfx::PointF new_scale_anchor =
      gfx::ScalePoint(gfx::PointF(adjusted_anchor), 1.f / page_scale);
  gfx::Vector2dF move = previous_scale_anchor - new_scale_anchor;

  // Scale back to viewport space since that's the coordinate space ScrollBy
  // uses.
  move.Scale(page_scale);

  // If clamping the inner viewport scroll offset causes a change, it should
  // be accounted for from the intended move.
  move -= scroll_tree().ClampScrollToMaxScrollOffset(InnerScrollNode(),
                                                     host_impl_->active_tree());

  Pan(move);
}

void Viewport::PinchEnd(const gfx::Point& anchor, bool snap_to_min) {
  if (snap_to_min) {
    LayerTreeImpl* active_tree = host_impl_->active_tree();
    DCHECK(active_tree->InnerViewportScrollNode());
    const float kMaxZoomForSnapToMin = 1.05f;
    const base::TimeDelta kSnapToMinZoomAnimationDuration =
        base::TimeDelta::FromMilliseconds(200);
    float page_scale = active_tree->current_page_scale_factor();
    float min_scale = active_tree->min_page_scale_factor();

    // If the page is close to minimum scale at pinch end, snap to minimum.
    if (page_scale < min_scale * kMaxZoomForSnapToMin &&
        page_scale != min_scale) {
      gfx::PointF adjusted_anchor =
          gfx::PointF(anchor + pinch_anchor_adjustment_);
      adjusted_anchor =
          gfx::ScalePoint(adjusted_anchor, min_scale / page_scale);
      adjusted_anchor += ScrollOffsetToVector2dF(TotalScrollOffset());
      host_impl_->StartPageScaleAnimation(
          ToRoundedVector2d(adjusted_anchor.OffsetFromOrigin()), true,
          min_scale, kSnapToMinZoomAnimationDuration);
    }
  }

  pinch_anchor_adjustment_ = gfx::Vector2d();
  pinch_zoom_active_ = false;
}

bool Viewport::ShouldScroll(const ScrollNode& scroll_node) {
  return scroll_node.scrolls_inner_viewport ||
         scroll_node.scrolls_outer_viewport;
}

ScrollNode* Viewport::MainScrollNode() const {
  return host_impl_->OuterViewportScrollNode();
}

gfx::Vector2dF Viewport::ScrollBrowserControls(const gfx::Vector2dF& delta) {
  gfx::Vector2dF excess_delta =
      host_impl_->browser_controls_manager()->ScrollBy(delta);

  return delta - excess_delta;
}

bool Viewport::ShouldBrowserControlsConsumeScroll(
    const gfx::Vector2dF& scroll_delta) const {
  // Always consume if it's in the direction to show the browser controls.
  if (scroll_delta.y() < 0)
    return true;

  if (TotalScrollOffset().y() < MaxTotalScrollOffset().y())
    return true;

  return false;
}

gfx::Vector2dF Viewport::AdjustOverscroll(const gfx::Vector2dF& delta) const {
  // TODO(tdresser): Use a more rational epsilon. See crbug.com/510550 for
  // details.
  const float kEpsilon = 0.1f;
  gfx::Vector2dF adjusted = delta;

  if (std::abs(adjusted.x()) < kEpsilon)
    adjusted.set_x(0.0f);
  if (std::abs(adjusted.y()) < kEpsilon)
    adjusted.set_y(0.0f);

  return adjusted;
}

gfx::ScrollOffset Viewport::MaxTotalScrollOffset() const {
  gfx::ScrollOffset offset;

  offset += scroll_tree().MaxScrollOffset(InnerScrollNode()->id);

  if (auto* outer_node = OuterScrollNode())
    offset += scroll_tree().MaxScrollOffset(outer_node->id);

  return offset;
}

gfx::ScrollOffset Viewport::TotalScrollOffset() const {
  gfx::ScrollOffset offset;

  if (!InnerScrollNode())
    return offset;

  offset += scroll_tree().current_scroll_offset(InnerScrollNode()->element_id);

  if (auto* outer_node = OuterScrollNode())
    offset += scroll_tree().current_scroll_offset(outer_node->element_id);

  return offset;
}

ScrollNode* Viewport::InnerScrollNode() const {
  return host_impl_->InnerViewportScrollNode();
}

ScrollNode* Viewport::OuterScrollNode() const {
  return host_impl_->OuterViewportScrollNode();
}

ScrollTree& Viewport::scroll_tree() const {
  return host_impl_->active_tree()->property_trees()->scroll_tree;
}

}  // namespace cc
