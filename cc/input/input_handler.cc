// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/input/input_handler.h"

#include <string>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/notreached.h"
#include "base/types/optional_ref.h"
#include "build/build_config.h"
#include "cc/base/features.h"
#include "cc/input/browser_controls_offset_manager.h"
#include "cc/input/browser_controls_offset_tags_info.h"
#include "cc/input/scroll_elasticity_helper.h"
#include "cc/input/scroll_utils.h"
#include "cc/input/scrollbar_controller.h"
#include "cc/input/snap_selection_strategy.h"
#include "cc/layers/viewport.h"
#include "cc/trees/compositor_commit_data.h"
#include "cc/trees/latency_info_swap_promise_monitor.h"
#include "cc/trees/layer_tree_host_impl.h"
#include "cc/trees/layer_tree_impl.h"
#include "cc/trees/layer_tree_settings.h"
#include "cc/trees/property_tree.h"
#include "cc/trees/scroll_node.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace cc {

namespace {

enum SlowScrollMetricThread { MAIN_THREAD, CC_THREAD };

InputHandlerClient::ScrollEventDispatchMode GetScrollEventDispatchMode() {
  const std::string mode_name = ::features::kScrollEventDispatchMode.Get();
  if (mode_name ==
      ::features::kScrollEventDispatchModeDispatchScrollEventsImmediately) {
    return InputHandlerClient::ScrollEventDispatchMode::
        kDispatchScrollEventsImmediately;
  } else if (mode_name ==
             ::features::
                 kScrollEventDispatchModeUseScrollPredictorForEmptyQueue) {
    return InputHandlerClient::ScrollEventDispatchMode::
        kUseScrollPredictorForEmptyQueue;
  } else if (mode_name ==
             ::features::
                 kScrollEventDispatchModeUseScrollPredictorForDeadline) {
    return InputHandlerClient::ScrollEventDispatchMode::
        kUseScrollPredictorForDeadline;
  }

  return InputHandlerClient::ScrollEventDispatchMode::kEnqueueScrollEvents;
}

}  // namespace

InputHandlerCommitData::InputHandlerCommitData() = default;
InputHandlerCommitData::~InputHandlerCommitData() = default;

// The minimum amount of scroll delta that must be consumed before we consider
// a scroll to have happened.
// TODO(tdresser): Use a more rational epsilon. See crbug.com/510550 for
// details.
const float kScrollEpsilon = 0.1f;

// static
base::WeakPtr<InputHandler> InputHandler::Create(
    CompositorDelegateForInput& compositor_delegate) {
  auto input_handler = std::make_unique<InputHandler>(compositor_delegate);
  base::WeakPtr<InputHandler> input_handler_weak = input_handler->AsWeakPtr();
  compositor_delegate.BindToInputHandler(std::move(input_handler));
  return input_handler_weak;
}

InputHandler::InputHandler(CompositorDelegateForInput& compositor_delegate)
    : compositor_delegate_(compositor_delegate),
      scrollbar_controller_(std::make_unique<ScrollbarController>(
          &compositor_delegate_->GetImplDeprecated())) {}

InputHandler::~InputHandler() = default;

//
// =========== InputHandler Interface
//

base::WeakPtr<InputHandler> InputHandler::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void InputHandler::BindToClient(InputHandlerClient* client) {
  DCHECK(input_handler_client_ == nullptr);
  input_handler_client_ = client;
  input_handler_client_->SetPrefersReducedMotion(prefers_reduced_motion_);
  input_handler_client_->SetScrollEventDispatchMode(
      GetScrollEventDispatchMode());
}

InputHandler::ScrollStatus InputHandler::ScrollBegin(ScrollState* scroll_state,
                                                     ui::ScrollInputType type) {
  DCHECK(scroll_state);
  DCHECK(scroll_state->delta_x() == 0 && scroll_state->delta_y() == 0);

  InputHandler::ScrollStatus scroll_status;
  TRACE_EVENT0("cc", "InputHandler::ScrollBegin");

  // If this ScrollBegin is non-animated then ensure we cancel any ongoing
  // animated scrolls.
  // TODO(bokan): This preserves existing behavior when we had diverging
  // paths for animated and non-animated scrolls but we should probably
  // decide when it best makes sense to cancel a scroll animation (maybe
  // ScrollBy is a better place to do it).
  if (scroll_state->delta_granularity() ==
      ui::ScrollGranularity::kScrollByPrecisePixel) {
    ScrollNode* scroll_node = CurrentlyScrollingNode();
    compositor_delegate_->GetImplDeprecated()
        .mutator_host()
        ->ScrollAnimationAbort(scroll_node ? scroll_node->element_id
                                           : ElementId());
    scroll_animating_snap_target_ids_ = TargetSnapAreaElementIds();
  }

  if (CurrentlyScrollingNode() && type == latched_scroll_type_) {
    // It's possible we haven't yet cleared the CurrentlyScrollingNode if we
    // received a GSE but we're still animating the last scroll. If that's the
    // case, we'll simply un-defer the GSE and continue latching to the same
    // node.
    DCHECK(deferred_scroll_end_);
    deferred_scroll_end_ = false;
    return scroll_status;
  }

  ScrollNode* scrolling_node = nullptr;

  // TODO(bokan): ClearCurrentlyScrollingNode shouldn't happen in ScrollBegin,
  // this should only happen in ScrollEnd. We should DCHECK here that the state
  // is cleared instead. https://crbug.com/1016229
  //
  // TODO(b/329346768): Validate that this is no longer needed.
  ClearCurrentlyScrollingNode();

  ElementId target_element_id = scroll_state->target_element_id();
  ScrollTree& scroll_tree = GetScrollTree();

  if (target_element_id && (!scroll_state->main_thread_hit_tested_reasons() ||
                            scroll_state->is_scrollbar_interaction())) {
    TRACE_EVENT_INSTANT0("cc", "Latched scroll node provided",
                         TRACE_EVENT_SCOPE_THREAD);
    // If the caller passed in an element_id we can skip all the hit-testing
    // bits and provide a node straight-away.
    scrolling_node = scroll_tree.FindNodeFromElementId(target_element_id);
  } else {
    ScrollNode* starting_node = nullptr;
    if (target_element_id) {
      TRACE_EVENT_INSTANT0("cc", "Unlatched scroll node provided",
                           TRACE_EVENT_SCOPE_THREAD);
      // We had an element id but we should still perform the walk up the
      // scroll tree from the targeted node to latch to a scroller that can
      // scroll in the given direction. This mode is only used when scroll
      // unification is enabled and the targeted scroller comes back from a
      // main thread hit test.
      DCHECK(scroll_state->main_thread_hit_tested_reasons());
      starting_node = scroll_tree.FindNodeFromElementId(target_element_id);

      if (!starting_node) {
        // The main thread sent us an element_id that the compositor doesn't
        // have a scroll node for. This can happen in some racy conditions, a
        // freshly created scroller hasn't yet been committed or a
        // scroller-destroying commit beats the hit test back to the compositor
        // thread. However, these cases shouldn't be user perceptible.
        scroll_status.thread = InputHandler::ScrollThread::kScrollIgnored;
        return scroll_status;
      }
    } else {  // !target_element_id
      TRACE_EVENT_INSTANT0("cc", "Hit Testing for ScrollNode",
                           TRACE_EVENT_SCOPE_THREAD);
      gfx::Point viewport_point(scroll_state->position_x(),
                                scroll_state->position_y());
      gfx::PointF device_viewport_point =
          gfx::ScalePoint(gfx::PointF(viewport_point),
                          compositor_delegate_->DeviceScaleFactor());

      // The client should have discarded the scroll when the hit test came back
      // with an invalid element id.
      CHECK(!scroll_state->main_thread_hit_tested_reasons());

      ScrollHitTestResult scroll_hit_test =
          HitTestScrollNode(device_viewport_point);

      if (!scroll_hit_test.hit_test_successful) {
        // This result tells the client that the compositor doesn't have
        // enough information to target this scroll. The client should
        // perform a hit test in Blink and call this method again, with the
        // ElementId of the hit-tested scroll node.
        TRACE_EVENT_INSTANT0("cc", "Request Main Thread Hit Test",
                             TRACE_EVENT_SCOPE_THREAD);
        scroll_status.thread = InputHandler::ScrollThread::kScrollOnImplThread;
        DCHECK(scroll_hit_test.main_thread_hit_test_reasons);
        scroll_status.main_thread_hit_test_reasons =
            scroll_hit_test.main_thread_hit_test_reasons;
        CHECK(MainThreadScrollingReason::AreHitTestReasons(
            scroll_status.main_thread_hit_test_reasons));
        return scroll_status;
      }

      starting_node = scroll_hit_test.scroll_node;
    }

    // The above finds the ScrollNode that's hit by the given point but we
    // still need to walk up the scroll tree looking for the first node that
    // can consume delta from the scroll state.
    scrolling_node = FindNodeToLatch(scroll_state, starting_node, type);

    // When using fluent overlay scrollbars and a subscroller receives a scroll
    // event, but the scroll chains up to a different node, we want to flash the
    // scrollbars to show that the node is scrollable.
    if (scrolling_node &&
        compositor_delegate_->GetSettings().enable_fluent_overlay_scrollbar &&
        scrolling_node->element_id != starting_node->element_id) {
      compositor_delegate_->WillScrollContent(starting_node->element_id);
    }
  }

  if (!scrolling_node) {
    if (compositor_delegate_->GetSettings().is_for_embedded_frame) {
      // OOPIFs or fenced frames never have a viewport scroll node so if we
      // can't scroll we need to be bubble up to the parent frame. This happens
      // by returning kScrollIgnored.
      TRACE_EVENT_INSTANT0("cc",
                           "Ignored - No ScrollNode (OOPIF or FencedFrame)",
                           TRACE_EVENT_SCOPE_THREAD);
    } else {
      // If we didn't hit a layer above we'd usually fallback to the
      // viewport scroll node. However, there may not be one if a scroll
      // is received before the root layer has been attached. Chrome now
      // drops input until the first commit is received so this probably
      // can't happen in a typical browser session but there may still be
      // configurations where input is allowed prior to a commit.
      TRACE_EVENT_INSTANT0("cc", "Ignored - No ScrollNode",
                           TRACE_EVENT_SCOPE_THREAD);
    }
    scroll_status.thread = InputHandler::ScrollThread::kScrollIgnored;
    return scroll_status;
  }

  DCHECK_EQ(scroll_status.thread,
            InputHandler::ScrollThread::kScrollOnImplThread);
  DCHECK(scrolling_node);

  ActiveTree().SetCurrentlyScrollingNode(scrolling_node);
  scroll_status.main_thread_repaint_reasons =
      scroll_tree.GetMainThreadRepaintReasons(*scrolling_node);
  CHECK(MainThreadScrollingReason::AreRepaintReasons(
      scroll_status.main_thread_repaint_reasons));

  DidLatchToScroller(*scroll_state, type);

  // If the viewport is scrolling and it cannot consume any delta hints, the
  // scroll event will need to get bubbled if the viewport is for a guest or
  // oopif.
  if (GetViewport().ShouldScroll(*CurrentlyScrollingNode())) {
    outer_viewport_consumed_delta_ = false;
    inner_viewport_consumed_delta_ = false;
    if (!GetViewport().CanScroll(*CurrentlyScrollingNode(), *scroll_state)) {
      // TODO(crbug.com/40735567): This is a temporary workaround for GuestViews
      // as they create viewport nodes and want to bubble scroll if the
      // viewport cannot scroll in the given delta directions. There should be
      // a parameter to ThreadInputHandler to specify whether unused delta is
      // consumed by the viewport or bubbles to the parent.
      scroll_status.viewport_cannot_scroll = true;
    }
  }

  return scroll_status;
}

