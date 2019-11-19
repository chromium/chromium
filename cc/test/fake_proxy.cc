// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/fake_proxy.h"

#include "cc/paint/paint_worklet_layer_painter.h"
#include "cc/trees/layer_tree_mutator.h"

namespace cc {

void FakeProxy::SetLayerTreeHost(LayerTreeHost* host) {
  layer_tree_host_ = host;
}

bool FakeProxy::RequestedAnimatePending() {
  return false;
}

bool FakeProxy::IsStarted() const { return true; }

bool FakeProxy::CommitRequested() const { return false; }

void FakeProxy::SetMutator(std::unique_ptr<LayerTreeMutator> mutator) {}

void FakeProxy::SetPaintWorkletLayerPainter(
    std::unique_ptr<PaintWorkletLayerPainter> painter) {}

bool FakeProxy::SupportsImplScrolling() const {
  return true;
}

bool FakeProxy::MainFrameWillHappenForTesting() {
  return false;
}

}  // namespace cc
