// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_LAYER_TREE_HOST_CLIENT_H_
#define CC_TREES_LAYER_TREE_HOST_CLIENT_H_

#include <memory>
#include <string>

#include "base/time/time.h"
#include "cc/cc_export.h"
#include "cc/input/browser_controls_state.h"
#include "cc/metrics/frame_sequence_tracker_collection.h"
#include "cc/trees/paint_holding_commit_trigger.h"
#include "cc/trees/paint_holding_reason.h"
#include "cc/trees/property_tree.h"
#include "components/viz/common/frame_timing_details.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace viz {
struct BeginFrameArgs;
}

namespace cc {
struct BeginMainFrameMetrics;
struct CommitState;

struct ApplyViewportChangesArgs {
  // Scroll offset delta of the inner (visual) viewport.
  gfx::Vector2dF inner_delta;

  // Elastic overscroll effect offset delta. This is used only on Mac. a.k.a
  // "rubber-banding" overscroll.
  gfx::Vector2dF elastic_overscroll_delta;

  // "Pinch-zoom" page scale delta. This is a multiplicative delta. i.e.
  // main_thread_scale * delta == impl_thread_scale.
  float page_scale_delta;

  // Indicates that a pinch gesture is currently active or not; used to allow
  // subframe compositors to throttle their re-rastering during the gesture.
  bool is_pinch_gesture_active;

  // How much the top controls have been shown or hidden. The ratio runs
  // between a set min-height (default 0) and 1 (full-shown). This is additive.
  float top_controls_delta;

  // How much the bottom controls have been shown or hidden. The ratio runs
  // between a set min-height (default 0) and 1 (full-shown). This is additive.
  float bottom_controls_delta;

  // Whether the browser controls have been locked to fully hidden or shown or
  // whether they can be freely moved.
  BrowserControlsState browser_controls_constraint;

  // Set to true when a scroll gesture being handled on the compositor has
  // ended.
  bool scroll_gesture_did_end;
};

using ManipulationInfo = uint32_t;
constexpr ManipulationInfo kManipulationInfoNone = 0;
constexpr ManipulationInfo kManipulationInfoWheel = 1 << 0;
constexpr ManipulationInfo kManipulationInfoTouch = 1 << 1;
constexpr ManipulationInfo kManipulationInfoPrecisionTouchPad = 1 << 2;
constexpr ManipulationInfo kManipulationInfoPinchZoom = 1 << 3;
constexpr ManipulationInfo kManipulationInfoScrollbar = 1 << 4;

struct PaintBenchmarkResult {
  double record_time_ms = 0;
  double record_time_caching_disabled_ms = 0;
  double record_time_subsequence_caching_disabled_ms = 0;
  double raster_invalidation_and_convert_time_ms = 0;
  double paint_artifact_compositor_update_time_ms = 0;
  size_t painter_memory_usage = 0;
};

// A LayerTreeHost is bound to a LayerTreeHostClient. The main rendering
// loop (in ProxyMain or SingleThreadProxy) calls methods on the
// LayerTreeHost, which then handles them and also calls into the equivalent
// methods on its LayerTreeHostClient when applicable.
//
// One important example of a LayerTreeHostClient is (via additional
// indirections) Blink.
//
// Note: Some API callbacks below are tied to a frame. Since LayerTreeHost
// maintains a pipeline of frames, it can be ambiguous which frame the callback
// is associated with. We rely on `source_frame_number` to tie the callback to
// its associated frame. See LayerTreeHost::SourceFrameNumber for details.
class CC_EXPORT LayerTreeHostClient {
 public:
  virtual void WillBeginMainFrame() = 0;
  // Marks finishing compositing-related tasks on the main thread. In threaded
  // mode, this corresponds to DidCommit().

  // For a LayerTreeHostClient backed by Blink, BeginMainFrame will:
  // -Dispatch BeginMainFrame-aligned input events.
  // -Advance frame-synchronized animations and callbacks. These include
  // gesture animations, autoscroll animations, declarative
  // CSS animations (including both main-thread and compositor thread
  // animations), and script-implemented requestAnimationFrame animations.
  //
  // Note: CSS animations which run on the main thread invalidate rendering
  // phases as appropriate. CSS animations which run on the compositor
  // invalidate styles, and then update transforms or opacity on the Layer tree.
  // Compositor animations need to be updated here, because there is no
  // other mechanism by which the compositor syncs animation state for these
  // animations to Blink.
  virtual void BeginMainFrame(const viz::BeginFrameArgs& args) = 0;

  virtual void BeginMainFrameNotExpectedSoon() = 0;
  virtual void BeginMainFrameNotExpectedUntil(base::TimeTicks time) = 0;
  // This is called immediately after notifying the impl thread that it should
  // do a commit, possibly before the commit has finished (depending on whether
  // features::kNonBlockingCommit is enabled). It is meant for work that must
  // happen prior to returning control to the main thread event loop.
  virtual void DidBeginMainFrame() = 0;
  virtual void WillUpdateLayers() = 0;
  virtual void DidUpdateLayers() = 0;

  virtual void DidObserveFirstScrollDelay(
      int source_frame_number,
      base::TimeDelta first_scroll_delay,
      base::TimeTicks first_scroll_timestamp) = 0;
  // Notification that the proxy started or stopped deferring main frame updates
  virtual void OnDeferMainFrameUpdatesChanged(bool) = 0;

