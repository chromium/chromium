// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/mojo_embedder/async_layer_tree_frame_sink.h"

#include <memory>

#include "base/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/threading/thread.h"
#include "cc/test/fake_layer_tree_frame_sink_client.h"
#include "components/viz/client/hit_test_data_provider_draw_quad.h"
#include "components/viz/common/quads/render_pass_draw_quad.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/common/quads/surface_draw_quad.h"
#include "components/viz/common/surfaces/surface_range.h"
#include "components/viz/test/compositor_frame_helpers.h"
#include "components/viz/test/test_context_provider.h"
#include "components/viz/test/test_gpu_memory_buffer_manager.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/viz/public/mojom/compositing/compositor_frame_sink.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace mojo_embedder {
namespace {

// Used to track the thread DidLoseLayerTreeFrameSink() is called on (and quit
// a RunLoop).
class ThreadTrackingLayerTreeFrameSinkClient
    : public FakeLayerTreeFrameSinkClient {
 public:
  ThreadTrackingLayerTreeFrameSinkClient(
      base::PlatformThreadId* called_thread_id,
      base::RunLoop* run_loop)
      : called_thread_id_(called_thread_id), run_loop_(run_loop) {}
  ThreadTrackingLayerTreeFrameSinkClient(
      const ThreadTrackingLayerTreeFrameSinkClient&) = delete;
  ~ThreadTrackingLayerTreeFrameSinkClient() override = default;

  ThreadTrackingLayerTreeFrameSinkClient& operator=(
      const ThreadTrackingLayerTreeFrameSinkClient&) = delete;

  // FakeLayerTreeFrameSinkClient:
  void DidLoseLayerTreeFrameSink() override {
    EXPECT_FALSE(did_lose_layer_tree_frame_sink_called());
    FakeLayerTreeFrameSinkClient::DidLoseLayerTreeFrameSink();
    *called_thread_id_ = base::PlatformThread::CurrentId();
    run_loop_->Quit();
  }

 private:
  base::PlatformThreadId* called_thread_id_;
  base::RunLoop* run_loop_;
};

TEST(AsyncLayerTreeFrameSinkTest,
     DidLoseLayerTreeFrameSinkCalledOnConnectionError) {
  base::Thread bg_thread("BG Thread");
  bg_thread.Start();

  scoped_refptr<viz::TestContextProvider> provider =
      viz::TestContextProvider::Create();
  viz::TestGpuMemoryBufferManager test_gpu_memory_buffer_manager;

  mojo::PendingRemote<viz::mojom::CompositorFrameSink> sink_remote;
  mojo::PendingReceiver<viz::mojom::CompositorFrameSink> sink_receiver =
      sink_remote.InitWithNewPipeAndPassReceiver();
  mojo::PendingRemote<viz::mojom::CompositorFrameSinkClient> client;

  AsyncLayerTreeFrameSink::InitParams init_params;
  init_params.compositor_task_runner = bg_thread.task_runner();
  init_params.gpu_memory_buffer_manager = &test_gpu_memory_buffer_manager;
  init_params.pipes.compositor_frame_sink_remote = std::move(sink_remote);
  init_params.pipes.client_receiver = client.InitWithNewPipeAndPassReceiver();
  auto layer_tree_frame_sink = std::make_unique<AsyncLayerTreeFrameSink>(
      std::move(provider), nullptr, &init_params);

  base::PlatformThreadId called_thread_id = base::kInvalidThreadId;
  base::RunLoop close_run_loop;
  ThreadTrackingLayerTreeFrameSinkClient frame_sink_client(&called_thread_id,
                                                           &close_run_loop);

  auto bind_in_background =
      [](AsyncLayerTreeFrameSink* layer_tree_frame_sink,
         ThreadTrackingLayerTreeFrameSinkClient* frame_sink_client) {
        layer_tree_frame_sink->BindToClient(frame_sink_client);
      };
  bg_thread.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(bind_in_background,
                                base::Unretained(layer_tree_frame_sink.get()),
                                base::Unretained(&frame_sink_client)));
  // Closes the pipe, which should trigger calling DidLoseLayerTreeFrameSink()
  // (and quitting the RunLoop). There is no need to wait for BindToClient()
  // to complete as mojo::Binding error callbacks are processed asynchronously.
  sink_receiver.reset();
  close_run_loop.Run();

