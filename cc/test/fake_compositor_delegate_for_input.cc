// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/fake_compositor_delegate_for_input.h"

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

}  // namespace cc
