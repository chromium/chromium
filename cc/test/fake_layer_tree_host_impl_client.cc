// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/fake_layer_tree_host_impl_client.h"
#include "cc/trees/mutator_host.h"

namespace cc {

bool FakeLayerTreeHostImplClient::IsInsideDraw() {
  return false;
}

bool FakeLayerTreeHostImplClient::IsBeginMainFrameExpected() {
  return true;
}

void FakeLayerTreeHostImplClient::PostAnimationEventsToMainThreadOnImplThread(
    std::unique_ptr<MutatorEvents> events) {}

void FakeLayerTreeHostImplClient::NeedsImplSideInvalidation(
    bool needs_first_draw_on_activation) {
  did_request_impl_side_invalidation_ = true;
}

void FakeLayerTreeHostImplClient::NotifyReadyToActivate() {
  ready_to_activate_ = true;
}

void FakeLayerTreeHostImplClient::NotifyReadyToDraw() {
  ready_to_draw_ = true;
}

}  // namespace cc
