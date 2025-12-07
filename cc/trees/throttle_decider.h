// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_THROTTLE_DECIDER_H_
#define CC_TREES_THROTTLE_DECIDER_H_

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "cc/cc_export.h"
#include "components/viz/common/quads/compositor_render_pass.h"
#include "components/viz/common/surfaces/frame_sink_id.h"

namespace cc {
class LayerImpl;

// This class is used to decide if any frame sinks in a render pass list
// satisfies the compositing-based criteria to be throttled.
class CC_EXPORT ThrottleDecider {
 public:
  ThrottleDecider();
  ~ThrottleDecider();
  ThrottleDecider(const ThrottleDecider&) = delete;
  ThrottleDecider& operator=(const ThrottleDecider&) = delete;

  // This function should be called at the beginning of each time when a render
  // pass list is about to be processed.
  void Prepare();
  // Go through the quads in |render_pass| and decide for each embedded surface
  // in SurfaceDrawQuad if it can be throttled. This is a simple version where
  // intersection calculation of surface/quad rects are confined to the render
  // pass's constituent quads.
  void ProcessRenderPass(const viz::CompositorRenderPass& render_pass);
  // Process a layer that will not draw. This is only relevant for surface
  // layers and checks if the embedded frame sink is qualified for throttling.
  void ProcessLayerNotToDraw(const LayerImpl* layer);
  bool HasThrottlingChanged() const;
  const base::flat_set<viz::FrameSinkId>& ids() const { return ids_; }

 private:
  base::flat_map<viz::CompositorRenderPassId,
                 raw_ptr<const viz::CompositorRenderPass, CtnExperimental>>
      id_to_pass_map_;
  // Ids of frame sinks that are qualified for throttling.
  base::flat_set<viz::FrameSinkId> ids_;
  // Ids of frame sinks that were qualified for throttling from last
  // compositing.
  base::flat_set<viz::FrameSinkId> last_ids_;
};

}  // namespace cc

#endif  // CC_TREES_THROTTLE_DECIDER_H_
