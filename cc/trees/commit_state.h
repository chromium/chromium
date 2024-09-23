// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_COMMIT_STATE_H_
#define CC_TREES_COMMIT_STATE_H_

#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/time/time.h"
#include "cc/benchmarks/micro_benchmark_impl.h"
#include "cc/cc_export.h"
#include "cc/debug/layer_tree_debug_state.h"
#include "cc/input/event_listener_properties.h"
#include "cc/input/layer_selection_bound.h"
#include "cc/input/overscroll_behavior.h"
#include "cc/input/page_scale_animation.h"
#include "cc/layers/layer.h"
#include "cc/layers/layer_list_iterator.h"
#include "cc/metrics/begin_main_frame_metrics.h"
#include "cc/metrics/event_metrics.h"
#include "cc/paint/paint_image.h"
#include "cc/resources/ui_resource_request.h"
#include "cc/trees/begin_main_frame_trace_id.h"
#include "cc/trees/browser_controls_params.h"
#include "cc/trees/presentation_time_callback_buffer.h"
#include "cc/trees/render_frame_metadata.h"
#include "cc/trees/swap_promise.h"
#include "cc/trees/viewport_property_ids.h"
#include "cc/view_transition/view_transition_request.h"
#include "components/viz/common/surfaces/local_surface_id.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/delegated_ink_metadata.h"
#include "ui/gfx/display_color_spaces.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/vector2d_f.h"
#include "ui/gfx/overlay_transform.h"

namespace cc {
static constexpr int kInvalidSourceFrameNumber = -1;

// CommitState and ThreadUnsafeCommitState contain all of the information from
// LayerTreeHost that is needed to run compositor commit. CommitState is
// effectively POD; the compositor gets its own copy, which it may read or write
// without any concurrency issues.  ThreadUnsafeCommitState is shared data that
// is *not* copied to the compositor. Main thread code must take care not to
// modify anything reachable from ThreadUnsafeCommitState while commit is
// running on the impl thread, typically by adding calls to
// LayerTreeHost::WaitForCommitCompletion() before attempting to mutate state.

struct CC_EXPORT CommitState {
  CommitState();
  // Note: the copy constructor only copies persistent fields
  CommitState(const CommitState&);
  CommitState& operator=(const CommitState&) = delete;
  ~CommitState();

  base::flat_set<viz::SurfaceRange> SurfaceRanges() const;
  EventListenerProperties GetEventListenerProperties(EventListenerClass) const;

  // -------------------------------------------------------------------------
  // Persistent: these values persist on the LayerTreeHost between commits.
  // When a new persistent field is added, it must also be added to the copy
  // constructor.

  base::flat_map<viz::SurfaceRange, int> surface_ranges;
  base::TimeDelta visual_properties_update_duration;
  bool needs_gpu_rasterization_histogram = false;
  bool have_scroll_event_handlers = false;
  bool is_external_pinch_gesture_active = false;
  // Set to true if viewport is mobile optimized by using meta tag
  // <meta name="viewport" content="width=device-width">
  // or
  // <meta name="viewport" content="initial-scale=1.0">
  bool is_viewport_mobile_optimized = false;
  bool may_throttle_if_undrawn_frames = true;
  bool prefers_reduced_motion = false;
  BrowserControlsParams browser_controls_params;
  EventListenerProperties
      event_listener_properties[static_cast<size_t>(EventListenerClass::kLast) +
                                1] = {EventListenerProperties::kNone};
  float bottom_controls_shown_ratio = 0.f;
  float device_scale_factor = 1.f;
  float external_page_scale_factor = 1.f;
  float max_page_scale_factor = 1.f;
  float min_page_scale_factor = 1.f;
  float page_scale_factor = 1.f;
  float painted_device_scale_factor = 1.f;
  float top_controls_shown_ratio = 0.f;
  gfx::DisplayColorSpaces display_color_spaces;
  // Display transform hint to tag generated compositor frames.
  gfx::OverlayTransform display_transform_hint = gfx::OVERLAY_TRANSFORM_NONE;
  gfx::Rect device_viewport_rect;
  gfx::Size visual_device_viewport_size;
  gfx::Vector2dF elastic_overscroll;
  int hud_layer_id = Layer::INVALID_ID;
  int source_frame_number = 0;
  LayerSelection selection;
  LayerTreeDebugState debug_state;
  OverscrollBehavior overscroll_behavior;
  SkColor4f background_color = SkColors::kWhite;
  ViewportPropertyIds viewport_property_ids;
  viz::LocalSurfaceId local_surface_id_from_parent;