InputHandler::ScrollStatus InputHandler::RootScrollBegin(
    ScrollState* scroll_state,
    ui::ScrollInputType type) {
  TRACE_EVENT0("cc", "InputHandler::RootScrollBegin");
  if (!OuterViewportScrollNode()) {
    InputHandler::ScrollStatus scroll_status;
    scroll_status.thread = InputHandler::ScrollThread::kScrollIgnored;
    return scroll_status;
  }

  scroll_state->data()->set_current_native_scrolling_element(
      OuterViewportScrollNode()->element_id);
  InputHandler::ScrollStatus scroll_status = ScrollBegin(scroll_state, type);

  // Since we provided an ElementId, there should never be a need to perform a
  // hit test.
  DCHECK(!scroll_status.main_thread_hit_test_reasons);

  return scroll_status;
}

InputHandlerScrollResult InputHandler::ScrollUpdate(
    ScrollState scroll_state,
    base::TimeDelta delayed_by) {
  // The current_native_scrolling_element should only be set for ScrollBegin.
  DCHECK(!scroll_state.data()->current_native_scrolling_element());
  TRACE_EVENT2("cc", "InputHandler::ScrollUpdate", "dx", scroll_state.delta_x(),
               "dy", scroll_state.delta_y());

  if (!CurrentlyScrollingNode())
    return InputHandlerScrollResult();

  const ScrollNode& scroll_node = *CurrentlyScrollingNode();
  last_scroll_update_state_ = scroll_state;

  // Snap on update if interacting with the scrollbar track or arrow buttons.
  // Interactions with the scrollbar thumb have kScrollByPrecisePixel
  // granularity.
  if (scroll_state.is_scrollbar_interaction() &&
      scroll_state.delta_granularity() !=
          ui::ScrollGranularity::kScrollByPrecisePixel) {
    AdjustScrollDeltaForScrollbarSnap(scroll_state);
  }

  gfx::Vector2dF resolvedScrollDelta = ResolveScrollGranularityToPixels(
      scroll_node,
      gfx::Vector2dF(scroll_state.delta_x(), scroll_state.delta_y()),
      scroll_state.delta_granularity());

  scroll_state.data()->delta_x = resolvedScrollDelta.x();
  scroll_state.data()->delta_y = resolvedScrollDelta.y();
  // The decision of whether or not we'll animate a scroll comes down to
  // whether the granularity is specified in precise pixels or not. Thus we
  // need to preserve a precise granularity if that's what was specified; all
  // others are animated and so can be resolved to regular pixels.
  if (scroll_state.delta_granularity() !=
      ui::ScrollGranularity::kScrollByPrecisePixel) {
    scroll_state.data()->delta_granularity =
        ui::ScrollGranularity::kScrollByPixel;
  }

  compositor_delegate_->AccumulateScrollDeltaForTracing(
      gfx::Vector2dF(scroll_state.delta_x(), scroll_state.delta_y()));

  compositor_delegate_->WillScrollContent(scroll_node.element_id);

  float initial_top_controls_offset = compositor_delegate_->GetImplDeprecated()
                                          .browser_controls_manager()
                                          ->ControlsTopOffset();

  ScrollLatchedScroller(scroll_state, delayed_by);

  bool did_scroll_x = scroll_state.caused_scroll_x();
  bool did_scroll_y = scroll_state.caused_scroll_y();

  delta_consumed_for_scroll_gesture_ |=
      scroll_state.delta_consumed_for_scroll_sequence();
  bool did_scroll_content = did_scroll_x || did_scroll_y;
  if (did_scroll_content) {
    bool is_animated_scroll = ShouldAnimateScroll(scroll_state);
    compositor_delegate_->DidScrollContent(scroll_node.element_id,
                                           is_animated_scroll);
  }

  SetNeedsCommit();

  // Scrolling along an axis resets accumulated root overscroll for that axis.
  if (did_scroll_x)
    accumulated_root_overscroll_.set_x(0);
  if (did_scroll_y)
    accumulated_root_overscroll_.set_y(0);

  gfx::Vector2dF unused_root_delta;
  if (GetViewport().ShouldScroll(scroll_node)) {
    unused_root_delta =
        gfx::Vector2dF(scroll_state.delta_x(), scroll_state.delta_y());
  }

  // When inner viewport is unscrollable, disable overscrolls.
  if (auto* inner_viewport_scroll_node = InnerViewportScrollNode()) {
    unused_root_delta =
        UserScrollableDelta(*inner_viewport_scroll_node, unused_root_delta);
  }

  accumulated_root_overscroll_ += unused_root_delta;

  bool did_scroll_top_controls =
      initial_top_controls_offset != compositor_delegate_->GetImplDeprecated()
                                         .browser_controls_manager()
                                         ->ControlsTopOffset();

  InputHandlerScrollResult scroll_result;
  scroll_result.did_scroll = did_scroll_content || did_scroll_top_controls;
  scroll_result.did_overscroll_root = !unused_root_delta.IsZero();
  scroll_result.accumulated_root_overscroll = accumulated_root_overscroll_;
  scroll_result.unused_scroll_delta = unused_root_delta;
  scroll_result.overscroll_behavior =
      scroll_state.is_scroll_chain_cut()
          ? OverscrollBehavior(OverscrollBehavior::Type::kNone)
          : ActiveTree().overscroll_behavior();

  if (scroll_result.did_scroll) {
    // Scrolling can change the root scroll offset, so inform the synchronous
    // input handler.
    UpdateRootLayerStateForSynchronousInputHandler();
  }

  scroll_result.current_visual_offset = GetVisualScrollOffset(scroll_node);
  float scale_factor = ActiveTree().page_scale_factor_for_scroll();
  scroll_result.current_visual_offset.Scale(scale_factor);

  if (GetScrollTree().ShouldRealizeScrollsOnMain(scroll_node)) {
    scroll_result.needs_main_thread_repaint = true;
  }

  // Run animations which need to respond to updated scroll offset.
  compositor_delegate_->GetImplDeprecated()
      .mutator_host()
      ->TickScrollAnimations(compositor_delegate_->GetImplDeprecated()
                                 .CurrentBeginFrameArgs()
                                 .frame_time,
                             GetScrollTree());

  return scroll_result;
}

void InputHandler::AdjustScrollDeltaForScrollbarSnap(
    ScrollState& scroll_state) {
  ScrollNode* scroll_node = CurrentlyScrollingNode();
  if (!scroll_node || !scroll_node->snap_container_data)
    return;

  // Ideally, scrollbar track and arrow interactions would have
  // kScrollByPage and kScrollByLine, respectively. Currently, both have
  // kScrollByPixel granularity.
  // TODO(crbug.com/41456637): Update snap strategy once the granularity is
  // properly set. Currently, track and arrow scrolls both use a direction
  // strategy; however, the track should be using an "end and direction"
  // strategy.
  gfx::PointF current_position = GetVisualScrollOffset(*scroll_node);
  const SnapContainerData& data = scroll_node->snap_container_data.value();
  std::unique_ptr<SnapSelectionStrategy> strategy =
      SnapSelectionStrategy::CreateForDirection(
          current_position,
          gfx::Vector2dF(scroll_state.delta_x(), scroll_state.delta_y()), true);

  SnapPositionData snap = data.FindSnapPosition(*strategy);
  if (snap.type == SnapPositionData::Type::kNone) {
    return;
  }

  scroll_state.data()->delta_x = snap.position.x() - current_position.x();
  scroll_state.data()->delta_y = snap.position.y() - current_position.y();
}

void InputHandler::ScrollEnd(bool should_snap) {
  scrollbar_controller_->ResetState();
  if (!CurrentlyScrollingNode())
    return;

  // Note that if we deferred the scroll end then we should not snap. We will
  // snap once we deliver the deferred scroll end.
  if (GetAnimatingNodeForCurrentScrollingNode()) {
    DCHECK(!deferred_scroll_end_);
    deferred_scroll_end_ = true;
    return;
  }

  if (should_snap && SnapAtScrollEnd(SnapReason::kGestureScrollEnd)) {
    deferred_scroll_end_ = true;
    return;
  }

  DCHECK(latched_scroll_type_.has_value());

  compositor_delegate_->GetImplDeprecated()
      .browser_controls_manager()
      ->ScrollEnd();

  // Only indicate that the scroll gesture ended if scrolling actually occurred
  // so that we don't fire a "scrollend" event.
  if (did_scroll_x_for_scroll_gesture_ || did_scroll_y_for_scroll_gesture_) {
    scroll_gesture_did_end_ = true;
  }
  ClearCurrentlyScrollingNode();
  deferred_scroll_end_ = false;
  SetNeedsCommit();
  snap_fling_state_ = kNoFling;
  snap_strategy_.reset();
}

void InputHandler::RecordScrollBegin(
    ui::ScrollInputType input_type,
    ScrollBeginThreadState scroll_start_state) {
  auto tracker_type = GetTrackerTypeForScroll(input_type);
  DCHECK_NE(tracker_type, FrameSequenceTrackerType::kMaxType);

  // The main-thread is the 'scrolling thread' if:
  //   (1) the scroll is driven by the main thread, or
  //   (2) the scroll is driven by the compositor, but blocked on the main
  //       thread.
  // Otherwise, the compositor-thread is the 'scrolling thread'.
  // TODO(crbug.com/40122138): We should also count 'main thread' as the
  // 'scrolling thread' if the layer being scrolled has scroll-event handlers.
  FrameInfo::SmoothEffectDrivingThread scrolling_thread;
  switch (scroll_start_state) {
    case ScrollBeginThreadState::kScrollingOnCompositor:
      scrolling_thread = FrameInfo::SmoothEffectDrivingThread::kCompositor;
      break;
    case ScrollBeginThreadState::kScrollingOnMain:
    case ScrollBeginThreadState::kScrollingOnCompositorBlockedOnMain:
      scrolling_thread = FrameInfo::SmoothEffectDrivingThread::kMain;
      break;
  }
  compositor_delegate_->GetImplDeprecated()
      .frame_trackers()
      .StartScrollSequence(tracker_type, scrolling_thread);
}

void InputHandler::RecordScrollEnd(ui::ScrollInputType input_type) {
  compositor_delegate_->GetImplDeprecated().frame_trackers().StopSequence(
      GetTrackerTypeForScroll(input_type));
}

InputHandlerPointerResult InputHandler::MouseMoveAt(
    const gfx::Point& viewport_point) {
  InputHandlerPointerResult result =
      scrollbar_controller_->HandlePointerMove(gfx::PointF(viewport_point));

  // Early out if there are no animation controllers and avoid the hit test.
  // This happens on platforms without animated scrollbars.
  if (!compositor_delegate_->HasAnimatedScrollbars())
    return result;

  gfx::PointF device_viewport_point = gfx::ScalePoint(
      gfx::PointF(viewport_point), compositor_delegate_->DeviceScaleFactor());

  ScrollHitTestResult hit_test = HitTestScrollNode(device_viewport_point);

  ScrollNode* scroll_node = hit_test.scroll_node;

  // The hit test can fail in some cases, e.g. we don't know if a region of a
  // squashed layer has content or is empty.
  if (!hit_test.hit_test_successful || !scroll_node)
    return result;

  // Scrollbars for the viewport are registered with the outer viewport layer.
  if (scroll_node->scrolls_inner_viewport)
    scroll_node = OuterViewportScrollNode();

  ElementId scroll_element_id = scroll_node->element_id;
  ScrollbarAnimationController* new_animation_controller =
      compositor_delegate_->GetImplDeprecated()
          .ScrollbarAnimationControllerForElementId(scroll_element_id);
  if (scroll_element_id != scroll_element_id_mouse_currently_over_) {
    ScrollbarAnimationController* old_animation_controller =
        compositor_delegate_->GetImplDeprecated()
            .ScrollbarAnimationControllerForElementId(
                scroll_element_id_mouse_currently_over_);
    if (old_animation_controller)
      old_animation_controller->DidMouseLeave();

    scroll_element_id_mouse_currently_over_ = scroll_element_id;
  }

  if (!new_animation_controller)
    return result;

  new_animation_controller->DidMouseMove(device_viewport_point);

  return result;
}

