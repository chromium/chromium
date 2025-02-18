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
  bool HasAnimatedScrollbars() const override;
  void SetNeedsCommit() override {}
  void SetNeedsFullViewportRedraw() override {}
  void SetDeferBeginMainFrame(bool defer_begin_main_frame) const override {}
  void DidUpdateScrollAnimationCurve() override {}
  void AccumulateScrollDeltaForTracing(const gfx::Vector2dF& delta) override {}
  void DidStartPinchZoom() override {}
  void DidUpdatePinchZoom() override {}
  void DidEndPinchZoom() override {}
  void DidStartScroll() override {}
  void DidEndScroll() override {}
  void DidMouseLeave() override {}
  bool IsInHighLatencyMode() const override;
  void WillScrollContent(ElementId element_id) override {}
  void DidScrollContent(ElementId element_id, bool animated) override {}
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
