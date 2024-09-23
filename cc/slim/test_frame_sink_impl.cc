// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/slim/test_frame_sink_impl.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "cc/slim/simple_scheduler.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/test/test_context_provider.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/viz/public/mojom/compositing/compositor_frame_sink.mojom.h"
#include "services/viz/public/mojom/compositing/layer_context.mojom.h"

namespace cc::slim {

class TestFrameSinkImpl::TestMojoCompositorFrameSink
    : public viz::mojom::CompositorFrameSink {
 public:
  TestMojoCompositorFrameSink() = default;
  void SetNeedsBeginFrame(bool needs_begin_frame) override {}
  void SetWantsAnimateOnlyBeginFrames() override {}
  void SetWantsBeginFrameAcks() override {}
  void SetAutoNeedsBeginFrame() override {}
  void SubmitCompositorFrame(
      const viz::LocalSurfaceId& local_surface_id,
      viz::CompositorFrame frame,
      std::optional<::viz::HitTestRegionList> hit_test_region_list,
      uint64_t submit_time) override {
    did_submit_ = true;
    last_frame_ = std::move(frame);
    hit_test_region_list_ = std::move(hit_test_region_list);
  }
  void SubmitCompositorFrameSync(
      const viz::LocalSurfaceId& local_surface_id,
      viz::CompositorFrame frame,
      std::optional<::viz::HitTestRegionList> hit_test_region_list,
      uint64_t submit_time,
      SubmitCompositorFrameSyncCallback callback) override {}
  void DidNotProduceFrame(const viz::BeginFrameAck& ack) override {
    did_not_produce_frame_ = true;
  }
  void DidAllocateSharedBitmap(base::ReadOnlySharedMemoryRegion region,
                               const viz::SharedBitmapId& id) override {}
  void DidDeleteSharedBitmap(const viz::SharedBitmapId& id) override {}
  void InitializeCompositorFrameSinkType(
      viz::mojom::CompositorFrameSinkType type) override {}
  void BindLayerContext(viz::mojom::PendingLayerContextPtr context) override {}
#if BUILDFLAG(IS_ANDROID)
  void SetThreadIds(const std::vector<int32_t>& thread_ids) override {}
#endif

  viz::CompositorFrame TakeLastFrame() { return std::move(last_frame_); }
  const std::optional<::viz::HitTestRegionList>& hit_test_region_list() const {
    return hit_test_region_list_;
  }

  bool GetDidSubmitAndReset() {
    bool rv = did_submit_;
    did_submit_ = false;
    return rv;
  }

  bool GetDidNotProduceFrameAndReset() {
    bool rv = did_not_produce_frame_;
    did_not_produce_frame_ = false;
    return rv;
  }

 private:
  viz::CompositorFrame last_frame_;
  std::optional<::viz::HitTestRegionList> hit_test_region_list_;
  bool did_submit_ = false;
  bool did_not_produce_frame_ = false;
};

// static
std::unique_ptr<TestFrameSinkImpl> TestFrameSinkImpl::Create() {
  auto task_runner = base::SingleThreadTaskRunner::GetCurrentDefault();
  mojo::PendingAssociatedRemote<viz::mojom::CompositorFrameSink> sink_remote;
  mojo::PendingAssociatedReceiver<viz::mojom::CompositorFrameSink>
      sink_receiver = sink_remote.InitWithNewEndpointAndPassReceiver();
  mojo::PendingReceiver<viz::mojom::CompositorFrameSinkClient> client;
  auto context_provider = viz::TestContextProvider::CreateRaster();

  return base::WrapUnique(new TestFrameSinkImpl(
      std::move(task_runner), std::move(sink_remote), std::move(client),
      std::move(context_provider), std::move(sink_receiver)));
}

TestFrameSinkImpl::TestFrameSinkImpl(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    mojo::PendingAssociatedRemote<viz::mojom::CompositorFrameSink>
        compositor_frame_sink_associated_remote,
    mojo::PendingReceiver<viz::mojom::CompositorFrameSinkClient>
        client_receiver,
    scoped_refptr<viz::RasterContextProvider> context_provider,
    mojo::PendingAssociatedReceiver<viz::mojom::CompositorFrameSink>
        sink_receiver)
    : FrameSinkImpl(std::move(task_runner),
                    std::move(compositor_frame_sink_associated_remote),
                    std::move(client_receiver),
                    std::move(context_provider),
                    base::kInvalidThreadId,
                    std::make_unique<SimpleScheduler>()),
      mojo_sink_(std::make_unique<TestMojoCompositorFrameSink>()) {}

TestFrameSinkImpl::~TestFrameSinkImpl() = default;

bool TestFrameSinkImpl::GetDidSubmitAndReset() {
  return mojo_sink_->GetDidSubmitAndReset();
}

bool TestFrameSinkImpl::GetDidNotProduceFrameAndReset() {
  return mojo_sink_->GetDidNotProduceFrameAndReset();
}

viz::CompositorFrame TestFrameSinkImpl::TakeLastFrame() {
  return mojo_sink_->TakeLastFrame();
}

const std::optional<::viz::HitTestRegionList>&
TestFrameSinkImpl::GetLastHitTestRegionList() const {
  return mojo_sink_->hit_test_region_list();
}

bool TestFrameSinkImpl::BindToClient(FrameSinkImplClient* client) {
  DCHECK(!bind_to_client_called_);
  client_ = client;
  frame_sink_ = mojo_sink_.get();
  bind_to_client_called_ = true;
  if (bind_to_client_result_) {
    context_provider_->BindToCurrentSequence();
  }
  return bind_to_client_result_;
}

void TestFrameSinkImpl::SetNeedsBeginFrame(bool needs_begin_frame) {
  needs_begin_frames_ = needs_begin_frame;
}

}  // namespace cc::slim
