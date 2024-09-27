// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_INPUT_INPUT_HANDLER_H_
#define CC_INPUT_INPUT_HANDLER_H_

#include <memory>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/time/time.h"
#include "base/types/optional_ref.h"
#include "cc/cc_export.h"
#include "cc/input/browser_controls_offset_tags_info.h"
#include "cc/input/browser_controls_state.h"
#include "cc/input/compositor_input_interfaces.h"
#include "cc/input/event_listener_properties.h"
#include "cc/input/main_thread_scrolling_reason.h"
#include "cc/input/overscroll_behavior.h"
#include "cc/input/scroll_state.h"
#include "cc/input/scrollbar.h"
#include "cc/input/touch_action.h"
#include "cc/metrics/events_metrics_manager.h"
#include "cc/metrics/frame_sequence_metrics.h"
#include "cc/paint/element_id.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "ui/events/types/scroll_input_type.h"
#include "ui/events/types/scroll_types.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace gfx {
class Point;
class SizeF;
}  // namespace gfx

namespace ui {
class LatencyInfo;
}  // namespace ui

namespace cc {

class CompositorDelegateForInput;
class LatencyInfoSwapPromiseMonitor;
class LayerImpl;
class ScrollbarController;
class ScrollElasticityHelper;
class Viewport;

enum class PointerResultType { kUnhandled = 0, kScrollbarScroll };

// These enum values are reported in UMA. So these values should never be
// removed or changed.
enum class ScrollBeginThreadState {
  kScrollingOnCompositor = 0,
  kScrollingOnCompositorBlockedOnMain = 1,
  kScrollingOnMain = 2,
  kMaxValue = kScrollingOnMain,
};

struct CC_EXPORT InputHandlerPointerResult {
  InputHandlerPointerResult() = default;
  // Tells what type of processing occurred in the input handler as a result of
  // the pointer event.
  PointerResultType type = PointerResultType::kUnhandled;

  // Tells what scroll_units should be used.
  ui::ScrollGranularity scroll_units =
      ui::ScrollGranularity::kScrollByPrecisePixel;

  // If the input handler processed the event as a scrollbar scroll, it will
  // return a gfx::Vector2dF that produces the necessary scroll. However,
  // it is still the client's responsibility to generate the gesture scrolls
  // instead of the input handler performing it as a part of handling the
  // pointer event (due to the latency attribution that happens at the
  // InputHandlerProxy level).
  gfx::Vector2dF scroll_delta;

  // Used to determine which scroll_node needs to be scrolled. The primary
  // purpose of this is to avoid hit testing for gestures that already know
  // which scroller to target.
  ElementId target_scroller;
};

struct CC_EXPORT InputHandlerScrollResult {
  InputHandlerScrollResult() = default;
  // Did any layer scroll as a result this ScrollUpdate call?
  bool did_scroll = false;
  // Was any of the scroll delta argument to this ScrollUpdate call not used?
  bool did_overscroll_root = false;
  // The total overscroll that has been accumulated by all ScrollUpdate calls
  // that have had overscroll since the last ScrollBegin call. This resets upon
  // a ScrollUpdate with no overscroll.
  gfx::Vector2dF accumulated_root_overscroll;
  // The amount of the scroll delta argument to this ScrollUpdate call that was
  // not used for scrolling.
  gfx::Vector2dF unused_scroll_delta;
  // How the browser should handle the overscroll navigation based on the css
  // property scroll-boundary-behavior.
  OverscrollBehavior overscroll_behavior;
  // The current offset of the currently scrolling node. It is in DIP or
  // physical pixels depending on the use-zoom-for-dsf flag. If the currently
  // scrolling node is the viewport, this would be the sum of the scroll offsets
  // of the inner and outer node, representing the visual scroll offset.
  gfx::PointF current_visual_offset;
  // Used only in scroll unification. Tells the caller that we have performed
  // the scroll (i.e. updated the offset in the scroll tree) on the compositor
  // thread, but we will need a main thread lifecycle update + commit before
  // the user will see the new pixels (for example, because the scroller does
  // not have a composited layer).
  bool needs_main_thread_repaint = false;
};

class CC_EXPORT InputHandlerClient {
 public:
  enum class ScrollEventDispatchMode {
    // Scroll events arriving will be enqueued to be dispatched during the next
    // `DeliverInputForBeginFrame`.
    kEnqueueScrollEvents,

