// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_INPUT_SCROLLBAR_CONTROLLER_H_
#define CC_INPUT_SCROLLBAR_CONTROLLER_H_

#include "cc/cc_export.h"
#include "cc/input/input_handler.h"
#include "cc/input/scrollbar.h"
#include "cc/layers/layer_impl.h"
#include "cc/layers/painted_scrollbar_layer_impl.h"

namespace cc {
// This class is responsible for hit testing composited scrollbars, event
// handling and creating gesture scroll deltas.
class CC_EXPORT ScrollbarController {
 public:
  explicit ScrollbarController(LayerTreeHostImpl*);
  virtual ~ScrollbarController();

  InputHandlerPointerResult HandlePointerDown(
      const gfx::PointF position_in_widget,
      const bool shift_modifier);
  InputHandlerPointerResult HandlePointerMove(
      const gfx::PointF position_in_widget);
  InputHandlerPointerResult HandlePointerUp(
      const gfx::PointF position_in_widget);

  // "velocity" here is calculated based on the initial scroll delta (See
  // InitialDeltaToAutoscrollVelocity). This value carries a "sign" which is
  // needed to determine whether we should set up the autoscrolling in the
  // forwards or the backwards direction.
  void StartAutoScrollAnimation(float velocity,
                                const ScrollbarLayerImplBase* scrollbar,
                                ScrollbarPart pressed_scrollbar_part);
  bool ScrollbarScrollIsActive() { return scrollbar_scroll_is_active_; }
  void DidUnregisterScrollbar(ElementId element_id);
  ScrollbarLayerImplBase* ScrollbarLayer();
  void WillBeginImplFrame();

 private:
  // "Autoscroll" here means the continuous scrolling that occurs when the
  // pointer is held down on a hit-testable area of the scrollbar such as an
  // arrows of the track itself.
  enum AutoScrollDirection { AUTOSCROLL_FORWARD, AUTOSCROLL_BACKWARD };

  struct CC_EXPORT AutoScrollState {
    // Can only be either AUTOSCROLL_FORWARD or AUTOSCROLL_BACKWARD.
    AutoScrollDirection direction = AutoScrollDirection::AUTOSCROLL_FORWARD;

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
    // This is used to track the pointer location relative to the thumb origin
    // when a drag has started.
    gfx::Vector2dF anchor_relative_to_thumb_;

    // This is needed for thumb snapping when the pointer moves too far away
    // from the track while scrolling.
    float scroll_position_at_start_;
  };

  struct CC_EXPORT CapturedScrollbarMetadata {
    // Needed to retrieve the ScrollbarSet for a particular ElementId.
    ElementId scroll_element_id;

    // Needed to identify the correct scrollbar from the ScrollbarSet.
    ScrollbarOrientation orientation;
  };

  // Returns the DSF based on whether use-zoom-for-dsf is enabled.
  float ScreenSpaceScaleFactor() const;

  // Helper to convert scroll offset to autoscroll velocity.
  float InitialDeltaToAutoscrollVelocity(
      const ScrollbarLayerImplBase* scrollbar,
      gfx::ScrollOffset scroll_offset) const;

  // Returns the hit tested ScrollbarPart based on the position_in_widget.
  ScrollbarPart GetScrollbarPartFromPointerDown(
      const ScrollbarLayerImplBase* scrollbar,
      const gfx::PointF position_in_widget);

  // Returns scroll offsets based on which ScrollbarPart was hit tested.
  gfx::ScrollOffset GetScrollOffsetForScrollbarPart(
      const ScrollbarLayerImplBase* scrollbar,
      const ScrollbarPart scrollbar_part,
      const bool shift_modifier);

  // Returns the rect for the ScrollbarPart.
  gfx::Rect GetRectForScrollbarPart(const ScrollbarLayerImplBase* scrollbar,
                                    const ScrollbarPart scrollbar_part);

  LayerImpl* GetLayerHitByPoint(const gfx::PointF position_in_widget);
  int GetScrollDeltaForScrollbarPart(const ScrollbarLayerImplBase* scrollbar,
                                     const ScrollbarPart scrollbar_part,
                                     const bool shift_modifier);

  // Makes position_in_widget relative to the scrollbar.
  gfx::PointF GetScrollbarRelativePosition(
      const ScrollbarLayerImplBase* scrollbar,
      const gfx::PointF position_in_widget,
      bool* clipped);

  // Decides if the scroller should snap to the offset that it was originally at
  // (i.e the offset before the thumb drag).
  bool SnapToDragOrigin(const ScrollbarLayerImplBase* scrollbar,
                        const gfx::PointF pointer_position_in_widget);

  // Decides whether a track autoscroll should be aborted (or restarted) due to
  // the thumb reaching the pointer or the pointer leaving (or re-entering) the
  // bounds.
  void RecomputeAutoscrollStateIfNeeded();
  void ResetState();

  // Shift (or "Option" in case of Mac) + click is expected to do a non-animated
  // jump to a certain offset.
  float GetScrollDeltaForAbsoluteJump(const ScrollbarLayerImplBase* scrollbar);

  // Determines if the delta needs to be animated.
  ui::input_types::ScrollGranularity Granularity(
      const ScrollbarPart scrollbar_part,
      bool shift_modifier);

  // Calculates the scroll_offset based on position_in_widget and
  // drag_anchor_relative_to_thumb_.
  gfx::ScrollOffset GetScrollOffsetForDragPosition(
      const ScrollbarLayerImplBase* scrollbar,
      const gfx::PointF pointer_position_in_widget);

  // Returns a Vector2dF for position_in_widget relative to the scrollbar thumb.
  gfx::Vector2dF GetThumbRelativePoint(const ScrollbarLayerImplBase* scrollbar,
                                       const gfx::PointF position_in_widget);

  // Returns the ratio of the scroller length to the scrollbar length. This is
  // needed to scale the scroll delta for thumb drag.
  float GetScrollerToScrollbarRatio(const ScrollbarLayerImplBase* scrollbar);
  LayerTreeHostImpl* layer_tree_host_impl_;

  // Used to safeguard against firing GSE without firing GSB and GSU. For
  // example, if mouse is pressed outside the scrollbar but released after
  // moving inside the scrollbar, a GSE will get queued up without this flag.
  bool scrollbar_scroll_is_active_;

  // This is relative to the RenderWidget's origin.
  gfx::PointF last_known_pointer_position_;

  // Set only while interacting with the scrollbar (eg: drag, click etc).
  base::Optional<CapturedScrollbarMetadata> captured_scrollbar_metadata_;

  // Holds information pertaining to autoscrolling. This member is empty if and
  // only if an autoscroll is *not* in progress.
  base::Optional<AutoScrollState> autoscroll_state_;

  // Holds information pertaining to thumb drags. Useful while making decisions
  // about thumb anchoring/snapping.
  base::Optional<DragState> drag_state_;

  // Used to track if a GSU was processed for the current frame or not. Without
  // this, thumb drag will appear jittery. The reason this happens is because
  // when the first GSU is processed, it gets queued in the compositor thread
  // event queue. So a second request within the same frame will end up
  // calculating an incorrect delta (as ComputeThumbQuadRect would not have
  // accounted for the delta in the first GSU that was not yet dispatched and
  // pointermoves are not VSync aligned).
  bool drag_processed_for_current_frame_;

  std::unique_ptr<base::CancelableClosure> cancelable_autoscroll_task_;
};

}  // namespace cc

#endif  // CC_INPUT_SCROLLBAR_CONTROLLER_H_
