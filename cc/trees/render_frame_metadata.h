// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_RENDER_FRAME_METADATA_H_
#define CC_TREES_RENDER_FRAME_METADATA_H_

#include "base/optional.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "cc/cc_export.h"
#include "components/viz/common/quads/selection.h"
#include "components/viz/common/surfaces/local_surface_id_allocation.h"
#include "components/viz/common/vertical_scroll_direction.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/geometry/vector2d_f.h"
#include "ui/gfx/selection_bound.h"

namespace cc {

class CC_EXPORT RenderFrameMetadata {
 public:
  RenderFrameMetadata();
  RenderFrameMetadata(const RenderFrameMetadata& other);
  RenderFrameMetadata(RenderFrameMetadata&& other);
  ~RenderFrameMetadata();

  RenderFrameMetadata& operator=(const RenderFrameMetadata&);
  RenderFrameMetadata& operator=(RenderFrameMetadata&& other);
  bool operator==(const RenderFrameMetadata& other) const;
  bool operator!=(const RenderFrameMetadata& other) const;

  // Indicates whether the scroll offset of the root layer is at top, i.e.,
  // whether scroll_offset.y() == 0.
  bool is_scroll_offset_at_top = true;

  // The background color of a CompositorFrame. It can be used for filling the
  // content area if the primary surface is unavailable and fallback is not
  // specified.
  SkColor root_background_color = SK_ColorWHITE;

  // Scroll offset of the root layer. This optional parameter is only valid
  // during tests.
  base::Optional<gfx::Vector2dF> root_scroll_offset;

  // Selection region relative to the current viewport. If the selection is
  // empty or otherwise unused, the bound types will indicate such.
  viz::Selection<gfx::SelectionBound> selection;

  // Determines whether the page is mobile optimized or not, which means at
  // least one of the following has to be true:
  // - page has a width=device-width or narrower viewport.
  // - page prevents zooming in or out (i.e. min and max page scale factors
  // are the same).
  bool is_mobile_optimized = false;

  // The device scale factor used to generate a CompositorFrame.
  float device_scale_factor = 1.f;

  // The size of the viewport used to generate a CompositorFrame. Equivalent to
  // the size of the root render pass.
  gfx::Size viewport_size_in_pixels;

  // The last viz::LocalSurfaceIdAllocation used to submit a CompositorFrame.
  base::Optional<viz::LocalSurfaceIdAllocation> local_surface_id_allocation;

  // Page scale factor (always 1.f for sub-frame renderers).
  float page_scale_factor = 1.f;
  // Used for testing propagation of page scale factor to sub-frame renderers.
  float external_page_scale_factor = 1.f;

  // Used to position the location top bar and page content, whose precise
  // position is computed by the renderer compositor.
  float top_controls_height = 0.f;
  float top_controls_shown_ratio = 0.f;

  // Indicates a change in the vertical scroll direction of the root layer since
  // the last drawn render frame. If no change occurred, this value is |kNull|.
  // Note that if a scroll in a given direction occurs, the scroll is completed,
  // and then another scroll in the *same* direction occurs, we will not
  // consider the second scroll event to have caused a change in direction.
  viz::VerticalScrollDirection new_vertical_scroll_direction =
      viz::VerticalScrollDirection::kNull;

#if defined(OS_ANDROID)
  // Used to position Android bottom bar, whose position is computed by the
  // renderer compositor.
  float bottom_controls_height = 0.f;
  float bottom_controls_shown_ratio = 0.f;

  // These limits can be used together with the scroll/scale fields above to
  // determine if scrolling/scaling in a particular direction is possible.
  float min_page_scale_factor = 0.f;
  float max_page_scale_factor = 0.f;
  bool root_overflow_y_hidden = false;

  gfx::SizeF scrollable_viewport_size;
  gfx::SizeF root_layer_size;

  // Returns whether the root RenderPass of the CompositorFrame has a
  // transparent background color.
  bool has_transparent_background = false;
#endif
};

}  // namespace cc

#endif  // CC_TREES_RENDER_FRAME_METADATA_H_
