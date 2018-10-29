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

struct ApplyViewportChangesArgs {
  // Scroll offset delta of the inner (visual) viewport.
  gfx::ScrollOffset inner_delta;

  // Elastic overscroll effect offset delta. This is used only on Mac. a.k.a
  // "rubber-banding" overscroll.
  gfx::Vector2dF elastic_overscroll_delta;

  // "Pinch-zoom" page scale delta. This is a multiplicative delta. i.e.
  // main_thread_scale * delta == impl_thread_scale.
  float page_scale_delta;

  // How much the browser controls have been shown or hidden. The ratio runs
  // between 0 (hidden) and 1 (full-shown). This is additive.
  float browser_controls_delta;

  // Whether the browser controls have been locked to fully hidden or shown or
  // whether they can be freely moved.
  BrowserControlsState browser_controls_constraint;
};

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

  virtual void RecordWheelAndTouchScrollingCount(
      bool has_scrolled_by_wheel,
      bool has_scrolled_by_touch) = 0;
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
  // Record UMA and UKM metrics that require the time from the start of
  // BeginMainFrame to the Commit, or early out.
  virtual void RecordEndOfFrameMetrics(base::TimeTicks frame_begin_time) = 0;

 protected:
  virtual ~LayerTreeHostClient() {}
};

}  // namespace cc

#endif  // CC_TREES_LAYER_TREE_HOST_CLIENT_H_