  EXPECT_NE(base::kInvalidThreadId, called_thread_id);
  EXPECT_EQ(called_thread_id, bg_thread.GetThreadId());

  // DetachFromClient() has to be called on the background thread.
  base::RunLoop detach_run_loop;
  auto detach_in_background =
      [](std::unique_ptr<AsyncLayerTreeFrameSink> layer_tree_frame_sink,
         base::RunLoop* detach_run_loop) {
        layer_tree_frame_sink->DetachFromClient();
        detach_run_loop->Quit();
      };
  bg_thread.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(detach_in_background, std::move(layer_tree_frame_sink),
                     base::Unretained(&detach_run_loop)));
  detach_run_loop.Run();
}

}  // namespace

// Boilerplate code for simple AsyncLayerTreeFrameSink. Friend of
// AsyncLayerTreeFrameSink.
class AsyncLayerTreeFrameSinkSimpleTest : public testing::Test {
 public:
  AsyncLayerTreeFrameSinkSimpleTest()
      : task_runner_(base::MakeRefCounted<base::TestMockTimeTaskRunner>(
            base::TestMockTimeTaskRunner::Type::kStandalone)),
        display_rect_(1, 1) {
    auto context_provider = viz::TestContextProvider::Create();

    mojo::PendingRemote<viz::mojom::CompositorFrameSink> sink_remote;
    mojo::PendingReceiver<viz::mojom::CompositorFrameSink> sink_receiver =
        sink_remote.InitWithNewPipeAndPassReceiver();
    mojo::PendingRemote<viz::mojom::CompositorFrameSinkClient> client;

    init_params_.compositor_task_runner = task_runner_;
    init_params_.gpu_memory_buffer_manager = &test_gpu_memory_buffer_manager_;
    init_params_.pipes.compositor_frame_sink_remote = std::move(sink_remote);
    init_params_.pipes.client_receiver =
        client.InitWithNewPipeAndPassReceiver();
    init_params_.hit_test_data_provider =
        std::make_unique<viz::HitTestDataProviderDrawQuad>(
            /*should_ask_for_child_region=*/true, /*root_accepts_events=*/true);

    layer_tree_frame_sink_ = std::make_unique<AsyncLayerTreeFrameSink>(
        std::move(context_provider), nullptr, &init_params_);

    viz::LocalSurfaceId local_surface_id(1, base::UnguessableToken::Create());
    layer_tree_frame_sink_->SetLocalSurfaceId(local_surface_id);
    layer_tree_frame_sink_->BindToClient(&layer_tree_frame_sink_client_);
  }

  void SendRenderPassList(viz::RenderPassList* pass_list,
                          bool hit_test_data_changed) {
    auto frame = viz::CompositorFrameBuilder()
                     .SetRenderPassList(std::move(*pass_list))
                     .Build();
    pass_list->clear();
    layer_tree_frame_sink_->SubmitCompositorFrame(
        std::move(frame), hit_test_data_changed,
        /*show_hit_test_borders=*/false);
  }

  const viz::HitTestRegionList& GetHitTestData() const {
    return layer_tree_frame_sink_->get_last_hit_test_data_for_testing();
  }

  AsyncLayerTreeFrameSink::InitParams init_params_;

  scoped_refptr<base::TestMockTimeTaskRunner> task_runner_;
  viz::TestGpuMemoryBufferManager test_gpu_memory_buffer_manager_;
  gfx::Rect display_rect_;
  std::unique_ptr<AsyncLayerTreeFrameSink> layer_tree_frame_sink_;
  FakeLayerTreeFrameSinkClient layer_tree_frame_sink_client_;
};