PointerResultType InputHandler::HitTest(const gfx::PointF& viewport_point) {
  return scrollbar_controller_->HitTest(viewport_point)
             ? PointerResultType::kScrollbarScroll
             : PointerResultType::kUnhandled;
}

InputHandlerPointerResult InputHandler::MouseDown(
    const gfx::PointF& viewport_point,
    bool shift_modifier) {
  ScrollbarAnimationController* animation_controller =
      compositor_delegate_->GetImplDeprecated()
          .ScrollbarAnimationControllerForElementId(
              scroll_element_id_mouse_currently_over_);
  if (animation_controller) {
    animation_controller->DidMouseDown();
    scroll_element_id_mouse_currently_captured_ =
        scroll_element_id_mouse_currently_over_;
  }
  return scrollbar_controller_->HandlePointerDown(viewport_point,
                                                  shift_modifier);
}

InputHandlerPointerResult InputHandler::MouseUp(
    const gfx::PointF& viewport_point) {
  if (scroll_element_id_mouse_currently_captured_) {
    ScrollbarAnimationController* animation_controller =
        compositor_delegate_->GetImplDeprecated()
            .ScrollbarAnimationControllerForElementId(
                scroll_element_id_mouse_currently_captured_);

    scroll_element_id_mouse_currently_captured_ = ElementId();

    if (animation_controller)
      animation_controller->DidMouseUp();
  }
  return scrollbar_controller_->HandlePointerUp(viewport_point);
}

void InputHandler::MouseLeave() {
  compositor_delegate_->DidMouseLeave();
  scroll_element_id_mouse_currently_over_ = ElementId();
}

ElementId InputHandler::FindFrameElementIdAtPoint(
    const gfx::PointF& viewport_point) {
  gfx::PointF device_viewport_point = gfx::ScalePoint(
      gfx::PointF(viewport_point), compositor_delegate_->DeviceScaleFactor());
  return ActiveTree().FindFrameElementIdAtPoint(device_viewport_point);
}

void InputHandler::RequestUpdateForSynchronousInputHandler() {
  UpdateRootLayerStateForSynchronousInputHandler();
}

void InputHandler::SetSynchronousInputHandlerRootScrollOffset(
    const gfx::PointF& root_content_offset) {
  TRACE_EVENT2("cc", "InputHandler::SetSynchronousInputHandlerRootScrollOffset",
               "offset_x", root_content_offset.x(), "offset_y",
               root_content_offset.y());

  gfx::Vector2dF physical_delta =
      root_content_offset - GetViewport().TotalScrollOffset();
  physical_delta.Scale(ActiveTree().page_scale_factor_for_scroll());

  bool changed = !GetViewport()
                      .ScrollBy(physical_delta,
                                /*viewport_point=*/gfx::Point(),
                                /*is_direct_manipulation=*/false,
                                /*affect_browser_controls=*/false,
                                /*scroll_outer_viewport=*/true)
                      .consumed_delta.IsZero();
  if (!changed)
    return;

  compositor_delegate_->DidScrollContent(OuterViewportScrollNode()->element_id,
                                         /*is_animated_scroll=*/false);
  SetNeedsCommit();

  // After applying the synchronous input handler's scroll offset, tell it what
  // we ended up with.
  UpdateRootLayerStateForSynchronousInputHandler();

  compositor_delegate_->SetNeedsFullViewportRedraw();
}

void InputHandler::PinchGestureBegin(const gfx::Point& anchor,
                                     ui::ScrollInputType source) {
  DCHECK(source == ui::ScrollInputType::kTouchscreen ||
         source == ui::ScrollInputType::kWheel);

  pinch_gesture_active_ = true;
  pinch_gesture_end_should_clear_scrolling_node_ = !CurrentlyScrollingNode();

  TRACE_EVENT_INSTANT1("cc", "SetCurrentlyScrollingNode PinchGestureBegin",
                       TRACE_EVENT_SCOPE_THREAD, "isNull",
                       OuterViewportScrollNode() ? false : true);

  // Some unit tests don't setup viewport scroll nodes but do initiate a pinch
  // zoom gesture. Ideally, those tests should either create the viewport
  // scroll nodes or avoid simulating a pinch gesture.
  if (OuterViewportScrollNode()) {
    ActiveTree().SetCurrentlyScrollingNode(OuterViewportScrollNode());

    ScrollStateData scroll_state_data;
    scroll_state_data.position_x = anchor.x();
    scroll_state_data.position_y = anchor.y();
    scroll_state_data.is_beginning = true;
    scroll_state_data.delta_granularity =
        ui::ScrollGranularity::kScrollByPrecisePixel;
    scroll_state_data.is_direct_manipulation =
        source == ui::ScrollInputType::kTouchscreen;
    ScrollState state(scroll_state_data);

    DidLatchToScroller(state, source);
  }

  compositor_delegate_->GetImplDeprecated()
      .browser_controls_manager()
      ->PinchBegin();
  compositor_delegate_->DidStartPinchZoom();
}

void InputHandler::PinchGestureUpdate(float magnify_delta,
                                      const gfx::Point& anchor) {
  TRACE_EVENT0("cc", "InputHandler::PinchGestureUpdate");
  if (!InnerViewportScrollNode())
    return;
  has_pinch_zoomed_ = true;
  GetViewport().PinchUpdate(magnify_delta, anchor);
  SetNeedsCommit();
  compositor_delegate_->DidUpdatePinchZoom();
  // Pinching can change the root scroll offset, so inform the synchronous input
  // handler.
  UpdateRootLayerStateForSynchronousInputHandler();
}

void InputHandler::PinchGestureEnd(const gfx::Point& anchor) {
  // Some tests create a pinch gesture without creating a viewport scroll node.
  // In those cases, PinchGestureBegin will not latch to a scroll node.
  DCHECK(latched_scroll_type_.has_value() || !CurrentlyScrollingNode());
  bool snap_to_min = latched_scroll_type_.has_value() &&
                     latched_scroll_type_ == ui::ScrollInputType::kWheel;
  pinch_gesture_active_ = false;
  if (pinch_gesture_end_should_clear_scrolling_node_) {
    pinch_gesture_end_should_clear_scrolling_node_ = false;
    ClearCurrentlyScrollingNode();
  }
  GetViewport().PinchEnd(anchor, snap_to_min);
  compositor_delegate_->GetImplDeprecated()
      .browser_controls_manager()
      ->PinchEnd();
  SetNeedsCommit();
  compositor_delegate_->DidEndPinchZoom();
}

void InputHandler::SetNeedsAnimateInput() {
  compositor_delegate_->GetImplDeprecated().SetNeedsAnimateInput();
}

bool InputHandler::IsCurrentlyScrollingViewport() const {
  auto* node = CurrentlyScrollingNode();
  if (!node)
    return false;
  return GetViewport().ShouldScroll(*node);
}

EventListenerProperties InputHandler::GetEventListenerProperties(
    EventListenerClass event_class) const {
  return ActiveTree().event_listener_properties(event_class);
}

bool InputHandler::HasBlockingWheelEventHandlerAt(
    const gfx::Point& viewport_point) const {
  gfx::PointF device_viewport_point = gfx::ScalePoint(
      gfx::PointF(viewport_point), compositor_delegate_->DeviceScaleFactor());

  LayerImpl* layer_impl_with_wheel_event_handler =
      ActiveTree().FindLayerThatIsHitByPointInWheelEventHandlerRegion(
          device_viewport_point);

  return layer_impl_with_wheel_event_handler;
}

InputHandler::TouchStartOrMoveEventListenerType
InputHandler::EventListenerTypeForTouchStartOrMoveAt(
    const gfx::Point& viewport_point,
    TouchAction* out_touch_action) {
  gfx::PointF device_viewport_point = gfx::ScalePoint(
      gfx::PointF(viewport_point), compositor_delegate_->DeviceScaleFactor());

  LayerImpl* layer_impl_with_touch_handler =
      ActiveTree().FindLayerThatIsHitByPointInTouchHandlerRegion(
          device_viewport_point);

  if (layer_impl_with_touch_handler == nullptr) {
    if (out_touch_action)
      *out_touch_action = TouchAction::kAuto;
    return InputHandler::TouchStartOrMoveEventListenerType::kNoHandler;
  }

  if (out_touch_action) {
    gfx::Transform layer_screen_space_transform =
        layer_impl_with_touch_handler->ScreenSpaceTransform();
    // Getting here indicates that |layer_impl_with_touch_handler| is non-null,
    // which means that the |hit| in FindClosestMatchingLayer() is true, which
    // indicates that the inverse is available.
    gfx::Transform inverse_layer_screen_space =
        layer_screen_space_transform.GetCheckedInverse();
    bool clipped = false;
    gfx::Point3F planar_point = MathUtil::ProjectPoint3D(
        inverse_layer_screen_space, device_viewport_point, &clipped);
    gfx::PointF hit_test_point_in_layer_space =
        gfx::PointF(planar_point.x(), planar_point.y());
    const auto& region = layer_impl_with_touch_handler->touch_action_region();
    gfx::Point point = gfx::ToRoundedPoint(hit_test_point_in_layer_space);
    *out_touch_action = region.GetAllowedTouchAction(point);
  }

  if (!CurrentlyScrollingNode())
    return InputHandler::TouchStartOrMoveEventListenerType::kHandler;

  // Check if the touch start (or move) hits on the current scrolling layer or
  // its descendant. layer_impl_with_touch_handler is the layer hit by the
  // pointer and has an event handler, otherwise it is null. We want to compare
  // the most inner layer we are hitting on which may not have an event listener
  // with the actual scrolling layer.
  LayerImpl* layer_impl =
      ActiveTree().FindLayerThatIsHitByPoint(device_viewport_point);
  bool is_ancestor = IsScrolledBy(layer_impl, CurrentlyScrollingNode());
  return is_ancestor
             ? InputHandler::TouchStartOrMoveEventListenerType::
                   kHandlerOnScrollingLayer
             : InputHandler::TouchStartOrMoveEventListenerType::kHandler;
}

std::unique_ptr<LatencyInfoSwapPromiseMonitor>
InputHandler::CreateLatencyInfoSwapPromiseMonitor(ui::LatencyInfo* latency) {
  return compositor_delegate_->GetImplDeprecated()
      .CreateLatencyInfoSwapPromiseMonitor(latency);
}

std::unique_ptr<EventsMetricsManager::ScopedMonitor>
InputHandler::GetScopedEventMetricsMonitor(
    EventsMetricsManager::ScopedMonitor::DoneCallback done_callback) {
  return compositor_delegate_->GetImplDeprecated().GetScopedEventMetricsMonitor(
      std::move(done_callback));
}

ScrollElasticityHelper* InputHandler::CreateScrollElasticityHelper() {
  DCHECK(!scroll_elasticity_helper_);
  if (compositor_delegate_->GetSettings().enable_elastic_overscroll) {
    scroll_elasticity_helper_.reset(
        ScrollElasticityHelper::CreateForLayerTreeHostImpl(
            &compositor_delegate_->GetImplDeprecated()));
  }
  return scroll_elasticity_helper_.get();
}

