// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_LAYER_TREE_HOST_CLIENT_H_
#define CC_TREES_LAYER_TREE_HOST_CLIENT_H_

#include <memory>

#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "cc/input/browser_controls_state.h"
#include "ui/gfx/geometry/scroll_offset.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace gfx {
struct PresentationFeedback;
}

namespace viz {
struct BeginFrameArgs;
}

namespace cc {
struct BeginMainFrameMetrics;
struct ElementId;

struct ApplyViewportChangesArgs {
  // Scroll offset delta of the inner (visual) viewport.
  gfx::ScrollOffset inner_delta;

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
  // between 0 (hidden) and 1 (full-shown). This is additive.
  float top_controls_delta;

  // How much the bottom controls have been shown or hidden. The ratio runs
  // between 0 (hidden) and 1 (full-shown). This is additive.
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
constexpr ManipulationInfo kManipulationInfoHasScrolledByWheel = 1 << 0;
constexpr ManipulationInfo kManipulationInfoHasScrolledByTouch = 1 << 1;
constexpr ManipulationInfo kManipulationInfoHasScrolledByPrecisionTouchPad =
    1 << 2;
constexpr ManipulationInfo kManipulationInfoHasPinchZoomed = 1 << 3;

// A LayerTreeHost is bound to a LayerTreeHostClient. The main rendering
// loop (in ProxyMain or SingleThreadProxy) calls methods on the
// LayerTreeHost, which then handles them and also calls into the equivalent
// methods on its LayerTreeHostClient when applicable.
//
// One important example of a LayerTreeHostClient is (via additional
// indirections) Blink.
class LayerTreeHostClient {
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
  virtual void DidBeginMainFrame() = 0;
  virtual void WillUpdateLayers() = 0;
  virtual void DidUpdateLayers() = 0;

  // Notification that the proxy started or stopped deferring main frame updates
  virtual void OnDeferMainFrameUpdatesChanged(bool) = 0;

  // Notification that the proxy started or stopped deferring commits.
  virtual void OnDeferCommitsChanged(bool) = 0;

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

  // Record use counts of different methods of scrolling (e.g. wheel, touch,
  // precision touchpad, etc.).
  virtual void RecordManipulationTypeCounts(ManipulationInfo info) = 0;

  // Notifies the client when an overscroll has happened.
  virtual void SendOverscrollEventFromImplSide(
      const gfx::Vector2dF& overscroll_delta,
      ElementId scroll_latched_element_id) = 0;
  // Notifies the client when a gesture scroll has ended.
  virtual void SendScrollEndEventFromImplSide(
      ElementId scroll_latched_element_id) = 0;

  // Request a LayerTreeFrameSink from the client. When the client has one it
  // should call LayerTreeHost::SetLayerTreeFrameSink. This will result in
  // either DidFailToInitializeLayerTreeFrameSink or
  // DidInitializeLayerTreeFrameSink being called.
  virtual void RequestNewLayerTreeFrameSink() = 0;
  virtual void DidInitializeLayerTreeFrameSink() = 0;
  virtual void DidFailToInitializeLayerTreeFrameSink() = 0;
  virtual void WillCommit() = 0;
  virtual void DidCommit() = 0;
  virtual void DidCommitAndDrawFrame() = 0;
  virtual void DidReceiveCompositorFrameAck() = 0;
  virtual void DidCompletePageScaleAnimation() = 0;
  virtual void DidPresentCompositorFrame(
      uint32_t frame_token,
      const gfx::PresentationFeedback& feedback) = 0;
  // Mark the frame start and end time for UMA and UKM metrics that require
  // the time from the start of BeginMainFrame to the Commit, or early out.
  virtual void RecordStartOfFrameMetrics() = 0;
  virtual void RecordEndOfFrameMetrics(base::TimeTicks frame_begin_time) = 0;
  // Return metrics information for the stages of BeginMainFrame. This is
  // ultimately implemented by Blink's LocalFrameUKMAggregator. It must be a
  // distinct call from the FrameMetrics above because the BeginMainFrameMetrics
  // for compositor latency must be gathered before the layer tree is
  // committed to the compositor, which is before the call to
  // RecordEndOfFrameMetrics.
  virtual std::unique_ptr<BeginMainFrameMetrics> GetBeginMainFrameMetrics() = 0;

 protected:
  virtual ~LayerTreeHostClient() {}
};

// LayerTreeHost->WebThreadScheduler callback interface. Instances of this class
// must be safe to use on both the compositor and main threads.
class LayerTreeHostSchedulingClient {
 public:
  // Indicates that the compositor thread scheduled a BeginMainFrame to run on
  // the main thread.
  virtual void DidScheduleBeginMainFrame() = 0;
  // Called unconditionally when BeginMainFrame runs on the main thread.
  virtual void DidRunBeginMainFrame() = 0;
};

}  // namespace cc

#endif  // CC_TREES_LAYER_TREE_HOST_CLIENT_H_
