// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/fake_layer_tree_frame_sink.h"

#include "base/bind.h"
#include "base/stl_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "cc/trees/layer_tree_frame_sink_client.h"
#include "components/viz/common/frame_sinks/begin_frame_source.h"
#include "components/viz/common/frame_sinks/delay_based_time_source.h"
#include "components/viz/common/resources/returned_resource.h"
#include "components/viz/test/begin_frame_args_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {

FakeLayerTreeFrameSink::Builder::Builder()
    : compositor_context_provider_(viz::TestContextProvider::Create()),
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
    scoped_refptr<viz::ContextProvider> context_provider,
    scoped_refptr<viz::RasterContextProvider> worker_context_provider)
    : LayerTreeFrameSink(std::move(context_provider),
                         std::move(worker_context_provider),
                         base::ThreadTaskRunnerHandle::Get(),
                         nullptr) {
  gpu_memory_buffer_manager_ =
      context_provider_ ? &test_gpu_memory_buffer_manager_ : nullptr;
}

FakeLayerTreeFrameSink::~FakeLayerTreeFrameSink() = default;

bool FakeLayerTreeFrameSink::BindToClient(LayerTreeFrameSinkClient* client) {
  if (!LayerTreeFrameSink::BindToClient(client))
    return false;
  begin_frame_source_ = std::make_unique<viz::BackToBackBeginFrameSource>(
      std::make_unique<viz::DelayBasedTimeSource>(
          base::ThreadTaskRunnerHandle::Get().get()));
  client_->SetBeginFrameSource(begin_frame_source_.get());
  return true;
}

void FakeLayerTreeFrameSink::DetachFromClient() {
  ReturnResourcesHeldByParent();
  LayerTreeFrameSink::DetachFromClient();
}

void FakeLayerTreeFrameSink::SubmitCompositorFrame(
    viz::CompositorFrame frame,
    bool hit_test_data_changed,
    bool submit_hit_test_borders) {
  ReturnResourcesHeldByParent();

  last_sent_frame_ = std::make_unique<viz::CompositorFrame>(std::move(frame));
  ++num_sent_frames_;

  last_swap_rect_ = last_sent_frame_->render_pass_list.back()->damage_rect;

  resources_held_by_parent_.insert(resources_held_by_parent_.end(),
                                   last_sent_frame_->resource_list.begin(),
                                   last_sent_frame_->resource_list.end());

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&FakeLayerTreeFrameSink::DidReceiveCompositorFrameAck,
                     weak_ptr_factory_.GetWeakPtr()));
}

void FakeLayerTreeFrameSink::DidNotProduceFrame(const viz::BeginFrameAck& ack) {
}

void FakeLayerTreeFrameSink::DidAllocateSharedBitmap(
    base::ReadOnlySharedMemoryRegion region,
    const viz::SharedBitmapId& id) {
  DCHECK(!base::Contains(shared_bitmaps_, id));
  shared_bitmaps_.push_back(id);
}

void FakeLayerTreeFrameSink::DidDeleteSharedBitmap(
    const viz::SharedBitmapId& id) {
  DCHECK(base::Contains(shared_bitmaps_, id));
  base::Erase(shared_bitmaps_, id);
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
    client_->ReclaimResources(resources);
  }
}

}  // namespace cc