    // Scroll events arriving will be dispatched immediately, if
    // `DeliverInputForBeginFrame` was called while scrolling, with no input
    // events in the queue. This will occur until frame production has started,
    // or completed.
    kDispatchScrollEventsImmediately,

    // If there are no queued events when `DeliverInputForBeginFrame` is called,
    // while we are scrolling. We will generate a new prediction, and then
    // dispatch a synthetic `GestureScrollUpdate` using the prediction.
    kUseScrollPredictorForEmptyQueue,

    // Will perform as `kDispatchScrollEventsImmediately` until the deadline.
    // Instead of immediately resuming frame production, we will first attempt
    // to generate a new prediction to dispatch. As in
    // `kUseScrollPredictorForEmptyQueue`. After which we will resume frame
    // production and enqueuing input.
    kUseScrollPredictorForDeadline,
  };

  InputHandlerClient(const InputHandlerClient&) = delete;
  virtual ~InputHandlerClient() = default;

  InputHandlerClient& operator=(const InputHandlerClient&) = delete;

  virtual void WillShutdown() = 0;
  virtual void Animate(base::TimeTicks time) = 0;
  virtual void ReconcileElasticOverscrollAndRootScroll() = 0;
  virtual void SetPrefersReducedMotion(bool prefers_reduced_motion) = 0;
  virtual void UpdateRootLayerStateForSynchronousInputHandler(
      const gfx::PointF& total_scroll_offset,
      const gfx::PointF& max_scroll_offset,
      const gfx::SizeF& scrollable_size,
      float page_scale_factor,
      float min_page_scale_factor,
      float max_page_scale_factor) = 0;
  virtual void DeliverInputForBeginFrame(const viz::BeginFrameArgs& args) = 0;
  virtual void DeliverInputForHighLatencyMode() = 0;
  virtual void DeliverInputForDeadline() = 0;
  virtual void DidFinishImplFrame() = 0;
  virtual bool HasQueuedInput() const = 0;
  virtual void SetScrollEventDispatchMode(ScrollEventDispatchMode mode) = 0;

 protected:
  InputHandlerClient() = default;
};

// Data passed from the input handler to the main thread.  Used to notify the
// main thread about changes that have occurred as a result of input since the
// last commit.
struct InputHandlerCommitData {
  // Defined in input_handler.cc to avoid inlining since flat_set has
  // non-trivial size destructor.
  InputHandlerCommitData();
  ~InputHandlerCommitData();

  // Unconsumed scroll delta since the last commit.
  gfx::Vector2dF overscroll_delta;

  // Elements that have scroll snapped to a new target since the last commit.
  base::flat_set<ElementId> updated_snapped_elements;

  // If a scroll was active at any point since the last commit, this will
  // identify the scroller (even if it has since ended).
  ElementId last_latched_scroller;

  // True if a scroll gesture has ended since the last commit.
  bool scroll_gesture_did_end = false;

  // The following bits are set if a gesture of any type was started since
  // the last commit.
  bool has_pinch_zoomed = false;
  bool has_scrolled_by_wheel = false;
  bool has_scrolled_by_touch = false;
  bool has_scrolled_by_precisiontouchpad = false;
};

// The InputHandler is a way for the embedders to interact with the input system
// running on the compositor thread. Each instance of a compositor (i.e. a
// LayerTreeHostImpl) is associated with one InputHandler instance. The
// InputHandler sits in between the embedder (the UI compositor or Blink) and
// the compositor (LayerTreeHostImpl); as such, it must be bound to both.
//
// To use the input handler, instantiate it by passing in the compositor's
// CompositorDelegateForInput to the Create factory method. The compositor
// assumes ownership of the InputHandler and will bind itself. Then, implement
// the InputHandlerClient interface and bind it to the handler by calling
// BindToClient on the input handler. This should all be done on the
// input-handling thread (i.e. the "compositor" thread if one exists).
//
// Many methods are virtual for input_handler_proxy_unittest.cc.
// TODO: consider revising these tests to reduce reliance on mocking.
class CC_EXPORT InputHandler : public InputDelegateForCompositor {
 public:
  // Creates an instance of the InputHandler and binds it to the layer tree
  // delegate. The delegate owns the InputHandler so their lifetimes
  // are tied together, hence, this returns a WeakPtr.
  static base::WeakPtr<InputHandler> Create(
      CompositorDelegateForInput& compositor_delegate);

