// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/fake_layer_tree_frame_sink.h"

#include <vector>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"
#include "cc/test/fake_layer_context.h"
#include "cc/test/test_client_shared_image_interface.h"
#include "cc/tiles/image_decode_cache_utils.h"
#include "cc/trees/layer_tree_frame_sink_client.h"
#include "components/viz/common/frame_sinks/begin_frame_source.h"
#include "components/viz/common/frame_sinks/delay_based_time_source.h"
#include "components/viz/common/resources/returned_resource.h"
#include "components/viz/test/begin_frame_args_test.h"
#include "gpu/ipc/client/client_shared_image_interface.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "gpu/ipc/common/gpu_channel.mojom.h"
#include "gpu/ipc/common/mock_gpu_channel.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {

FakeLayerTreeFrameSink::Builder::Builder()
    : compositor_context_provider_(viz::TestContextProvider::CreateRaster()),
      worker_context_provider_(viz::TestContextProvider::CreateWorker()) {}

FakeLayerTreeFrameSink::Builder::~Builder() = default;

std::unique_ptr<FakeLayerTreeFrameSink>
FakeLayerTreeFrameSink::Builder::Build() {
  DCHECK(compositor_context_provider_);
  DCHECK(worker_context_provider_);
  return FakeLayerTreeFrameSink::Create3d(
      std::move(compositor_context_provider_),
      std::move(worker_context_provider_));
}

FakeLayerTreeFrameSink::FakeLayerTreeFrameSink(
    scoped_refptr<viz::RasterContextProvider> context_provider,
    scoped_refptr<viz::RasterContextProvider> worker_context_provider)
    : LayerTreeFrameSink(
          std::move(context_provider),
          std::move(worker_context_provider),
          base::SingleThreadTaskRunner::GetCurrentDefault(),
          base::MakeRefCounted<TestClientSharedImageInterface>(
              base::MakeRefCounted<gpu::TestSharedImageInterface>())) {}

FakeLayerTreeFrameSink::~FakeLayerTreeFrameSink() = default;

bool FakeLayerTreeFrameSink::BindToClient(LayerTreeFrameSinkClient* client) {
  if (!LayerTreeFrameSink::BindToClient(client))
    return false;
  begin_frame_source_ = std::make_unique<viz::BackToBackBeginFrameSource>(
      std::make_unique<viz::DelayBasedTimeSource>(
          base::SingleThreadTaskRunner::GetCurrentDefault().get()));
  client_->SetBeginFrameSource(begin_frame_source_.get());
  return true;
}

void FakeLayerTreeFrameSink::DetachFromClient() {
  ReturnResourcesHeldByParent();
  LayerTreeFrameSink::DetachFromClient();
}

void FakeLayerTreeFrameSink::SubmitCompositorFrame(viz::CompositorFrame frame,
                                                   bool hit_test_data_changed) {
  ReturnResourcesHeldByParent();

  last_sent_frame_ = std::make_unique<viz::CompositorFrame>(std::move(frame));
  ++num_sent_frames_;

  last_swap_rect_ = last_sent_frame_->render_pass_list.back()->damage_rect;

  resources_held_by_parent_.insert(resources_held_by_parent_.end(),
                                   last_sent_frame_->resource_list.begin(),
                                   last_sent_frame_->resource_list.end());

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&FakeLayerTreeFrameSink::DidReceiveCompositorFrameAck,
                     weak_ptr_factory_.GetWeakPtr()));
}

void FakeLayerTreeFrameSink::DidNotProduceFrame(const viz::BeginFrameAck& ack,
                                                FrameSkippedReason reason) {}

std::unique_ptr<LayerContext> FakeLayerTreeFrameSink::CreateLayerContext(
    LayerTreeHostImpl& host_impl) {
  return std::make_unique<FakeLayerContext>();
}

void FakeLayerTreeFrameSink::DidReceiveCompositorFrameAck() {
  client_->DidReceiveCompositorFrameAck();
}

void FakeLayerTreeFrameSink::ReturnResourcesHeldByParent() {
  if (last_sent_frame_) {
    // Return the last frame's resources immediately.
    std::vector<viz::ReturnedResource> resources;
    for (const auto& resource : resources_held_by_parent_)
      resources.push_back(resource.ToReturnedResource());
    resources_held_by_parent_.clear();
    client_->ReclaimResources(std::move(resources));
  }
}

void FakeLayerTreeFrameSink::NotifyDidPresentCompositorFrame(
    uint32_t frame_token,
    const viz::FrameTimingDetails& details) {
  client_->DidPresentCompositorFrame(frame_token, details);
}

}  // namespace cc