  // Notification that the proxy started or stopped deferring commits. |reason|
  // indicates why commits are/were deferred. |trigger| indicates why the commit
  // restarted. |trigger| is always provided on restarts, when |defer_status|
  // switches to false.
  virtual void OnDeferCommitsChanged(
      bool defer_status,
      PaintHoldingReason reason,
      std::optional<PaintHoldingCommitTrigger> trigger) = 0;

  // Notification that a compositing update has been requested.
  virtual void OnCommitRequested() = 0;

  // Visual frame-based updates to the state of the LayerTreeHost are expected
  // to happen only in calls to LayerTreeHostClient::UpdateLayerTreeHost, which
  // should mutate/invalidate the layer tree or other page parameters as
  // appropriate.
  //
  // For a LayerTreeHostClient backed by Blink, this method will update
  // (Blink's notions of) style, layout, paint invalidation and compositing
  // state. (The "compositing state" will result in a mutated layer tree on the
  // LayerTreeHost via additional interface indirections which lead back to
  // mutations on the LayerTreeHost.)
  virtual void UpdateLayerTreeHost() = 0;

  // Notifies the client of viewport-related changes that occured in the
  // LayerTreeHost since the last commit. This typically includes things
  // related to pinch-zoom, browser controls (aka URL bar), overscroll, etc.
  virtual void ApplyViewportChanges(const ApplyViewportChangesArgs& args) = 0;

  // Notifies the client about scroll and input related changes that occurred in
  // the LayerTreeHost since the last commit.
  virtual void UpdateCompositorScrollState(
      const CompositorCommitData& commit_data) = 0;

  // Request a LayerTreeFrameSink from the client. When the client has one it
  // should call LayerTreeHost::SetLayerTreeFrameSink. This will result in
  // either DidFailToInitializeLayerTreeFrameSink or
  // DidInitializeLayerTreeFrameSink being called.
  virtual void RequestNewLayerTreeFrameSink() = 0;
  virtual void DidInitializeLayerTreeFrameSink() = 0;
  virtual void DidFailToInitializeLayerTreeFrameSink() = 0;
  virtual void WillCommit(const CommitState&) = 0;
  // Report that a commit to the impl thread has completed. The
  // commit_start_time is the time that the impl thread began processing the
  // commit, or base::TimeTicks() if the commit did not require action by the
  // impl thread.
  virtual void DidCommit(int source_frame_number,
                         base::TimeTicks commit_start_time,
                         base::TimeTicks commit_finish_time) = 0;
  virtual void DidCommitAndDrawFrame(int source_frame_number) = 0;
  virtual void DidCompletePageScaleAnimation(int source_frame_number) = 0;
  virtual void DidPresentCompositorFrame(
      uint32_t frame_token,
      const viz::FrameTimingDetails& frame_timing_details) = 0;
  // Mark the frame start and end time for UMA and UKM metrics that require
  // the time from the start of BeginMainFrame to the Commit, or early out.
  virtual void RecordStartOfFrameMetrics() = 0;
  // This is called immediately after notifying the impl thread that it should
  // do a commit, possibly before the commit has finished (depending on whether
  // features::kNonBlockingCommit is enabled). It is meant to record the time
  // when the main thread is finished with its part of a main frame, and will
  // return control to the main thread event loop.
  virtual void RecordEndOfFrameMetrics(
      base::TimeTicks frame_begin_time,
      ActiveFrameSequenceTrackers trackers) = 0;
  // Return metrics information for the stages of BeginMainFrame. This is
  // ultimately implemented by Blink's LocalFrameUKMAggregator. It must be a
  // distinct call from the FrameMetrics above because the BeginMainFrameMetrics
  // for compositor latency must be gathered before the layer tree is
  // committed to the compositor, which is before the call to
  // RecordEndOfFrameMetrics.
  virtual std::unique_ptr<BeginMainFrameMetrics> GetBeginMainFrameMetrics() = 0;
  virtual void NotifyThroughputTrackerResults(CustomTrackerResults results) = 0;

  virtual void RunPaintBenchmark(int repeat_count,
                                 PaintBenchmarkResult& result) {}

  // Return a string that is the paused debugger message for the heads-up
  // display overlay.
  virtual std::string GetPausedDebuggerLocalizedMessage();

  // This is an inaccurate signal that has been used to represent that content
  // was displayed. This actually maps to the removal of backpressure by the
  // GPU. This can be signalled when the GPU attempts to Draw; when a submitted
  // frame, that has not drawn, is being replaced by a newer one; or merged with
  // future OnBeginFrames.
  //
  // To determine when presentation occurred see `DidPresentCompositorFrame`.
  virtual void DidReceiveCompositorFrameAckDeprecatedForCompositor() {}

 protected:
  virtual ~LayerTreeHostClient() = default;
};

// LayerTreeHost->WebThreadScheduler callback interface. Instances of this class
// must be safe to use on both the compositor and main threads.
class LayerTreeHostSchedulingClient {
 public:
  // Called unconditionally when BeginMainFrame runs on the main thread.
  virtual void DidRunBeginMainFrame() = 0;
};

}  // namespace cc

#endif  // CC_TREES_LAYER_TREE_HOST_CLIENT_H_