  // Note these are used in a histogram. Do not reorder or delete existing
  // entries.
  enum class ScrollThread {
    // kScrollOnMainThread is not used anymore. However we'll keep this entry
    // as per the comment above.
    kScrollOnMainThread_NotUsed = 0,
    kScrollOnImplThread,
    kScrollIgnored,
    // kScrollUnknown is not used anymore. However we'll keep this entry as per
    // the comment above.
    kScrollUnknown_NotUsed,
    kLastScrollStatus = kScrollUnknown_NotUsed,
  };

  explicit InputHandler(CompositorDelegateForInput& compositor_delegate);
  ~InputHandler() override;

  InputHandler(const InputHandler&) = delete;
  InputHandler& operator=(const InputHandler&) = delete;

  struct ScrollStatus {
    ScrollThread thread = ScrollThread::kScrollOnImplThread;

    // If nonzero, it tells the caller that the input handler detected a case
    // where it cannot reliably target a scroll node and needs the main thread
    // to perform a hit test. If nonzero, this will be one or more values from
    // MainThreadScrollingReason::kHitTestReasons.
    uint32_t main_thread_hit_test_reasons =
        MainThreadScrollingReason::kNotScrollingOnMain;

    // A nonzero value means we have performed the scroll (i.e. updated the
    // offset in the scroll tree) on the compositor thread, but we will need a
    // main thread lifecycle update + commit before the user will see the new
    // pixels (for example, because the scroller does not have a composited
    // layer). If nonzero, this will be one or more values from the
    // MainThreadScrollingReason::kRepaintReasons.
    uint32_t main_thread_repaint_reasons =
        MainThreadScrollingReason::kNotScrollingOnMain;

    // TODO(crbug.com/40735567): This is a temporary workaround for GuestViews
    // as they create viewport nodes and want to bubble scroll if the
    // viewport cannot scroll in the given delta directions. There should be
    // a parameter to ThreadInputHandler to specify whether unused delta is
    // consumed by the viewport or bubbles to the parent.
    bool viewport_cannot_scroll = false;
  };

  // ViewportScrollResult records, for a scroll gesture affecting a page's
  // viewport:
  // - the amount from the scroll gesture's delta that actually resulted in
  //   scrolling: |consumed_delta|,
  // - the amount from the scroll gesture's delta that applied to the content of
  //   the page, i.e. excluding movement of browser controls.
  // - the distribution of the scroll gesture's delta between the inner and
  //   outer viewports, {inner,outer}_viewport_consumed_delta_
  // TODO(tdresser): eventually |consumed_delta| should equal
  // |content_scrolled_delta|. See crbug.com/510045 for details.
  struct ViewportScrollResult {
    gfx::Vector2dF consumed_delta;
    gfx::Vector2dF content_scrolled_delta;
    gfx::Vector2dF outer_viewport_scrolled_delta;
    gfx::Vector2dF inner_viewport_scrolled_delta;
  };

  enum class TouchStartOrMoveEventListenerType {
    kNoHandler,
    kHandler,
    kHandlerOnScrollingLayer
  };

  virtual base::WeakPtr<InputHandler> AsWeakPtr();

  // Binds a client to this handler to receive notifications. Only one client
  // can be bound to an InputHandler. The client must live at least until the
  // handler calls WillShutdown() on the client.
  virtual void BindToClient(InputHandlerClient* client);

  // Selects a ScrollNode to be "latched" for scrolling using the
  // |scroll_state| start position. The selected node remains latched until the
  // gesture is ended by a call to ScrollEnd.  Returns SCROLL_STARTED if a node
  // at the coordinates can be scrolled and was latched, SCROLL_ON_MAIN_THREAD
  // if the scroll event should instead be delegated to the main thread, or
  // kScrollUnknown if there is nothing to be scrolled at the given
  // coordinates.
  virtual ScrollStatus ScrollBegin(ScrollState* scroll_state,
                                   ui::ScrollInputType type);

  // Similar to ScrollBegin, except the hit test is skipped and scroll always
  // targets at the root layer.
  virtual ScrollStatus RootScrollBegin(ScrollState* scroll_state,
                                       ui::ScrollInputType type);

