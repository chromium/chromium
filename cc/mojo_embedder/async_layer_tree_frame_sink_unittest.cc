// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/mojo_embedder/async_layer_tree_frame_sink.h"

#include <cstdint>
#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/threading/thread.h"
#include "cc/base/features.h"
#include "cc/test/fake_layer_tree_frame_sink_client.h"
#include "components/viz/common/frame_timing_details.h"
#include "components/viz/common/frame_timing_details_map.h"
#include "components/viz/common/quads/compositor_render_pass_draw_quad.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/common/quads/surface_draw_quad.h"
#include "components/viz/common/surfaces/surface_range.h"
#include "components/viz/test/begin_frame_args_test.h"
#include "components/viz/test/compositor_frame_helpers.h"
#include "components/viz/test/test_context_provider.h"
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
  raw_ptr<base::PlatformThreadId> called_thread_id_;
  raw_ptr<base::RunLoop> run_loop_;
};

TEST(AsyncLayerTreeFrameSinkTest,
     DidLoseLayerTreeFrameSinkCalledOnConnectionError) {
  base::Thread bg_thread("BG Thread");
  bg_thread.Start();

  scoped_refptr<viz::TestContextProvider> provider =
      viz::TestContextProvider::CreateRaster();

  mojo::PendingRemote<viz::mojom::CompositorFrameSink> sink_remote;
  mojo::PendingReceiver<viz::mojom::CompositorFrameSink> sink_receiver =
      sink_remote.InitWithNewPipeAndPassReceiver();
  mojo::PendingRemote<viz::mojom::CompositorFrameSinkClient> client;

  AsyncLayerTreeFrameSink::InitParams init_params;
  init_params.compositor_task_runner = bg_thread.task_runner();
  init_params.pipes.compositor_frame_sink_remote = std::move(sink_remote);
  init_params.pipes.client_receiver = client.InitWithNewPipeAndPassReceiver();
  auto layer_tree_frame_sink = std::make_unique<AsyncLayerTreeFrameSink>(
      std::move(provider), nullptr, /*shared_image_interface=*/nullptr,
      &init_params);

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
  // to complete as mojo::Receiver error callbacks are processed asynchronously.
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

// Mocks DidPresentCompositorFrame class in order to test at what point
// in the frame lifecycle the method gets called.
class MockFakeLayerTreeFrameSinkClient : public FakeLayerTreeFrameSinkClient {
 public:
  MOCK_METHOD2(DidPresentCompositorFrame,
               void(uint32_t frame_token,
                    const viz::FrameTimingDetails& details));
};

// Boilerplate code for simple AsyncLayerTreeFrameSink. Friend of
// AsyncLayerTreeFrameSink.
class AsyncLayerTreeFrameSinkSimpleTest : public testing::TestWithParam<bool> {
 public:
  AsyncLayerTreeFrameSinkSimpleTest()
      : task_runner_(base::MakeRefCounted<base::TestMockTimeTaskRunner>(
            base::TestMockTimeTaskRunner::Type::kStandalone)),
        display_rect_(1, 1),
        layer_tree_frame_sink_client_(MockFakeLayerTreeFrameSinkClient()) {
    auto context_provider = viz::TestContextProvider::CreateRaster();

    mojo::PendingRemote<viz::mojom::CompositorFrameSink> sink_remote;
    mojo::PendingReceiver<viz::mojom::CompositorFrameSink> sink_receiver =
        sink_remote.InitWithNewPipeAndPassReceiver();
    mojo::PendingRemote<viz::mojom::CompositorFrameSinkClient> client;

    init_params_.compositor_task_runner = task_runner_;
    init_params_.pipes.compositor_frame_sink_remote = std::move(sink_remote);
    init_params_.pipes.client_receiver =
        client.InitWithNewPipeAndPassReceiver();

    layer_tree_frame_sink_ = std::make_unique<AsyncLayerTreeFrameSink>(
        std::move(context_provider), nullptr,
        /*shared_image_interface=*/nullptr, &init_params_);

    viz::LocalSurfaceId local_surface_id(1, base::UnguessableToken::Create());
    layer_tree_frame_sink_->SetLocalSurfaceId(local_surface_id);
    layer_tree_frame_sink_->BindToClient(&layer_tree_frame_sink_client_);
  }

  void SendRenderPassList(viz::CompositorRenderPassList* pass_list,
                          bool hit_test_data_changed) {
    auto frame = viz::CompositorFrameBuilder()
                     .SetRenderPassList(std::move(*pass_list))
                     .Build();
    pass_list->clear();
    layer_tree_frame_sink_->SubmitCompositorFrame(std::move(frame),
                                                  hit_test_data_changed);
  }

  void OnBeginFrame(const viz::BeginFrameArgs& args,
                    const viz::FrameTimingDetailsMap& timing_details,
                    bool frame_ack,
                    std::vector<viz::ReturnedResource> resources) {
    layer_tree_frame_sink_->OnBeginFrame(args, timing_details, frame_ack,
                                         std::move(resources));
  }

  void DidNotProduceFrame(const viz::BeginFrameAck& ack,
                          FrameSkippedReason reason) {
    layer_tree_frame_sink_->DidNotProduceFrame(ack, reason);
  }

  void SubmitCompositorFrame(viz::CompositorFrame frame,
                             bool hit_test_data_changed) {
    layer_tree_frame_sink_->SubmitCompositorFrame(std::move(frame),
                                                  hit_test_data_changed);
  }

  void SetNeedsBeginFrame() {
    layer_tree_frame_sink_->OnNeedsBeginFrames(true);
  }

  const viz::HitTestRegionList& GetHitTestData() const {
    return layer_tree_frame_sink_->get_last_hit_test_data_for_testing();
  }

  AsyncLayerTreeFrameSink::InitParams init_params_;

  scoped_refptr<base::TestMockTimeTaskRunner> task_runner_;
  gfx::Rect display_rect_;
  std::unique_ptr<AsyncLayerTreeFrameSink> layer_tree_frame_sink_;
  MockFakeLayerTreeFrameSinkClient layer_tree_frame_sink_client_;
};

TEST_F(AsyncLayerTreeFrameSinkSimpleTest, HitTestRegionListEmpty) {
  viz::CompositorRenderPassList pass_list;
  auto pass = viz::CompositorRenderPass::Create();
  pass->id = viz::CompositorRenderPassId{1};
  pass->output_rect = display_rect_;
  pass_list.push_back(std::move(pass));

  SendRenderPassList(&pass_list, /*hit_test_data_changed=*/false);
  task_runner_->RunUntilIdle();

  EXPECT_TRUE(viz::HitTestRegionList::IsEqual(viz::HitTestRegionList(),
                                              GetHitTestData()));
}

TEST_F(AsyncLayerTreeFrameSinkSimpleTest, HitTestRegionListDuplicate) {
  viz::CompositorRenderPassList pass_list;
  // Initial submission.
  auto pass1 = viz::CompositorRenderPass::Create();
  pass1->id = viz::CompositorRenderPassId{1};
  pass1->output_rect = display_rect_;
  pass_list.push_back(std::move(pass1));

  viz::HitTestRegionList region_list1;
  region_list1.flags = viz::HitTestRegionFlags::kHitTestMine;
  region_list1.bounds.SetRect(0, 0, 1024, 768);
  layer_tree_frame_sink_client_.set_hit_test_region_list(region_list1);

  SendRenderPassList(&pass_list, /*hit_test_data_changed=*/false);
  task_runner_->RunUntilIdle();
  const viz::HitTestRegionList hit_test_region_list = GetHitTestData();

  // Identical submission.
  auto pass2 = viz::CompositorRenderPass::Create();
  pass2->id = viz::CompositorRenderPassId{2};
  pass2->output_rect = display_rect_;
  pass_list.push_back(std::move(pass2));

  SendRenderPassList(&pass_list, /*hit_test_data_changed=*/false);
  task_runner_->RunUntilIdle();

  EXPECT_TRUE(
      viz::HitTestRegionList::IsEqual(hit_test_region_list, GetHitTestData()));

  // Different submission.
  auto pass3 = viz::CompositorRenderPass::Create();
  pass3->id = viz::CompositorRenderPassId{3};
  pass3->output_rect = display_rect_;
  pass_list.push_back(std::move(pass3));

  viz::HitTestRegionList region_list2;
  region_list2.flags = viz::HitTestRegionFlags::kHitTestMine;
  region_list2.bounds.SetRect(0, 0, 800, 600);
  layer_tree_frame_sink_client_.set_hit_test_region_list(region_list2);

  SendRenderPassList(&pass_list, /*hit_test_data_changed=*/false);
  task_runner_->RunUntilIdle();
  EXPECT_FALSE(
      viz::HitTestRegionList::IsEqual(hit_test_region_list, GetHitTestData()));
}

TEST_F(AsyncLayerTreeFrameSinkSimpleTest,
       HitTestRegionListDuplicateChangedFlip) {
  viz::CompositorRenderPassList pass_list;

  // Initial submission.
  auto pass1 = viz::CompositorRenderPass::Create();
  pass1->id = viz::CompositorRenderPassId{1};
  pass1->output_rect = display_rect_;
  pass_list.push_back(std::move(pass1));

  viz::HitTestRegionList region_list1;
  region_list1.flags = viz::HitTestRegionFlags::kHitTestMine;
  region_list1.bounds.SetRect(0, 0, 1024, 768);
  layer_tree_frame_sink_client_.set_hit_test_region_list(region_list1);

  SendRenderPassList(&pass_list, /*hit_test_data_changed=*/false);
  task_runner_->RunUntilIdle();
  viz::HitTestRegionList hit_test_region_list = GetHitTestData();

  // Different submission with |hit_test_data_changed| set to true.
  auto pass2 = viz::CompositorRenderPass::Create();
  pass2->id = viz::CompositorRenderPassId{2};
  pass2->output_rect = display_rect_;
  pass_list.push_back(std::move(pass2));

  viz::HitTestRegionList region_list2;
  region_list2.flags = viz::HitTestRegionFlags::kHitTestMine;
  region_list2.bounds.SetRect(0, 0, 800, 600);
  layer_tree_frame_sink_client_.set_hit_test_region_list(region_list2);

  SendRenderPassList(&pass_list, /*hit_test_data_changed=*/true);
  task_runner_->RunUntilIdle();

  EXPECT_FALSE(
      viz::HitTestRegionList::IsEqual(hit_test_region_list, GetHitTestData()));
  hit_test_region_list = GetHitTestData();

  // Different submission with |hit_test_data_changed| set back to false. We
  // expect the hit-data to still have been sent.
  auto pass3 = viz::CompositorRenderPass::Create();
  pass3->id = viz::CompositorRenderPassId{3};
  pass3->output_rect = display_rect_;
  pass_list.push_back(std::move(pass3));

  viz::HitTestRegionList region_list3;
  region_list3.flags = viz::HitTestRegionFlags::kHitTestChildSurface;
  region_list3.bounds.SetRect(0, 0, 800, 600);
  layer_tree_frame_sink_client_.set_hit_test_region_list(region_list3);

  SendRenderPassList(&pass_list, /*hit_test_data_changed=*/false);
  task_runner_->RunUntilIdle();

  EXPECT_FALSE(
      viz::HitTestRegionList::IsEqual(hit_test_region_list, GetHitTestData()));
}

class AsyncLayerTreeFrameSinkMetricsRefactorTest
    : public AsyncLayerTreeFrameSinkSimpleTest {
 public:
  AsyncLayerTreeFrameSinkMetricsRefactorTest() {
    if (GetParam()) {
      scoped_feature_list_.InitAndEnableFeature(
          features::kExportFrameTimingAfterFrameDone);
    }
  }
  ~AsyncLayerTreeFrameSinkMetricsRefactorTest() override = default;

  viz::BeginFrameArgs CreateAndDispatchNewBeginFrame() {
    viz::BeginFrameArgs args = viz::CreateBeginFrameArgsForTesting(
        BEGINFRAME_FROM_HERE, viz::BeginFrameArgs::kManualSourceId, 1,
        base::TimeTicks() + base::Milliseconds(1));
    viz::FrameTimingDetailsMap timing_details_map;
    viz::FrameTimingDetails timing_details;
    timing_details.presentation_feedback.timestamp = base::TimeTicks::Now();
    timing_details_map[++frame_token_] = timing_details;
    SetNeedsBeginFrame();
    OnBeginFrame(args, timing_details_map, false,
                 std::vector<viz::ReturnedResource>());
    return args;
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  uint32_t frame_token_ = 0;
};

INSTANTIATE_TEST_SUITE_P(AsyncLayerTreeFrameSinkRefactorTest,
                         AsyncLayerTreeFrameSinkMetricsRefactorTest,
                         testing::Bool(),
                         [](auto& param) {
                           return (param.param) ? "MetricExportOnEndFrame"
                                                : "MetricExportOnBeginFrame";
                         });

TEST_P(AsyncLayerTreeFrameSinkMetricsRefactorTest, DroppedFrameExportsMetrics) {
  // Establish that DidPresentCompositorFrame should be called exactly once.
  EXPECT_CALL(layer_tree_frame_sink_client_,
              DidPresentCompositorFrame(testing::_, testing::_))
      .Times(GetParam() ? 0 : 1);

  // Simulate an OnBeginFrame call from viz.
  viz::BeginFrameArgs args = CreateAndDispatchNewBeginFrame();
  testing::Mock::VerifyAndClearExpectations(&layer_tree_frame_sink_client_);

  // Check that that either the OnBeginFrame call or the subsequent
  // DidNotProduceFrame call has exported timing metrics to the client,
  // depending on test params.
  if (GetParam()) {
    EXPECT_CALL(layer_tree_frame_sink_client_,
                DidPresentCompositorFrame(testing::_, testing::_))
        .Times(1);
    DidNotProduceFrame(viz::BeginFrameAck(args, false),
                       FrameSkippedReason::kDrawThrottled);
    testing::Mock::VerifyAndClearExpectations(&layer_tree_frame_sink_client_);
  }
}

TEST_P(AsyncLayerTreeFrameSinkMetricsRefactorTest, SubmitFrameExportsMetrics) {
  // Establish that DidPresentCompositorFrame should be called exactly once.
  EXPECT_CALL(layer_tree_frame_sink_client_,
              DidPresentCompositorFrame(testing::_, testing::_))
      .Times(GetParam() ? 0 : 1);

  // Simulate an OnBeginFrame call from viz.
  viz::BeginFrameArgs args = CreateAndDispatchNewBeginFrame();
  testing::Mock::VerifyAndClearExpectations(&layer_tree_frame_sink_client_);

  // Check that that either the OnBeginFrame call or the subsequent
  // SubmitCompositorFrame call has exported timing metrics to the client,
  // depending on test params.
  if (GetParam()) {
    EXPECT_CALL(layer_tree_frame_sink_client_,
                DidPresentCompositorFrame(testing::_, testing::_))
        .Times(1);
    // Valid frame requires a pass list.
    viz::CompositorRenderPassList pass_list;
    auto pass = viz::CompositorRenderPass::Create();
    pass->id = viz::CompositorRenderPassId{1};
    pass->output_rect = display_rect_;
    pass_list.push_back(std::move(pass));
    SubmitCompositorFrame(viz::CompositorFrameBuilder()
                              .SetRenderPassList(std::move(pass_list))
                              .Build(),
                          false);
    testing::Mock::VerifyAndClearExpectations(&layer_tree_frame_sink_client_);
  }
}

}  // namespace mojo_embedder
}  // namespace cc
