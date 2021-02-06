// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_FAKE_LAYER_TREE_FRAME_SINK_CLIENT_H_
#define CC_TEST_FAKE_LAYER_TREE_FRAME_SINK_CLIENT_H_

#include "cc/trees/layer_tree_frame_sink_client.h"

#include "cc/trees/managed_memory_policy.h"
#include "components/viz/common/hit_test/hit_test_region_list.h"

namespace viz {
struct FrameTimingDetails;
}

namespace cc {

class FakeLayerTreeFrameSinkClient : public LayerTreeFrameSinkClient {
 public:
  FakeLayerTreeFrameSinkClient();
  ~FakeLayerTreeFrameSinkClient() override;

  void SetBeginFrameSource(viz::BeginFrameSource* source) override;
  base::Optional<viz::HitTestRegionList> BuildHitTestData() override;
  void DidReceiveCompositorFrameAck() override;
  void DidPresentCompositorFrame(
      uint32_t frame_token,
      const viz::FrameTimingDetails& details) override {}
  void ReclaimResources(
      const std::vector<viz::ReturnedResource>& resources) override {}
  void DidLoseLayerTreeFrameSink() override;
  void SetExternalTilePriorityConstraints(
      const gfx::Rect& viewport_rect_for_tile_priority,
      const gfx::Transform& transform_for_tile_priority) override {}
  void SetMemoryPolicy(const ManagedMemoryPolicy& policy) override;
  void SetTreeActivationCallback(base::RepeatingClosure callback) override {}
  void OnDraw(const gfx::Transform& transform,
              const gfx::Rect& viewport,
              bool resourceless_software_draw,
              bool skip_draw) override {}

  int ack_count() { return ack_count_; }

  bool did_lose_layer_tree_frame_sink_called() {
    return did_lose_layer_tree_frame_sink_called_;
  }

  const ManagedMemoryPolicy& memory_policy() const { return memory_policy_; }

  viz::BeginFrameSource* begin_frame_source() const {
    return begin_frame_source_;
  }

  void set_hit_test_region_list(
      const base::Optional<viz::HitTestRegionList>& hit_test_region_list) {
    hit_test_region_list_ = hit_test_region_list;
  }

 private:
  int ack_count_ = 0;
  bool did_lose_layer_tree_frame_sink_called_ = false;
  ManagedMemoryPolicy memory_policy_{0};
  viz::BeginFrameSource* begin_frame_source_;
  base::Optional<viz::HitTestRegionList> hit_test_region_list_;
};

}  // namespace cc

#endif  // CC_TEST_FAKE_LAYER_TREE_FRAME_SINK_CLIENT_H_
