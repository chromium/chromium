// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_INPUT_THREADED_INPUT_HANDLER_H_
#define CC_INPUT_THREADED_INPUT_HANDLER_H_

#include <memory>

#include "base/containers/flat_set.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "cc/input/compositor_input_interfaces.h"
#include "cc/input/event_listener_properties.h"
#include "cc/input/input_handler.h"
#include "cc/input/scroll_snap_data.h"
#include "cc/input/scroll_state.h"
#include "cc/input/touch_action.h"
#include "cc/metrics/events_metrics_manager.h"
#include "cc/metrics/frame_sequence_metrics.h"
#include "cc/paint/element_id.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "ui/events/types/scroll_input_type.h"

namespace gfx {
class Point;
class PointF;
class ScrollOffset;
}  // namespace gfx

namespace cc {

class LayerImpl;
class ScrollbarController;
class ScrollElasticityHelper;
struct ScrollNode;
class ScrollTree;
class SwapPromiseMonitor;
class Viewport;

class CC_EXPORT ThreadedInputHandler : public InputHandler,
                                       public InputDelegateForCompositor {
 public:
  explicit ThreadedInputHandler(
      CompositorDelegateForInput& compositor_delegate);
  ~ThreadedInputHandler() override;

  // =========== InputHandler "Interface" - will override in a future CL
  base::WeakPtr<InputHandler> AsWeakPtr() const override;
  void BindToClient(InputHandlerClient* client) override;
  InputHandler::ScrollStatus ScrollBegin(ScrollState* scroll_state,
                                         ui::ScrollInputType type) override;
  InputHandler::ScrollStatus RootScrollBegin(ScrollState* scroll_state,
                                             ui::ScrollInputType type) override;
  InputHandlerScrollResult ScrollUpdate(
      ScrollState* scroll_state,
      base::TimeDelta delayed_by = base::TimeDelta()) override;
  void ScrollEnd(bool should_snap = false) override;
  void RecordScrollBegin(ui::ScrollInputType input_type,
                         ScrollBeginThreadState scroll_start_state) override;
  void RecordScrollEnd(ui::ScrollInputType input_type) override;
  InputHandlerPointerResult MouseMoveAt(
      const gfx::Point& viewport_point) override;
  InputHandlerPointerResult MouseDown(const gfx::PointF& viewport_point,
                                      bool shift_modifier) override;
  InputHandlerPointerResult MouseUp(const gfx::PointF& viewport_point) override;
  void MouseLeave() override;
  ElementId FindFrameElementIdAtPoint(
      const gfx::PointF& viewport_point) override;
  void RequestUpdateForSynchronousInputHandler() override;
  void SetSynchronousInputHandlerRootScrollOffset(
      const gfx::ScrollOffset& root_content_offset) override;
  void PinchGestureBegin() override;
  void PinchGestureUpdate(float magnify_delta,
                          const gfx::Point& anchor) override;
  void PinchGestureEnd(const gfx::Point& anchor, bool snap_to_min) override;
  void SetNeedsAnimateInput() override;
  bool IsCurrentlyScrollingViewport() const override;
  EventListenerProperties GetEventListenerProperties(
      EventListenerClass event_class) const override;
  bool HasBlockingWheelEventHandlerAt(
      const gfx::Point& viewport_point) const override;
  InputHandler::TouchStartOrMoveEventListenerType
  EventListenerTypeForTouchStartOrMoveAt(
      const gfx::Point& viewport_port,
      TouchAction* out_touch_action) override;
  std::unique_ptr<SwapPromiseMonitor> CreateLatencyInfoSwapPromiseMonitor(
      ui::LatencyInfo* latency) override;
  std::unique_ptr<EventsMetricsManager::ScopedMonitor>
  GetScopedEventMetricsMonitor(
      std::unique_ptr<EventMetrics> event_metrics) override;
  ScrollElasticityHelper* CreateScrollElasticityHelper() override;
  bool GetScrollOffsetForLayer(ElementId element_id,
                               gfx::ScrollOffset* offset) override;
  bool ScrollLayerTo(ElementId element_id,
                     const gfx::ScrollOffset& offset) override;
  bool ScrollingShouldSwitchtoMainThread() override;
  bool GetSnapFlingInfoAndSetAnimatingSnapTarget(
      const gfx::Vector2dF& natural_displacement_in_viewport,
      gfx::Vector2dF* out_initial_position,
      gfx::Vector2dF* out_target_position) override;
  void ScrollEndForSnapFling(bool did_finish) override;
  void NotifyInputEvent() override;

  // =========== InputDelegateForCompositor Interface - This section implements
  // the interface that LayerTreeHostImpl uses to communicate with the input
  // system.
  void ProcessCommitDeltas(CompositorCommitData* commit_data) override;
  void TickAnimations(base::TimeTicks monotonic_time) override;
  void WillShutdown() override;
  void WillDraw() override;
  void WillBeginImplFrame(const viz::BeginFrameArgs& args) override;
  void DidCommit() override;
  void DidActivatePendingTree() override;
  void RootLayerStateMayHaveChanged() override;
  void DidUnregisterScrollbar(ElementId scroll_element_id,
                              ScrollbarOrientation orientation) override;
  void ScrollOffsetAnimationFinished() override;
  bool IsCurrentlyScrolling() const override;
  bool IsActivelyPrecisionScrolling() const override;

  // =========== Public Interface

  bool CanConsumeDelta(const ScrollState& scroll_state,
                       const ScrollNode& scroll_node);
  // Returns the amount of delta that can be applied to scroll_node, taking
  // page scale into account.
  gfx::Vector2dF ComputeScrollDelta(const ScrollNode& scroll_node,
                                    const gfx::Vector2dF& delta);

  gfx::Vector2dF ScrollSingleNode(const ScrollNode& scroll_node,
                                  const gfx::Vector2dF& delta,
                                  const gfx::Point& viewport_point,
                                  bool is_direct_manipulation);

  // Resolves a delta in the given granularity for the |scroll_node| into
  // physical pixels to scroll.
  gfx::Vector2dF ResolveScrollGranularityToPixels(
      const ScrollNode& scroll_node,
      const gfx::Vector2dF& scroll_delta,
      ui::ScrollGranularity granularity);

  // Used to set the pinch gesture active state when the pinch gesture is
  // handled on another layer tree. In a page with OOPIFs, only the main
  // frame's layer tree directly handles pinch events. But layer trees for
  // sub-frames need to know when pinch gestures are active so they can
  // throttle the re-rastering. This function allows setting this flag on
  // OOPIF layer trees using information sent (initially) from the main-frame.
  void set_external_pinch_gesture_active(bool external_pinch_gesture_active) {
    external_pinch_gesture_active_ = external_pinch_gesture_active;
    // Only one of the flags should ever be true at any given time.
    DCHECK(!pinch_gesture_active_ || !external_pinch_gesture_active_);
  }

  bool pinch_gesture_active() const {
    return pinch_gesture_active_ || external_pinch_gesture_active_;
  }

  void set_force_smooth_wheel_scrolling_for_testing(bool enabled) {
    force_smooth_wheel_scrolling_for_testing_ = enabled;
  }

  gfx::Vector2dF accumulated_root_overscroll_for_testing() const {
    return accumulated_root_overscroll_;
  }

  bool animating_for_snap_for_testing() const { return IsAnimatingForSnap(); }

 private:
  FRIEND_TEST_ALL_PREFIXES(ScrollUnifiedLayerTreeHostImplTest,
                           AnimatedScrollYielding);
  FRIEND_TEST_ALL_PREFIXES(ScrollUnifiedLayerTreeHostImplTest,
                           AutoscrollOnDeletedScrollbar);
  FRIEND_TEST_ALL_PREFIXES(ScrollUnifiedLayerTreeHostImplTest,
                           ThumbDragAfterJumpClick);
  FRIEND_TEST_ALL_PREFIXES(LayerTreeHostImplTest, AutoscrollTaskAbort);

  // This method gets the scroll offset for a regular scroller, or the combined
  // visual and layout offsets of the viewport.
  gfx::ScrollOffset GetVisualScrollOffset(const ScrollNode& scroll_node) const;
  bool IsScrolledBy(LayerImpl* child, ScrollNode* ancestor);
  bool IsAnimatingForSnap() const;

  ScrollNode* CurrentlyScrollingNode();
  const ScrollNode* CurrentlyScrollingNode() const;
  void ClearCurrentlyScrollingNode();
  ScrollTree& GetScrollTree();
  ScrollTree& GetScrollTree() const;
  Viewport& GetViewport() const;

  ScrollNode* InnerViewportScrollNode() const;
  ScrollNode* OuterViewportScrollNode() const;

  void SetNeedsCommit();
  LayerTreeImpl& ActiveTree();
  LayerTreeImpl& ActiveTree() const;

  bool IsMainThreadScrolling(const InputHandler::ScrollStatus& status,
                             const ScrollNode* scroll_node) const;

  bool IsTouchDraggingScrollbar(
      LayerImpl* first_scrolling_layer_or_drawn_scrollbar,
      ui::ScrollInputType type);

  void UpdateRootLayerStateForSynchronousInputHandler();

  // Called during ScrollBegin once a scroller was successfully latched to
  // (i.e.  it can and will consume scroll delta on the compositor thread). The
  // latched scroller is now available in CurrentlyScrollingNode().
  // TODO(bokan): There's some debate about the name of this method. We should
  // get consensus on terminology to use and apply it consistently.
  // https://crrev.com/c/1981336/9/cc/trees/layer_tree_host_impl.cc#4520
  void DidLatchToScroller(const ScrollState& scroll_state,
                          ui::ScrollInputType type);

  // This function keeps track of sources of scrolls that are handled in the
  // compositor side. The information gets shared by the main thread as part of
  // the begin_main_frame_state. Finally Use counters are updated in the main
  // thread side to keep track of the frequency of scrolling with different
  // sources per page load. TODO(crbug.com/691886): Use GRC API to plumb the
  // scroll source info for Use Counters.
  void UpdateScrollSourceInfo(const ScrollState& scroll_state,
                              ui::ScrollInputType type);

  // Applies the scroll_state to the currently latched scroller. See comment in
  // InputHandler::ScrollUpdate declaration for the meaning of |delayed_by|.
  void ScrollLatchedScroller(ScrollState* scroll_state,
                             base::TimeDelta delayed_by);

  // Determines whether the given scroll node can scroll on the compositor
  // thread or if there are any reasons it must be scrolled on the main thread
  // or not at all. Note: in general, this is not sufficient to determine if a
  // scroll can occur on the compositor thread. If hit testing to a scroll
  // node, the caller must also check whether the hit point intersects a
  // non-fast-scrolling-region of any ancestor scrolling layers. Can be removed
  // after scroll unification https://crbug.com/476553.
  InputHandler::ScrollStatus TryScroll(const ScrollTree& scroll_tree,
                                       ScrollNode* scroll_node) const;

  // Creates an animation curve and returns true if we need to update the
  // scroll position to a snap point. Otherwise returns false.
  bool SnapAtScrollEnd();

  // |layer| is returned from a regular hit test, and
  // |first_scrolling_layer_or_drawn_scrollbar| is returned from a hit test
  // performed only on scrollers and scrollbars. Initial scroll hit testing can
  // be unreliable if the latter is not the direct scroll ancestor of the
  // former. In this case, we will fall back to main thread scrolling because
  // the compositor thread doesn't know which layer to scroll. This happens when
  // a layer covers a scroller that doesn't scroll the former, or a scroller is
  // masked by a mask layer for mask image, clip-path, rounded border, etc.
  //
  // Note, position: fixed layers use the inner viewport as their ScrollNode
  // (since they don't scroll with the outer viewport), however, scrolls from
  // the fixed layer still chain to the outer viewport. It's also possible for a
  // node to have the inner viewport as its ancestor without going through the
  // outer viewport; however, it may still scroll using the viewport(). Hence,
  // this method must use the same scroll chaining logic we use in ApplyScroll.
  bool IsInitialScrollHitTestReliable(
      const LayerImpl* layer,
      const LayerImpl* first_scrolling_layer_or_drawn_scrollbar) const;

  // Similar to above but includes complicated logic to determine whether the
  // ScrollNode is able to be scrolled on the compositor or requires main
  // thread scrolling. If main thread scrolling is required
  // |scroll_on_main_thread| is set to true and the reason is given in
  // |main_thread_scrolling_reason| to on of the enum values in
  // main_thread_scrolling_reason.h. Can be removed after scroll unification
  // https://crbug.com/476553.
  ScrollNode* FindScrollNodeForCompositedScrolling(
      const gfx::PointF& device_viewport_point,
      LayerImpl* layer_hit_by_point,
      bool* scroll_on_main_thread,
      uint32_t* main_thread_scrolling_reason);

  // Return all ScrollNode indices that have an associated layer with a non-fast
  // region that intersects the point.
  base::flat_set<int> NonFastScrollableNodes(
      const gfx::PointF& device_viewport_point) const;

  // Returns the ScrollNode we should use to scroll, accounting for viewport
  // scroll chaining rules.
  ScrollNode* GetNodeToScroll(ScrollNode* node) const;

  // Given a starting node (determined by hit-test), walks up the scroll tree
  // looking for the first node that can consume scroll from the given
  // scroll_state and returns the first such node. If none is found, or if
  // starting_node is nullptr, returns nullptr;
  ScrollNode* FindNodeToLatch(ScrollState* scroll_state,
                              ScrollNode* starting_node,
                              ui::ScrollInputType type);

  bool CanPropagate(ScrollNode* scroll_node, float x, float y);

  // Performs a hit test to determine the ScrollNode to use when scrolling at
  // |viewport_point|. If no layer is hit, this falls back to the inner
  // viewport scroll node. Returns:
  // - If |hit_test_sucessful| is false, hit testing has failed and the
  //   compositor cannot determine the correct scroll node (e.g. see comments in
  //   IsInitialScrollHitTestReliable). |scroll_node| is always nullptr in this
  //   case.
  // - If |hit_test_successful| is true, returns the ScrollNode to use in
  //   |scroll_node|. This can be nullptr if no layer was hit and there are no
  //   viewport nodes (e.g. OOPIF, UI compositor).
  struct ScrollHitTestResult {
    ScrollNode* scroll_node;
    bool hit_test_successful;
  };
  ScrollHitTestResult HitTestScrollNode(
      const gfx::PointF& device_viewport_point) const;

  bool ShouldAnimateScroll(const ScrollState& scroll_state) const;

  bool ScrollAnimationUpdateTarget(const ScrollNode& scroll_node,
                                   const gfx::Vector2dF& scroll_delta,
                                   base::TimeDelta delayed_by);

  // Transforms viewport start point and scroll delta to local start point and
  // local delta, respectively. If the transformation of either the start or end
  // point of a scroll is clipped, the function returns false.
  bool CalculateLocalScrollDeltaAndStartPoint(
      const ScrollNode& scroll_node,
      const gfx::PointF& viewport_point,
      const gfx::Vector2dF& viewport_delta,
      gfx::Vector2dF* out_local_scroll_delta,
      gfx::PointF* out_local_start_point = nullptr);
  gfx::Vector2dF ScrollNodeWithViewportSpaceDelta(
      const ScrollNode& scroll_node,
      const gfx::PointF& viewport_point,
      const gfx::Vector2dF& viewport_delta);
  gfx::Vector2dF ScrollNodeWithLocalDelta(
      const ScrollNode& scroll_node,
      const gfx::Vector2dF& local_delta) const;
  // This helper returns an adjusted version of |delta| where the scroll delta
  // is cleared in any axis in which user scrolling is disabled (e.g. by
  // |overflow-x: hidden|).
  gfx::Vector2dF UserScrollableDelta(const ScrollNode& node,
                                     const gfx::Vector2dF& delta) const;

  FrameSequenceTrackerType GetTrackerTypeForScroll(
      ui::ScrollInputType input_type) const;

  ScrollbarController* scrollbar_controller_for_testing() const {
    return scrollbar_controller_.get();
  }

  // The input handler is owned by the delegate so their lifetimes are tied
  // together.
  CompositorDelegateForInput& compositor_delegate_;

  InputHandlerClient* input_handler_client_ = nullptr;

  // An object to implement the ScrollElasticityHelper interface and
  // hold all state related to elasticity. May be nullptr if never requested.
  std::unique_ptr<ScrollElasticityHelper> scroll_elasticity_helper_;

  // Manages composited scrollbar hit testing.
  std::unique_ptr<ScrollbarController> scrollbar_controller_;

  // Overscroll delta accumulated on the viewport throughout a scroll gesture;
  // reset when the gesture ends.
  gfx::Vector2dF accumulated_root_overscroll_;

  // Unconsumed scroll delta sent to the main thread for firing overscroll DOM
  // events. Resets after each commit.
  gfx::Vector2dF overscroll_delta_for_main_thread_;

  // The source device type that started the scroll gesture. Only set between a
  // ScrollBegin and ScrollEnd.
  base::Optional<ui::ScrollInputType> latched_scroll_type_;

  // Tracks the last scroll update/begin state received. Used to infer the most
  // recent scroll type and direction.
  base::Optional<ScrollState> last_scroll_begin_state_;
  base::Optional<ScrollState> last_scroll_update_state_;

  // If a scroll snap is being animated, then the value of this will be the
  // element id(s) of the target(s). Otherwise, the ids will be invalid.
  // At the end of a scroll animation, the target should be set as the scroll
  // node's snap target.
  TargetSnapAreaElementIds scroll_animating_snap_target_ids_;

  // A set of elements that scroll-snapped to a new target since the last
  // begin main frame. The snap target ids of these elements will be sent to
  // the main thread in the next begin main frame.
  base::flat_set<ElementId> updated_snapped_elements_;

  ElementId scroll_element_id_mouse_currently_over_;
  ElementId scroll_element_id_mouse_currently_captured_;

  // Set in ScrollBegin and outlives the currently scrolling node so it can be
  // used to send the scrollend and overscroll DOM events from the main thread
  // when scrolling occurs on the compositor thread. This value is cleared at
  // the first commit after a GSE.
  ElementId last_latched_scroller_;

  // Scroll animation can finish either before or after GSE arrival.
  // deferred_scroll_end_ is set when the GSE has arrvied before scroll
  // animation completion. ScrollEnd will get called once the animation is
  // over.
  bool deferred_scroll_end_ = false;

  // Set to true when a scroll gesture being handled on the compositor has
  // ended. i.e. When a GSE has arrived and any ongoing scroll animation has
  // ended.
  bool scroll_gesture_did_end_ = false;

  // True iff some of the delta has been consumed for the current scroll
  // sequence on the specific axis.
  bool did_scroll_x_for_scroll_gesture_ = false;
  bool did_scroll_y_for_scroll_gesture_ = false;

  // TODO(bokan): Mac doesn't yet have smooth scrolling for wheel; however, to
  // allow consistency in tests we use this bit to override that decision.
  // https://crbug.com/574283.
  bool force_smooth_wheel_scrolling_for_testing_ = false;

  // This value is used to allow the compositor to throttle re-rastering during
  // pinch gestures, when the page scale factor may be changing frequently. It
  // is set in one of two ways:
  // i) In a layer tree serving the root of the frame/compositor tree, it is
  // directly set during processing of GesturePinch events on the impl thread
  // (only the root layer tree has access to these).
  // ii) In a layer tree serving a sub-frame in the frame/compositor tree, it
  // is set from the main thread during the commit process, using information
  // sent from the root layer tree via IPC messaging.
  bool pinch_gesture_active_ = false;
  bool external_pinch_gesture_active_ = false;
  bool pinch_gesture_end_should_clear_scrolling_node_ = false;

  // These are used to transfer usage of different types of scrolling to the
  // main thread.
  bool has_pinch_zoomed_ = false;
  bool has_scrolled_by_wheel_ = false;
  bool has_scrolled_by_touch_ = false;
  bool has_scrolled_by_precisiontouchpad_ = false;

  // Must be the last member to ensure this is destroyed first in the
  // destruction order and invalidates all weak pointers.
  base::WeakPtrFactory<ThreadedInputHandler> weak_factory_{this};
};

}  // namespace cc

#endif  // CC_INPUT_THREADED_INPUT_HANDLER_H_
