// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_INPUT_INPUT_HANDLER_H_
#define CC_INPUT_INPUT_HANDLER_H_

#include <memory>

#include "base/time/time.h"
#include "cc/cc_export.h"
#include "cc/input/event_listener_properties.h"
#include "cc/input/main_thread_scrolling_reason.h"
#include "cc/input/overscroll_behavior.h"
#include "cc/input/scroll_state.h"
#include "cc/input/scrollbar.h"
#include "cc/input/touch_action.h"
#include "cc/paint/element_id.h"
#include "cc/trees/swap_promise_monitor.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "ui/events/types/scroll_types.h"

namespace gfx {
class Point;
class ScrollOffset;
class SizeF;
class Vector2dF;
}  // namespace gfx

namespace ui {
class LatencyInfo;
}  // namespace ui

namespace cc {

class ScrollElasticityHelper;

enum PointerResultType { kUnhandled = 0, kScrollbarScroll };

struct CC_EXPORT InputHandlerPointerResult {
  InputHandlerPointerResult();
  // Tells what type of processing occurred in the input handler as a result of
  // the pointer event.
  PointerResultType type;

  // Tells what scroll_units should be used.
  ui::input_types::ScrollGranularity scroll_units;

  // If the input handler processed the event as a scrollbar scroll, it will
  // return a gfx::ScrollOffset that produces the necessary scroll. However,
  // it is still the client's responsibility to generate the gesture scrolls
  // instead of the input handler performing it as a part of handling the
  // pointer event (due to the latency attribution that happens at the
  // InputHandlerProxy level).
  gfx::ScrollOffset scroll_offset;

  // Used to determine which scroll_node needs to be scrolled. The primary
  // purpose of this is to avoid hit testing for gestures that already know
  // which scroller to target.
  ElementId target_scroller;
};

struct CC_EXPORT InputHandlerScrollResult {
  InputHandlerScrollResult();
  // Did any layer scroll as a result this ScrollBy call?
  bool did_scroll;
  // Was any of the scroll delta argument to this ScrollBy call not used?
  bool did_overscroll_root;
  // The total overscroll that has been accumulated by all ScrollBy calls that
  // have had overscroll since the last ScrollBegin call. This resets upon a
  // ScrollBy with no overscroll.
  gfx::Vector2dF accumulated_root_overscroll;
  // The amount of the scroll delta argument to this ScrollBy call that was not
  // used for scrolling.
  gfx::Vector2dF unused_scroll_delta;
  // How the browser should handle the overscroll navigation based on the css
  // property scroll-boundary-behavior.
  OverscrollBehavior overscroll_behavior;
  // The current offset of the currently scrolling node. It is in DIP or
  // physical pixels depending on the use-zoom-for-dsf flag. If the currently
  // scrolling node is the viewport, this would be the sum of the scroll offsets
  // of the inner and outer node, representing the visual scroll offset.
  gfx::Vector2dF current_visual_offset;
};

class CC_EXPORT InputHandlerClient {
 public:
  InputHandlerClient(const InputHandlerClient&) = delete;
  virtual ~InputHandlerClient() = default;

  InputHandlerClient& operator=(const InputHandlerClient&) = delete;

  virtual void WillShutdown() = 0;
  virtual void Animate(base::TimeTicks time) = 0;
  virtual void ReconcileElasticOverscrollAndRootScroll() = 0;
  virtual void UpdateRootLayerStateForSynchronousInputHandler(
      const gfx::ScrollOffset& total_scroll_offset,
      const gfx::ScrollOffset& max_scroll_offset,
      const gfx::SizeF& scrollable_size,
      float page_scale_factor,
      float min_page_scale_factor,
      float max_page_scale_factor) = 0;
  virtual void DeliverInputForBeginFrame(const viz::BeginFrameArgs& args) = 0;
  virtual void DeliverInputForHighLatencyMode() = 0;

 protected:
  InputHandlerClient() = default;
};

// The InputHandler is a way for the embedders to interact with the impl thread
// side of the compositor implementation. There is one InputHandler per
// LayerTreeHost. To use the input handler, implement the InputHanderClient
// interface and bind it to the handler on the compositor thread.
class CC_EXPORT InputHandler {
 public:
  // Note these are used in a histogram. Do not reorder or delete existing
  // entries.
  enum ScrollThread {
    SCROLL_ON_MAIN_THREAD = 0,
    SCROLL_ON_IMPL_THREAD,
    SCROLL_IGNORED,
    SCROLL_UNKNOWN,
    LAST_SCROLL_STATUS = SCROLL_UNKNOWN
  };

