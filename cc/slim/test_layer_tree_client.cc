// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/slim/test_layer_tree_client.h"

namespace cc::slim {

void TestLayerTreeClient::RequestNewFrameSink() {
  request_new_frame_sink_count_++;
}

void TestLayerTreeClient::DidInitializeLayerTreeFrameSink() {
  did_initialize_layer_tree_frame_sink_count_++;
}

void TestLayerTreeClient::DidFailToInitializeLayerTreeFrameSink() {
  did_fail_to_initialize_layer_tree_frame_sink_count_++;
}

void TestLayerTreeClient::DidLoseLayerTreeFrameSink() {
  did_lose_layer_tree_frame_sink_count_++;
}

}  // namespace cc::slim
