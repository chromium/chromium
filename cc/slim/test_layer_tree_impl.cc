// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/slim/test_layer_tree_impl.h"

#include "base/check.h"
#include "cc/slim/frame_sink_impl.h"

namespace cc::slim {

void TestLayerTreeImpl::ResetNeedsBeginFrame() {
  client_needs_one_begin_frame_ = false;
  needs_draw_ = false;
  DCHECK(!NeedsBeginFrames());
  if (frame_sink_) {
    frame_sink_->SetNeedsBeginFrame(false);
  }
}

}  // namespace cc::slim
