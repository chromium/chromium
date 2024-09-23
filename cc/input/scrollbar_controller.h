// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_INPUT_SCROLLBAR_CONTROLLER_H_
#define CC_INPUT_SCROLLBAR_CONTROLLER_H_

#include <memory>
#include <optional>

#include "base/cancelable_callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "cc/cc_export.h"
#include "cc/input/input_handler.h"
#include "cc/input/scrollbar.h"
#include "cc/layers/layer_impl.h"
#include "cc/layers/painted_scrollbar_layer_impl.h"

// High level documentation:
// https://source.chromium.org/chromium/chromium/src/+/main:cc/input/README.md

// Click scrolling.
// - A click is considered as a kMouseDown and a kMouseUp in quick succession.
// Every click on a composited non-custom arrow leads to 3 GestureEvents in
// total.
// - GSB and GSU on get queued in the CTEQ on mousedown and a GSE on mouseup.
// - The delta scrolled is constant at 40px (scaled by the device_scale_factor)
// for scrollbar arrows and a function of the viewport length in the case of
// track autoscroll.

// Thumb dragging.
// - The sequence of events in the CTEQ would be something like GSB, GSU, GSU,
// GSU..., GSE
// - On every pointermove, the scroll delta is determined is as current pointer
// position - the point at which we got the initial mousedown.
// - The delta is then scaled by the scroller to scrollbar ratio so that
// dragging the thumb moves the scroller proportionately.
// - This ratio is calculated as:
// (scroll_layer_length - viewport_length) /
// (scrollbar_track_length - scrollbar_thumb_length)
// - On pointerup, the GSE clears state as mentioned above.

// VSync aligned autoscroll.
// - Autoscroll is implemented as a "scroll animation" which has a linear timing
// function (see cc::LinearTimingFunction) and a curve with a constant velocity.
// - The main thread does autoscrolling by pumping events at 50ms interval. To
// have a similar kind of behaviour on the compositor thread, the autoscroll
// velocity is set to 800px per second for scrollbar arrows.
// - For track autoscrolling however, the velocity is a function of the viewport
// length.
// - Based on this velocity, an autoscroll curve is created.
// - An autoscroll animation is set up. (via
// LayerTreeHostImpl::ScrollAnimationCreateInternal) on the the known
// scroll_node and the scroller starts animation when the pointer is held.

// Nuances:
// Thumb snapping.
// - During a thumb drag, if a pointer moves too far away from the scrollbar
// track, the thumb is supposed to snap back to it original place (i.e to the
// point before the thumb drag started).
// - This is done by having an imaginary no_snap_rect around the scrollbar
// track. This extends about 8 times the width of the track on either side. When
// a manipulation is in progress, the mouse is expected to stay within the
// bounds of this rect. Assuming a standard scrollbar, 17px wide, this is how
// it'd look like.
// https://github.com/rahul8805/CompositorThreadedScrollbarDocs/blob/master/snap.PNG?raw=true

// - When a pointerdown is received, record the original offset of the thumb.
// - On every pointermove, check if the pointer is within the bounds of the
// no_snap_rect. If false, snap to the initial_scroll_offset and stop processing
// pointermove(s) until the pointer reenters the bounds of the rect.
// - The moment the mouse re-enters the bounds of the no_snap_rect, we snap to
// the initial_scroll_offset + event.PositionInWidget.

// Thumb anchoring.
// - During a thumb drag, if the pointer runs off the track, there should be no
// additional scrolling until the pointer reenters the track and crosses the
// original mousedown point.
// - This is done by sending "clamped" deltas. The amount of scrollable delta is
// computed using LayerTreeHostImpl::ComputeScrollDelta.
// - Since the deltas are clamped, overscroll doesn't occur if it can't be
// consumed by the CurrentlyScrollingNode.

