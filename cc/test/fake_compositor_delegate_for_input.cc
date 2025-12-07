// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/fake_compositor_delegate_for_input.h"

#include "cc/paint/element_id.h"

namespace cc {

FakeCompositorDelegateForInput::FakeCompositorDelegateForInput()
    : host_impl_(&task_runner_provider_, &task_graph_runner_) {}

FakeCompositorDelegateForInput::~FakeCompositorDelegateForInput() = default;

ScrollTree& FakeCompositorDelegateForInput::GetScrollTree() const {
  return scroll_tree_;
}

bool FakeCompositorDelegateForInput::HasAnimatedScrollbars() const {
  return false;
}

bool FakeCompositorDelegateForInput::IsInHighLatencyMode() const {
  return false;
}

float FakeCompositorDelegateForInput::DeviceScaleFactor() const {
  return 0;
}

float FakeCompositorDelegateForInput::PageScaleFactor() const {
  return 0;
}

gfx::Size FakeCompositorDelegateForInput::VisualDeviceViewportSize() const {
  return gfx::Size();
}

const LayerTreeSettings& FakeCompositorDelegateForInput::GetSettings() const {
  return settings_;
}

LayerTreeHostImpl& FakeCompositorDelegateForInput::GetImplDeprecated() {
  return host_impl_;
}

const LayerTreeHostImpl& FakeCompositorDelegateForInput::GetImplDeprecated()
    const {
  return host_impl_;
}

bool FakeCompositorDelegateForInput::HasScrollLinkedAnimation(
    ElementId for_scroller) const {
  return false;
}

float FakeCompositorDelegateForInput::GetBrowserControlsTopOffset() const {
  return 0.0;
}

bool FakeCompositorDelegateForInput::ScrollbarAnimationMouseDown(
    ElementId element_id) const {
  return false;
}

bool FakeCompositorDelegateForInput::ScrollbarAnimationMouseUp(
    ElementId element_id) const {
  return false;
}

double FakeCompositorDelegateForInput::PredictViewportBoundsDelta(
    double current_bounds_delta,
    gfx::Vector2dF scroll_distance) const {
  return 0.0;
}

bool FakeCompositorDelegateForInput::ElementHasImplOnlyScrollAnimation(
    ElementId) const {
  return false;
}

std::optional<gfx::PointF>
FakeCompositorDelegateForInput::UpdateImplAnimationScrollTargetWithDelta(
    gfx::Vector2dF adjusted_delta,
    int scroll_node_id,
    base::TimeDelta delayed_by,
    ElementId element_id) const {
  return std::nullopt;
}

std::unique_ptr<EventsMetricsManager::ScopedMonitor>
FakeCompositorDelegateForInput::GetScopedEventMetricsMonitor(
    EventsMetricsManager::ScopedMonitor::DoneCallback done_callback) {
  return nullptr;
}

std::unique_ptr<LatencyInfoSwapPromiseMonitor>
FakeCompositorDelegateForInput::CreateLatencyInfoSwapPromiseMonitor(
    ui::LatencyInfo* latency) {
  return nullptr;
}

bool FakeCompositorDelegateForInput::ScrollAnimationCreate(
    const ScrollNode& scroll_node,
    const gfx::Vector2dF& scroll_amount,
    base::TimeDelta delayed_by) {
  return false;
}

}  // namespace cc
