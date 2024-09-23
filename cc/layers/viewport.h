// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_VIEWPORT_H_
#define CC_LAYERS_VIEWPORT_H_

#include <memory>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "cc/layers/layer_impl.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace cc {

class LayerTreeHostImpl;
struct ScrollNode;

// Encapsulates gesture handling logic on the viewport layers. The "viewport"
// is made up of two scrolling layers, the inner viewport (visual) and the
// outer viewport (layout) scroll layers. These layers have different scroll
// bubbling behavior from the rest of the layer tree which is encoded in this
// class.
//
// When performing any kind of scroll operations on either the inner or outer
// scroll node, they must be done using this class. Typically, the outer
// viewport's scroll node will be used in the scroll chain to represent a full
// viewport scroll (i.e. one that will use this class to scroll both inner and
// outer viewports, as appropriate). However, in some situations (see comments
// in LayerTreeHostImpl::GetNodeToScroll) we may wish to scroll only the inner
// viewport. In that case, the inner viewport is used in the scroll chain, but
// we should still scroll using this class.
class CC_EXPORT Viewport {
 public:
  using ScrollResult = InputHandler::ViewportScrollResult;

  // If the pinch zoom anchor on the first PinchUpdate is within this length
  // of the screen edge, "snap" the zoom to that edge. Experimentally
  // determined.
  static const int kPinchZoomSnapMarginDips = 100;

  static std::unique_ptr<Viewport> Create(LayerTreeHostImpl* host_impl);

  Viewport(const Viewport&) = delete;
  Viewport& operator=(const Viewport&) = delete;

  // Differs from scrolling in that only the visual viewport is moved, without
  // affecting the browser controls or outer viewport.
  void Pan(const gfx::Vector2dF& delta);

  // Scrolls the viewport, applying the unique bubbling between the inner and
  // outer viewport unless the scroll_outer_viewport bit is off. Scrolls can be
  // consumed by browser controls. The delta is in physical pixels, that is, it
  // will be scaled by the page scale to ensure the content moves
  // |physical_delta| number of pixels.
  ScrollResult ScrollBy(const gfx::Vector2dF& physical_delta,
                        const gfx::Point& viewport_point,
                        bool is_direct_manipulation,
                        bool affect_browser_controls,
                        bool scroll_outer_viewport);

  // TODO(bokan): Callers can now be replaced by ScrollBy.
  void ScrollByInnerFirst(const gfx::Vector2dF& delta);

  // Scrolls the viewport, bubbling the delta between the inner and outer
  // viewport. Only animates either of the two viewports. Returns the amount of
  // delta that was consumed.
  ScrollResult ScrollAnimated(const gfx::Vector2dF& delta,
                              base::TimeDelta delayed_by);

  gfx::PointF TotalScrollOffset() const;

  void PinchUpdate(float magnify_delta, const gfx::Point& anchor);
  void PinchEnd(const gfx::Point& anchor, bool snap_to_min);

  // Returns true if the given scroll node should be scrolled via this class,
  // false if it should be scrolled directly. Scrolling either the inner or
  // outer viewport nodes must be done using this class.
  bool ShouldScroll(const ScrollNode& scroll_node) const;

  // Returns true if the viewport can consume any of the delta for the given
  // the |scroll_state|. This method takes a ScrollNode because viewport
  // scrolling can occur for either the inner or outer scroll nodes. If it
  // outer is used, we do a combined scroll that distributes the scroll among
  // the inner and outer viewports. If the inner is used, only the inner
  // viewport is scrolled. It is an error to pass any other node to this
  // method.
  bool CanScroll(const ScrollNode& node, const ScrollState& scroll_state) const;

  // Takes |scroll_delta| and returns a clamped delta based on distributing
  // |scroll_delta| first to the inner viewport, then the outer viewport.
  // The passed delta must be scaled by the page scale factor and the returned
  // delta will also be scaled.
  gfx::Vector2dF ComputeClampedDelta(const gfx::Vector2dF& scroll_delta) const;

  // Returns the innert viewport size, excluding scrollbars. This is not the
  // same as the inner viewport container_bounds size, which does not account
  // for scrollbars. This method can be useful for calculating the area of the
  // inner viewport where content is visible.
  gfx::SizeF GetInnerViewportSizeExcludingScrollbars() const;

  // Performs an instant snap if the viewport is a snap container and no scroll
  // gesture is in progress.
  void SnapIfNeeded();

 private:
  explicit Viewport(LayerTreeHostImpl* host_impl);

  // Returns true if viewport_delta is stricly less than pending_delta.
  static bool ShouldAnimateViewport(const gfx::Vector2dF& viewport_delta,
                                    const gfx::Vector2dF& pending_delta);
  bool ShouldBrowserControlsConsumeScroll(
      const gfx::Vector2dF& scroll_delta) const;
  gfx::Vector2dF AdjustOverscroll(const gfx::Vector2dF& delta) const;

  // Sends the delta to the browser controls, returns the amount applied.
  gfx::Vector2dF ScrollBrowserControls(const gfx::Vector2dF& delta);

  float MaxUserReachableTotalScrollOffsetY() const;

  ScrollNode* InnerScrollNode() const;
  ScrollNode* OuterScrollNode() const;
  ScrollTree& scroll_tree() const;

  void SnapPinchAnchorIfWithinMargin(const gfx::Point& anchor);

  raw_ptr<LayerTreeHostImpl> host_impl_;

  bool pinch_zoom_active_;

  // The pinch zoom anchor point is adjusted by this amount during a pinch. This
  // is used to "snap" a pinch-zoom to the edge of the screen.
  gfx::Vector2d pinch_anchor_adjustment_;

  FRIEND_TEST_ALL_PREFIXES(ViewportTest, ShouldAnimateViewport);
};

}  // namespace cc

#endif  // CC_LAYERS_VIEWPORT_H_
