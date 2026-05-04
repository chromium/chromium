// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/fake_layer_tree_host_impl_delegate.h"

#include "cc/trees/mutator_host.h"

namespace cc {

void FakeLayerTreeHostImplDelegate::DidLoseLayerTreeFrameSinkOnImplThread() {
  did_lose_layer_tree_frame_sink_on_impl_thread_ = true;
}

bool FakeLayerTreeHostImplDelegate::IsInsideDraw() {
  return false;
}

void FakeLayerTreeHostImplDelegate::SetNeedsImplSideInvalidation(
    bool needs_first_draw_on_activation) {
  did_request_impl_side_invalidation_ = true;
}

void FakeLayerTreeHostImplDelegate::NotifyReadyToActivate() {
  ready_to_activate_ = true;
}

bool FakeLayerTreeHostImplDelegate::IsReadyToActivate() {
  return ready_to_activate();
}

void FakeLayerTreeHostImplDelegate::NotifyReadyToDraw() {
  ready_to_draw_ = true;
}

bool FakeLayerTreeHostImplDelegate::IsInSynchronousComposite() const {
  return is_synchronous_composite_;
}

size_t FakeLayerTreeHostImplDelegate::CommitDurationSampleCountForTesting()
    const {
  return 0;
}

}  // namespace cc