TEST_F(AsyncLayerTreeFrameSinkSimpleTest, HitTestRegionListDuplicate) {
  viz::RenderPassList pass_list;

  // Initial submission.
  auto pass1 = viz::RenderPass::Create();
  pass1->id = 1;
  pass1->output_rect = display_rect_;
  auto* shared_quad_state1 = pass1->CreateAndAppendSharedQuadState();
  gfx::Rect rect1(display_rect_);
  shared_quad_state1->SetAll(
      gfx::Transform(), /*quad_layer_rect=*/rect1,
      /*visible_quad_layer_rect=*/rect1,
      /*rounded_corner_bounds=*/gfx::RRectF(), /*clip_rect=*/rect1,
      /*is_clipped=*/false, /*are_contents_opaque=*/false,
      /*opacity=*/0.5f, SkBlendMode::kSrcOver, /*sorting_context_id=*/0);
  auto* quad1 =
      pass1->quad_list.AllocateAndConstruct<viz::SolidColorDrawQuad>();
  quad1->SetNew(shared_quad_state1, /*rect=*/rect1,
                /*visible_rect=*/rect1, SK_ColorBLACK,
                /*force_anti_aliasing_off=*/false);
  pass_list.push_back(move(pass1));
  SendRenderPassList(&pass_list, /*hit_test_data_changed=*/false);
  task_runner_->RunUntilIdle();
  const viz::HitTestRegionList hit_test_region_list = GetHitTestData();

  // Identical submission.
  auto pass2 = viz::RenderPass::Create();
  pass2->id = 2;
  pass2->output_rect = display_rect_;
  auto* shared_quad_state2 = pass2->CreateAndAppendSharedQuadState();
  gfx::Rect rect2(display_rect_);
  shared_quad_state2->SetAll(
      gfx::Transform(), /*quad_layer_rect=*/rect2,
      /*visible_quad_layer_rect=*/rect2,
      /*rounded_corner_bounds=*/gfx::RRectF(), /*clip_rect=*/rect2,
      /*is_clipped=*/false, /*are_contents_opaque=*/false,
      /*opacity=*/0.5f, SkBlendMode::kSrcOver, /*sorting_context_id=*/0);
  auto* quad2 =
      pass2->quad_list.AllocateAndConstruct<viz::SolidColorDrawQuad>();
  quad2->SetNew(shared_quad_state2, /*rect=*/rect2,
                /*visible_rect=*/rect2, SK_ColorBLACK,
                /*force_anti_aliasing_off=*/false);
  pass_list.push_back(move(pass2));
  SendRenderPassList(&pass_list, /*hit_test_data_changed=*/false);
  task_runner_->RunUntilIdle();

  EXPECT_TRUE(
      viz::HitTestRegionList::IsEqual(hit_test_region_list, GetHitTestData()));

  // Different submission.
  const viz::SurfaceId child_surface_id(
      viz::FrameSinkId(1, 1),
      viz::LocalSurfaceId(2, base::UnguessableToken::Create()));
  auto pass3_0 = viz::RenderPass::Create();
  pass3_0->output_rect = display_rect_;
  pass3_0->id = 3;
  auto* shared_quad_state3_0 = pass3_0->CreateAndAppendSharedQuadState();
  gfx::Rect rect3_0(display_rect_);
  gfx::Transform transform3_0;
  transform3_0.Translate(-200, -100);
  shared_quad_state3_0->SetAll(
      transform3_0, /*quad_layer_rect=*/rect3_0,
      /*visible_quad_layer_rect=*/rect3_0,
      /*rounded_corner_bounds=*/gfx::RRectF(), /*clip_rect=*/rect3_0,
      /*is_clipped=*/false, /*are_contents_opaque=*/false,
      /*opacity=*/0.5f, SkBlendMode::kSrcOver, /*sorting_context_id=*/0);
  auto* quad3_0 =
      pass3_0->quad_list.AllocateAndConstruct<viz::SurfaceDrawQuad>();
  quad3_0->SetNew(shared_quad_state3_0, /*rect=*/rect3_0,
                  /*visible_rect=*/rect3_0,
                  viz::SurfaceRange(base::nullopt, child_surface_id),
                  SK_ColorBLACK,
                  /*stretch_content_to_fill_bounds=*/false,
                  /*ignores_input_event=*/false);
  pass_list.push_back(std::move(pass3_0));

  auto pass3_1 = viz::RenderPass::Create();
  pass3_1->output_rect = display_rect_;
  pass3_1->id = 4;
  auto* shared_quad_state3_1 = pass3_1->CreateAndAppendSharedQuadState();
  gfx::Rect rect3_1(display_rect_);
  shared_quad_state3_1->SetAll(
      gfx::Transform(), /*quad_layer_rect=*/rect3_1,
      /*visible_quad_layer_rect=*/rect3_1,
      /*rounded_corner_bounds=*/gfx::RRectF(), /*clip_rect=*/rect3_1,
      /*is_clipped=*/false, /*are_contents_opaque=*/false,
      /*opacity=*/0.5f, SkBlendMode::kSrcOver, /*sorting_context_id=*/0);
  auto* quad3_1 =
      pass3_1->quad_list.AllocateAndConstruct<viz::SolidColorDrawQuad>();
  quad3_1->SetNew(shared_quad_state3_1, /*rect=*/rect3_1,
                  /*visible_rect=*/rect3_1, SK_ColorBLACK,
                  /*force_anti_aliasing_off=*/false);
  pass_list.push_back(std::move(pass3_1));

  auto pass3_root = viz::RenderPass::Create();
  pass3_root->output_rect = display_rect_;
  pass3_root->id = 5;
  auto* shared_quad_state3_root = pass3_root->CreateAndAppendSharedQuadState();
  gfx::Rect rect3_root(display_rect_);
  shared_quad_state3_root->SetAll(
      gfx::Transform(), /*quad_layer_rect=*/rect3_root,
      /*visible_quad_layer_rect=*/rect3_root,
      /*rounded_corner_bounds=*/gfx::RRectF(), /*clip_rect=*/rect3_root,
      /*is_clipped=*/false, /*are_contents_opaque=*/false,
      /*opacity=*/0.5f, SkBlendMode::kSrcOver, /*sorting_context_id=*/0);
  auto* quad3_root_1 =
      pass3_root->quad_list.AllocateAndConstruct<viz::RenderPassDrawQuad>();
  quad3_root_1->SetNew(shared_quad_state3_root, /*rect=*/rect3_root,
                       /*visible_rect=*/rect3_root, /*render_pass_id=*/3,
                       /*mask_resource_id=*/0, gfx::RectF(), gfx::Size(),
                       gfx::Vector2dF(1, 1), gfx::PointF(), gfx::RectF(), false,
                       1.0f);
  auto* quad3_root_2 =
      pass3_root->quad_list.AllocateAndConstruct<viz::RenderPassDrawQuad>();
  quad3_root_2->SetNew(shared_quad_state3_root, /*rect=*/rect3_root,
                       /*visible_rect=*/rect3_root, /*render_pass_id=*/4,
                       /*mask_resource_id=*/0, gfx::RectF(), gfx::Size(),
                       gfx::Vector2dF(1, 1), gfx::PointF(), gfx::RectF(), false,
                       1.0f);
  pass_list.push_back(std::move(pass3_root));

  SendRenderPassList(&pass_list, /*hit_test_data_changed=*/false);
  task_runner_->RunUntilIdle();
  EXPECT_FALSE(
      viz::HitTestRegionList::IsEqual(hit_test_region_list, GetHitTestData()));
}

