// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/stub_layer_tree_host_client.h"

#include "cc/metrics/begin_main_frame_metrics.h"

namespace cc {

StubLayerTreeHostClient::~StubLayerTreeHostClient() = default;

std::unique_ptr<BeginMainFrameMetrics>
StubLayerTreeHostClient::GetBeginMainFrameMetrics() {
  return nullptr;
}

}  // namespace cc