  InputHandler(const InputHandler&) = delete;
  InputHandler& operator=(const InputHandler&) = delete;

  struct ScrollStatus {
    ScrollStatus()
        : thread(SCROLL_ON_IMPL_THREAD),
          main_thread_scrolling_reasons(
              MainThreadScrollingReason::kNotScrollingOnMain),
          bubble(false) {}
    ScrollStatus(ScrollThread thread, uint32_t main_thread_scrolling_reasons)
        : thread(thread),
          main_thread_scrolling_reasons(main_thread_scrolling_reasons) {}
    ScrollThread thread;
    uint32_t main_thread_scrolling_reasons;
    bool bubble;
  };

  enum ScrollInputType {
    TOUCHSCREEN,
    WHEEL,
    AUTOSCROLL,
    SCROLLBAR,
    SCROLL_INPUT_UNKNOWN
  };

  enum class TouchStartOrMoveEventListenerType {
    NO_HANDLER,
    HANDLER,
    HANDLER_ON_SCROLLING_LAYER
  };

  // Binds a client to this handler to receive notifications. Only one client
  // can be bound to an InputHandler. The client must live at least until the
  // handler calls WillShutdown() on the client.
  virtual void BindToClient(InputHandlerClient* client) = 0;

  // Selects a layer to be scrolled using the |scroll_state| start position.
  // Returns SCROLL_STARTED if the layer at the coordinates can be scrolled,
  // SCROLL_ON_MAIN_THREAD if the scroll event should instead be delegated to
  // the main thread, or SCROLL_IGNORED if there is nothing to be scrolled at
  // the given coordinates.
  virtual ScrollStatus ScrollBegin(ScrollState* scroll_state,
                                   ScrollInputType type) = 0;

  // Similar to ScrollBegin, except the hit test is skipped and scroll always
  // targets at the root layer.
  virtual ScrollStatus RootScrollBegin(ScrollState* scroll_state,
                                       ScrollInputType type) = 0;

  // Returns SCROLL_ON_IMPL_THREAD if a layer is actively being scrolled or
  // a subsequent call to ScrollAnimated can begin on the impl thread.
  virtual ScrollStatus ScrollAnimatedBegin(ScrollState* scroll_state) = 0;

  // Returns SCROLL_ON_IMPL_THREAD if an animation is initiated on the impl
  // thread. delayed_by is the delay that is taken into account when determining
  // the duration of the animation.
  virtual ScrollStatus ScrollAnimated(const gfx::Point& viewport_point,
                                      const gfx::Vector2dF& scroll_delta,
                                      base::TimeDelta delayed_by) = 0;

  // Scroll the layer selected by |ScrollBegin| by given |scroll_state| delta.
  // Internally, the delta is transformed to local layer's coordinate space for
  // scrolls gestures that are direct manipulation (e.g. touch). If there is no
  // room to move the layer in the requested direction, its first ancestor layer
  // that can be scrolled will be moved instead. The return value's |did_scroll|
  // field is set to false if no layer can be moved in the requested direction
  // at all, and set to true if any layer is moved. If the scroll delta hits the
  // root layer, and the layer can no longer move, the root overscroll
  // accumulated within this ScrollBegin() scope is reported in the return
  // value's |accumulated_overscroll| field. Should only be called if
  // ScrollBegin() returned SCROLL_STARTED.
  virtual InputHandlerScrollResult ScrollBy(ScrollState* scroll_state) = 0;

  virtual InputHandlerPointerResult MouseMoveAt(
      const gfx::Point& mouse_position) = 0;
  // TODO(arakeri): Pass in the modifier instead of a bool once the refactor
  // (crbug.com/1022097) is done. For details, see crbug.com/1016955.
  virtual InputHandlerPointerResult MouseDown(const gfx::PointF& mouse_position,
                                              bool shift_modifier) = 0;
  virtual InputHandlerPointerResult MouseUp(
      const gfx::PointF& mouse_position) = 0;
  virtual void MouseLeave() = 0;

