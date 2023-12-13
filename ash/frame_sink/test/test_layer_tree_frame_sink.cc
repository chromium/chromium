// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/frame_sink/test/test_layer_tree_frame_sink.h"

#include <optional>

#include "gpu/ipc/client/client_shared_image_interface.h"

namespace ash {

TestLayerTreeFrameSink::TestLayerTreeFrameSink()
    : LayerTreeFrameSink(/*context_provider=*/nullptr,
                         /*worker_context_provider_wrapper=*/nullptr,
                         /*compositor_task_runner=*/nullptr,
                         /*gpu_memory_buffer_manager=*/nullptr,
                         /*shared_image_interface=*/nullptr) {}

TestLayerTreeFrameSink::~TestLayerTreeFrameSink() = default;

void TestLayerTreeFrameSink::SubmitCompositorFrame(viz::CompositorFrame frame,
                                                   bool hit_test_data_changed) {
  for (auto resource : frame.resource_list) {
    resources_in_use_.push_back(resource);
  }

  latest_received_frame_ = std::move(frame);
  num_of_frames_received_++;
}

void TestLayerTreeFrameSink::DidNotProduceFrame(const viz::BeginFrameAck& ack,
                                                cc::FrameSkippedReason reason) {
  latest_frame_skipped_reason_ = reason;
}

void TestLayerTreeFrameSink::DidAllocateSharedBitmap(
    base::ReadOnlySharedMemoryRegion region,
    const viz::SharedBitmapId& id) {}

void TestLayerTreeFrameSink::DidDeleteSharedBitmap(
    const viz::SharedBitmapId& id) {}

void TestLayerTreeFrameSink::GetFrameResourcesToReturn(
    std::vector<viz::ReturnedResource>& return_resources) {
  for (auto resource : resources_in_use_) {
    return_resources.push_back(resource.ToReturnedResource());
  }
}

std::optional<cc::FrameSkippedReason>
TestLayerTreeFrameSink::GetLatestFrameSkippedReason() const {
  return latest_frame_skipped_reason_;
}

int TestLayerTreeFrameSink::num_of_frames_received() const {
  return num_of_frames_received_;
}

cc::LayerTreeFrameSinkClient* TestLayerTreeFrameSink::client() {
  return client_;
}

const viz::CompositorFrame& TestLayerTreeFrameSink::GetLatestReceivedFrame() {
  return latest_received_frame_;
}

void TestLayerTreeFrameSink::ResetLatestFrameState() {
  latest_frame_skipped_reason_.reset();
  latest_received_frame_ = viz::CompositorFrame();
  resources_in_use_.clear();
}

void TestLayerTreeFrameSink::ResetNumOfFramesReceived() {
  num_of_frames_received_ = 0;
}

}  // namespace ash