void InputHandler::DestroyScrollElasticityHelper() {
  // Remove any stretch before destroying helper.
  scroll_elasticity_helper_->SetStretchAmount(gfx::Vector2dF());
  scroll_elasticity_helper_.reset();
}

bool InputHandler::GetScrollOffsetForLayer(ElementId element_id,
                                           gfx::PointF* offset) {
  ScrollTree& scroll_tree = GetScrollTree();
  ScrollNode* scroll_node = scroll_tree.FindNodeFromElementId(element_id);
  if (!scroll_node)
    return false;
  *offset = scroll_tree.current_scroll_offset(element_id);
  return true;
}

bool InputHandler::ScrollLayerTo(ElementId element_id,
                                 const gfx::PointF& offset) {
  ScrollTree& scroll_tree = GetScrollTree();
  ScrollNode* scroll_node = scroll_tree.FindNodeFromElementId(element_id);
  if (!scroll_node)
    return false;

  scroll_tree.ScrollBy(*scroll_node,
                       offset - scroll_tree.current_scroll_offset(element_id),
                       &ActiveTree());
  return true;
}

std::optional<gfx::PointF> InputHandler::ConstrainFling(gfx::PointF original) {
  gfx::PointF fling = original;
  if (fling_snap_constrain_x_) {
    fling.set_x(std::clamp(fling.x(), fling_snap_constrain_x_->GetMin(),
                           fling_snap_constrain_x_->GetMax()));
  }
  if (fling_snap_constrain_y_) {
    fling.set_y(std::clamp(fling.y(), fling_snap_constrain_y_->GetMin(),
                           fling_snap_constrain_y_->GetMax()));
  }
  return original == fling ? std::nullopt : std::make_optional(fling);
}

double InputHandler::PredictViewportBoundsDelta(
    gfx::Vector2dF scroll_distance) {
  // This adjustment is just an estimate. If we're wrong about where to aim a
  // snap fling curve, SnapAtScrollEnd will probably take us to a good place.
  // And if all else fails, the main thread will fix things in SnapAfterLayout
  // which runs after cc has stopped scrolling. But it does look nicer when no
  // corrections are needed, so we try to achieve that in the common cases.

  // The outer_viewport_container_bounds_delta is how much the true viewport
  // size currently differs from what Blink thinks it is.
  double current_bounds_delta = GetScrollTree()
                                    .property_trees()
                                    ->outer_viewport_container_bounds_delta()
                                    .y();
  return compositor_delegate_->GetImplDeprecated()
      .browser_controls_manager()
      ->PredictViewportBoundsDelta(current_bounds_delta, scroll_distance);
}

bool InputHandler::GetSnapFlingInfoAndSetAnimatingSnapTarget(
    const gfx::Vector2dF& current_delta,
    const gfx::Vector2dF& natural_displacement_in_viewport,
    gfx::PointF* out_initial_position,
    gfx::PointF* out_target_position) {
  ScrollNode* scroll_node = CurrentlyScrollingNode();
  if (!scroll_node || !scroll_node->snap_container_data.has_value() ||
      snap_fling_state_ == kNativeFling) {
    return false;
  }
  SnapContainerData& data = scroll_node->snap_container_data.value();

  float scale_factor = ActiveTree().page_scale_factor_for_scroll();
  gfx::Vector2dF current_delta_in_content =
      gfx::ScaleVector2d(current_delta, 1.f / scale_factor);
  gfx::Vector2dF snap_displacement =
      gfx::ScaleVector2d(natural_displacement_in_viewport, 1.f / scale_factor);

  gfx::PointF current_offset = GetVisualScrollOffset(*scroll_node);
  gfx::PointF new_offset = current_offset + current_delta_in_content;

  if (snap_fling_state_ == kConstrainedNativeFling) {
    if (std::optional<gfx::PointF> constrained = ConstrainFling(new_offset)) {
      snap_displacement = *constrained - current_offset;
    } else {
      return false;
    }
  }

  // CC side always uses fractional scroll deltas.
  bool use_fractional_offsets = true;
  std::unique_ptr<SnapSelectionStrategy> strategy =
      SnapSelectionStrategy::CreateForEndAndDirection(
          current_offset, snap_displacement, use_fractional_offsets);

  double snapport_height_adjustment =
      scroll_node->scrolls_outer_viewport
          ? PredictViewportBoundsDelta(snap_displacement)
          : 0;
  SnapPositionData snap = data.FindSnapPositionWithViewportAdjustment(
      *strategy, snapport_height_adjustment);
  if (snap.type == SnapPositionData::Type::kNone) {
    snap_fling_state_ = kNativeFling;
    return false;
  }

  if (snap_fling_state_ == kNoFling &&
      snap.type == SnapPositionData::Type::kCovered) {
    fling_snap_constrain_x_ = snap.covered_range_x;
    fling_snap_constrain_y_ = snap.covered_range_y;
    if (base::FeatureList::IsEnabled(
            features::kScrollSnapCoveringUseNativeFling) &&
        !ConstrainFling(new_offset)) {
      snap_fling_state_ = kConstrainedNativeFling;
      return false;
    }
  }
  snap_strategy_ = std::move(strategy);

  *out_initial_position = current_offset;
  *out_target_position = snap.position;

  out_target_position->Scale(scale_factor);
  out_initial_position->Scale(scale_factor);

  scroll_animating_snap_target_ids_ = snap.target_element_ids;
  snap_fling_state_ = kSnapFling;
  return true;
}

void InputHandler::ScrollEndForSnapFling(bool did_finish) {
  ScrollNode* scroll_node = CurrentlyScrollingNode();
  // When a snap fling animation reaches its intended target then we update the
  // scrolled node's snap targets. This also ensures blink learns about the new
  // snap targets for this scrolling element.
  if (did_finish && scroll_node &&
      scroll_node->snap_container_data.has_value()) {
    scroll_node->snap_container_data.value().SetTargetSnapAreaElementIds(
        scroll_animating_snap_target_ids_);
    updated_snapped_elements_[scroll_node->element_id] =
        scroll_animating_snap_target_ids_;
    SetNeedsCommit();
  }
  scroll_animating_snap_target_ids_ = TargetSnapAreaElementIds();
  ScrollEnd(true /* should_snap */);
}

void InputHandler::NotifyInputEvent() {
  compositor_delegate_->GetImplDeprecated().NotifyInputEvent();
}

//
// =========== InputDelegateForCompositor Interface
//

void InputHandler::ProcessCommitDeltas(
    CompositorCommitData* commit_data,
    const MutatorHost* main_thread_mutator_host) {
  DCHECK(commit_data);
  if (ActiveTree().LayerListIsEmpty())
    return;

  ElementId inner_viewport_scroll_element_id =
      InnerViewportScrollNode() ? InnerViewportScrollNode()->element_id
                                : ElementId();

  base::flat_map<ElementId, TargetSnapAreaElementIds> snapped_elements;
  updated_snapped_elements_.swap(snapped_elements);

  // Scroll commit data is stored in the scroll tree so it has its own method
  // for getting it.
  // TODO(bokan): It's a bug that CollectScrollDeltas is here, it means the
  // compositor cannot commit scroll changes without an InputHandler which it
  // should be able to. To move it back, we'll need to split out the
  // |snapped_elements| part of ScrollTree::CollectScrollDeltas though which is
  // an input responsibility.
  GetScrollTree().CollectScrollDeltas(
      commit_data, inner_viewport_scroll_element_id,
      compositor_delegate_->GetSettings().commit_fractional_scroll_deltas,
      snapped_elements, main_thread_mutator_host);

  // Record and reset scroll source flags.
  DCHECK(!commit_data->manipulation_info);
  if (has_scrolled_by_wheel_)
    commit_data->manipulation_info |= kManipulationInfoWheel;
  if (has_scrolled_by_touch_)
    commit_data->manipulation_info |= kManipulationInfoTouch;
  if (has_scrolled_by_precisiontouchpad_)
    commit_data->manipulation_info |= kManipulationInfoPrecisionTouchPad;
  if (has_pinch_zoomed_)
    commit_data->manipulation_info |= kManipulationInfoPinchZoom;
  if (has_scrolled_by_scrollbar_)
    commit_data->manipulation_info |= kManipulationInfoScrollbar;

  has_scrolled_by_wheel_ = false;
  has_scrolled_by_touch_ = false;
  has_scrolled_by_precisiontouchpad_ = false;
  has_pinch_zoomed_ = false;
  has_scrolled_by_scrollbar_ = false;

  commit_data->scroll_end_data.scroll_gesture_did_end = scroll_gesture_did_end_;
  scroll_gesture_did_end_ = false;

  commit_data->overscroll_delta = overscroll_delta_for_main_thread_;
  overscroll_delta_for_main_thread_ = gfx::Vector2dF();

  if (snap_strategy_) {
    commit_data->snap_strategy = snap_strategy_->Clone();
  }

  // Use the |last_latched_scroller_| rather than the
  // |CurrentlyScrollingNode| since the latter may be cleared by a GSE before
  // we've committed these values to the main thread.
  // TODO(bokan): This is wrong - if we also started a scroll this frame then
  // this will clear this value for that scroll. https://crbug.com/1116780.
  commit_data->scroll_latched_element_id = last_latched_scroller_;
  if (commit_data->scroll_end_data.scroll_gesture_did_end) {
    last_latched_scroller_ = ElementId();
    commit_data->scroll_end_data.gesture_affects_outer_viewport_scroll =
        outer_viewport_consumed_delta_;
    outer_viewport_consumed_delta_ = false;
    commit_data->scroll_end_data.gesture_affects_inner_viewport_scroll =
        inner_viewport_consumed_delta_;
    inner_viewport_consumed_delta_ = false;
  }
}

void InputHandler::TickAnimations(base::TimeTicks monotonic_time) {
  if (input_handler_client_) {
    // This does not set did_animate, because if the InputHandlerClient
    // changes anything it will be through the InputHandler interface which
    // does SetNeedsRedraw.
    input_handler_client_->Animate(monotonic_time);
  }
}

void InputHandler::WillShutdown() {
  if (input_handler_client_) {
    input_handler_client_.ExtractAsDangling()->WillShutdown();
  }

  if (scroll_elasticity_helper_)
    scroll_elasticity_helper_.reset();
}

void InputHandler::WillDraw() {
  if (input_handler_client_)
    input_handler_client_->ReconcileElasticOverscrollAndRootScroll();
}

void InputHandler::WillBeginImplFrame(const viz::BeginFrameArgs& args) {
  if (input_handler_client_) {
    scrollbar_controller_->WillBeginImplFrame();
    input_handler_client_->DeliverInputForBeginFrame(args);
  }
}

void InputHandler::DidCommit() {
  // In high latency mode commit cannot finish within the same frame. We need to
  // flush input here to make sure they got picked up by |PrepareTiles()|.
  if (input_handler_client_ && compositor_delegate_->IsInHighLatencyMode())
    input_handler_client_->DeliverInputForHighLatencyMode();
}

void InputHandler::DidActivatePendingTree() {
  // The previous scrolling node might no longer exist in the new tree.
  if (!CurrentlyScrollingNode())
    ClearCurrentlyScrollingNode();

  // Activation can change the root scroll offset, so inform the synchronous
  // input handler.
  UpdateRootLayerStateForSynchronousInputHandler();
}

void InputHandler::DidFinishImplFrame() {
  if (input_handler_client_) {
    input_handler_client_->DidFinishImplFrame();
  }
}

