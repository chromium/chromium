// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_FRAME_DATA_H_
#define CC_TREES_FRAME_DATA_H_

#include <string>
#include <vector>

#include "base/trace_event/traced_value.h"
#include "cc/cc_export.h"
#include "cc/layers/layer_collections.h"
#include "cc/trees/damage_reason.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "components/viz/common/quads/compositor_render_pass.h"
#include "components/viz/common/quads/trees_in_viz_timing.h"
#include "components/viz/common/surfaces/surface_id.h"

namespace cc {

// Status of tracked elements in a frame.
enum class TrackedElementStatus {
  // There are no tracked elements in any layer in this frame.
  kNone,
  // There are tracked elements in this frame, but they don't require
  // occlusion computation and subtraction.
  kHasTrackedElements,
  // There are tracked elements in this frame, and at least one of them
  // requires occlusion computation and subtraction.
  kHasTrackedElementsNeedingOcclusion,
};

struct CC_EXPORT FrameData {
  FrameData();
  FrameData(const FrameData&) = delete;
  ~FrameData();

  FrameData& operator=(const FrameData&) = delete;
  void AsValueInto(base::trace_event::TracedValue* value) const;
  std::string ToString() const;
  void set_trees_in_viz_timestamps(const viz::TreesInVizTiming& timing_details);

  // frame_token is populated by the LayerTreeHostImpl when submitted.
  uint32_t frame_token = 0;

  bool checkerboarded_needs_raster = false;
  bool checkerboarded_needs_record = false;

  std::vector<viz::SurfaceId> activation_dependencies;
  std::optional<uint32_t> deadline_in_frames;
  bool use_default_lower_bound_deadline = false;
  viz::CompositorRenderPassList render_passes;
  viz::CompositorRenderPassList unbounded_render_passes;
  // RAW_PTR_EXCLUSION: Renderer performance: visible in sampling profiler
  // stacks.
  RAW_PTR_EXCLUSION const RenderSurfaceList* render_surface_list = nullptr;
  RAW_PTR_EXCLUSION LayerImplList will_draw_layers;
  bool has_no_damage = false;
  viz::BeginFrameAck begin_frame_ack;
  // The original BeginFrameArgs that triggered the latest update from the
  // main thread.
  viz::BeginFrameArgs origin_begin_main_frame_args;
  DamageReasonSet damage_reasons;
  // Preferred frame rate of VideoLayerImpl mapped to number of layers.
  base::flat_map<base::TimeDelta, uint32_t> video_layer_preferred_intervals;
  // Indicates if there are SharedElementDrawQuads in this frame.
  bool has_shared_element_resources = false;
  // Indicates if this frame has a save directive which will add copy requests
  // for render passes in the Viz process.
  bool has_view_transition_save_directive = false;
  // Indicates if this frame had any copy requests, and is used to ensure
  // that we clear pending copy requests after drawing a frame and request
  // a new tree commit.
  bool has_copy_requests = false;
  // Only set when LTHI is in TreesInViz mode
  std::optional<viz::TreesInVizTiming> trees_in_viz_timing_details;
  // Indicates if this frame has any layers with tracked elements, and if they
  // require occlusion subtraction.
  TrackedElementStatus tracked_element_status = TrackedElementStatus::kNone;
};

}  // namespace cc

#endif  // CC_TREES_FRAME_DATA_H_
