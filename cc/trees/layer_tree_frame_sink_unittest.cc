// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "cc/trees/layer_tree_frame_sink.h"

#include "base/memory/read_only_shared_memory_region.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/test_simple_task_runner.h"
#include "cc/test/fake_layer_tree_frame_sink_client.h"
#include "cc/tiles/image_decode_cache_utils.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/test/test_context_provider.h"
#include "components/viz/test/test_raster_interface.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/raster_interface.h"
#include "gpu/ipc/client/client_shared_image_interface.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

class StubLayerTreeFrameSink : public LayerTreeFrameSink {
 public:
  explicit StubLayerTreeFrameSink(
      scoped_refptr<viz::RasterContextProvider> context_provider,
      scoped_refptr<viz::RasterContextProvider> worker_context_provider,
      scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner)
      : LayerTreeFrameSink(
            std::move(context_provider),
            base::MakeRefCounted<RasterContextProviderWrapper>(
                std::move(worker_context_provider),
                /*dark_mode_filter=*/nullptr,
                ImageDecodeCacheUtils::GetWorkingSetBytesForImageDecode(
                    /*for_renderer=*/false)),
            std::move(compositor_task_runner),
            nullptr,
            /*shared_image_interface=*/nullptr) {}

  void SubmitCompositorFrame(viz::CompositorFrame frame,
                             bool hit_test_data_changed) override {
    client_->DidReceiveCompositorFrameAck();
  }
  void DidNotProduceFrame(const viz::BeginFrameAck& ack,
                          FrameSkippedReason reason) override {}
  void DidAllocateSharedBitmap(base::ReadOnlySharedMemoryRegion region,
                               const viz::SharedBitmapId& id) override {}
  void DidDeleteSharedBitmap(const viz::SharedBitmapId& id) override {}
};

TEST(LayerTreeFrameSinkTest, ContextLossInformsClient) {
  scoped_refptr<viz::TestContextProvider> provider =
      viz::TestContextProvider::CreateRaster();
  scoped_refptr<viz::TestContextProvider> worker_provider =
      viz::TestContextProvider::CreateWorker();
  auto task_runner = base::MakeRefCounted<base::TestSimpleTaskRunner>();
  StubLayerTreeFrameSink layer_tree_frame_sink(provider, worker_provider,
                                               task_runner);
  EXPECT_FALSE(layer_tree_frame_sink.HasClient());

  FakeLayerTreeFrameSinkClient client;
  EXPECT_TRUE(layer_tree_frame_sink.BindToClient(&client));
  EXPECT_TRUE(layer_tree_frame_sink.HasClient());

  // Verify DidLoseLayerTreeFrameSink callback is hooked up correctly.
  EXPECT_FALSE(client.did_lose_layer_tree_frame_sink_called());
  layer_tree_frame_sink.context_provider()
      ->RasterInterface()
      ->LoseContextCHROMIUM(GL_GUILTY_CONTEXT_RESET_ARB,
                            GL_INNOCENT_CONTEXT_RESET_ARB);
  layer_tree_frame_sink.context_provider()->RasterInterface()->Flush();
  EXPECT_TRUE(client.did_lose_layer_tree_frame_sink_called());

  layer_tree_frame_sink.DetachFromClient();
}

TEST(LayerTreeFrameSinkTest, ContextLossFailsBind) {
  scoped_refptr<viz::TestContextProvider> context_provider =
      viz::TestContextProvider::CreateRaster();
  scoped_refptr<viz::TestContextProvider> worker_provider =
      viz::TestContextProvider::CreateWorker();

  // Lose the context so BindToClient fails.
  context_provider->UnboundTestRasterInterface()->set_context_lost(true);

  auto task_runner = base::MakeRefCounted<base::TestSimpleTaskRunner>();
  StubLayerTreeFrameSink layer_tree_frame_sink(context_provider,
                                               worker_provider, task_runner);
  EXPECT_FALSE(layer_tree_frame_sink.HasClient());

  FakeLayerTreeFrameSinkClient client;
  EXPECT_FALSE(layer_tree_frame_sink.BindToClient(&client));
  EXPECT_FALSE(layer_tree_frame_sink.HasClient());

  layer_tree_frame_sink.DetachFromClient();
}

TEST(LayerTreeFrameSinkTest, WorkerContextLossInformsClient) {
  scoped_refptr<viz::TestContextProvider> provider =
      viz::TestContextProvider::CreateRaster();
  scoped_refptr<viz::TestContextProvider> worker_provider =
      viz::TestContextProvider::CreateWorker();
  auto task_runner = base::MakeRefCounted<base::TestSimpleTaskRunner>();
  StubLayerTreeFrameSink layer_tree_frame_sink(provider, worker_provider,
                                               task_runner);
  EXPECT_FALSE(layer_tree_frame_sink.HasClient());

  FakeLayerTreeFrameSinkClient client;
  EXPECT_TRUE(layer_tree_frame_sink.BindToClient(&client));
  EXPECT_TRUE(layer_tree_frame_sink.HasClient());

  // Verify DidLoseLayerTreeFrameSink callback is hooked up correctly.
  EXPECT_FALSE(client.did_lose_layer_tree_frame_sink_called());
  {
    viz::RasterContextProvider::ScopedRasterContextLock context_lock(
        layer_tree_frame_sink.worker_context_provider());
    context_lock.RasterInterface()->LoseContextCHROMIUM(
        GL_GUILTY_CONTEXT_RESET_ARB, GL_INNOCENT_CONTEXT_RESET_ARB);
    context_lock.RasterInterface()->Flush();
  }
  task_runner->RunPendingTasks();
  EXPECT_TRUE(client.did_lose_layer_tree_frame_sink_called());

  layer_tree_frame_sink.DetachFromClient();
}

TEST(LayerTreeFrameSinkTest, WorkerContextLossFailsBind) {
  scoped_refptr<viz::TestContextProvider> context_provider =
      viz::TestContextProvider::CreateRaster();
  scoped_refptr<viz::TestContextProvider> worker_provider =
      viz::TestContextProvider::CreateWorker();

  // Lose the context so BindToClient fails.
  worker_provider->UnboundTestRasterInterface()->set_context_lost(true);

  auto task_runner = base::MakeRefCounted<base::TestSimpleTaskRunner>();
  StubLayerTreeFrameSink layer_tree_frame_sink(context_provider,
                                               worker_provider, task_runner);
  EXPECT_FALSE(layer_tree_frame_sink.HasClient());

  FakeLayerTreeFrameSinkClient client;
  EXPECT_FALSE(layer_tree_frame_sink.BindToClient(&client));
  EXPECT_FALSE(layer_tree_frame_sink.HasClient());

  layer_tree_frame_sink.DetachFromClient();
}

}  // namespace
}  // namespace cc