void InputHandler::OnBeginImplFrameDeadline() {
  if (!IsCurrentlyScrolling()) {
    return;
  }
  if (input_handler_client_) {
    input_handler_client_->DeliverInputForDeadline();
  }
}

void InputHandler::RootLayerStateMayHaveChanged() {
  UpdateRootLayerStateForSynchronousInputHandler();
}

void InputHandler::DidRegisterScrollbar(ElementId scroll_element_id,
                                        ScrollbarOrientation orientation) {
  scrollbar_controller_->DidRegisterScrollbar(scroll_element_id, orientation);
}

void InputHandler::DidUnregisterScrollbar(ElementId scroll_element_id,
                                          ScrollbarOrientation orientation) {
  scrollbar_controller_->DidUnregisterScrollbar(scroll_element_id, orientation);
}

void InputHandler::ScrollOffsetAnimationFinished() {
  TRACE_EVENT0("cc", "InputHandler::ScrollOffsetAnimationFinished");
  // ScrollOffsetAnimationFinished is called in two cases:
  //  1- smooth scrolling animation is over (IsAnimatingForSnap == false).
  //  2- snap scroll animation is over (IsAnimatingForSnap == true).
  //
  //  Only for case (1) we should check and run snap scroll animation if needed.
  if (!IsAnimatingForSnap() &&
      SnapAtScrollEnd(SnapReason::kScrollOffsetAnimationFinished))
    return;

  // The end of a scroll offset animation means that the scrolling node is at
  // the target offset.
  ScrollNode* scroll_node = CurrentlyScrollingNode();
  if (scroll_node && scroll_node->snap_container_data.has_value()) {
    scroll_node->snap_container_data.value().SetTargetSnapAreaElementIds(
        scroll_animating_snap_target_ids_);
    updated_snapped_elements_[scroll_node->element_id] =
        scroll_animating_snap_target_ids_;
    SetNeedsCommit();
  }
  scroll_animating_snap_target_ids_ = TargetSnapAreaElementIds();

  // Call scrollEnd with the deferred scroll end state when the scroll animation
  // completes after GSE arrival.
  if (deferred_scroll_end_) {
    ScrollEnd(/*should_snap=*/false);
    return;
  }
}

void InputHandler::SetPrefersReducedMotion(bool prefers_reduced_motion) {
  if (prefers_reduced_motion_ == prefers_reduced_motion)
    return;
  prefers_reduced_motion_ = prefers_reduced_motion;

  if (input_handler_client_)
    input_handler_client_->SetPrefersReducedMotion(prefers_reduced_motion_);
}

bool InputHandler::IsCurrentlyScrolling() const {
  return CurrentlyScrollingNode();
}

ActivelyScrollingType InputHandler::GetActivelyScrollingType() const {
  if (!CurrentlyScrollingNode())
    return ActivelyScrollingType::kNone;

  if (!last_scroll_update_state_)
    return ActivelyScrollingType::kNone;

  if (!delta_consumed_for_scroll_gesture_)
    return ActivelyScrollingType::kNone;

  if (ShouldAnimateScroll(last_scroll_update_state_.value()))
    return ActivelyScrollingType::kAnimated;

  return ActivelyScrollingType::kPrecise;
}

bool InputHandler::IsHandlingTouchSequence() const {
  return is_handling_touch_sequence_;
}

bool InputHandler::IsCurrentScrollMainRepainted() const {
  const ScrollNode* scroll_node = CurrentlyScrollingNode();
  if (!scroll_node)
    return false;
  uint32_t repaint_reasons =
      GetScrollTree().GetMainThreadRepaintReasons(*scroll_node);
  return repaint_reasons != MainThreadScrollingReason::kNotScrollingOnMain;
}

bool InputHandler::HasQueuedInput() const {
  if (input_handler_client_) {
    return input_handler_client_->HasQueuedInput();
  }
  return false;
}

ScrollNode* InputHandler::CurrentlyScrollingNode() {
  return GetScrollTree().CurrentlyScrollingNode();
}

const ScrollNode* InputHandler::CurrentlyScrollingNode() const {
  return GetScrollTree().CurrentlyScrollingNode();
}

ScrollTree& InputHandler::GetScrollTree() {
  return compositor_delegate_->GetScrollTree();
}

ScrollTree& InputHandler::GetScrollTree() const {
  return compositor_delegate_->GetScrollTree();
}

ScrollNode* InputHandler::InnerViewportScrollNode() const {
  return ActiveTree().InnerViewportScrollNode();
}

ScrollNode* InputHandler::OuterViewportScrollNode() const {
  return ActiveTree().OuterViewportScrollNode();
}

Viewport& InputHandler::GetViewport() const {
  return compositor_delegate_->GetImplDeprecated().viewport();
}

void InputHandler::SetNeedsCommit() {
  compositor_delegate_->SetNeedsCommit();
}

LayerTreeImpl& InputHandler::ActiveTree() {
  DCHECK(compositor_delegate_->GetImplDeprecated().active_tree());
  return *compositor_delegate_->GetImplDeprecated().active_tree();
}

LayerTreeImpl& InputHandler::ActiveTree() const {
  DCHECK(compositor_delegate_->GetImplDeprecated().active_tree());
  return *compositor_delegate_->GetImplDeprecated().active_tree();
}

FrameSequenceTrackerType InputHandler::GetTrackerTypeForScroll(
    ui::ScrollInputType input_type) const {
  switch (input_type) {
    case ui::ScrollInputType::kWheel:
      return FrameSequenceTrackerType::kWheelScroll;
    case ui::ScrollInputType::kTouchscreen:
      return FrameSequenceTrackerType::kTouchScroll;
    case ui::ScrollInputType::kScrollbar:
      return FrameSequenceTrackerType::kScrollbarScroll;
    case ui::ScrollInputType::kAutoscroll:
      return FrameSequenceTrackerType::kMaxType;
  }
}

float InputHandler::LineStep() const {
  return kPixelsPerLineStep * ActiveTree().painted_device_scale_factor();
}

// TODO(mehdika): There is some redundancy between this function and
// ScrollbarController::GetScrollDistanceForScrollbarPart, these two need to be
// kept in sync.
gfx::Vector2dF InputHandler::ResolveScrollGranularityToPixels(
    const ScrollNode& scroll_node,
    const gfx::Vector2dF& scroll_delta,
    ui::ScrollGranularity granularity) {
  gfx::Vector2dF pixel_delta = scroll_delta;

  if (granularity == ui::ScrollGranularity::kScrollByPage) {
    // Page should use a percentage of the scroller so change the parameters
    // and let the percentage case below resolve it.
    granularity = ui::ScrollGranularity::kScrollByPercentage;
    pixel_delta.Scale(kMinFractionToStepWhenPaging);
  }

  if (granularity == ui::ScrollGranularity::kScrollByPercentage) {
    gfx::SizeF scroller_size = gfx::SizeF(scroll_node.container_bounds);
    gfx::SizeF viewport_size(compositor_delegate_->VisualDeviceViewportSize());

    // Convert from rootframe coordinates to screen coordinates (physical
    // pixels if --use-zoom-for-dsf enabled, DIPs otherwise).
    scroller_size.Scale(compositor_delegate_->PageScaleFactor());

    // Convert from physical pixels to screen coordinates (if --use-zoom-for-dsf
    // enabled, `DeviceScaleFactor()` returns 1).
    viewport_size.InvScale(compositor_delegate_->DeviceScaleFactor());

    pixel_delta = ScrollUtils::ResolveScrollPercentageToPixels(
        pixel_delta, scroller_size, viewport_size);
  }

  if (granularity == ui::ScrollGranularity::kScrollByLine) {
    pixel_delta.Scale(LineStep(), LineStep());
  }

  return pixel_delta;
}

InputHandler::ScrollHitTestResult InputHandler::HitTestScrollNode(
    const gfx::PointF& device_viewport_point) const {
  ScrollHitTestResult result;
  result.scroll_node = nullptr;
  result.hit_test_successful = false;

  std::vector<const LayerImpl*> layers =
      ActiveTree().FindLayersUpToFirstScrollableOrOpaqueToHitTest(
          device_viewport_point);

  const LayerImpl* first_scrollable_or_opaque_to_hit_test_layer = nullptr;
  if (!layers.empty()) {
    if (compositor_delegate_->GetSettings().enable_hit_test_opaqueness) {
      if (layers.back()->OpaqueToHitTest()) {
        first_scrollable_or_opaque_to_hit_test_layer = layers.back();
      }
    } else if (layers.back()->IsScrollerOrScrollbar()) {
      first_scrollable_or_opaque_to_hit_test_layer = layers.back();
    }
  }
  ScrollNode* node_to_scroll = nullptr;

  // Go through each layer up to (and including) the scroller. Any may block
  // scrolling if they come from outside the scroller's scroll-subtree or if we
  // hit a non-fast-scrolling-region.
  for (const auto* layer_impl : layers) {
    if (!IsInitialScrollHitTestReliable(
            layer_impl, first_scrollable_or_opaque_to_hit_test_layer,
            node_to_scroll)) {
      TRACE_EVENT_INSTANT0("cc", "Failed Hit Test", TRACE_EVENT_SCOPE_THREAD);
      result.main_thread_hit_test_reasons =
          MainThreadScrollingReason::kFailedHitTest;
      return result;
    }

    // If we hit a main thread hit test region, that means there's some reason
    // we can't scroll in this region. Primarily, because there's another
    // scroller there that isn't composited and we don't know about so we'll
    // return failure.
    if (ActiveTree().PointHitsMainThreadScrollHitTestRegion(
            device_viewport_point, *layer_impl)) {
      result.main_thread_hit_test_reasons =
          MainThreadScrollingReason::kMainThreadScrollHitTestRegion;
      return result;
    }

    if (ElementId scroll_element_id = ActiveTree().PointHitsNonCompositedScroll(
            device_viewport_point, *layer_impl)) {
      node_to_scroll = GetScrollTree().FindNodeFromElementId(scroll_element_id);
      CHECK(node_to_scroll);
      break;
    }
  }

  // It's theoretically possible to hit no layers or only non-scrolling layers.
  // e.g. an API hit test outside the viewport, or sending a scroll to an OOPIF
  // that does not have overflow. If we made it to here, we also don't have any
  // non-fast scroll regions. Fallback to scrolling the viewport.
  if (!node_to_scroll) {
    result.hit_test_successful = true;
    if (InnerViewportScrollNode())
      result.scroll_node = GetNodeToScroll(InnerViewportScrollNode());

    return result;
  }

  result.scroll_node = node_to_scroll;
  result.hit_test_successful = true;
  return result;
}

ScrollNode* InputHandler::GetNodeToScroll(ScrollNode* node) const {
  // The root and the secondary root are sentinel nodes and don't contribute to
  // scrolling.
  if (node->id <= kSecondaryRootPropertyNodeId) {
    return nullptr;
  }

  // Blink has a notion of a "root scroller", which is the scroller in a page
  // that is considered to host the main content. Typically this will be the
  // document/LayoutView contents; however, in some situations Blink may choose
  // a sub-scroller (div, iframe) that should scroll with "viewport" behavior.
  // The "root scroller" is the node designated as the outer viewport in CC.
  // See third_party/blink/renderer/core/page/scrolling/README.md for details.
  //
  // "Viewport" scrolling ensures generation of overscroll events, top controls
  // movement, as well as correct multi-viewport panning in pinch-zoom and
  // other scenarios.  We use the viewport's outer scroll node to represent the
  // viewport in the scroll chain and apply scroll delta using CC's Viewport
  // class.
  //
  // Scrolling from position: fixed layers will chain directly up to the inner
  // viewport. Whether that should use the outer viewport (and thus the
  // Viewport class) to scroll or not depends on the root scroller scenario
  // because we don't want setting a root scroller to change the scroll chain
  // order. The |prevent_viewport_scrolling_from_inner| bit is used to
  // communicate that context.
  DCHECK(!node->prevent_viewport_scrolling_from_inner ||
         node->scrolls_inner_viewport);

  if (node->scrolls_inner_viewport &&
      !node->prevent_viewport_scrolling_from_inner) {
    DCHECK(OuterViewportScrollNode());
    return OuterViewportScrollNode();
  }

  return node;
}

