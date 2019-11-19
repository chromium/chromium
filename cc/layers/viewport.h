// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_VIEWPORT_H_
#define CC_LAYERS_VIEWPORT_H_

#include <memory>

#include "base/gtest_prod_util.h"
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
class CC_EXPORT Viewport {
 public:
  // If the pinch zoom anchor on the first PinchUpdate is within this length
  // of the screen edge, "snap" the zoom to that edge. Experimentally
  // determined.
  static const int kPinchZoomSnapMarginDips = 100;

  // TODO(tdresser): eventually |consumed_delta| should equal
  // |content_scrolled_delta|. See crbug.com/510045 for details.
  struct ScrollResult {
    gfx::Vector2dF consumed_delta;
    gfx::Vector2dF content_scrolled_delta;
  };

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

  bool CanScroll(const ScrollState& scroll_state) const;

  // TODO(bokan): Callers can now be replaced by ScrollBy.
  void ScrollByInnerFirst(const gfx::Vector2dF& delta);

  // Scrolls the viewport, bubbling the delta between the inner and outer
  // viewport. Only animates either of the two viewports.
  gfx::Vector2dF ScrollAnimated(const gfx::Vector2dF& delta,
                                base::TimeDelta delayed_by);

  gfx::ScrollOffset TotalScrollOffset() const;

  void PinchUpdate(float magnify_delta, const gfx::Point& anchor);
  void PinchEnd(const gfx::Point& anchor, bool snap_to_min);

  // Returns true if the given scroll node should be scrolled via this class,
  // false if it should be scrolled directly.
  bool ShouldScroll(const ScrollNode& scroll_node);

  // Returns the "representative" viewport scroll node. That is, the one that's
  // set as the currently scrolling node when the viewport scrolls.
  ScrollNode* MainScrollNode() const;

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

  gfx::ScrollOffset MaxTotalScrollOffset() const;

  ScrollNode* InnerScrollNode() const;
  ScrollNode* OuterScrollNode() const;
  ScrollTree& scroll_tree() const;

  void SnapPinchAnchorIfWithinMargin(const gfx::Point& anchor);

  LayerTreeHostImpl* host_impl_;

  bool pinch_zoom_active_;

  // The pinch zoom anchor point is adjusted by this amount during a pinch. This
  // is used to "snap" a pinch-zoom to the edge of the screen.
  gfx::Vector2d pinch_anchor_adjustment_;

  FRIEND_TEST_ALL_PREFIXES(ViewportTest, ShouldAnimateViewport);
};

}  // namespace cc

#endif  // CC_LAYERS_VIEWPORT_H_