// Autoscroll play/pause.
// - When the pointer moves in and out of bounds of a scrollbar part that can
// initiate autoscrolls (like arrows or track), the autoscroll animation is
// expected to play or pause accordingly.
// - On every ScrollbarController::WillBeginMainFrame, the pointer location is
// constantly checked and if it is outside the bounds of the scrollbar part that
// initiated the autoscroll, the autoscroll is stopped.
// - Similarly, when the pointer reenters the bounds, autoscroll is restarted
// again. All the vital information during autoscrolling such the velocity,
// direction, scroll layer length etc is held in
// cc::ScrollbarController::AutoscrollState.

// Shift + click.
// - Doing a shift click on any part of a scrollbar track is supposed to do an
// instant scroll to that location (such that the thumb is still centered on the
// pointer).
// - When the MouseEvent reaches the
// InputHandlerProxy::RouteToTypeSpecificHandler, if the event is found to have
// a "Shift" modifier, the ScrollbarController calculates the offset based on
// the pointers current location on the track.
// - Once the offset is determined, the InputHandlerProxy creates a GSU with
// state that tells the LayerTreeHostImpl to perform a non-animated scroll to
// the offset.

// Continuous autoscrolling.
// - This builds on top of the autoscolling implementation. "Continuous"
// autoscrolling is when an autoscroll is in progress and the size of the
// content keeps increasing. For eg: When you keep the down arrow pressed on
// websites like Facebook, the autoscrolling is expected to keep on going until
// the mouse is released.
// - This is implemented by monitoring the length of the scroller layer at every
// frame and if the length increases (and if autoscroll in the forward direction
// is already in progress), the old animation is aborted and a new autoscroll
// animation with the new scroller length is kicked off.

namespace cc {
class LayerTreeHostImpl;

// This class is responsible for hit testing composited scrollbars, event
// handling and creating gesture scroll deltas.
class CC_EXPORT ScrollbarController {
 public:
  explicit ScrollbarController(LayerTreeHostImpl*);
  virtual ~ScrollbarController();

  // On Mac, the "jump to the spot that's clicked" setting can be dynamically
  // set via System Preferences. When enabled, the expectation is that regular
  // clicks on the scrollbar should make the scroller "jump" to the clicked
  // location rather than animated scrolling. Additionally, when this is enabled
  // and the user does an Option + click on the scrollbar, the scroller should
  // *not* jump to that spot (i.e it should be treated as a regular track
  // click). When this setting is disabled on the Mac, Option + click should
  // make the scroller jump and a regular click should animate the scroll
  // offset. On all other platforms, the "jump on click" option is available
  // (via Shift + click) but is not configurable.
  InputHandlerPointerResult HandlePointerDown(
      const gfx::PointF position_in_widget,
      const bool jump_key_modifier);
  InputHandlerPointerResult HandlePointerMove(
      const gfx::PointF position_in_widget);
  InputHandlerPointerResult HandlePointerUp(
      const gfx::PointF position_in_widget);

  bool AutoscrollTaskIsScheduled() const {
    return cancelable_autoscroll_task_ != nullptr;
  }
  bool ScrollbarScrollIsActive() const { return scrollbar_scroll_is_active_; }
  void DidRegisterScrollbar(ElementId element_id,
                            ScrollbarOrientation orientation);
  void DidUnregisterScrollbar(ElementId element_id,
                              ScrollbarOrientation orientation);
  ScrollbarLayerImplBase* ScrollbarLayer() const;
  void WillBeginImplFrame();
  void ResetState();
  const ScrollbarLayerImplBase* HitTest(
      const gfx::PointF position_in_widget) const;

 private:
  FRIEND_TEST_ALL_PREFIXES(LayerTreeHostImplTest, ThumbDragAfterJumpClick);
  FRIEND_TEST_ALL_PREFIXES(LayerTreeHostImplTest,
                           AbortAnimatedScrollBeforeStartingAutoscroll);
  FRIEND_TEST_ALL_PREFIXES(LayerTreeHostImplTest, AutoscrollOnDeletedScrollbar);
  FRIEND_TEST_ALL_PREFIXES(LayerTreeHostImplTest, ScrollOnLargeThumb);