  // Scroll the layer selected by |ScrollBegin| by given |scroll_state| delta.
  // Internally, the delta is transformed to local layer's coordinate space for
  // scrolls gestures that are direct manipulation (e.g. touch). If the
  // viewport is latched, and it can no longer scroll, the root overscroll
  // accumulated within this ScrollBegin() scope is reported in the return
  // value's |accumulated_overscroll| field. Should only be called if
  // ScrollBegin() returned SCROLL_STARTED.
  //
  // Is a no-op if no scroller was latched to in ScrollBegin and returns an
  // empty-initialized InputHandlerScrollResult.
  //
  // |delayed_by| is the delay from the event that caused the scroll. This is
  // taken into account when determining the duration of the animation if one
  // is created.
  virtual InputHandlerScrollResult ScrollUpdate(
      ScrollState scroll_state,
      base::TimeDelta delayed_by = base::TimeDelta());

  // Stop scrolling the selected layer. Must be called only if ScrollBegin()
  // returned SCROLL_STARTED. No-op if ScrollBegin wasn't called or didn't
  // result in a successful scroll latch. Snap to a snap position if
  // |should_snap| is true.
  virtual void ScrollEnd(bool should_snap = false);

  // Called to notify every time scroll-begin/end is attempted by an input
  // event.
  virtual void RecordScrollBegin(ui::ScrollInputType input_type,
                                 ScrollBeginThreadState scroll_start_state);
  virtual void RecordScrollEnd(ui::ScrollInputType input_type);

  virtual PointerResultType HitTest(const gfx::PointF& mouse_position);
  virtual InputHandlerPointerResult MouseMoveAt(
      const gfx::Point& mouse_position);
  // TODO(arakeri): Pass in the modifier instead of a bool once the refactor
  // (crbug.com/1022097) is done. For details, see crbug.com/1016955.
  virtual InputHandlerPointerResult MouseDown(const gfx::PointF& mouse_position,
                                              bool shift_modifier);
  virtual InputHandlerPointerResult MouseUp(const gfx::PointF& mouse_position);
  virtual void MouseLeave();

  // Returns visible_frame_element_id from the layer hit by the given point.
  // If the hit test failed, an invalid element ID is returned.
  virtual ElementId FindFrameElementIdAtPoint(
      const gfx::PointF& mouse_position);

  // Requests a callback to UpdateRootLayerStateForSynchronousInputHandler()
  // giving the current root scroll and page scale information.
  virtual void RequestUpdateForSynchronousInputHandler();

  // Called when the root scroll offset has been changed in the synchronous
  // input handler by the application (outside of input event handling). Offset
  // is expected in "content/page coordinates".
  virtual void SetSynchronousInputHandlerRootScrollOffset(
      const gfx::PointF& root_content_offset);

  virtual void PinchGestureBegin(const gfx::Point& anchor,
                                 ui::ScrollInputType source);
  virtual void PinchGestureUpdate(float magnify_delta,
                                  const gfx::Point& anchor);
  virtual void PinchGestureEnd(const gfx::Point& anchor);

  // Request another callback to InputHandlerClient::Animate().
  virtual void SetNeedsAnimateInput();

  // Returns true if there is an active scroll on the viewport.
  virtual bool IsCurrentlyScrollingViewport() const;

  virtual EventListenerProperties GetEventListenerProperties(
      EventListenerClass event_class) const;

  // Returns true if |viewport_point| hits a wheel event handler region that
  // could block scrolling.
  virtual bool HasBlockingWheelEventHandlerAt(
      const gfx::Point& viewport_point) const;

  // It returns the type of a touch start or move event listener at
  // |viewport_point|. Whether the page should be given the opportunity to
  // suppress scrolling by consuming touch events that started at
  // |viewport_point|, and whether |viewport_point| is on the currently
  // scrolling layer.
  // |out_touch_action| is assigned the allowed touch action for the
  // |viewport_point|. In the case there are no touch handlers or touch action
  // regions, |out_touch_action| is assigned TouchAction::kAuto since the
  // default touch action is auto.
  virtual TouchStartOrMoveEventListenerType
  EventListenerTypeForTouchStartOrMoveAt(const gfx::Point& viewport_point,
                                         TouchAction* out_touch_action);