  // -------------------------------------------------------------------------
  // Take/reset: these values are reset on the LayerTreeHost between commits.

  // The number of SurfaceLayers that have (fallback,primary) set to
  // viz::SurfaceRange.
  bool clear_caches_on_next_commit = false;
  // Whether we have a pending request to force send RenderFrameMetadata with
  // the next frame.
  bool force_send_metadata_request = false;
  bool commit_waits_for_activation = false;
  bool needs_full_tree_sync = false;
  bool needs_surface_ranges_sync = false;
  bool new_local_surface_id_request = false;
  bool next_commit_forces_recalculate_raster_scales = false;
  bool next_commit_forces_redraw = false;
  BeginMainFrameTraceId trace_id{0};
  EventMetrics::List event_metrics;

  // Latency information for work done in ProxyMain::BeginMainFrame. The
  // unique_ptr is allocated in RequestMainFrameUpdate, and passed to Blink's
  // LocalFrameView that fills in the fields. This object adds the timing for
  // UpdateLayers. CC reads the data during commit, and clears the unique_ptr.
  std::unique_ptr<BeginMainFrameMetrics> begin_main_frame_metrics;

  // Metadata required for drawing a delegated ink trail onto the end of a
  // stroke. std::unique_ptr was specifically chosen so that it would be
  // cleared as it is forwarded along the pipeline to avoid old information
  // incorrectly sticking around and potentially being reused.
  std::unique_ptr<gfx::DelegatedInkMetadata> delegated_ink_metadata;

  std::unique_ptr<PendingPageScaleAnimation> pending_page_scale_animation;
  std::vector<std::pair<int, std::unique_ptr<PaintImage>>> queued_image_decodes;

  // Presentation time callbacks requested for the next frame are initially
  // added here.
  std::vector<PresentationTimeCallbackBuffer::Callback>
      pending_presentation_callbacks;
  std::vector<PresentationTimeCallbackBuffer::SuccessfulCallbackWithDetails>
      pending_successful_presentation_callbacks;

  std::vector<std::unique_ptr<MicroBenchmarkImpl>> benchmarks;

  // A list of view transitions that need to be transported from Blink to
  // Viz, as a CompositorFrameTransitionDirective.
  std::vector<std::unique_ptr<ViewTransitionRequest>> view_transition_requests;

  std::vector<std::unique_ptr<SwapPromise>> swap_promises;
  std::vector<UIResourceRequest> ui_resource_request_queue;
  base::flat_map<UIResourceId, gfx::Size> ui_resource_sizes;
  PropertyTreesChangeState property_trees_change_state;
  // RAW_PTR_EXCLUSION: Performance reasons (based on analysis of speedometer3).
  RAW_PTR_EXCLUSION base::flat_set<Layer*> layers_that_should_push_properties;

  // Specific scrollers may request clobbering the active delta value on the
  // compositor when committing the current scroll offset to ensure the scroll
  // is set to a specific value, overriding any compositor updates.
  base::flat_set<ElementId> scrollers_clobbering_active_value;

  // When non-empty, the next compositor frame also informs viz to issue a
  // screenshot against the previous surface.
  base::UnguessableToken screenshot_destination_token;

  // Indicates the `item_sequence_number` for the primary main frame's
  // `content::FrameNavigationEntry`. This is only set if the primary main frame
  // is rendering to this compositor.
  int64_t primary_main_frame_item_sequence_number =
      RenderFrameMetadata::kInvalidItemSequenceNumber;
};

struct CC_EXPORT ThreadUnsafeCommitState {
  ThreadUnsafeCommitState(MutatorHost* mh,
                          const ProtectedSequenceSynchronizer& synchronizer);
  ~ThreadUnsafeCommitState();

  // TODO(szager/vmpstr): These methods are to support range-based 'for' loops,
  // which is weird because ThreadUnsafeCommitState is not a collection or
  // container. We should do something more sensible and less weird.
  LayerListConstIterator begin() const {
    return LayerListConstIterator(root_layer.get());
  }
  LayerListConstIterator end() const { return LayerListConstIterator(nullptr); }

  raw_ptr<MutatorHost> mutator_host;
  PropertyTrees property_trees;
  scoped_refptr<Layer> root_layer;
};

struct CC_EXPORT CommitTimestamps {
  // Time when the compositor first became aware that a commit was requested by
  // the main thread.
  base::TimeTicks start;
  // Time when the compositor finished the commit.
  base::TimeTicks finish;
};

}  // namespace cc

#endif  // CC_TREES_COMMIT_STATE_H_