TEST_F(AsyncLayerTreeFrameSinkSimpleTest,
       HitTestRegionListDuplicateChangedFlip) {
  viz::RenderPassList pass_list;

  // Initial submission.
  auto pass1 = viz::RenderPass::Create();
  pass1->id = 1;
  pass1->output_rect = display_rect_;
  auto* shared_quad_state1 = pass1->CreateAndAppendSharedQuadState();
  gfx::Rect rect1(display_rect_);
  shared_quad_state1->SetAll(
      gfx::Transform(), /*quad_layer_rect=*/rect1,
      /*visible_quad_layer_rect=*/rect1,
      /*rounded_corner_bounds=*/gfx::RRectF(), /*clip_rect=*/rect1,
      /*is_clipped=*/false, /*are_contents_opaque=*/false,
      /*opacity=*/0.5f, SkBlendMode::kSrcOver, /*sorting_context_id=*/0);
  auto* quad1 =
      pass1->quad_list.AllocateAndConstruct<viz::SolidColorDrawQuad>();
  quad1->SetNew(shared_quad_state1, /*rect=*/rect1,
                /*visible_rect=*/rect1, SK_ColorBLACK,
                /*force_anti_aliasing_off=*/false);
  pass_list.push_back(move(pass1));
  SendRenderPassList(&pass_list, /*hit_test_data_changed=*/false);
  task_runner_->RunUntilIdle();
  viz::HitTestRegionList hit_test_region_list = GetHitTestData();

  // Different submission with |hit_test_data_changed| set to true.
  const viz::SurfaceId child_surface_id(
      viz::FrameSinkId(1, 1),
      viz::LocalSurfaceId(2, base::UnguessableToken::Create()));
  auto pass2_0 = viz::RenderPass::Create();
  pass2_0->output_rect = display_rect_;
  pass2_0->id = 2;
  auto* shared_quad_state2_0 = pass2_0->CreateAndAppendSharedQuadState();
  gfx::Rect rect2_0(display_rect_);
  gfx::Transform transform2_0;
  transform2_0.Translate(-200, -100);
  shared_quad_state2_0->SetAll(
      transform2_0, /*quad_layer_rect=*/rect2_0,
      /*visible_quad_layer_rect=*/rect2_0,
      /*rounded_corner_bounds=*/gfx::RRectF(), /*clip_rect=*/rect2_0,
      /*is_clipped=*/false, /*are_contents_opaque=*/false,
      /*opacity=*/0.5f, SkBlendMode::kSrcOver, /*sorting_context_id=*/0);
  auto* quad2_0 =
      pass2_0->quad_list.AllocateAndConstruct<viz::SurfaceDrawQuad>();
  quad2_0->SetNew(shared_quad_state2_0, /*rect=*/rect2_0,
                  /*visible_rect=*/rect2_0,
                  viz::SurfaceRange(base::nullopt, child_surface_id),
                  SK_ColorBLACK,
                  /*stretch_content_to_fill_bounds=*/false,
                  /*ignores_input_event=*/false);
  pass_list.push_back(std::move(pass2_0));

  auto pass2_1 = viz::RenderPass::Create();
  pass2_1->output_rect = display_rect_;
  pass2_1->id = 3;
  auto* shared_quad_state2_1 = pass2_1->CreateAndAppendSharedQuadState();
  gfx::Rect rect2_1(display_rect_);
  shared_quad_state2_1->SetAll(
      gfx::Transform(), /*quad_layer_rect=*/rect2_1,
      /*visible_quad_layer_rect=*/rect2_1,
      /*rounded_corner_bounds=*/gfx::RRectF(), /*clip_rect=*/rect2_1,
      /*is_clipped=*/false, /*are_contents_opaque=*/false,
      /*opacity=*/0.5f, SkBlendMode::kSrcOver, /*sorting_context_id=*/0);
  auto* quad2_1 =
      pass2_1->quad_list.AllocateAndConstruct<viz::SolidColorDrawQuad>();
  quad2_1->SetNew(shared_quad_state2_1, /*rect=*/rect2_1,
                  /*visible_rect=*/rect2_1, SK_ColorBLACK,
                  /*force_anti_aliasing_off=*/false);
  pass_list.push_back(std::move(pass2_1));

  auto pass2_root = viz::RenderPass::Create();
  pass2_root->output_rect = display_rect_;
  pass2_root->id = 4;
  auto* shared_quad_state2_root = pass2_root->CreateAndAppendSharedQuadState();
  gfx::Rect rect2_root(display_rect_);
  shared_quad_state2_root->SetAll(
      gfx::Transform(), /*quad_layer_rect=*/rect2_root,
      /*visible_quad_layer_rect=*/rect2_root,
      /*rounded_corner_bounds=*/gfx::RRectF(), /*clip_rect=*/rect2_root,
      /*is_clipped=*/false, /*are_contents_opaque=*/false,
      /*opacity=*/0.5f, SkBlendMode::kSrcOver, /*sorting_context_id=*/0);
  auto* quad2_root_1 =
      pass2_root->quad_list.AllocateAndConstruct<viz::RenderPassDrawQuad>();
  quad2_root_1->SetNew(shared_quad_state2_root, /*rect=*/rect2_root,
                       /*visible_rect=*/rect2_root, /*render_pass_id=*/2,
                       /*mask_resource_id=*/0, gfx::RectF(), gfx::Size(),
                       gfx::Vector2dF(1, 1), gfx::PointF(), gfx::RectF(), false,
                       1.0f);
  auto* quad2_root_2 =
      pass2_root->quad_list.AllocateAndConstruct<viz::RenderPassDrawQuad>();
  quad2_root_2->SetNew(shared_quad_state2_root, /*rect=*/rect2_root,
                       /*visible_rect=*/rect2_root, /*render_pass_id=*/3,
                       /*mask_resource_id=*/0, gfx::RectF(), gfx::Size(),
                       gfx::Vector2dF(1, 1), gfx::PointF(), gfx::RectF(), false,
                       1.0f);
  pass_list.push_back(std::move(pass2_root));

  SendRenderPassList(&pass_list, /*hit_test_data_changed=*/true);
  task_runner_->RunUntilIdle();

  EXPECT_FALSE(
      viz::HitTestRegionList::IsEqual(hit_test_region_list, GetHitTestData()));
  hit_test_region_list = GetHitTestData();

  // Identical submission with |hit_test_data_changed| set back to false. We
  // expect the hit-data to still have been sent.
  auto pass3 = viz::RenderPass::Create();
  pass3->id = 4;
  pass3->output_rect = display_rect_;
  auto* shared_quad_state3 = pass3->CreateAndAppendSharedQuadState();
  gfx::Rect rect3(display_rect_);
  shared_quad_state3->SetAll(
      gfx::Transform(), /*quad_layer_rect=*/rect3,
      /*visible_quad_layer_rect=*/rect3,
      /*rounded_corner_bounds=*/gfx::RRectF(), /*clip_rect=*/rect3,
      /*is_clipped=*/false, /*are_contents_opaque=*/false,
      /*opacity=*/0.5f, SkBlendMode::kSrcOver, /*sorting_context_id=*/0);
  auto* quad3 =
      pass3->quad_list.AllocateAndConstruct<viz::SolidColorDrawQuad>();
  quad3->SetNew(shared_quad_state3, /*rect=*/rect3,
                /*visible_rect=*/rect3, SK_ColorBLACK,
                /*force_anti_aliasing_off=*/false);
  pass_list.push_back(move(pass3));
  SendRenderPassList(&pass_list, /*hit_test_data_changed=*/false);
  task_runner_->RunUntilIdle();

  EXPECT_FALSE(
      viz::HitTestRegionList::IsEqual(hit_test_region_list, GetHitTestData()));
}

}  // namespace mojo_embedder
}  // namespace cc
