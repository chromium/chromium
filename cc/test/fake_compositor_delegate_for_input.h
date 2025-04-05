// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_FAKE_COMPOSITOR_DELEGATE_FOR_INPUT_H_
#define CC_TEST_FAKE_COMPOSITOR_DELEGATE_FOR_INPUT_H_

#include <memory>

#include "base/types/optional_ref.h"
#include "cc/input/browser_controls_offset_tag_modifications.h"
#include "cc/input/browser_controls_state.h"
#include "cc/input/compositor_input_interfaces.h"
#include "cc/paint/element_id.h"
#include "cc/test/fake_impl_task_runner_provider.h"
#include "cc/test/fake_layer_tree_host_impl.h"
#include "cc/test/test_task_graph_runner.h"
#include "cc/trees/layer_tree_settings.h"
#include "cc/trees/property_tree.h"
#include "ui/gfx/geometry/size.h"

namespace cc {

class FakeCompositorDelegateForInput : public CompositorDelegateForInput {
 public:
  FakeCompositorDelegateForInput();
  ~FakeCompositorDelegateForInput() override;
  void BindToInputHandler(
      std::unique_ptr<InputDelegateForCompositor> delegate) override {}
  ScrollTree& GetScrollTree() const override;
  void ScrollAnimationAbort(ElementId element_id) const override {}
  float GetBrowserControlsTopOffset() const override;
  void ScrollBegin() const override {}
  void ScrollEnd() const override {}
  void StartScrollSequence(
      FrameSequenceTrackerType type,
      FrameInfo::SmoothEffectDrivingThread scrolling_thread) override {}
  void StopSequence(FrameSequenceTrackerType type) override {}
  void PinchBegin() const override {}
  void PinchEnd() const override {}
  void TickScrollAnimations() const override {}
  void ScrollbarAnimationMouseLeave(ElementId element_id) const override {}
  void ScrollbarAnimationMouseMove(
      ElementId element_id,
      gfx::PointF device_viewport_point) const override {}
  bool ScrollbarAnimationMouseDown(ElementId element_id) const override;
  bool ScrollbarAnimationMouseUp(ElementId element_id) const override;
  double PredictViewportBoundsDelta(
      double current_bounds_delta,
      gfx::Vector2dF scroll_distance) const override;
  bool ElementHasImplOnlyScrollAnimation(ElementId) const override;
  std::optional<gfx::PointF> UpdateImplAnimationScrollTargetWithDelta(
      gfx::Vector2dF adjusted_delta,
      int scroll_node_id,
      base::TimeDelta delayed_by,
      ElementId element_id) const override;
  std::unique_ptr<EventsMetricsManager::ScopedMonitor>
  GetScopedEventMetricsMonitor(
      EventsMetricsManager::ScopedMonitor::DoneCallback done_callback) override;
  void NotifyInputEvent(bool is_fling) override {}
  std::unique_ptr<LatencyInfoSwapPromiseMonitor>
  CreateLatencyInfoSwapPromiseMonitor(ui::LatencyInfo* latency) override;
  void SetNeedsAnimateInput() override {}
  bool ScrollAnimationCreate(const ScrollNode& scroll_node,
                             const gfx::Vector2dF& scroll_amount,
                             base::TimeDelta delayed_by) override;
  bool HasAnimatedScrollbars() const override;
  void SetNeedsCommit() override {}
  void SetNeedsFullViewportRedraw() override {}
  void SetDeferBeginMainFrame(bool defer_begin_main_frame) const override {}
  void DidUpdateScrollAnimationCurve() override {}
  void DidStartPinchZoom() override {}
  void DidUpdatePinchZoom() override {}
  void DidEndPinchZoom() override {}
  void DidStartScroll() override {}
  void DidEndScroll() override {}
  void DidMouseLeave() override {}
  bool IsInHighLatencyMode() const override;
  void WillScrollContent(ElementId element_id) override {}
  void DidScrollContent(ElementId element_id,
                        bool animated,
                        const gfx::Vector2dF& scroll_delta) override {}
  float DeviceScaleFactor() const override;
  float PageScaleFactor() const override;
  gfx::Size VisualDeviceViewportSize() const override;
  const LayerTreeSettings& GetSettings() const override;
  LayerTreeHostImpl& GetImplDeprecated() override;
  const LayerTreeHostImpl& GetImplDeprecated() const override;
  void UpdateBrowserControlsState(
      BrowserControlsState constraints,
      BrowserControlsState current,
      bool animate,
      base::optional_ref<const BrowserControlsOffsetTagModifications>
          offset_tag_modifications) override {}
  bool HasScrollLinkedAnimation(ElementId for_scroller) const override;

 private:
  mutable ScrollTree scroll_tree_;
  LayerTreeSettings settings_;
  FakeImplTaskRunnerProvider task_runner_provider_;
  TestTaskGraphRunner task_graph_runner_;
  FakeLayerTreeHostImpl host_impl_;
};

}  // namespace cc

#endif  // CC_TEST_FAKE_COMPOSITOR_DELEGATE_FOR_INPUT_H_
