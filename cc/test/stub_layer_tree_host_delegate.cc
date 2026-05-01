// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/stub_layer_tree_host_delegate.h"

#include "cc/metrics/begin_main_frame_metrics.h"

namespace cc {

StubLayerTreeHostDelegate::~StubLayerTreeHostDelegate() = default;

std::unique_ptr<BeginMainFrameMetrics>
StubLayerTreeHostDelegate::GetBeginMainFrameMetrics() {
  return nullptr;
}

}  // namespace cc