ScrollNode* InputHandler::GetNodeToScrollForLayer(
    const LayerImpl* layer) const {
  if (layer->IsScrollbarLayer()) {
    // If we hit a scrollbar layer, get the ScrollNode from its associated
    // scrolling layer, rather than directly from the scrollbar layer. The
    // latter would return the parent scroller's ScrollNode.
    if (auto* scroll_node = GetScrollTree().FindNodeFromElementId(
            ToScrollbarLayer(layer)->scroll_element_id())) {
      return GetNodeToScroll(scroll_node);
    }
    return nullptr;
  }
  return GetNodeToScroll(GetScrollTree().Node(layer->scroll_tree_index()));
}

bool InputHandler::IsInitialScrollHitTestReliable(
    const LayerImpl* layer_impl,
    const LayerImpl* first_scrollable_or_opaque_to_hit_test_layer,
    ScrollNode*& out_node_to_scroll) const {
  ScrollNode* scroll_node = GetNodeToScrollForLayer(layer_impl);

  if (layer_impl == first_scrollable_or_opaque_to_hit_test_layer) {
    out_node_to_scroll = scroll_node;
    return true;
  }

  // If there's a scrolling layer, we should also have a closest scroll node,
  // and vice versa. Otherwise, the hit test is not reliable.
  if ((first_scrollable_or_opaque_to_hit_test_layer && !scroll_node) ||
      (scroll_node && !first_scrollable_or_opaque_to_hit_test_layer)) {
    return false;
  }
  if (!first_scrollable_or_opaque_to_hit_test_layer && !scroll_node) {
    // It's ok if we have neither.
    out_node_to_scroll = nullptr;
    return true;
  }

  // If `first_scrollable_or_opaque_to_hit_test_layer` and `layer_impl` will
  // scroll the same scroll node, the hit test has not escaped to other areas
  // of the scroll tree and is reliable so far.
  if (scroll_node ==
      GetNodeToScrollForLayer(first_scrollable_or_opaque_to_hit_test_layer)) {
    out_node_to_scroll = scroll_node;
    return true;
  }

  return false;
}

gfx::Vector2dF InputHandler::ComputeScrollDelta(const ScrollNode& scroll_node,
                                                const gfx::Vector2dF& delta) {
  ScrollTree& scroll_tree = GetScrollTree();
  float scale_factor = compositor_delegate_->PageScaleFactor();

  gfx::Vector2dF adjusted_scroll(delta);
  adjusted_scroll.InvScale(scale_factor);
  adjusted_scroll = UserScrollableDelta(scroll_node, adjusted_scroll);

  gfx::PointF old_offset =
      scroll_tree.current_scroll_offset(scroll_node.element_id);
  gfx::PointF new_offset = scroll_tree.ClampScrollOffsetToLimits(
      old_offset + adjusted_scroll, scroll_node);

  return new_offset - old_offset;
}

bool InputHandler::CalculateLocalScrollDeltaAndStartPoint(
    const ScrollNode& scroll_node,
    const gfx::PointF& viewport_point,
    const gfx::Vector2dF& viewport_delta,
    gfx::Vector2dF* out_local_scroll_delta,
    gfx::PointF* out_local_start_point /*= nullptr*/) {
  if (scroll_node.transform_id == kInvalidPropertyNodeId) {
    return false;
  }

  // Layers with non-invertible screen space transforms should not have passed
  // the scroll hit test in the first place.
  const gfx::Transform screen_space_transform =
      GetScrollTree().ScreenSpaceTransform(scroll_node.id);
  // TODO(shawnsingh): With the advent of impl-side scrolling for non-root
  // layers, we may need to explicitly handle uninvertible transforms here.
  gfx::Transform inverse_screen_space_transform =
      screen_space_transform.GetCheckedInverse();

  float scale_from_viewport_to_screen_space =
      compositor_delegate_->DeviceScaleFactor();
  gfx::PointF screen_space_point =
      gfx::ScalePoint(viewport_point, scale_from_viewport_to_screen_space);

  gfx::Vector2dF screen_space_delta = viewport_delta;
  screen_space_delta.Scale(scale_from_viewport_to_screen_space);

  // Project the scroll start and end points to local layer space to find the
  // scroll delta in layer coordinates.
  bool start_clipped, end_clipped;
  gfx::PointF screen_space_end_point = screen_space_point + screen_space_delta;
  gfx::PointF local_start_point = MathUtil::ProjectPoint(
      inverse_screen_space_transform, screen_space_point, &start_clipped);
  gfx::PointF local_end_point = MathUtil::ProjectPoint(
      inverse_screen_space_transform, screen_space_end_point, &end_clipped);
  DCHECK(out_local_scroll_delta);
  *out_local_scroll_delta = local_end_point - local_start_point;

  if (out_local_start_point)
    *out_local_start_point = local_start_point;

  if (start_clipped || end_clipped)
    return false;

  return true;
}

gfx::Vector2dF InputHandler::ScrollNodeWithViewportSpaceDelta(
    const ScrollNode& scroll_node,
    const gfx::PointF& viewport_point,
    const gfx::Vector2dF& viewport_delta) {
  ScrollTree& scroll_tree = GetScrollTree();
  gfx::PointF local_start_point;
  gfx::Vector2dF local_scroll_delta;
  if (!CalculateLocalScrollDeltaAndStartPoint(
          scroll_node, viewport_point, viewport_delta, &local_scroll_delta,
          &local_start_point)) {
    return gfx::Vector2dF();
  }

  bool scrolls_outer_viewport = scroll_node.scrolls_outer_viewport;
  TRACE_EVENT2("cc", "ScrollNodeWithViewportSpaceDelta", "delta_y",
               local_scroll_delta.y(), "is_outer", scrolls_outer_viewport);

  // Apply the scroll delta.
  gfx::PointF previous_offset =
      scroll_tree.current_scroll_offset(scroll_node.element_id);
  scroll_tree.ScrollBy(scroll_node, local_scroll_delta, &ActiveTree());
  gfx::Vector2dF scrolled =
      scroll_tree.current_scroll_offset(scroll_node.element_id) -
      previous_offset;

  TRACE_EVENT_INSTANT1("cc", "ConsumedDelta", TRACE_EVENT_SCOPE_THREAD, "y",
                       scrolled.y());

  // Get the end point in the layer's content space so we can apply its
  // ScreenSpaceTransform.
  gfx::PointF actual_local_end_point = local_start_point + scrolled;

  // Calculate the applied scroll delta in viewport space coordinates.
  bool end_clipped;
  const gfx::Transform screen_space_transform =
      scroll_tree.ScreenSpaceTransform(scroll_node.id);
  gfx::PointF actual_screen_space_end_point = MathUtil::MapPoint(
      screen_space_transform, actual_local_end_point, &end_clipped);
  DCHECK(!end_clipped);
  if (end_clipped)
    return gfx::Vector2dF();

  float scale_from_viewport_to_screen_space =
      compositor_delegate_->DeviceScaleFactor();
  gfx::PointF actual_viewport_end_point = gfx::ScalePoint(
      actual_screen_space_end_point, 1.f / scale_from_viewport_to_screen_space);
  return actual_viewport_end_point - viewport_point;
}

gfx::Vector2dF InputHandler::ScrollNodeWithLocalDelta(
    const ScrollNode& scroll_node,
    const gfx::Vector2dF& local_delta) const {
  bool scrolls_outer_viewport = scroll_node.scrolls_outer_viewport;
  TRACE_EVENT2("cc", "ScrollNodeWithLocalDelta", "delta_y", local_delta.y(),
               "is_outer", scrolls_outer_viewport);
  float page_scale_factor = compositor_delegate_->PageScaleFactor();

  ScrollTree& scroll_tree = GetScrollTree();
  gfx::PointF previous_offset =
      scroll_tree.current_scroll_offset(scroll_node.element_id);
  gfx::Vector2dF delta = local_delta;
  delta.InvScale(page_scale_factor);
  scroll_tree.ScrollBy(scroll_node, delta, &ActiveTree());
  gfx::Vector2dF scrolled =
      scroll_tree.current_scroll_offset(scroll_node.element_id) -
      previous_offset;
  gfx::Vector2dF consumed_scroll(scrolled.x(), scrolled.y());
  consumed_scroll.Scale(page_scale_factor);
  TRACE_EVENT_INSTANT1("cc", "ConsumedDelta", TRACE_EVENT_SCOPE_THREAD, "y",
                       consumed_scroll.y());

  return consumed_scroll;
}

// TODO(danakj): Make this into two functions, one with delta, one with
// viewport_point, no bool required.
gfx::Vector2dF InputHandler::ScrollSingleNode(const ScrollNode& scroll_node,
                                              const gfx::Vector2dF& delta,
                                              const gfx::Point& viewport_point,
                                              bool is_direct_manipulation) {
  gfx::Vector2dF adjusted_delta = UserScrollableDelta(scroll_node, delta);

  // Events representing direct manipulation of the screen (such as gesture
  // events) need to be transformed from viewport coordinates to local layer
  // coordinates so that the scrolling contents exactly follow the user's
  // finger. In contrast, events not representing direct manipulation of the
  // screen (such as wheel events) represent a fixed amount of scrolling so we
  // can just apply them directly, but the page scale factor is applied to the
  // scroll delta.
  if (is_direct_manipulation) {
    // For touch-scroll we need to scale the delta here, as the transform tree
    // won't know anything about the external page scale factors used by OOPIFs.
    gfx::Vector2dF scaled_delta(adjusted_delta);
    scaled_delta.InvScale(ActiveTree().external_page_scale_factor());
    return ScrollNodeWithViewportSpaceDelta(
        scroll_node, gfx::PointF(viewport_point), scaled_delta);
  }
  return ScrollNodeWithLocalDelta(scroll_node, adjusted_delta);
}

ScrollNode* InputHandler::GetAnimatingNodeForCurrentScrollingNode() {
  ScrollNode* scroll_node = CurrentlyScrollingNode();
  if (compositor_delegate_->GetImplDeprecated()
          .mutator_host()
          ->ElementHasImplOnlyScrollAnimation(scroll_node->element_id)) {
    return scroll_node;
  }

  // Usually the CurrentlyScrollingNode will be the currently animating
  // one. The one exception is the inner viewport. Scrolling the combined
  // viewport will always set the outer viewport as the currently scrolling
  // node. However, if an animation is created on the inner viewport we
  // must use it when updating the animation curve.
  ScrollNode* inner_viewport_scroll_node = InnerViewportScrollNode();
  if (scroll_node->scrolls_outer_viewport && inner_viewport_scroll_node) {
    if (compositor_delegate_->GetImplDeprecated()
            .mutator_host()
            ->ElementHasImplOnlyScrollAnimation(
                inner_viewport_scroll_node->element_id)) {
      return inner_viewport_scroll_node;
    }
  }

  return nullptr;
}