  // Stop scrolling the selected layer. Should only be called if ScrollBegin()
  // returned SCROLL_STARTED. Snap to a snap position if |should_snap| is true.
  virtual void ScrollEnd(ScrollState* scroll_state, bool should_snap) = 0;

  // Requests a callback to UpdateRootLayerStateForSynchronousInputHandler()
  // giving the current root scroll and page scale information.
  virtual void RequestUpdateForSynchronousInputHandler() = 0;

  // Called when the root scroll offset has been changed in the synchronous
  // input handler by the application (outside of input event handling). Offset
  // is expected in "content/page coordinates".
  virtual void SetSynchronousInputHandlerRootScrollOffset(
      const gfx::ScrollOffset& root_content_offset) = 0;

  virtual void PinchGestureBegin() = 0;
  virtual void PinchGestureUpdate(float magnify_delta,
                                  const gfx::Point& anchor) = 0;
  virtual void PinchGestureEnd(const gfx::Point& anchor, bool snap_to_min) = 0;

  // Request another callback to InputHandlerClient::Animate().
  virtual void SetNeedsAnimateInput() = 0;

  // Returns true if there is an active scroll on the viewport.
  virtual bool IsCurrentlyScrollingViewport() const = 0;

  // Whether the layer under |viewport_point| is the currently scrolling layer.
  virtual bool IsCurrentlyScrollingLayerAt(
      const gfx::Point& viewport_point) const = 0;

  virtual EventListenerProperties GetEventListenerProperties(
      EventListenerClass event_class) const = 0;

  // Returns true if |viewport_point| hits a wheel event handler region that
  // could block scrolling.
  virtual bool HasBlockingWheelEventHandlerAt(
      const gfx::Point& viewport_point) const = 0;

  // It returns the type of a touch start or move event listener at
  // |viewport_point|. Whether the page should be given the opportunity to
  // suppress scrolling by consuming touch events that started at
  // |viewport_point|, and whether |viewport_point| is on the currently
  // scrolling layer.
  // |out_touch_action| is assigned the whitelisted touch action for the
  // |viewport_point|. In the case there are no touch handlers or touch action
  // regions, |out_touch_action| is assigned kTouchActionAuto since the default
  // touch action is auto.
  virtual TouchStartOrMoveEventListenerType
  EventListenerTypeForTouchStartOrMoveAt(const gfx::Point& viewport_point,
                                         TouchAction* out_touch_action) = 0;

  // Calling CreateLatencyInfoSwapPromiseMonitor() to get a scoped
  // LatencyInfoSwapPromiseMonitor. During the life time of the
  // LatencyInfoSwapPromiseMonitor, if SetNeedsRedraw() or SetNeedsRedrawRect()
  // is called on LayerTreeHostImpl, the original latency info will be turned
  // into a LatencyInfoSwapPromise.
  virtual std::unique_ptr<SwapPromiseMonitor>
  CreateLatencyInfoSwapPromiseMonitor(ui::LatencyInfo* latency) = 0;

  virtual ScrollElasticityHelper* CreateScrollElasticityHelper() = 0;

  // Called by the single-threaded UI Compositor to get or set the scroll offset
  // on the impl side. Returns false if |element_id| isn't in the active tree.
  virtual bool GetScrollOffsetForLayer(ElementId element_id,
                                       gfx::ScrollOffset* offset) = 0;
  virtual bool ScrollLayerTo(ElementId element_id,
                             const gfx::ScrollOffset& offset) = 0;

  virtual bool ScrollingShouldSwitchtoMainThread() = 0;

  // Sets the initial and target offset for scroll snapping for the currently
  // scrolling node and the given natural displacement.
  // |natural_displacement_in_viewport| is the estimated total scrolling for
  // the active scroll sequence.
  // Returns false if their is no position to snap to.
  virtual bool GetSnapFlingInfoAndSetSnapTarget(
      const gfx::Vector2dF& natural_displacement_in_viewport,
      gfx::Vector2dF* initial_offset,
      gfx::Vector2dF* target_offset) = 0;

 protected:
  InputHandler() = default;
  virtual ~InputHandler() = default;
};

}  // namespace cc

#endif  // CC_INPUT_INPUT_HANDLER_H_
