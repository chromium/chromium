// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/fake_layer_tree_host_impl_client.h"
#include "cc/trees/mutator_host.h"

namespace cc {

bool FakeLayerTreeHostImplClient::IsInsideDraw() {
  return false;
}

void FakeLayerTreeHostImplClient::SetNeedsImplSideInvalidation(
    bool needs_first_draw_on_activation) {
  did_request_impl_side_invalidation_ = true;
}

void FakeLayerTreeHostImplClient::NotifyReadyToActivate() {
  ready_to_activate_ = true;
}

bool FakeLayerTreeHostImplClient::IsReadyToActivate() {
  return ready_to_activate();
}

void FakeLayerTreeHostImplClient::NotifyReadyToDraw() {
  ready_to_draw_ = true;
}

bool FakeLayerTreeHostImplClient::IsInSynchronousComposite() const {
  return is_synchronous_composite_;
}

size_t FakeLayerTreeHostImplClient::CommitDurationSampleCountForTesting()
    const {
  return 0;
}

}  // namespace cc