void InputHandler::ScrollLatchedScroller(ScrollState& scroll_state,
                                         base::TimeDelta delayed_by) {
  DCHECK(CurrentlyScrollingNode());
  DCHECK(latched_scroll_type_.has_value());

  ScrollNode& scroll_node = *CurrentlyScrollingNode();
  const gfx::Vector2dF delta(scroll_state.delta_x(), scroll_state.delta_y());
  TRACE_EVENT2("cc", "InputHandler::ScrollLatchedScroller", "delta_x",
               delta.x(), "delta_y", delta.y());
  gfx::Vector2dF applied_delta;
  gfx::Vector2dF delta_applied_to_content;
  std::optional<gfx::PointF> snap_strategy_offset;

  if (ShouldAnimateScroll(scroll_state)) {
    DCHECK(!scroll_state.is_in_inertial_phase());

    if (ScrollNode* animating_scroll_node =
            GetAnimatingNodeForCurrentScrollingNode()) {
      TRACE_EVENT_INSTANT0("cc", "UpdateExistingAnimation",
                           TRACE_EVENT_SCOPE_THREAD);

      // See comment in GetAnimatingNodeForCurrentScrollingNode for explanation
      // of this DCHECK.
      DCHECK(animating_scroll_node->id == scroll_node.id ||
             animating_scroll_node->scrolls_inner_viewport);

      snap_strategy_offset = ScrollAnimationUpdateTarget(*animating_scroll_node,
                                                         delta, delayed_by);

      if (snap_strategy_offset) {
        // Because we updated the animation target, consume delta so we notify
        // the `LatencyInfoSwapPromiseMonitor` to tell it that something
        // happened that will cause a swap in the future. This will happen
        // within the scope of the dispatch of a gesture scroll update input
        // event. If we don't notify during the handling of the input event, the
        // `LatencyInfo` associated with the input event will not be added as a
        // swap promise and we won't get any swap results.
        applied_delta = delta;
      } else {
        TRACE_EVENT_INSTANT0("cc", "Didn't Update Animation",
                             TRACE_EVENT_SCOPE_THREAD);
      }
    } else {
      TRACE_EVENT_INSTANT0("cc", "CreateNewAnimation",
                           TRACE_EVENT_SCOPE_THREAD);
      if (scroll_node.scrolls_outer_viewport) {
        auto result = GetViewport().ScrollAnimated(delta, delayed_by);
        applied_delta = result.consumed_delta;
        SetViewportConsumedDelta(result);
      } else {
        applied_delta = ComputeScrollDelta(scroll_node, delta);
        compositor_delegate_->GetImplDeprecated().ScrollAnimationCreate(
            scroll_node, applied_delta, delayed_by);
      }
      gfx::PointF current_scroll_offset = GetVisualScrollOffset(scroll_node);
      snap_strategy_offset = GetScrollTree().ClampScrollOffsetToLimits(
          current_scroll_offset + applied_delta, scroll_node);
    }

    // Animated scrolling always applied only to the content (i.e. not to the
    // browser controls).
    delta_applied_to_content = delta;
  } else {
    gfx::Point viewport_point(scroll_state.position_x(),
                              scroll_state.position_y());
    if (GetViewport().ShouldScroll(scroll_node)) {
      // |scrolls_outer_viewport| will only ever be false if the scroll chains
      // up to the viewport without going through the outer viewport scroll
      // node. This is because we normally terminate the scroll chain at the
      // outer viewport node.  For example, if we start scrolling from an
      // element that's not a descendant of the root scroller. In these cases we
      // want to scroll *only* the inner viewport -- to allow panning while
      // zoomed -- but still use Viewport::ScrollBy to also move browser
      // controls if needed.
      ViewportScrollResult result = GetViewport().ScrollBy(
          delta, viewport_point, scroll_state.is_direct_manipulation(),
          latched_scroll_type_ != ui::ScrollInputType::kWheel,
          scroll_node.scrolls_outer_viewport);

      applied_delta = result.consumed_delta;
      delta_applied_to_content = result.content_scrolled_delta;
      SetViewportConsumedDelta(result);
    } else {
      applied_delta = ScrollSingleNode(scroll_node, delta, viewport_point,
                                       scroll_state.is_direct_manipulation());
    }
    snap_strategy_offset = GetVisualScrollOffset(scroll_node);
  }
  overscroll_delta_for_main_thread_ += delta - applied_delta;

  // If the layer wasn't able to move, try the next one in the hierarchy.
  bool scrolled = std::abs(applied_delta.x()) > kScrollEpsilon;
  scrolled = scrolled || std::abs(applied_delta.y()) > kScrollEpsilon;
  if (!scrolled) {
    // TODO(bokan): This preserves existing behavior by not allowing tiny
    // scrolls to produce overscroll but is inconsistent in how delta gets
    // chained up. We need to clean this up.
    if (scroll_node.scrolls_outer_viewport)
      scroll_state.ConsumeDelta(applied_delta.x(), applied_delta.y());
    return;
  }

  if (!GetViewport().ShouldScroll(scroll_node)) {
    // If the applied delta is within 45 degrees of the input
    // delta, bail out to make it easier to scroll just one layer
    // in one direction without affecting any of its parents.
    float angle_threshold = 45;
    if (MathUtil::SmallestAngleBetweenVectors(applied_delta, delta) <
        angle_threshold) {
      applied_delta = delta;
    } else {
      // Allow further movement only on an axis perpendicular to the direction
      // in which the layer moved.
      applied_delta = MathUtil::ProjectVector(delta, applied_delta);
    }
    delta_applied_to_content = applied_delta;
  }

  scroll_state.set_caused_scroll(
      std::abs(delta_applied_to_content.x()) > kScrollEpsilon,
      std::abs(delta_applied_to_content.y()) > kScrollEpsilon);
  scroll_state.ConsumeDelta(applied_delta.x(), applied_delta.y());

  did_scroll_x_for_scroll_gesture_ |= scroll_state.caused_scroll_x();
  did_scroll_y_for_scroll_gesture_ |= scroll_state.caused_scroll_y();

  if (snap_strategy_offset && !scroll_state.is_in_inertial_phase()) {
    // We use |last_scroll_update_state_| instead of |scroll_state| as that more
    // closely matches what InputHandler::SnapAtScrollend would use.
    //
    // We validate that `last_scroll_update_state_` exists before using it. As
    // we have seen rare crashes with it null. We do not use
    // `std::optional::value_or` here as that performs a copy of the
    // alternative. Which will rarely ever be needed.
    snap_strategy_ =
        CreateSnapStrategy(last_scroll_update_state_.has_value()
                               ? last_scroll_update_state_.value()
                               : scroll_state,
                           snap_strategy_offset.value(),
                           SnapReason::kScrollOffsetAnimationFinished);
  }
}

bool InputHandler::CanPropagate(ScrollNode* scroll_node, float x, float y) {
  return (x == 0 || scroll_node->overscroll_behavior.x ==
                        OverscrollBehavior::Type::kAuto) &&
         (y == 0 || scroll_node->overscroll_behavior.y ==
                        OverscrollBehavior::Type::kAuto);
}

ScrollNode* InputHandler::FindNodeToLatch(ScrollState* scroll_state,
                                          ScrollNode* starting_node,
                                          ui::ScrollInputType type) {
  ScrollTree& scroll_tree = GetScrollTree();
  ScrollNode* scroll_node = nullptr;
  ScrollNode* first_scrollable_node = nullptr;
  for (ScrollNode* cur_node = starting_node; cur_node;
       cur_node = scroll_tree.parent(cur_node)) {
    if (GetViewport().ShouldScroll(*cur_node)) {
      // Don't chain scrolls past a viewport node. Once we reach that, we
      // should scroll using the appropriate viewport node which may not be
      // |cur_node|.
      scroll_node = GetNodeToScroll(cur_node);
      break;
    }

    if (!cur_node->user_scrollable_horizontal &&
        !cur_node->user_scrollable_vertical) {
      continue;
    }

    if (!first_scrollable_node) {
      first_scrollable_node = cur_node;
    }

    if (CanConsumeDelta(*scroll_state, *cur_node)) {
      scroll_node = cur_node;
      break;
    }

    float delta_x = scroll_state->is_beginning() ? scroll_state->delta_x_hint()
                                                 : scroll_state->delta_x();
    float delta_y = scroll_state->is_beginning() ? scroll_state->delta_y_hint()
                                                 : scroll_state->delta_y();

    if (!CanPropagate(cur_node, delta_x, delta_y)) {
      // If we reach a node with non-auto overscroll-behavior and we still
      // haven't latched, we must latch to it. Consider a fully scrolled node
      // with non-auto overscroll-behavior: we are not allowed to further
      // chain scroll delta passed to it in the current direction but if we
      // reverse direction we should scroll it so we must be latched to it.
      scroll_node = cur_node;
      scroll_state->set_is_scroll_chain_cut(true);
      break;
    }
  }

  // If the root scroller can not consume delta in an autoscroll, latch on
  // to the top most autoscrollable scroller. See https://crbug.com/969150
  if ((type == ui::ScrollInputType::kAutoscroll) && first_scrollable_node) {
    // If scroll_node is nullptr or delta can not be consumed
    if (!(scroll_node && CanConsumeDelta(*scroll_state, *scroll_node)))
      scroll_node = first_scrollable_node;
  }

  return scroll_node;
}

void InputHandler::UpdateRootLayerStateForSynchronousInputHandler() {
  if (!input_handler_client_)
    return;
  input_handler_client_->UpdateRootLayerStateForSynchronousInputHandler(
      ActiveTree().TotalScrollOffset(), ActiveTree().TotalMaxScrollOffset(),
      ActiveTree().ScrollableSize(), ActiveTree().current_page_scale_factor(),
      ActiveTree().min_page_scale_factor(),
      ActiveTree().max_page_scale_factor());
}

void InputHandler::DidLatchToScroller(const ScrollState& scroll_state,
                                      ui::ScrollInputType type) {
  DCHECK(CurrentlyScrollingNode());
  deferred_scroll_end_ = false;
  compositor_delegate_->GetImplDeprecated()
      .browser_controls_manager()
      ->ScrollBegin();
  compositor_delegate_->GetImplDeprecated()
      .mutator_host()
      ->ScrollAnimationAbort(CurrentlyScrollingNode()->element_id);

  scroll_animating_snap_target_ids_ = TargetSnapAreaElementIds();

  last_latched_scroller_ = CurrentlyScrollingNode()->element_id;
  latched_scroll_type_ = type;
  last_scroll_begin_state_ = scroll_state;

  compositor_delegate_->DidStartScroll();

  UpdateScrollSourceInfo(scroll_state, type);
}

bool InputHandler::CanConsumeDelta(const ScrollState& scroll_state,
                                   const ScrollNode& scroll_node) {
  gfx::Vector2dF delta_to_scroll;
  if (scroll_state.is_beginning()) {
    delta_to_scroll = gfx::Vector2dF(scroll_state.delta_x_hint(),
                                     scroll_state.delta_y_hint());
  } else {
    delta_to_scroll =
        gfx::Vector2dF(scroll_state.delta_x(), scroll_state.delta_y());
  }

  if (delta_to_scroll == gfx::Vector2dF())
    return true;

  if (scroll_state.is_direct_manipulation()) {
    gfx::Vector2dF local_scroll_delta;
    if (!CalculateLocalScrollDeltaAndStartPoint(
            scroll_node,
            gfx::PointF(scroll_state.position_x(), scroll_state.position_y()),
            delta_to_scroll, &local_scroll_delta)) {
      return false;
    }
    delta_to_scroll = local_scroll_delta;
  } else {
    delta_to_scroll = ResolveScrollGranularityToPixels(
        scroll_node, delta_to_scroll, scroll_state.delta_granularity());
  }

  if (ComputeScrollDelta(scroll_node, delta_to_scroll) != gfx::Vector2dF())
    return true;

  return false;
}

