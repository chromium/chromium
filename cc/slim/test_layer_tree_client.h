// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SLIM_TEST_LAYER_TREE_CLIENT_H_
#define CC_SLIM_TEST_LAYER_TREE_CLIENT_H_

#include "cc/slim/layer_tree_client.h"

namespace cc::slim {

class TestLayerTreeClient : public LayerTreeClient {
 public:
  TestLayerTreeClient() = default;
  ~TestLayerTreeClient() override = default;

  void BeginFrame(const viz::BeginFrameArgs& args) override {}
  void DidSubmitCompositorFrame() override {}
  void DidReceiveCompositorFrameAck() override {}
  void RequestNewFrameSink() override;
  void DidInitializeLayerTreeFrameSink() override;
  void DidFailToInitializeLayerTreeFrameSink() override;
  void DidLoseLayerTreeFrameSink() override;

  uint32_t request_new_frame_sink_count() const {
    return request_new_frame_sink_count_;
  }
  uint32_t did_initialize_layer_tree_frame_sink_count() const {
    return did_initialize_layer_tree_frame_sink_count_;
  }
  uint32_t did_fail_to_initialize_layer_tree_frame_sink_count() const {
    return did_fail_to_initialize_layer_tree_frame_sink_count_;
  }
  uint32_t did_lose_layer_tree_frame_sink_count() const {
    return did_lose_layer_tree_frame_sink_count_;
  }

 private:
  uint32_t request_new_frame_sink_count_ = 0u;
  uint32_t did_initialize_layer_tree_frame_sink_count_ = 0u;
  uint32_t did_fail_to_initialize_layer_tree_frame_sink_count_ = 0u;
  uint32_t did_lose_layer_tree_frame_sink_count_ = 0u;
};

}  // namespace cc::slim

#endif  // CC_SLIM_TEST_LAYER_TREE_CLIENT_H_
