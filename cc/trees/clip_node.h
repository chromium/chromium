// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_CLIP_NODE_H_
#define CC_TREES_CLIP_NODE_H_

#include "cc/cc_export.h"
#include "cc/trees/property_ids.h"
#include "third_party/abseil-cpp/absl/container/inlined_vector.h"
#include "ui/gfx/geometry/rect_f.h"

namespace base {
namespace trace_event {
class TracedValue;
}  // namespace trace_event
}  // namespace base

namespace cc {

struct ConditionalClip {
  bool is_clipped;
  gfx::RectF clip_rect;
};

struct ClipRectData {
  int target_id;
  ConditionalClip clip;
};

struct CC_EXPORT ClipNode {
  ClipNode();
  ClipNode(const ClipNode& other);

  ClipNode& operator=(const ClipNode& other);

  ~ClipNode();

  // Returns true if we should apply |clip|. Otherwise we should map the
  // accumulated clip by the filter specified by |pixel_moving_filter_id|.
  bool AppliesLocalClip() const;

  // The node index of this node in the clip tree node vector.
  int id = kInvalidPropertyNodeId;
  // The node index of the parent node in the clip tree node vector.
  int parent_id = kInvalidPropertyNodeId;

  // The clip rect that this node contributes, expressed in the space of its
  // transform node. This field is ignored if AppliesLocalClip() is false.
  gfx::RectF clip;

  // Each element of this cache stores the accumulated clip from this clip
  // node to a particular target.  The number of cached clip rects required
  // per node is roughly proportional to the number of render targets a
  // given clip rect participates in.  On many pages with only a root
  // render target, the number of cached clip rects per node is 1.
  // Any more than 3, and this will overflow rects onto the heap, so this
  // number is a tradeoff of ClipNode size on average and access speed.
  mutable absl::InlinedVector<ClipRectData, 3> cached_clip_rects;

  // This rect accumulates all clips from this node to the root in screen space.
  // It is used in the computation of layer's visible rect.
  gfx::RectF cached_accumulated_rect_in_screen_space;

  // If valid, it's the id of a pixel-moving filter in the effect tree.
  // Instead of applying |clip|, this clip node expands the accumulated clip
  // to include any pixels in the contents that can affect the rendering result
  // with the filter.
  int pixel_moving_filter_id = kInvalidPropertyNodeId;

  // The id of the transform node that defines the clip node's local space.
  int transform_id = kInvalidPropertyNodeId;

#if DCHECK_IS_ON()
  bool operator==(const ClipNode& other) const;
#endif

  void AsValueInto(base::trace_event::TracedValue* value) const;
};

}  // namespace cc

#endif  // CC_TREES_CLIP_NODE_H_
