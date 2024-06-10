// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_INPUT_COMPOSITOR_INPUT_INTERFACES_H_
#define CC_INPUT_COMPOSITOR_INPUT_INTERFACES_H_

#include <memory>

#include "base/time/time.h"
#include "base/types/optional_ref.h"
#include "cc/input/actively_scrolling_type.h"
#include "cc/input/browser_controls_offset_tags_info.h"
#include "cc/input/browser_controls_state.h"
#include "cc/paint/element_id.h"
#include "ui/gfx/geometry/size.h"

namespace viz {
struct BeginFrameArgs;
}

namespace gfx {
class Vector2dF;
}

namespace cc {

struct CompositorCommitData;
class LayerTreeHostImpl;
class LayerTreeSettings;
class MutatorHost;
class ScrollTree;
enum class ScrollbarOrientation;

// This is the interface that LayerTreeHostImpl and the "graphics" side of the
// compositor uses to talk to the compositor InputHandler. This interface is
// two-way; it's used used both to communicate state changes from the LayerTree
// to the input handler and also to query and update state in the input handler.
class InputDelegateForCompositor {
 public:
  virtual ~InputDelegateForCompositor() = default;

  // Called during a commit to fill in the changes that have occurred since the
  // last commit.
  virtual void ProcessCommitDeltas(
      CompositorCommitData* commit_data,
      const MutatorHost* main_thread_mutator_host) = 0;

  // Called to let the input handler perform animations.
  virtual void TickAnimations(base::TimeTicks monotonic_time) = 0;

  // Compositor lifecycle state observation.
  virtual void WillShutdown() = 0;
  virtual void WillDraw() = 0;
  virtual void WillBeginImplFrame(const viz::BeginFrameArgs& args) = 0;
  virtual void DidCommit() = 0;
  virtual void DidActivatePendingTree() = 0;
  virtual void DidFinishImplFrame() = 0;
  virtual void OnBeginImplFrameDeadline() = 0;

  // Called when the state of the "root layer" may have changed from outside
  // the input system. The state includes: scroll offset, scrollable size,
  // scroll limits, page scale, page scale limits.
  virtual void RootLayerStateMayHaveChanged() = 0;

  // Called to let the input handler know that a scrollbar for the given
  // elementId has been added.
  virtual void DidRegisterScrollbar(ElementId scroll_element_id,
                                    ScrollbarOrientation orientation) = 0;
  // Called to let the input handler know that a scrollbar for the given
  // elementId has been removed.
  virtual void DidUnregisterScrollbar(ElementId scroll_element_id,
                                      ScrollbarOrientation orientation) = 0;

  // Called to let the input handler know that a scroll offset animation has
  // completed.
  virtual void ScrollOffsetAnimationFinished() = 0;

  // Called to inform the input handler when prefers-reduced-motion changes.
  virtual void SetPrefersReducedMotion(bool prefers_reduced_motion) = 0;

  // Returns true if we're currently in a "gesture" (user-initiated) scroll.
  // That is, between a GestureScrollBegin and a GestureScrollEnd. Note, a
  // GestureScrollEnd is deferred if the gesture ended but we're still
  // animating the scroll to its final position (e.g. the user released their
  // finger from the touchscreen but we're scroll snapping).
  virtual bool IsCurrentlyScrolling() const = 0;

  // Indicates the type (Animated or Precise) of an active scroll, if there is
  // one, in progress. "Active" here means that it's been latched (i.e. we have
  // a CurrentlyScrollingNode()) but also that some ScrollUpdates have been
  // received and their delta consumed for scrolling. These can differ
  // significantly e.g. the page allows the touchstart but preventDefaults all
  // the touchmoves. In that case, we latch and have a CurrentlyScrollingNode()
  // but will never receive a ScrollUpdate.
  virtual ActivelyScrollingType GetActivelyScrollingType() const = 0;

  // Returns true if the user is currently touching the device.
  virtual bool IsHandlingTouchSequence() const = 0;

  // Returns true if we're currently scrolling and the scroll must be realized
  // on the main thread (see ScrollTree::CanRealizeScrollsOnCompositor).
  // TODO(skobes): Combine IsCurrentlyScrolling, GetActivelyScrollingType, and
  // IsCurrentScrollMainRepainted into a single method returning everything.
  virtual bool IsCurrentScrollMainRepainted() const = 0;

  // Returns true if there are input events queued to be dispatched at the start
  // of the next frame.
  virtual bool HasQueuedInput() const = 0;
};

// This is the interface that's exposed by the LayerTreeHostImpl to the input
// handler.
class CompositorDelegateForInput {
 public:
  virtual ~CompositorDelegateForInput() = default;

  virtual void BindToInputHandler(
      std::unique_ptr<InputDelegateForCompositor> delegate) = 0;

  virtual ScrollTree& GetScrollTree() const = 0;
  virtual bool HasAnimatedScrollbars() const = 0;
  virtual void SetNeedsCommit() = 0;
  virtual void SetNeedsFullViewportRedraw() = 0;
  virtual void SetDeferBeginMainFrame(bool defer_begin_main_frame) const = 0;
  virtual void DidUpdateScrollAnimationCurve() = 0;
  virtual void AccumulateScrollDeltaForTracing(const gfx::Vector2dF& delta) = 0;
  virtual void DidStartPinchZoom() = 0;
  virtual void DidUpdatePinchZoom() = 0;
  virtual void DidEndPinchZoom() = 0;
  virtual void DidStartScroll() = 0;
  virtual void DidEndScroll() = 0;
  virtual void DidMouseLeave() = 0;
  virtual bool IsInHighLatencyMode() const = 0;
  virtual void WillScrollContent(ElementId element_id) = 0;
  virtual void DidScrollContent(ElementId element_id, bool animated) = 0;
  virtual float DeviceScaleFactor() const = 0;
  virtual float PageScaleFactor() const = 0;
  virtual gfx::Size VisualDeviceViewportSize() const = 0;
  virtual const LayerTreeSettings& GetSettings() const = 0;
  virtual void UpdateBrowserControlsState(
      BrowserControlsState constraints,
      BrowserControlsState current,
      bool animate,
      base::optional_ref<const BrowserControlsOffsetTagsInfo>
          offset_tags_info) = 0;
  virtual bool HasScrollLinkedAnimation(ElementId for_scroller) const = 0;

  // TODO(bokan): Temporary escape hatch for code that hasn't yet been
  // converted to use the input<->compositor interface. This will eventually be
  // removed.
  virtual LayerTreeHostImpl& GetImplDeprecated() = 0;
  virtual const LayerTreeHostImpl& GetImplDeprecated() const = 0;
};

}  // namespace cc

#endif  // CC_INPUT_COMPOSITOR_INPUT_INTERFACES_H_