  // Calling `CreateLatencyInfoSwapPromiseMonitor()` to get a scoped
  // `LatencyInfoSwapPromiseMonitor`. During the life time of the
  // `LatencyInfoSwapPromiseMonitor`, if `SetNeedsRedraw()` or
  // `SetNeedsRedrawRect()` is called on `LayerTreeHostImpl`, the original
  // latency info will be turned into a `LatencyInfoSwapPromise`.
  virtual std::unique_ptr<LatencyInfoSwapPromiseMonitor>
  CreateLatencyInfoSwapPromiseMonitor(ui::LatencyInfo* latency);

  // Returns a new instance of `EventsMetricsManager::ScopedMonitor` to monitor
  // the scope of handling an event. If `done_callback` is not a null callback,
  // it will be called when the scope ends. If During the lifetime of the scoped
  // monitor, `SetNeedsOneBeginImplFrame()` or `SetNeedsRedraw()` are called on
  // `LayerTreeHostImpl` or a scroll animation is updated, the callback will be
  // called in the end with `handled` argument set to true, denoting that the
  // event was handled and the client should return `EventMetrics` associated
  // with the event if it is interested in reporting event latency metrics for
  // it.
  virtual std::unique_ptr<EventsMetricsManager::ScopedMonitor>
  GetScopedEventMetricsMonitor(
      EventsMetricsManager::ScopedMonitor::DoneCallback done_callback);

  virtual ScrollElasticityHelper* CreateScrollElasticityHelper();
  virtual void DestroyScrollElasticityHelper();

  // Called by the single-threaded UI Compositor to get or set the scroll offset
  // on the impl side. Returns false if |element_id| isn't in the active tree.
  virtual bool GetScrollOffsetForLayer(ElementId element_id,
                                       gfx::PointF* offset);
  virtual bool ScrollLayerTo(ElementId element_id, const gfx::PointF& offset);

  // Sets the initial and target offset for scroll snapping for the currently
  // scrolling node and the given natural displacement. Also sets the target
  // element of the snap's scrolling animation.
  // |natural_displacement_in_viewport| is the estimated total scrolling for
  // the active scroll sequence.
  // Returns false if their is no position to snap to.
  virtual bool GetSnapFlingInfoAndSetAnimatingSnapTarget(
      const gfx::Vector2dF& current_delta,
      const gfx::Vector2dF& natural_displacement_in_viewport,
      gfx::PointF* initial_offset,
      gfx::PointF* target_offset);

  // |did_finish| is true if the animation reached its target position (i.e.
  // it wasn't aborted).
  virtual void ScrollEndForSnapFling(bool did_finish);

  // Notifies when any input event is received, irrespective of whether it is
  // being handled by the InputHandler or not.
  virtual void NotifyInputEvent();

  // Returns true if ScrollbarController is in the middle of a scroll operation.
  virtual bool ScrollbarScrollIsActive();

  // Defers posting BeginMainFrame tasks. This is used during the main thread
  // hit test for a GestureScrollBegin, to avoid posting a frame before the
  // compositor thread has had a chance to update the scroll offset.
  virtual void SetDeferBeginMainFrame(bool defer_begin_main_frame) const;

  virtual void UpdateBrowserControlsState(
      BrowserControlsState constraints,
      BrowserControlsState current,
      bool animate,
      base::optional_ref<const BrowserControlsOffsetTagsInfo> offset_tags_info);

  virtual void SetIsHandlingTouchSequence(bool is_handling_touch_sequence);

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

  float LineStep() const;

  // Resolves a delta in the given granularity for the |scroll_node| into
  // physical pixels to scroll.
  gfx::Vector2dF ResolveScrollGranularityToPixels(
      const ScrollNode& scroll_node,
      const gfx::Vector2dF& scroll_delta,
      ui::ScrollGranularity granularity);

  bool CurrentScrollNeedsFrameAlignment() const;

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

  const std::unique_ptr<SnapSelectionStrategy>& snap_strategy_for_testing()
      const {
    return snap_strategy_;
  }

  // Detects whether or not the scroll generating the |result| affected the
  // inner or outer viewports.
  void SetViewportConsumedDelta(const ViewportScrollResult& result);

