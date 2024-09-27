// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/scrollbar_layer_impl_base.h"

#include <algorithm>

#include "base/cancelable_callback.h"
#include "cc/base/features.h"
#include "cc/base/math_util.h"
#include "cc/input/scroll_utils.h"
#include "cc/input/scrollbar.h"
#include "cc/input/scrollbar_controller.h"
#include "cc/layers/viewport.h"
#include "cc/trees/layer_tree_impl.h"
#include "cc/trees/scroll_node.h"

namespace cc {
ScrollbarController::~ScrollbarController() {
  if (cancelable_autoscroll_task_) {
    cancelable_autoscroll_task_->Cancel();
    cancelable_autoscroll_task_.reset();
  }
}

ScrollbarController::ScrollbarController(
    LayerTreeHostImpl* layer_tree_host_impl)
    : layer_tree_host_impl_(layer_tree_host_impl),
      scrollbar_scroll_is_active_(false),
      last_known_pointer_position_(gfx::PointF(0, 0)),
      drag_processed_for_current_frame_(false),
      cancelable_autoscroll_task_(nullptr) {}

void ScrollbarController::WillBeginImplFrame() {
  drag_processed_for_current_frame_ = false;
  RecomputeAutoscrollStateIfNeeded();
}

// Retrieves the ScrollbarLayerImplBase corresponding to the stashed ElementId.
ScrollbarLayerImplBase* ScrollbarController::ScrollbarLayer() const {
  if (!captured_scrollbar_metadata_.has_value())
    return nullptr;

  const ScrollbarSet scrollbars = layer_tree_host_impl_->ScrollbarsFor(
      captured_scrollbar_metadata_->scroll_element_id);
  for (ScrollbarLayerImplBase* scrollbar : scrollbars) {
    if (captured_scrollbar_metadata_->orientation == scrollbar->orientation())
      return scrollbar;
  }
  return nullptr;
}

const ScrollbarLayerImplBase* ScrollbarController::HitTest(
    const gfx::PointF position_in_widget) const {
  // If a non-custom scrollbar layer was not found, we return early as there is
  // no point in setting additional state in the ScrollbarController. Return an
  // empty InputHandlerPointerResult in this case so that when it is bubbled up
  // to InputHandlerProxy::RouteToTypeSpecificHandler, the pointer event gets
  // passed on to the main thread.
  const LayerImpl* layer_impl = GetLayerHitByPoint(position_in_widget);
  if (!(layer_impl && layer_impl->IsScrollbarLayer()))
    return nullptr;

  // If the scrollbar layer has faded out (eg: Overlay scrollbars), don't
  // initiate a scroll.
  const ScrollbarLayerImplBase* scrollbar = ToScrollbarLayer(layer_impl);
  return scrollbar->OverlayScrollbarOpacity() == 0.f ? nullptr : scrollbar;
}

// Performs hit test and prepares scroll deltas that will be used by GSB and
// GSU.
InputHandlerPointerResult ScrollbarController::HandlePointerDown(
    const gfx::PointF position_in_widget,
    bool jump_key_modifier) {
  const ScrollbarLayerImplBase* scrollbar = HitTest(position_in_widget);
  if (!scrollbar) {
    return InputHandlerPointerResult();
  }

  captured_scrollbar_metadata_ = CapturedScrollbarMetadata();
  captured_scrollbar_metadata_->scroll_element_id =
      scrollbar->scroll_element_id();
  captured_scrollbar_metadata_->orientation = scrollbar->orientation();

  InputHandlerPointerResult scroll_result;
  scroll_result.target_scroller = scrollbar->scroll_element_id();
  scroll_result.type = PointerResultType::kScrollbarScroll;
  const ScrollbarPart scrollbar_part =
      GetScrollbarPartFromPointerDown(position_in_widget);
  const bool perform_jump_click_on_track =
      scrollbar->JumpOnTrackClick() != jump_key_modifier;
  scroll_result.scroll_delta = GetScrollDeltaForScrollbarPart(
      scrollbar_part, perform_jump_click_on_track);
  last_known_pointer_position_ = position_in_widget;
  scrollbar_scroll_is_active_ = true;
  scroll_result.scroll_units =
      Granularity(scrollbar_part, perform_jump_click_on_track);

  // Initialize drag state if either the scrollbar thumb is being dragged OR the
  // user has initiated a jump click (since the thumb would have jumped under
  // the pointer).
  if (scrollbar_part == ScrollbarPart::kThumb || perform_jump_click_on_track) {
    drag_state_ = DragState();
    bool clipped = false;

    drag_state_->drag_origin =
        perform_jump_click_on_track
            ? DragOriginForJumpClick(scrollbar)
            : GetScrollbarRelativePosition(position_in_widget, &clipped);

    // If the point were clipped we shouldn't have hit tested to a valid
    // part.
    DCHECK(!clipped);

    // Record the current scroller offset. This will be needed to snap the
    // thumb back to its original position if the pointer moves too far away
    // from the track during a thumb drag.
    drag_state_->scroll_position_at_start_ = scrollbar->current_pos();
    drag_state_->scroller_length_at_previous_move =
        scrollbar->scroll_layer_length();
  }

  if (!scroll_result.scroll_delta.IsZero() && !perform_jump_click_on_track) {
    // Thumb drag is the only scrollbar manipulation that cannot produce an
    // autoscroll. All other interactions like clicking on arrows/trackparts
    // have the potential of initiating an autoscroll (if held down for long
    // enough).
    DCHECK(scrollbar_part != ScrollbarPart::kThumb);
    autoscroll_state_ = AutoScrollState();
    autoscroll_state_->velocity =
        InitialDeltaToAutoscrollVelocity(scroll_result.scroll_delta);
    autoscroll_state_->pressed_scrollbar_part = scrollbar_part;
    PostAutoscrollTask(kInitialAutoscrollTimerDelay);
  }
  return scroll_result;
}

void ScrollbarController::PostAutoscrollTask(const base::TimeDelta delay) {
  cancelable_autoscroll_task_ =
      std::make_unique<base::CancelableOnceClosure>(base::BindOnce(
          &ScrollbarController::StartAutoScroll, base::Unretained(this)));
  layer_tree_host_impl_->GetTaskRunner()->PostDelayedTask(
      FROM_HERE, cancelable_autoscroll_task_->callback(), delay);
}

gfx::PointF ScrollbarController::DragOriginForJumpClick(
    const ScrollbarLayerImplBase* scrollbar) const {
  // If the user initiated a jump click, create an artificial drag origin to the
  // middle of the thumb's current position. This is to simulate a jump click as
  // though the user had initiated a drag normally and pulled the thumb down to
  // the jump click position. This also keeps consistency with
  // scroll_position_at_start_ which is used both to calculate scroll positions
  // as well as for snapping back to origin if the user moves their mouse away.
  gfx::Rect thumb = scrollbar->ComputeThumbQuadRect();
  return scrollbar->orientation() == ScrollbarOrientation::kHorizontal
             ? gfx::PointF(thumb.x() + thumb.width() / 2, 0)
             : gfx::PointF(0, thumb.y() + thumb.height() / 2);
}

bool ScrollbarController::SnapToDragOrigin(
    const gfx::PointF pointer_position_in_widget) const {
  // Consult the ScrollbarTheme to check if thumb snapping is supported on the
  // current platform.
  const ScrollbarLayerImplBase* scrollbar = ScrollbarLayer();
  if (!(scrollbar && scrollbar->SupportsDragSnapBack()))
    return false;

  bool clipped = false;
  const gfx::PointF pointer_position_in_layer =
      GetScrollbarRelativePosition(pointer_position_in_widget, &clipped);

  if (clipped)
    return false;

  const ScrollbarOrientation orientation = scrollbar->orientation();
  const gfx::Rect forward_track_rect = scrollbar->ForwardTrackRect();

  // When dragging the thumb, there needs to exist "gutters" on either side of
  // the track. The thickness of these gutters is a multiple of the track (or
  // thumb) thickness. As long as the pointer remains within the bounds of these
  // gutters in the non-scrolling direction, thumb drag proceeds as expected.
  // The moment the pointer moves outside the bounds, the scroller needs to snap
  // back to the drag_origin (aka the scroll offset of the parent scroller
  // before the thumb drag initiated).
  int track_thickness = orientation == ScrollbarOrientation::kVertical
                            ? forward_track_rect.width()
                            : forward_track_rect.height();

  if (!track_thickness) {
    // For overlay scrollbars (or for tests that do not set up a track
    // thickness), use the thumb_thickness instead to determine the gutters.
    const int thumb_thickness = scrollbar->ThumbThickness();

    // If the thumb doesn't have thickness, the gutters can't be determined.
    // Snapping shouldn't occur in this case.
    if (!thumb_thickness)
      return false;

    track_thickness = thumb_thickness;
  }

  const float gutter_thickness = kOffSideMultiplier * track_thickness;
  const float gutter_min_bound =
      orientation == ScrollbarOrientation::kVertical
          ? (forward_track_rect.x() - gutter_thickness)
          : (forward_track_rect.y() - gutter_thickness);
  const float gutter_max_bound =
      orientation == ScrollbarOrientation::kVertical
          ? (forward_track_rect.x() + track_thickness + gutter_thickness)
          : (forward_track_rect.y() + track_thickness + gutter_thickness);

  const float pointer_location = orientation == ScrollbarOrientation::kVertical
                                     ? pointer_position_in_layer.x()
                                     : pointer_position_in_layer.y();

  return pointer_location < gutter_min_bound ||
         pointer_location > gutter_max_bound;
}

ui::ScrollGranularity ScrollbarController::Granularity(
    const ScrollbarPart scrollbar_part,
    const bool jump_key_modifier) const {
  const bool shift_click_on_scrollbar_track =
      jump_key_modifier && (scrollbar_part == ScrollbarPart::kForwardTrack ||
                            scrollbar_part == ScrollbarPart::kBackTrack);
  if (shift_click_on_scrollbar_track ||
      scrollbar_part == ScrollbarPart::kThumb) {
    return ui::ScrollGranularity::kScrollByPrecisePixel;
  }

  // TODO(arakeri): This needs to be updated to kLine once cc implements
  // handling it. crbug.com/959441
  return ui::ScrollGranularity::kScrollByPixel;
}

float ScrollbarController::GetScrollDistanceForAbsoluteJump() const {
  bool clipped = false;
  const gfx::PointF pointer_position_in_layer =
      GetScrollbarRelativePosition(last_known_pointer_position_, &clipped);

  if (clipped)
    return 0;

  const ScrollbarLayerImplBase* scrollbar = ScrollbarLayer();
  const float pointer_location =
      scrollbar->orientation() == ScrollbarOrientation::kVertical
          ? pointer_position_in_layer.y()
          : pointer_position_in_layer.x();

  // During a shift + click, the pointers current location (on the track) needs
  // to be considered as the center of the thumb and the thumb origin needs to
  // be calculated based on that. This will ensure that when shift + click is
  // processed, the thumb will be centered on the pointer.
  const int thumb_length = scrollbar->ThumbLength();
  const float desired_thumb_origin = pointer_location - thumb_length / 2.f;

  const gfx::Rect thumb_rect(scrollbar->ComputeThumbQuadRect());
  const float current_thumb_origin =
      scrollbar->orientation() == ScrollbarOrientation::kVertical
          ? thumb_rect.y()
          : thumb_rect.x();

  const float distance =
      round(std::abs(desired_thumb_origin - current_thumb_origin));
  return distance * GetScrollerToScrollbarRatio() *
         GetPageScaleFactorForScroll();
}

float ScrollbarController::GetScrollDistanceForDragPosition(
    const gfx::PointF pointer_position_in_widget) const {
  const ScrollbarLayerImplBase* scrollbar = ScrollbarLayer();
  // Convert the move position to scrollbar layer relative for comparison with
  // |drag_state_| drag_origin. Ignore clipping as if we're within the region
  // that doesn't cause snapping back, we do want the delta in the appropriate
  // dimension to cause a scroll.
  bool clipped = false;
  const gfx::PointF scrollbar_relative_position(
      GetScrollbarRelativePosition(pointer_position_in_widget, &clipped));
  float pointer_delta =
      scrollbar->orientation() == ScrollbarOrientation::kVertical
          ? scrollbar_relative_position.y() - drag_state_->drag_origin.y()
          : scrollbar_relative_position.x() - drag_state_->drag_origin.x();

  const float new_offset = pointer_delta * GetScrollerToScrollbarRatio();
  float distance = drag_state_->scroll_position_at_start_ + new_offset -
                   scrollbar->current_pos();

  // The scroll delta computed is layer relative. In order to scroll the
  // correct amount, we have to convert the delta to be unscaled (i.e. multiply
  // by the page scale factor), as GSU deltas are always unscaled.
  distance *= GetPageScaleFactorForScroll();
  return distance;
}

// Performs hit test and prepares scroll deltas that will be used by GSU.
InputHandlerPointerResult ScrollbarController::HandlePointerMove(
    const gfx::PointF position_in_widget) {
  last_known_pointer_position_ = position_in_widget;
  RecomputeAutoscrollStateIfNeeded();
  InputHandlerPointerResult scroll_result;

  const ScrollbarLayerImplBase* scrollbar = ScrollbarLayer();
  if (!scrollbar || !drag_state_.has_value())
    return scroll_result;

  // If the scrollbar thumb is being dragged, it qualifies as a kScrollbarScroll
  // (although the delta might still be zero). Setting the "type" to
  // kScrollbarScroll ensures that the correct event modifier (in
  // InputHandlerProxy) is set which in-turn tells the main thread to invalidate
  // the respective scrollbar parts. This needs to be done for all
  // pointermove(s) since they are not VSync aligned.
  scroll_result.type = PointerResultType::kScrollbarScroll;

  // If a GSU was already produced for a thumb drag in this frame, there's no
  // point in continuing on. Please see the header file for details.
  if (drag_processed_for_current_frame_)
    return scroll_result;

  if (SnapToDragOrigin(position_in_widget)) {
    const float delta =
        scrollbar->current_pos() - drag_state_->scroll_position_at_start_;
    scroll_result.scroll_units = ui::ScrollGranularity::kScrollByPrecisePixel;
    scroll_result.scroll_delta =
        scrollbar->orientation() == ScrollbarOrientation::kVertical
            ? gfx::Vector2dF(0, -delta)
            : gfx::Vector2dF(-delta, 0);
    drag_processed_for_current_frame_ = true;
    return scroll_result;
  }

  // When initiating a thumb drag, a pointerdown and a pointermove can both
  // arrive a the ScrollbarController in succession before a GSB would have
  // been dispatched. So, querying LayerTreeHostImpl::CurrentlyScrollingNode()
  // can potentially be null. Hence, a better way to look the target_node to be
  // scrolled is by using ScrollbarLayerImplBase::scroll_element_id().
  const ScrollNode* target_node =
      layer_tree_host_impl_->active_tree()
          ->property_trees()
          ->scroll_tree()
          .FindNodeFromElementId(scrollbar->scroll_element_id());

  // If a scrollbar exists, it should always have an ElementId pointing to a
  // valid ScrollNode.
  DCHECK(target_node);

  float distance = GetScrollDistanceForDragPosition(position_in_widget);
  if (drag_state_->scroller_length_at_previous_move !=
      scrollbar->scroll_layer_length()) {
    drag_state_->scroller_displacement = distance;
    drag_state_->scroller_length_at_previous_move =
        scrollbar->scroll_layer_length();

    // This is done to ensure that, when the scroller length changes mid thumb
    // drag, the scroller shouldn't jump. We early out because the delta would
    // be zero in this case anyway (since drag_state_->scroller_displacement =
    // delta). So that means, in the worst case you'd miss 1 GSU every time the
    // scroller expands while a thumb drag is in progress.
    return scroll_result;
  }
  distance -= drag_state_->scroller_displacement;

  // If scroll_offset can't be consumed, there's no point in continuing on.
  const gfx::Vector2dF scroll_delta =
      scrollbar->orientation() == ScrollbarOrientation::kVertical
          ? gfx::Vector2dF(0, distance)
          : gfx::Vector2dF(distance, 0);
  const gfx::Vector2dF clamped_scroll_delta =
      ComputeClampedDelta(*target_node, scroll_delta);

  if (clamped_scroll_delta.IsZero())
    return scroll_result;

  // Thumb drags have more granularity and are purely dependent on the pointer
  // movement. Hence we use kPrecisePixel when dragging the thumb.
  scroll_result.scroll_units = ui::ScrollGranularity::kScrollByPrecisePixel;
  scroll_result.scroll_delta = clamped_scroll_delta;
  drag_processed_for_current_frame_ = true;

  return scroll_result;
}

gfx::Vector2dF ScrollbarController::ComputeClampedDelta(
    const ScrollNode& target_node,
    const gfx::Vector2dF& scroll_delta) const {
  DCHECK(!target_node.scrolls_inner_viewport);
  if (target_node.scrolls_outer_viewport)
    return layer_tree_host_impl_->viewport().ComputeClampedDelta(scroll_delta);

  // ComputeScrollDelta returns a delta accounting for the current page zoom
  // level. Since we're producing a delta for an injected GSU, we need to get
  // back to and unscaled delta (i.e. multiply by the page scale factor).
  gfx::Vector2dF clamped_delta =
      layer_tree_host_impl_->GetInputHandler().ComputeScrollDelta(target_node,
                                                                  scroll_delta);
  const float scale_factor = GetPageScaleFactorForScroll();
  clamped_delta.Scale(scale_factor);
  return clamped_delta;
}

float ScrollbarController::GetScrollerToScrollbarRatio() const {
  // Calculating the delta by which the scroller layer should move when
  // dragging the thumb depends on the following factors:
  // - scrollbar_track_length
  // - scrollbar_thumb_length
  // - scroll_layer_length
  // - viewport_length
  // - position_in_widget
  //
  // When a thumb drag is in progress, for every pixel that the pointer moves,
  // the delta for the corresponding scroll_layer needs to be scaled by the
  // following ratio:
  // scaled_scroller_to_scrollbar_ratio =
  //  (scroll_layer_length - viewport_length) /
  //   (scrollbar_track_length - scrollbar_thumb_length)
  //
  // PS: Note that since this is a "ratio", it need not be scaled by the DSF.
  //
  // |<--------------------- scroll_layer_length -------------------------->|
  //
  // +------------------------------------------------+......................
  // |                                                |                     .
  // |<-------------- viewport_length --------------->|                     .
  // |                                                |                     .
  // |                                                |                     .
  // |                                                |                     .
  // |                                                |                     .
  // |                                                |                     .
  // |                                                |                     .
  // |                                                |                     .
  // |                                                |                     .
  // |                                                |                     .
  // |  |<------- scrollbar_track_length --------->|  |                     .
  // |                                                |                     .
  // +--+-----+----------------------------+-------+--+......................
  // |<||     |############################|       ||>|
  // +--+-----+----------------------------+-------+--+
  //
  //          |<- scrollbar_thumb_length ->|
  //
  const ScrollbarLayerImplBase* scrollbar = ScrollbarLayer();
  float scroll_layer_length = scrollbar->scroll_layer_length();
  float scrollbar_track_length = scrollbar->TrackLength();
  gfx::Rect thumb_rect(scrollbar->ComputeThumbQuadRect());
  float scrollbar_thumb_length =
      scrollbar->orientation() == ScrollbarOrientation::kVertical
          ? thumb_rect.height()
          : thumb_rect.width();
  float viewport_length = GetViewportLength();

  if (scrollbar_track_length == scrollbar_thumb_length)
    return 0;

  return (scroll_layer_length - viewport_length) /
         (scrollbar_track_length - scrollbar_thumb_length);
}

void ScrollbarController::ResetState() {
  drag_processed_for_current_frame_ = false;
  drag_state_ = std::nullopt;
  autoscroll_state_ = std::nullopt;
  captured_scrollbar_metadata_ = std::nullopt;
  if (cancelable_autoscroll_task_) {
    cancelable_autoscroll_task_->Cancel();
    cancelable_autoscroll_task_.reset();
  }
}

void ScrollbarController::DidRegisterScrollbar(
    ElementId element_id,
    ScrollbarOrientation orientation) {
  if (autoscroll_state_.has_value() &&
      captured_scrollbar_metadata_->scroll_element_id == element_id &&
      captured_scrollbar_metadata_->orientation == orientation &&
      autoscroll_state_->status == AutoScrollStatus::kAutoscrollReady) {
    // This is necessary, as when the scrollbar is being registered the layer
    // tree will not yet have synced its layer properties and cannot update
    // scrollbar geometries yet. We need to wait until the sync is over
    PostAutoscrollTask(base::TimeDelta::Min());
  }
}

void ScrollbarController::DidUnregisterScrollbar(
    ElementId element_id,
    ScrollbarOrientation orientation) {
  if (autoscroll_state_.has_value() &&
      captured_scrollbar_metadata_->scroll_element_id == element_id &&
      captured_scrollbar_metadata_->orientation == orientation &&
      autoscroll_state_->status == AutoScrollStatus::kAutoscrollScrolling) {
    layer_tree_host_impl_->mutator_host()->ScrollAnimationAbort(element_id);
    autoscroll_state_->status = AutoScrollStatus::kAutoscrollReady;
  }
}

void ScrollbarController::RecomputeAutoscrollStateIfNeeded() {
  if (!autoscroll_state_.has_value() ||
      !captured_scrollbar_metadata_.has_value() ||
      autoscroll_state_->status != AutoScrollStatus::kAutoscrollScrolling) {
    return;
  }

  bool clipped;
  gfx::PointF scroller_relative_position(
      GetScrollbarRelativePosition(last_known_pointer_position_, &clipped));

  if (clipped)
    return;

  // Based on the orientation of the scrollbar and the direction of the
  // autoscroll, the code below makes a decision of whether the track autoscroll
  // should be canceled or not.
  int thumb_start = 0;
  int thumb_end = 0;
  int pointer_position = 0;
  const ScrollbarLayerImplBase* scrollbar = ScrollbarLayer();
  const gfx::Rect thumb_quad = scrollbar->ComputeThumbQuadRect();
  if (scrollbar->orientation() == ScrollbarOrientation::kVertical) {
    thumb_start = thumb_quad.y();
    thumb_end = thumb_quad.y() + thumb_quad.height();
    pointer_position = scroller_relative_position.y();
  } else {
    thumb_start = thumb_quad.x();
    thumb_end = thumb_quad.x() + thumb_quad.width();
    pointer_position = scroller_relative_position.x();
  }

  // If the thumb reaches the pointer while autoscrolling, abort.
  if ((autoscroll_state_->direction ==
           AutoScrollDirection::kAutoscrollForward &&
       thumb_end > pointer_position) ||
      (autoscroll_state_->direction ==
           AutoScrollDirection::kAutoscrollBackward &&
       thumb_start < pointer_position)) {
    layer_tree_host_impl_->mutator_host()->ScrollAnimationAbort(
        captured_scrollbar_metadata_->scroll_element_id);
  }

  // When the scroller is autoscrolling forward, its dimensions need to be
  // monitored. If the length of the scroller layer increases, the old one needs
  // to be aborted and a new autoscroll animation needs to start. This needs to
  // be done only for the "autoscroll forward" case. Autoscrolling backward
  // always has a constant value to animate to (which is '0'. See the function
  // ScrollbarController::StartAutoScrollAnimation).
  if (autoscroll_state_->direction == AutoScrollDirection::kAutoscrollForward) {
    const float scroll_layer_length = scrollbar->scroll_layer_length();
    if (autoscroll_state_->scroll_layer_length != scroll_layer_length) {
      layer_tree_host_impl_->mutator_host()->ScrollAnimationAbort(
          scrollbar->scroll_element_id());
      StartAutoScrollAnimation();
    }
  }

  // The animations need to be aborted/restarted based on the pointer location
  // (i.e leaving/entering the track/arrows, reaching the track end etc). The
  // autoscroll_state_ however, needs to be reset on pointer changes.
  const gfx::RectF scrollbar_part_rect(
      GetRectForScrollbarPart(autoscroll_state_->pressed_scrollbar_part));
  if (!scrollbar_part_rect.Contains(scroller_relative_position)) {
    // Stop animating if pointer moves outside the rect bounds.
    layer_tree_host_impl_->mutator_host()->ScrollAnimationAbort(
        scrollbar->scroll_element_id());
  } else if (scrollbar_part_rect.Contains(scroller_relative_position) &&
             !layer_tree_host_impl_->mutator_host()->IsElementAnimating(
                 scrollbar->scroll_element_id())) {
    // Start animating if pointer re-enters the bounds.
    StartAutoScrollAnimation();
  }
}

// Helper to calculate the autoscroll velocity.
float ScrollbarController::InitialDeltaToAutoscrollVelocity(
    gfx::Vector2dF scroll_delta) const {
  DCHECK(captured_scrollbar_metadata_.has_value());
  const float delta =
      ScrollbarLayer()->orientation() == ScrollbarOrientation::kVertical
          ? scroll_delta.y()
          : scroll_delta.x();
  return delta * kAutoscrollMultiplier;
}

void ScrollbarController::StartAutoScroll() {
  DCHECK(autoscroll_state_.has_value());

  if (ScrollbarLayer()) {
    autoscroll_state_->status = AutoScrollStatus::kAutoscrollScrolling;
    StartAutoScrollAnimation();
  } else {
    autoscroll_state_->status = AutoScrollStatus::kAutoscrollReady;
  }
}

void ScrollbarController::StartAutoScrollAnimation() {
  // Autoscroll and thumb drag are mutually exclusive. Both can't be active at
  // the same time.
  DCHECK(!drag_state_.has_value());
  DCHECK(captured_scrollbar_metadata_.has_value());
  DCHECK(autoscroll_state_.has_value());
  DCHECK(ScrollbarLayer());
  DCHECK_EQ(autoscroll_state_->status, AutoScrollStatus::kAutoscrollScrolling);
  DCHECK_NE(autoscroll_state_->velocity, 0);

  // scroll_node is set up while handling GSB. If there's no node to scroll, we
  // don't need to create any animation for it.
  const ScrollbarLayerImplBase* scrollbar = ScrollbarLayer();
  const ScrollTree& scroll_tree =
      layer_tree_host_impl_->active_tree()->property_trees()->scroll_tree();
  const ScrollNode* scroll_node =
      scroll_tree.FindNodeFromElementId(scrollbar->scroll_element_id());

  if (!(scroll_node && scrollbar_scroll_is_active_))
    return;

  float scroll_layer_length = scrollbar->scroll_layer_length();
  gfx::PointF current_offset =
      scroll_tree.current_scroll_offset(scroll_node->element_id);

  // Determine the max offset for the scroll based on the scrolling direction.
  // Negative scroll velocity indicates backwards scrolling whereas a positive
  // value indicates forwards scrolling.
  const float target_offset_in_orientation =
      autoscroll_state_->velocity < 0 ? 0 : scroll_layer_length;
  const gfx::PointF target_offset_2d =
      scrollbar->orientation() == ScrollbarOrientation::kVertical
          ? gfx::PointF(current_offset.x(), target_offset_in_orientation)
          : gfx::PointF(target_offset_in_orientation, current_offset.y());

  autoscroll_state_->scroll_layer_length = scroll_layer_length;
  autoscroll_state_->direction = autoscroll_state_->velocity < 0
                                     ? AutoScrollDirection::kAutoscrollBackward
                                     : AutoScrollDirection::kAutoscrollForward;

  layer_tree_host_impl_->mutator_host()->ScrollAnimationAbort(
      scroll_node->element_id);
  layer_tree_host_impl_->AutoScrollAnimationCreate(
      *scroll_node, target_offset_2d, std::abs(autoscroll_state_->velocity));
}

// Performs hit test and prepares scroll deltas that will be used by GSE.
InputHandlerPointerResult ScrollbarController::HandlePointerUp(
    const gfx::PointF position_in_widget) {
  InputHandlerPointerResult scroll_result;
  if (scrollbar_scroll_is_active_) {
    scrollbar_scroll_is_active_ = false;
    scroll_result.type = PointerResultType::kScrollbarScroll;
  }

  // TODO(arakeri): This needs to be moved to ScrollOffsetAnimationsImpl as it
  // has knowledge about what type of animation is running. crbug.com/976353
  // Only abort the animation if it is an "autoscroll" animation.
  if (autoscroll_state_.has_value() &&
      autoscroll_state_->status == AutoScrollStatus::kAutoscrollScrolling) {
    layer_tree_host_impl_->mutator_host()->ScrollAnimationAbort(
        captured_scrollbar_metadata_->scroll_element_id);
  }

  ResetState();
  return scroll_result;
}

// Returns the layer that is hit by the position_in_widget.
LayerImpl* ScrollbarController::GetLayerHitByPoint(
    const gfx::PointF position_in_widget) const {
  LayerTreeImpl* active_tree = layer_tree_host_impl_->active_tree();
  gfx::Point viewport_point(position_in_widget.x(), position_in_widget.y());

  gfx::PointF device_viewport_point = gfx::ScalePoint(
      gfx::PointF(viewport_point), active_tree->device_scale_factor());
  LayerImpl* layer_impl =
      active_tree->FindLayerThatIsHitByPoint(device_viewport_point);

  return layer_impl;
}

float ScrollbarController::GetViewportLength() const {
  const ScrollbarLayerImplBase* scrollbar = ScrollbarLayer();
  const ScrollNode* scroll_node =
      layer_tree_host_impl_->active_tree()
          ->property_trees()
          ->scroll_tree()
          .FindNodeFromElementId(scrollbar->scroll_element_id());
  DCHECK(scroll_node);
  if (!scroll_node->scrolls_outer_viewport) {
    float length = scrollbar->orientation() == ScrollbarOrientation::kVertical
                       ? scroll_node->container_bounds.height()
                       : scroll_node->container_bounds.width();
    return length;
  }

  gfx::SizeF viewport_size = layer_tree_host_impl_->viewport()
                                 .GetInnerViewportSizeExcludingScrollbars();
  float length = scrollbar->orientation() == ScrollbarOrientation::kVertical
                     ? viewport_size.height()
                     : viewport_size.width();
  return length / GetPageScaleFactorForScroll();
}

float ScrollbarController::GetScrollDistanceForPercentBasedScroll() const {
  const ScrollbarLayerImplBase* scrollbar = ScrollbarLayer();

  const ScrollNode* scroll_node =
      layer_tree_host_impl_->active_tree()
          ->property_trees()
          ->scroll_tree()
          .FindNodeFromElementId(scrollbar->scroll_element_id());
  DCHECK(scroll_node);

  const gfx::Vector2dF scroll_delta =
      scrollbar->orientation() == ScrollbarOrientation::kVertical
          ? gfx::Vector2dF(0, kPercentDeltaForDirectionalScroll)
          : gfx::Vector2dF(kPercentDeltaForDirectionalScroll, 0);

  const gfx::Vector2dF pixel_delta =
      layer_tree_host_impl_->GetInputHandler().ResolveScrollGranularityToPixels(
          *scroll_node, scroll_delta,
          ui::ScrollGranularity::kScrollByPercentage);

  return scrollbar->orientation() == ScrollbarOrientation::kVertical
             ? pixel_delta.y()
             : pixel_delta.x();
}

float ScrollbarController::GetPageScaleFactorForScroll() const {
  return layer_tree_host_impl_->active_tree()->page_scale_factor_for_scroll();
}

float ScrollbarController::GetScrollDistanceForScrollbarPart(
    const ScrollbarPart scrollbar_part,
    const bool jump_key_modifier) const {
  float scroll_delta = 0;

  switch (scrollbar_part) {
    case ScrollbarPart::kBackButton:
    case ScrollbarPart::kForwardButton:
      if (layer_tree_host_impl_->settings().percent_based_scrolling) {
        scroll_delta = GetScrollDistanceForPercentBasedScroll();
      } else {
        scroll_delta = kPixelsPerLineStep * ScreenSpaceScaleFactor();
      }
      break;
    case ScrollbarPart::kBackTrack:
    case ScrollbarPart::kForwardTrack: {
      if (jump_key_modifier) {
        scroll_delta = GetScrollDistanceForAbsoluteJump();
        break;
      }
      // TODO(savella) Use snapport length instead of viewport length to match
      // main thread behaviour. See https://crbug.com/1098383.
      scroll_delta = GetViewportLength() * kMinFractionToStepWhenPaging;
      break;
    }
    default:
      scroll_delta = 0;
  }

  return scroll_delta;
}

float ScrollbarController::ScreenSpaceScaleFactor() const {
  return layer_tree_host_impl_->active_tree()->painted_device_scale_factor();
}

gfx::PointF ScrollbarController::GetScrollbarRelativePosition(
    const gfx::PointF position_in_widget,
    bool* clipped) const {
  // This is a speculative fix for crbug.com/1254865. On Mac, we occasionally
  // enter into a state where a scrollbar layer becomes null mid interaction. If
  // this happens, early out.
  const ScrollbarLayerImplBase* scrollbar = ScrollbarLayer();
  if (!scrollbar) {
    // Set clipped to true so that GetScrollbarPartFromPointerDown returns early
    // without trying to find a ScrollbarPart (since it wouldn't matter anyway).
    *clipped = true;
    return gfx::PointF(0, 0);
  }

  gfx::Transform inverse_screen_space_transform;

  gfx::Transform scaled_screen_space_transform(
      scrollbar->ScreenSpaceTransform());
  if (!scaled_screen_space_transform.GetInverse(
          &inverse_screen_space_transform))
    return gfx::PointF(0, 0);

  return gfx::PointF(MathUtil::ProjectPoint(inverse_screen_space_transform,
                                            position_in_widget, clipped));
}

// Determines the ScrollbarPart based on the position_in_widget.
ScrollbarPart ScrollbarController::GetScrollbarPartFromPointerDown(
    const gfx::PointF position_in_widget) const {
  // position_in_widget needs to be transformed and made relative to the
  // scrollbar layer because hit testing assumes layer relative coordinates.
  const ScrollbarLayerImplBase* scrollbar = ScrollbarLayer();
  bool clipped = false;

  const gfx::PointF scroller_relative_position(
      GetScrollbarRelativePosition(position_in_widget, &clipped));

  if (clipped)
    return ScrollbarPart::kNoPart;

  return scrollbar->IdentifyScrollbarPart(scroller_relative_position);
}

// Determines the corresponding rect for the given scrollbar part.
gfx::Rect ScrollbarController::GetRectForScrollbarPart(
    const ScrollbarPart scrollbar_part) const {
  const ScrollbarLayerImplBase* scrollbar = ScrollbarLayer();
  if (scrollbar_part == ScrollbarPart::kBackButton) {
    return scrollbar->BackButtonRect();
  }
  if (scrollbar_part == ScrollbarPart::kForwardButton) {
    return scrollbar->ForwardButtonRect();
  }
  if (scrollbar_part == ScrollbarPart::kBackTrack) {
    return scrollbar->BackTrackRect();
  }
  if (scrollbar_part == ScrollbarPart::kForwardTrack) {
    return scrollbar->ForwardTrackRect();
  }
  return gfx::Rect(0, 0);
}

// Determines the scroll delta as a gfx::Vector2dF based on the ScrollbarPart
// and the scrollbar orientation.
gfx::Vector2dF ScrollbarController::GetScrollDeltaForScrollbarPart(
    const ScrollbarPart scrollbar_part,
    const bool jump_key_modifier) const {
  const ScrollbarLayerImplBase* scrollbar = ScrollbarLayer();
  float distance =
      GetScrollDistanceForScrollbarPart(scrollbar_part, jump_key_modifier);

  // See CreateScrollStateForGesture for more information on how these values
  // will be interpreted.
  if (scrollbar_part == ScrollbarPart::kBackButton) {
    return scrollbar->orientation() == ScrollbarOrientation::kVertical
               ? gfx::Vector2dF(0, -distance)   // Up arrow
               : gfx::Vector2dF(-distance, 0);  // Left arrow
  } else if (scrollbar_part == ScrollbarPart::kForwardButton) {
    return scrollbar->orientation() == ScrollbarOrientation::kVertical
               ? gfx::Vector2dF(0, distance)   // Down arrow
               : gfx::Vector2dF(distance, 0);  // Right arrow
  } else if (scrollbar_part == ScrollbarPart::kBackTrack) {
    return scrollbar->orientation() == ScrollbarOrientation::kVertical
               ? gfx::Vector2dF(0, -distance)   // Track click up
               : gfx::Vector2dF(-distance, 0);  // Track click left
  } else if (scrollbar_part == ScrollbarPart::kForwardTrack) {
    return scrollbar->orientation() == ScrollbarOrientation::kVertical
               ? gfx::Vector2dF(0, distance)   // Track click down
               : gfx::Vector2dF(distance, 0);  // Track click right
  }

  return gfx::Vector2dF(0, 0);
}

}  // namespace cc
