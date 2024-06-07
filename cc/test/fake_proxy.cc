// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/fake_proxy.h"

#include <memory>

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

bool FakeProxy::StartDeferringCommits(base::TimeDelta timeout,
                                      PaintHoldingReason reason) {
  return false;
}

bool FakeProxy::IsDeferringCommits() const {
  return false;
}

bool FakeProxy::CommitRequested() const { return false; }

void FakeProxy::QueueImageDecode(int request_id, const PaintImage& image) {}

void FakeProxy::SetMutator(std::unique_ptr<LayerTreeMutator> mutator) {}

void FakeProxy::SetPaintWorkletLayerPainter(
    std::unique_ptr<PaintWorkletLayerPainter> painter) {}

bool FakeProxy::MainFrameWillHappenForTesting() {
  return false;
}

double FakeProxy::GetPercentDroppedFrames() const {
  return 0.0;
}

}  // namespace cc