bool InputHandler::ShouldAnimateScroll(const ScrollState& scroll_state) const {
  if (!compositor_delegate_->GetSettings().enable_smooth_scroll)
    return false;

  bool has_precise_scroll_deltas = scroll_state.delta_granularity() ==
                                   ui::ScrollGranularity::kScrollByPrecisePixel;

  return !has_precise_scroll_deltas;
}

bool InputHandler::SnapAtScrollEnd(SnapReason reason) {
  ScrollNode* scroll_node = CurrentlyScrollingNode();
  if (!scroll_node || !scroll_node->snap_container_data.has_value())
    return false;

  SnapContainerData& data = scroll_node->snap_container_data.value();
  gfx::PointF current_position = GetVisualScrollOffset(*scroll_node);

  if (!snap_strategy_ || snap_fling_state_ == kConstrainedNativeFling) {
    // If this was a constrained native fling, SnapFlingController would not
    // have had the correct final scroll position with which to create the snap
    // strategy.
    // Also, you might think that if a scroll never received a scroll update we
    // could just drop the snap. However, if the GSB+GSE arrived while we were
    // mid-snap from a previous gesture, this would leave the scroller at a
    // non-snap-point.
    DCHECK(last_scroll_update_state_ || last_scroll_begin_state_);
    ScrollState& last_scroll_state = last_scroll_update_state_
                                         ? *last_scroll_update_state_
                                         : *last_scroll_begin_state_;
    snap_strategy_ =
        CreateSnapStrategy(last_scroll_state, current_position, reason);
  }

  double snapport_height_adjustment =
      scroll_node->scrolls_outer_viewport
          ? PredictViewportBoundsDelta(gfx::Vector2dF())
          : 0;
  SnapPositionData snap = data.FindSnapPositionWithViewportAdjustment(
      *snap_strategy_, snapport_height_adjustment);
  if (snap.type == SnapPositionData::Type::kNone) {
    // Ensure we retain the ids of any element we were previously snapped to and
    // are still snapped to in case of scrolls in an axis where no snapping
    // happens.
    if (reason == SnapReason::kScrollOffsetAnimationFinished) {
      scroll_animating_snap_target_ids_ = snap.target_element_ids;
    } else if (data.SetTargetSnapAreaElementIds(snap.target_element_ids)) {
      updated_snapped_elements_[scroll_node->element_id] =
          snap.target_element_ids;
    }
    return false;
  }

  // TODO(bokan): Why only on the viewport?
  if (GetViewport().ShouldScroll(*scroll_node)) {
    compositor_delegate_->WillScrollContent(scroll_node->element_id);
  }

  gfx::Vector2dF delta = snap.position - current_position;
  bool did_animate = false;
  if (scroll_node->scrolls_outer_viewport) {
    gfx::Vector2dF scaled_delta(delta);
    scaled_delta.Scale(compositor_delegate_->PageScaleFactor());
    auto result = GetViewport().ScrollAnimated(scaled_delta, base::TimeDelta());
    gfx::Vector2dF consumed_delta = result.consumed_delta;
    did_animate = !consumed_delta.IsZero();
    SetViewportConsumedDelta(result);
  } else {
    did_animate =
        compositor_delegate_->GetImplDeprecated().ScrollAnimationCreate(
            *scroll_node, delta, base::TimeDelta());
  }
  DCHECK(!IsAnimatingForSnap());
  if (did_animate) {
    // The snap target will be set when the animation is completed.
    scroll_animating_snap_target_ids_ = snap.target_element_ids;
  } else if (data.SetTargetSnapAreaElementIds(snap.target_element_ids)) {
    updated_snapped_elements_[scroll_node->element_id] =
        snap.target_element_ids;
    SetNeedsCommit();
  }
  return did_animate;
}

bool InputHandler::IsAnimatingForSnap() const {
  return scroll_animating_snap_target_ids_ != TargetSnapAreaElementIds();
}

gfx::PointF InputHandler::GetVisualScrollOffset(
    const ScrollNode& scroll_node) const {
  if (scroll_node.scrolls_outer_viewport)
    return GetViewport().TotalScrollOffset();
  return GetScrollTree().current_scroll_offset(scroll_node.element_id);
}

void InputHandler::ClearCurrentlyScrollingNode() {
  TRACE_EVENT0("cc", "InputHandler::ClearCurrentlyScrollingNode");
  ActiveTree().ClearCurrentlyScrollingNode();
  accumulated_root_overscroll_ = gfx::Vector2dF();
  did_scroll_x_for_scroll_gesture_ = false;
  did_scroll_y_for_scroll_gesture_ = false;
  delta_consumed_for_scroll_gesture_ = false;
  scroll_animating_snap_target_ids_ = TargetSnapAreaElementIds();
  latched_scroll_type_.reset();
  last_scroll_update_state_.reset();
  last_scroll_begin_state_.reset();
  compositor_delegate_->DidEndScroll();
}

std::optional<gfx::PointF> InputHandler::ScrollAnimationUpdateTarget(
    const ScrollNode& scroll_node,
    const gfx::Vector2dF& scroll_delta,
    base::TimeDelta delayed_by) {
  // TODO(bokan): Remove |scroll_node| as a parameter and just use the value
  // coming from |mutator_host|.
  DCHECK(compositor_delegate_->GetImplDeprecated()
             .mutator_host()
             ->ElementHasImplOnlyScrollAnimation(scroll_node.element_id));

  float scale_factor = compositor_delegate_->PageScaleFactor();
  gfx::Vector2dF adjusted_delta =
      gfx::ScaleVector2d(scroll_delta, 1.f / scale_factor);
  adjusted_delta = UserScrollableDelta(scroll_node, adjusted_delta);

  std::optional<gfx::PointF> animation_target =
      compositor_delegate_->GetImplDeprecated()
          .mutator_host()
          ->ImplOnlyScrollAnimationUpdateTarget(
              adjusted_delta, GetScrollTree().MaxScrollOffset(scroll_node.id),
              compositor_delegate_->GetImplDeprecated()
                  .CurrentBeginFrameArgs()
                  .frame_time,
              delayed_by, scroll_node.element_id);
  if (animation_target) {
    compositor_delegate_->DidUpdateScrollAnimationCurve();

    // The animation is no longer targeting a snap position. By clearing the
    // target, this will ensure that we attempt to resnap at the end of this
    // animation.
    scroll_animating_snap_target_ids_ = TargetSnapAreaElementIds();
  }

  return animation_target;
}

void InputHandler::UpdateScrollSourceInfo(const ScrollState& scroll_state,
                                          ui::ScrollInputType type) {
  if (type == ui::ScrollInputType::kWheel &&
      scroll_state.delta_granularity() ==
          ui::ScrollGranularity::kScrollByPrecisePixel) {
    has_scrolled_by_precisiontouchpad_ = true;
  } else if (type == ui::ScrollInputType::kWheel) {
    has_scrolled_by_wheel_ = true;
  } else if (type == ui::ScrollInputType::kTouchscreen) {
    has_scrolled_by_touch_ = true;
  } else if (type == ui::ScrollInputType::kScrollbar) {
    has_scrolled_by_scrollbar_ = true;
  }
}

// Return true if scrollable node for 'ancestor' is the same as 'child' or an
// ancestor along the scroll tree.
bool InputHandler::IsScrolledBy(LayerImpl* child, ScrollNode* ancestor) {
  DCHECK(ancestor && (ancestor->user_scrollable_horizontal ||
                      ancestor->user_scrollable_vertical));
  if (!child)
    return false;
  DCHECK_EQ(child->layer_tree_impl(), &ActiveTree());
  ScrollTree& scroll_tree = GetScrollTree();
  for (ScrollNode* scroll_node = scroll_tree.Node(child->scroll_tree_index());
       scroll_node; scroll_node = scroll_tree.parent(scroll_node)) {
    if (scroll_node->id == ancestor->id)
      return true;
  }
  return false;
}

gfx::Vector2dF InputHandler::UserScrollableDelta(
    const ScrollNode& node,
    const gfx::Vector2dF& delta) const {
  gfx::Vector2dF adjusted_delta = delta;
  if (!node.user_scrollable_horizontal)
    adjusted_delta.set_x(0);
  if (!node.user_scrollable_vertical)
    adjusted_delta.set_y(0);

  return adjusted_delta;
}

bool InputHandler::ScrollbarScrollIsActive() {
  return scrollbar_controller_->ScrollbarScrollIsActive();
}

void InputHandler::SetDeferBeginMainFrame(bool defer_begin_main_frame) const {
  compositor_delegate_->SetDeferBeginMainFrame(defer_begin_main_frame);
}

void InputHandler::UpdateBrowserControlsState(
    BrowserControlsState constraints,
    BrowserControlsState current,
    bool animate,
    base::optional_ref<const BrowserControlsOffsetTagsInfo> offset_tags_info) {
  compositor_delegate_->UpdateBrowserControlsState(constraints, current,
                                                   animate, offset_tags_info);
}

void InputHandler::SetIsHandlingTouchSequence(bool is_handling_touch_sequence) {
  // We should not attempt to start handling a touch sequence twice.
  DCHECK(!is_handling_touch_sequence || !is_handling_touch_sequence_);
  is_handling_touch_sequence_ = is_handling_touch_sequence;
}

bool InputHandler::CurrentScrollNeedsFrameAlignment() const {
  if (const ScrollNode* node = CurrentlyScrollingNode()) {
    // We need frame-aligned handling of GestureScrollUpdate if an animation
    // is linked to the scroll position.  If we update the scroll offset between
    // tick and draw, then things will be out of sync in the drawn frame.
    if (compositor_delegate_->HasScrollLinkedAnimation(node->element_id)) {
      return true;
    }
  }
  return false;
}

std::unique_ptr<SnapSelectionStrategy> InputHandler::CreateSnapStrategy(
    const ScrollState& scroll_state,
    const gfx::PointF& current_offset,
    SnapReason snap_reason) const {
  const gfx::Vector2dF scroll_delta = scroll_state.DeltaOrHint();
  if (latched_scroll_type_ == ui::ScrollInputType::kWheel &&
      scroll_state.delta_granularity() !=
          ui::ScrollGranularity::kScrollByPrecisePixel &&
      !scroll_delta.IsZero() &&
      snap_reason == SnapReason::kScrollOffsetAnimationFinished) {
    // This was an imprecise wheel scroll so use direction snapping.
    // Note: gesture scroll end is delayed in anticipation of future wheel
    // scrolls so it is fired well after the scroll ends as opposed to precise
    // touch devices where we fire it as soon as the user lifts their finger.
    // TODO(crbug.com/40762499): The directional scroll should probably be
    // triggered at gesture scroll begin to improve responsiveness.
    return SnapSelectionStrategy::CreateForDirection(current_offset,
                                                     scroll_delta, true);
  } else {
    return SnapSelectionStrategy::CreateForEndPosition(
        current_offset, did_scroll_x_for_scroll_gesture_,
        did_scroll_y_for_scroll_gesture_);
  }
}

void InputHandler::SetViewportConsumedDelta(
    const ViewportScrollResult& result) {
  if (std::abs(result.outer_viewport_scrolled_delta.x()) > kScrollEpsilon ||
      std::abs(result.outer_viewport_scrolled_delta.y()) > kScrollEpsilon) {
    outer_viewport_consumed_delta_ = true;
  }
  if (std::abs(result.inner_viewport_scrolled_delta.x()) > kScrollEpsilon ||
      std::abs(result.inner_viewport_scrolled_delta.y()) > kScrollEpsilon) {
    inner_viewport_consumed_delta_ = true;
  }
}

}  // namespace cc