  // "Autoscroll" here means the continuous scrolling that occurs when the
  // pointer is held down on a hit-testable area of the scrollbar such as an
  // arrows of the track itself.
  enum class AutoScrollDirection { kAutoscrollForward, kAutoscrollBackward };

  enum class AutoScrollStatus {
    // For when the 250ms delay before an autoscroll starts animating has not
    // yet elapsed
    kAutoscrollWaiting,
    // For when the delay has elapsed, but the autoscroll cannot animate for
    // some reason (the scrollbar being unregistered)
    kAutoscrollReady,
    // For when the autoscroll is animating
    kAutoscrollScrolling
  };

  struct CC_EXPORT AutoScrollState {
    // Can only be either kAutoscrollForward or kAutoscrollBackward.
    AutoScrollDirection direction = AutoScrollDirection::kAutoscrollForward;

    AutoScrollStatus status = AutoScrollStatus::kAutoscrollWaiting;

    // Stores the autoscroll velocity. The sign is used to set the "direction".
    float velocity = 0.f;

    // Used to track the scroller length while autoscrolling. Helpful for
    // setting up infinite scrolling.
    float scroll_layer_length = 0.f;

    // Used to lookup the rect corresponding to the ScrollbarPart so that
    // autoscroll animations can be played/paused depending on the current
    // pointer location.
    ScrollbarPart pressed_scrollbar_part;
  };

  struct CC_EXPORT DragState {
    // This marks the point at which the drag initiated (relative to the
    // scrollbar layer).
    gfx::PointF drag_origin;

    // This is needed for thumb snapping when the pointer moves too far away
    // from the track while scrolling.
    float scroll_position_at_start_;

    // The |scroller_displacement| indicates the scroll offset compensation that
    // needs to be applied when the scroller's length changes dynamically mid
    // thumb drag. This is needed done to ensure that the scroller does not jump
    // while a thumb drag is in progress.
    float scroller_displacement;
    float scroller_length_at_previous_move;
  };

  struct CC_EXPORT CapturedScrollbarMetadata {
    // Needed to retrieve the ScrollbarSet for a particular ElementId.
    ElementId scroll_element_id;

    // Needed to identify the correct scrollbar from the ScrollbarSet.
    ScrollbarOrientation orientation;
  };

  // Posts an autoscroll task based on the autoscroll state, with the given
  // delay
  void PostAutoscrollTask(const base::TimeDelta delay);

  // Initiates an autoscroll, setting the necessary status and starting the
  // animation, if possible
  void StartAutoScroll();

  // Starts/restarts an autoscroll animation based off of the information in
  // autoscroll_state_
  void StartAutoScrollAnimation();

  // Returns the DSF based on whether use-zoom-for-dsf is enabled.
  float ScreenSpaceScaleFactor() const;

  // Helper to convert scroll offset to autoscroll velocity.
  float InitialDeltaToAutoscrollVelocity(gfx::Vector2dF scroll_delta) const;

  // Returns the hit tested ScrollbarPart based on the position_in_widget.
  ScrollbarPart GetScrollbarPartFromPointerDown(
      const gfx::PointF position_in_widget) const;

  // Clamps |scroll_delta| based on the available scrollable amount of
  // |target_node|. The returned delta includes the page scale factor and is
  // appropriate for use directly as a delta for GSU.
  gfx::Vector2dF ComputeClampedDelta(const ScrollNode& target_node,
                                     const gfx::Vector2dF& scroll_delta) const;

  // Returns the rect for the ScrollbarPart.
  gfx::Rect GetRectForScrollbarPart(const ScrollbarPart scrollbar_part) const;

  LayerImpl* GetLayerHitByPoint(const gfx::PointF position_in_widget) const;

