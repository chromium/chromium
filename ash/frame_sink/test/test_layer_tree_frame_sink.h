// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_FRAME_SINK_TEST_TEST_LAYER_TREE_FRAME_SINK_H_
#define ASH_FRAME_SINK_TEST_TEST_LAYER_TREE_FRAME_SINK_H_

#include <optional>

#include "cc/trees/layer_tree_frame_sink.h"
#include "cc/trees/layer_tree_frame_sink_client.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/common/resources/transferable_resource.h"

namespace ash {

// This class keeps track of the latest frame that is submitted by the
// `LayerTreeFrameSinkClient`. This class is used to help test the interactions
// of FrameSinkHosts and FrameSinkHolder with the display compositor.
class TestLayerTreeFrameSink : public cc::LayerTreeFrameSink {
 public:
  TestLayerTreeFrameSink();

  TestLayerTreeFrameSink(const TestLayerTreeFrameSink&) = delete;
  TestLayerTreeFrameSink& operator=(const TestLayerTreeFrameSink&) = delete;

  ~TestLayerTreeFrameSink() override;

  cc::LayerTreeFrameSinkClient* client();

  int num_of_frames_received() const;

  std::optional<cc::FrameSkippedReason> GetLatestFrameSkippedReason() const;

  const viz::CompositorFrame& GetLatestReceivedFrame();

  void ResetLatestFrameState();

  void ResetNumOfFramesReceived();

  void GetFrameResourcesToReturn(
      std::vector<viz::ReturnedResource>& return_resources);

  // cc::LayerTreeFrameSink:
  void SubmitCompositorFrame(viz::CompositorFrame frame,
                             bool hit_test_data_changed) override;
  void DidNotProduceFrame(const viz::BeginFrameAck& ack,
                          cc::FrameSkippedReason reason) override;
  void DidAllocateSharedBitmap(base::ReadOnlySharedMemoryRegion region,
                               const viz::SharedBitmapId& id) override;
  void DidDeleteSharedBitmap(const viz::SharedBitmapId& id) override;

 private:
  std::vector<viz::TransferableResource> resources_in_use_;
  std::optional<cc::FrameSkippedReason> latest_frame_skipped_reason_ =
      std::nullopt;
  viz::CompositorFrame latest_received_frame_;
  int num_of_frames_received_ = 0;
};

}  // namespace ash

#endif  // ASH_FRAME_SINK_TEST_TEST_LAYER_TREE_FRAME_SINK_H_
