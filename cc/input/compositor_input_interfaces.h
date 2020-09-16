// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_INPUT_COMPOSITOR_INPUT_INTERFACES_H_
#define CC_INPUT_COMPOSITOR_INPUT_INTERFACES_H_

#include "base/time/time.h"
#include "cc/input/scrollbar.h"
#include "cc/paint/element_id.h"

namespace viz {
struct BeginFrameArgs;
}

namespace cc {

struct CompositorCommitData;

// This is the interface that LayerTreeHostImpl and the "graphics" side of the
// compositor uses to talk to the compositor ThreadedInputHandler. This
// interface is two-way; it's used used both to communicate state changes from
// the LayerTree to the input handler and also to query and update state in the
// input handler.
class InputDelegateForCompositor {
 public:
  // Called during a commit to fill in the changes that have occurred since the
  // last commit.
  virtual void ProcessCommitDeltas(CompositorCommitData* commit_data) = 0;

  // Called to let the input handler perform animations.
  virtual void TickAnimations(base::TimeTicks monotonic_time) = 0;

  // Compositor lifecycle state observation.
  virtual void WillShutdown() = 0;
  virtual void WillDraw() = 0;
  virtual void WillBeginImplFrame(const viz::BeginFrameArgs& args) = 0;
  virtual void DidCommit() = 0;
  virtual void DidActivatePendingTree() = 0;

  // Called when the state of the "root layer" may have changed from outside
  // the input system. The state includes: scroll offset, scrollable size,
  // scroll limits, page scale, page scale limits.
  virtual void RootLayerStateMayHaveChanged() = 0;

  // Called to let the input handler know that a scrollbar for the given
  // elementId has been removed.
  virtual void DidUnregisterScrollbar(ElementId scroll_element_id,
                                      ScrollbarOrientation orientation) = 0;

  // Called to let the input handler know that a scroll offset animation has
  // completed.
  virtual void ScrollOffsetAnimationFinished() = 0;

  // Returns true if we're currently in a "gesture" (user-initiated) scroll.
  // That is, between a GestureScrollBegin and a GestureScrollEnd. Note, a
  // GestureScrollEnd is deferred if the gesture ended but we're still
  // animating the scroll to its final position (e.g. the user released their
  // finger from the touchscreen but we're scroll snapping).
  virtual bool IsCurrentlyScrolling() const = 0;

  // Returns true if there is an active scroll in progress.  "Active" here
  // means that it's been latched (i.e. we have a CurrentlyScrollingNode()) but
  // also that some ScrollUpdates have been received and their delta consumed
  // for scrolling. These can differ significantly e.g. the page allows the
  // touchstart but preventDefaults all the touchmoves. In that case, we latch
  // and have a CurrentlyScrollingNode() but will never receive a ScrollUpdate.
  //
  // "Precision" means it's a non-animated scroll like a touchscreen or
  // high-precision touchpad. The latter distinction is important for things
  // like scheduling decisions which might schedule a wheel and a touch
  // scrolling differently due to user perception.
  virtual bool IsActivelyPrecisionScrolling() const = 0;
};

}  // namespace cc

#endif  // CC_INPUT_COMPOSITOR_INPUT_INTERFACES_H_