  // Returns scroll delta as Vector2dF based on which ScrollbarPart was hit
  // tested.
  gfx::Vector2dF GetScrollDeltaForScrollbarPart(
      const ScrollbarPart scrollbar_part,
      const bool jump_key_modifier) const;
  // Returns scroll delta in the direction of the scrollbar's orientation.
  float GetScrollDistanceForScrollbarPart(const ScrollbarPart scrollbar_part,
                                          const bool jump_key_modifier) const;

  // Makes position_in_widget relative to the scrollbar.
  gfx::PointF GetScrollbarRelativePosition(const gfx::PointF position_in_widget,
                                           bool* clipped) const;

  // Computes an aritificial drag origin for jump clicks, to give the scrollbar
  // a proper place to snap back to on a jump click then drag
  gfx::PointF DragOriginForJumpClick(
      const ScrollbarLayerImplBase* scrollbar) const;

  // Decides if the scroller should snap to the offset that it was
  // originally at (i.e the offset before the thumb drag).
  bool SnapToDragOrigin(const gfx::PointF pointer_position_in_widget) const;

  // Decides whether a track autoscroll should be aborted (or restarted) due to
  // the thumb reaching the pointer or the pointer leaving (or re-entering) the
  // bounds.
  void RecomputeAutoscrollStateIfNeeded();

  // Shift (or "Option" in case of Mac) + click is expected to do a non-animated
  // jump to a certain offset.
  float GetScrollDistanceForAbsoluteJump() const;

  // Determines if the delta needs to be animated.
  ui::ScrollGranularity Granularity(const ScrollbarPart scrollbar_part,
                                    bool jump_key_modifier) const;

  // Calculates the distance based on position_in_widget and drag_origin.
  float GetScrollDistanceForDragPosition(
      const gfx::PointF pointer_position_in_widget) const;

  // Returns the ratio of the scroller length to the scrollbar length. This is
  // needed to scale the scroll delta for thumb drag.
  float GetScrollerToScrollbarRatio() const;

  float GetViewportLength() const;

  // Returns the pixel distance for a percent-based scroll of the scrollbar
  float GetScrollDistanceForPercentBasedScroll() const;

  // Returns the page scale factor (i.e. pinch zoom factor). This is relevant
  // for root viewport scrollbar scrolling.
  float GetPageScaleFactorForScroll() const;

  raw_ptr<LayerTreeHostImpl> layer_tree_host_impl_;

  // Used to safeguard against firing GSE without firing GSB and GSU. For
  // example, if mouse is pressed outside the scrollbar but released after
  // moving inside the scrollbar, a GSE will get queued up without this flag.
  bool scrollbar_scroll_is_active_;

  // This is relative to the RenderWidget's origin.
  gfx::PointF last_known_pointer_position_;

  // Set only while interacting with the scrollbar (eg: drag, click etc).
  std::optional<CapturedScrollbarMetadata> captured_scrollbar_metadata_;

  // Holds information pertaining to autoscrolling. This member is empty if and
  // only if an autoscroll is *not* in progress or scheduled
  std::optional<AutoScrollState> autoscroll_state_;

  // Holds information pertaining to thumb drags. Useful while making decisions
  // about thumb anchoring/snapping.
  std::optional<DragState> drag_state_;

  // Used to track if a GSU was processed for the current frame or not. Without
  // this, thumb drag will appear jittery. The reason this happens is because
  // when the first GSU is processed, it gets queued in the compositor thread
  // event queue. So a second request within the same frame will end up
  // calculating an incorrect delta (as ComputeThumbQuadRect would not have
  // accounted for the delta in the first GSU that was not yet dispatched and
  // pointermoves are not VSync aligned).
  bool drag_processed_for_current_frame_;

  std::unique_ptr<base::CancelableOnceClosure> cancelable_autoscroll_task_;
};

}  // namespace cc

#endif  // CC_INPUT_SCROLLBAR_CONTROLLER_H_