  // =========== InputDelegateForCompositor Interface - This section implements
  // the interface that LayerTreeHostImpl uses to communicate with the input
  // system.
  void ProcessCommitDeltas(
      CompositorCommitData* commit_data,
      const MutatorHost* main_thread_mutator_host) override;
  void TickAnimations(base::TimeTicks monotonic_time) override;
  void WillShutdown() override;
  void WillDraw() override;
  void WillBeginImplFrame(const viz::BeginFrameArgs& args) override;
  void DidCommit() override;
  void DidActivatePendingTree() override;
  void DidFinishImplFrame() override;
  void OnBeginImplFrameDeadline() override;
  void RootLayerStateMayHaveChanged() override;
  void DidRegisterScrollbar(ElementId scroll_element_id,
                            ScrollbarOrientation orientation) override;
  void DidUnregisterScrollbar(ElementId scroll_element_id,
                              ScrollbarOrientation orientation) override;
  void ScrollOffsetAnimationFinished() override;
  void SetPrefersReducedMotion(bool prefers_reduced_motion) override;
  bool IsCurrentlyScrolling() const override;
  ActivelyScrollingType GetActivelyScrollingType() const override;
  bool IsHandlingTouchSequence() const override;
  bool IsCurrentScrollMainRepainted() const override;
  bool HasQueuedInput() const override;

 private:
  FRIEND_TEST_ALL_PREFIXES(LayerTreeHostImplTest,
                           AbortAnimatedScrollBeforeStartingAutoscroll);
  FRIEND_TEST_ALL_PREFIXES(LayerTreeHostImplTest, AnimatedScrollYielding);
  FRIEND_TEST_ALL_PREFIXES(LayerTreeHostImplTest, AutoscrollOnDeletedScrollbar);
  FRIEND_TEST_ALL_PREFIXES(LayerTreeHostImplTest, ThumbDragAfterJumpClick);
  FRIEND_TEST_ALL_PREFIXES(LayerTreeHostImplTest, ScrollOnLargeThumb);
  FRIEND_TEST_ALL_PREFIXES(LayerTreeHostImplTest, AutoscrollTaskAbort);

  // This method gets the scroll offset for a regular scroller, or the combined
  // visual and layout offsets of the viewport.
  gfx::PointF GetVisualScrollOffset(const ScrollNode& scroll_node) const;
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
  // sources per page load. TODO(crbug.com/40506330): Use GRC API to plumb the
  // scroll source info for Use Counters.
  void UpdateScrollSourceInfo(const ScrollState& scroll_state,
                              ui::ScrollInputType type);

  // Applies the scroll_state to the currently latched scroller. See comment in
  // InputHandler::ScrollUpdate declaration for the meaning of |delayed_by|.
  void ScrollLatchedScroller(ScrollState& scroll_state,
                             base::TimeDelta delayed_by);

  enum class SnapReason { kGestureScrollEnd, kScrollOffsetAnimationFinished };

  // Creates an animation curve and returns true if we need to update the
  // scroll position to a snap point. Otherwise returns false.
  bool SnapAtScrollEnd(SnapReason reason);

  // `layer` is returned from a regular hit test, and
  // `first_scrollable_or_opaque_to_hit_test_layer` is the first scroller,
  // scrollbar, or layer opaque to hit test, when we perform a hit test for
  // all layers from top to bottom in z order.
  // This function returns true if we know which scroller to scroll, and
  // `out_node_to_scroll` is set to the scroll node to scroll. It returns
  // false when `layer` covers `first_layer_scrollable_or_opaque_to_hit_test`
  // but they have different nearest scroll ancestors, and we don't know
  // which scroll ancestor to scroll. This includes the case that a scroller is
  // masked by a mask layer for mask image, clip-path, rounded border, etc.
  bool IsInitialScrollHitTestReliable(
      const LayerImpl* layer,
      const LayerImpl* first_layer_scrollable_or_opaque_to_hit_test,
      ScrollNode*& out_node_to_scroll) const;

  // Returns the ScrollNode we should use to scroll, accounting for viewport
  // scroll chaining rules.
  ScrollNode* GetNodeToScroll(ScrollNode* node) const;
  ScrollNode* GetNodeToScrollForLayer(const LayerImpl* layer) const;

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
    raw_ptr<ScrollNode> scroll_node;
    bool hit_test_successful;
    uint32_t main_thread_hit_test_reasons =
        MainThreadScrollingReason::kNotScrollingOnMain;
  };
  ScrollHitTestResult HitTestScrollNode(
      const gfx::PointF& device_viewport_point) const;

