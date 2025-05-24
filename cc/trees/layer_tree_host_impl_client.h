// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_LAYER_TREE_HOST_IMPL_CLIENT_H_
#define CC_TREES_LAYER_TREE_HOST_IMPL_CLIENT_H_

#include <stdint.h>

#include "base/containers/flat_set.h"
#include "base/functional/callback_forward.h"
#include "base/time/time.h"
#include "cc/metrics/frame_sequence_tracker_collection.h"
#include "cc/scheduler/scheduler.h"
#include "cc/trees/presentation_time_callback_buffer.h"
#include "components/viz/common/resources/returned_resource.h"
#include "components/viz/common/view_transition_element_resource_id.h"

namespace viz {
class BeginFrameSource;
class FrameSinkId;
struct FrameTimingDetails;
}  // namespace viz

namespace cc {

enum class AnimationWorkletMutationState;
enum class ElementListType;

// LayerTreeHost->Proxy callback interface.
class LayerTreeHostImplClient {
 public:
  virtual void DidLoseLayerTreeFrameSinkOnImplThread() = 0;
  virtual void SetBeginFrameSource(viz::BeginFrameSource* source) = 0;
  virtual void DidReceiveCompositorFrameAckOnImplThread() = 0;
  virtual void OnCanDrawStateChanged(bool can_draw) = 0;
  virtual void NotifyReadyToActivate() = 0;
  virtual bool IsReadyToActivate() = 0;
  virtual void NotifyReadyToDraw() = 0;
  // Please call these 2 functions through
  // LayerTreeHostImpl's SetNeedsRedraw() and SetNeedsOneBeginImplFrame().
  virtual void SetNeedsRedrawOnImplThread() = 0;
  virtual void SetNeedsOneBeginImplFrameOnImplThread() = 0;
  virtual void SetNeedsCommitOnImplThread(bool urgent = false) = 0;
  virtual void SetNeedsPrepareTilesOnImplThread() = 0;
  virtual void SetVideoNeedsBeginFrames(bool needs_begin_frames) = 0;
  virtual void SetDeferBeginMainFrameFromImpl(bool defer_begin_main_frame) = 0;
  virtual bool IsInsideDraw() = 0;
  virtual void RenewTreePriority() = 0;
  virtual void PostDelayedAnimationTaskOnImplThread(base::OnceClosure task,
                                                    base::TimeDelta delay) = 0;
  virtual void DidActivateSyncTree() = 0;
  virtual void DidPrepareTiles() = 0;

  // Called when page scale animation has completed on the impl thread.
  virtual void DidCompletePageScaleAnimationOnImplThread() = 0;

  // Called when output surface asks for a draw.
  virtual void OnDrawForLayerTreeFrameSink(bool resourceless_software_draw,
                                           bool skip_draw) = 0;

  virtual void SetNeedsImplSideInvalidation(
      bool needs_first_draw_on_activation) = 0;

  virtual void NotifyImageDecodeRequestFinished(int request_id,
                                                bool speculative,
                                                bool decode_succeeded) = 0;
  virtual void NotifyTransitionRequestFinished(
      uint32_t sequence_id,
      const viz::ViewTransitionElementResourceRects&) = 0;

  // Called when a presentation time is requested. |frame_token| identifies
  // the frame that was presented. |callbacks| holds both impl side and main
  // side callbacks to be called.
  virtual void DidPresentCompositorFrameOnImplThread(
      uint32_t frame_token,
      PresentationTimeCallbackBuffer::PendingCallbacks callbacks,
      const viz::FrameTimingDetails& details) = 0;

  virtual void NotifyAnimationWorkletStateChange(
      AnimationWorkletMutationState state,
      ElementListType tree_type) = 0;

  virtual void NotifyPaintWorkletStateChange(
      Scheduler::PaintWorkletState state) = 0;

  virtual void NotifyCompositorMetricsTrackerResults(
      CustomTrackerResults results) = 0;

  virtual void DidObserveFirstScrollDelay(
      int source_frame_number,
      base::TimeDelta first_scroll_delay,
      base::TimeTicks first_scroll_timestamp) = 0;

  // Returns true if the client is currently compositing synchronously. This is
  // only true in tests, but some behavior needs to be synchronized in non-test
  // code as a result.
  virtual bool IsInSynchronousComposite() const = 0;

  virtual void FrameSinksToThrottleUpdated(
      const base::flat_set<viz::FrameSinkId>& ids) = 0;

  virtual void ClearHistory() = 0;

  virtual void SetHasActiveThreadedScroll(bool is_scrolling) = 0;
  virtual void SetWaitingForScrollEvent(bool waiting_for_scroll_event) = 0;

  virtual void ReturnResource(viz::ReturnedResource returned_resource) {}

  virtual size_t CommitDurationSampleCountForTesting() const = 0;

 protected:
  virtual ~LayerTreeHostImplClient() = default;
};

}  // namespace cc

#endif  // CC_TREES_LAYER_TREE_HOST_IMPL_CLIENT_H_
