// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_MOCK_INPUT_HANDLER_H_
#define CC_TEST_MOCK_INPUT_HANDLER_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "cc/input/browser_controls_offset_tag_modifications.h"
#include "cc/input/browser_controls_state.h"
#include "cc/input/event_listener_properties.h"
#include "cc/input/input_handler.h"
#include "cc/input/scroll_elasticity_helper.h"
#include "cc/input/scroll_state.h"
#include "cc/metrics/events_metrics_manager.h"
#include "cc/paint/element_id.h"
#include "cc/test/fake_compositor_delegate_for_input.h"
#include "cc/trees/latency_info_swap_promise_monitor.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/events/types/scroll_input_type.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace cc {

class MockInputHandler : public InputHandler {
 public:
  MockInputHandler();
  MockInputHandler(const MockInputHandler&) = delete;
  MockInputHandler& operator=(const MockInputHandler&) = delete;

  ~MockInputHandler() override;

  base::WeakPtr<InputHandler> AsWeakPtr() override {
    return weak_ptr_factory_.GetWeakPtr();
  }

  MOCK_METHOD2(PinchGestureBegin,
               void(const gfx::Point& anchor, ui::ScrollInputType type));
  MOCK_METHOD2(PinchGestureUpdate,
               void(float magnify_delta, const gfx::Point& anchor));
  MOCK_METHOD1(PinchGestureEnd, void(const gfx::Point& anchor));

  MOCK_METHOD0(SetNeedsAnimateInput, void());

  MOCK_METHOD2(ScrollBegin,
               ScrollStatus(ScrollState*, ui::ScrollInputType type));
  MOCK_METHOD2(RootScrollBegin,
               ScrollStatus(ScrollState*, ui::ScrollInputType type));
  MOCK_METHOD2(ScrollUpdate,
               InputHandlerScrollResult(ScrollState, base::TimeDelta));
  MOCK_METHOD1(ScrollEnd, void(bool));
  MOCK_METHOD2(RecordScrollBegin,
               void(ui::ScrollInputType type, ScrollBeginThreadState state));
  MOCK_METHOD1(RecordScrollEnd, void(ui::ScrollInputType type));
  MOCK_METHOD1(HitTest, PointerResultType(const gfx::PointF& mouse_position));
  MOCK_METHOD2(MouseDown,
               InputHandlerPointerResult(const gfx::PointF& mouse_position,
                                         const bool shift_modifier));
  MOCK_METHOD1(MouseUp,
               InputHandlerPointerResult(const gfx::PointF& mouse_position));
  MOCK_METHOD1(SetIsHandlingTouchSequence, void(bool));
  void NotifyInputEvent(bool is_fling) override {}

  std::unique_ptr<LatencyInfoSwapPromiseMonitor>
  CreateLatencyInfoSwapPromiseMonitor(ui::LatencyInfo* latency) override {
    return nullptr;
  }

  std::unique_ptr<EventsMetricsManager::ScopedMonitor>
  GetScopedEventMetricsMonitor(
      EventsMetricsManager::ScopedMonitor::DoneCallback) override {
    return nullptr;
  }

  ScrollElasticityHelper* CreateScrollElasticityHelper() override {
    return nullptr;
  }
  void DestroyScrollElasticityHelper() override {}

  bool GetScrollOffsetForLayer(ElementId element_id,
                               gfx::PointF* offset) override {
    return false;
  }
  bool ScrollLayerTo(ElementId element_id, const gfx::PointF& offset) override {
    return false;
  }

  void BindToClient(InputHandlerClient* client) override {}

  void MouseLeave() override {}

  MOCK_METHOD1(FindFrameElementIdAtPoint, ElementId(const gfx::PointF&));

  InputHandlerPointerResult MouseMoveAt(
      const gfx::Point& mouse_position) override {
    return InputHandlerPointerResult();
  }

  MOCK_CONST_METHOD1(GetEventListenerProperties,
                     EventListenerProperties(EventListenerClass event_class));
  MOCK_METHOD2(EventListenerTypeForTouchStartOrMoveAt,
               InputHandler::TouchStartOrMoveEventListenerType(
                   const gfx::Point& point,
                   TouchAction* touch_action));
  MOCK_CONST_METHOD1(HasBlockingWheelEventHandlerAt, bool(const gfx::Point&));

  MOCK_METHOD0(RequestUpdateForSynchronousInputHandler, void());
  MOCK_METHOD1(SetSynchronousInputHandlerRootScrollOffset,
               void(const gfx::PointF& root_offset));

  bool IsCurrentlyScrollingViewport() const override {
    return is_scrolling_root_;
  }
  void set_is_scrolling_root(bool is) { is_scrolling_root_ = is; }

  MOCK_METHOD4(GetSnapFlingInfoAndSetAnimatingSnapTarget,
               bool(const gfx::Vector2dF& current_delta,
                    const gfx::Vector2dF& natural_displacement,
                    gfx::PointF* initial_offset,
                    gfx::PointF* target_offset));
  MOCK_METHOD1(ScrollEndForSnapFling, void(bool));

  bool ScrollbarScrollIsActive() override { return false; }

  void SetDeferBeginMainFrame(bool defer_begin_main_frame) const override {}

  MOCK_METHOD4(
      UpdateBrowserControlsState,
      void(BrowserControlsState constraints,
           BrowserControlsState current,
           bool animate,
           base::optional_ref<const BrowserControlsOffsetTagModifications>
               offset_tag_modifications));

 private:
  bool is_scrolling_root_ = true;

  base::WeakPtrFactory<MockInputHandler> weak_ptr_factory_{this};
};

}  // namespace cc
#endif  // CC_TEST_MOCK_INPUT_HANDLER_H_