  bool ShouldAnimateScroll(const ScrollState& scroll_state) const;

  std::optional<gfx::PointF> ScrollAnimationUpdateTarget(
      const ScrollNode& scroll_node,
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

  void AdjustScrollDeltaForScrollbarSnap(ScrollState& scroll_state);

  FrameSequenceTrackerType GetTrackerTypeForScroll(
      ui::ScrollInputType input_type) const;

  ScrollbarController* scrollbar_controller_for_testing() const {
    return scrollbar_controller_.get();
  }

  std::optional<gfx::PointF> ConstrainFling(gfx::PointF original);

  // Estimate how to adjust the height of the snapport rect based on the state
  // of browser controls that are being shown or hidden during a scroll gesture
  // before the Blink WebView is resized to reflect the new state.
  double PredictViewportBoundsDelta(gfx::Vector2dF scroll_distance);

  std::unique_ptr<SnapSelectionStrategy> CreateSnapStrategy(
      const ScrollState& scroll_state,
      const gfx::PointF& current_offset,
      SnapReason snap_reason) const;

  // This returns the ScrollNode associated with the CurrentlyScrollingNode()
  // that is currently animating, if one exists.
  // It is usually the same ScrollNode as the CurrentlyScrollingNode(), except
  // when the inner viewport node is animating, in which case the
  // CurrentlyScrollingNode() is still the outer viewport node.
  ScrollNode* GetAnimatingNodeForCurrentScrollingNode();

  // The input handler is owned by the delegate so their lifetimes are tied
  // together.
  const raw_ref<CompositorDelegateForInput> compositor_delegate_;

  raw_ptr<InputHandlerClient> input_handler_client_ = nullptr;

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
  std::optional<ui::ScrollInputType> latched_scroll_type_;

  // Tracks the last scroll update/begin state received. Used to infer the most
  // recent scroll type and direction.
  std::optional<ScrollState> last_scroll_begin_state_;
  std::optional<ScrollState> last_scroll_update_state_;

  // If a scroll snap is being animated, then the value of this will be the
  // element id(s) of the target(s). Otherwise, the ids will be invalid.
  // At the end of a scroll animation, the target should be set as the scroll
  // node's snap target.
  TargetSnapAreaElementIds scroll_animating_snap_target_ids_;

  enum SnapFlingState {
    kNoFling,
    kNativeFling,
    kConstrainedNativeFling,
    kSnapFling
  };
  SnapFlingState snap_fling_state_ = kNoFling;
  std::optional<gfx::RangeF> fling_snap_constrain_x_;
  std::optional<gfx::RangeF> fling_snap_constrain_y_;

  // A set of elements that scroll-snapped to a new target since the last
  // begin main frame. The snap target ids of these elements will be sent to
  // the main thread in the next begin main frame.
  base::flat_map<ElementId, TargetSnapAreaElementIds> updated_snapped_elements_;

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

  // did_scroll_x/y_for_scroll_gesture_ is true when contents consume the delta,
  // but delta_consumed_for_scroll_gesture_ can be true when only browser
  // controls consume all the delta.
  bool delta_consumed_for_scroll_gesture_ = false;

  // True if any of the non-zero deltas in a begin/update/end sequence was
  // applied to the layout viewport.
  bool outer_viewport_consumed_delta_ = false;

  // True if any of the non-zero deltas in a begin/update/end sequence was
  // applied to the visual viewport.
  bool inner_viewport_consumed_delta_ = false;

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
  bool has_scrolled_by_scrollbar_ = false;

  bool prefers_reduced_motion_ = false;

  bool is_handling_touch_sequence_ = false;

  // This tracks the strategy cc will use to snap at the end of the current
  // scroll based on the scroll updates so far. The |current_offset|
  // in SnapSelectionStrategy is set based on whether the scroll is animated
  // or non animated. For non-animated scrolls, it is the same as the
  // offset in the ScrollTree. For animated scrolls it is the target offset that
  // is being animated to.
  std::unique_ptr<SnapSelectionStrategy> snap_strategy_;

  // Must be the last member to ensure this is destroyed first in the
  // destruction order and invalidates all weak pointers.
  base::WeakPtrFactory<InputHandler> weak_factory_{this};
};

}  // namespace cc

#endif  // CC_INPUT_INPUT_HANDLER_H_
