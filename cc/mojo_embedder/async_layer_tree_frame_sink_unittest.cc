// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/mojo_embedder/async_layer_tree_frame_sink.h"

#include <cstdint>
#include <memory>
#include <utility>

#include "base/feature_list.h"
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
#include "components/viz/common/features.h"
#include "components/viz/common/frame_timing_details.h"
#include "components/viz/common/frame_timing_details_map.h"
#include "components/viz/common/performance_hint_utils.h"
#include "components/viz/common/quads/compositor_render_pass_draw_quad.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/common/quads/surface_draw_quad.h"
#include "components/viz/common/surfaces/surface_range.h"
#include "components/viz/test/begin_frame_args_test.h"
#include "components/viz/test/compositor_frame_helpers.h"
#include "components/viz/test/fake_delay_based_time_source.h"
#include "components/viz/test/test_context_provider.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/viz/public/mojom/compositing/compositor_frame_sink.mojom.h"
#include "services/viz/public/mojom/compositing/layer_context.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
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

// Used to track the client begin frame when connect/disconnect to Viz.
class BeginFrameTrackingLayerTreeFrameSinkClient
    : public FakeLayerTreeFrameSinkClient,
      public viz::BeginFrameObserverBase {
 public:
  BeginFrameTrackingLayerTreeFrameSinkClient() {}
  BeginFrameTrackingLayerTreeFrameSinkClient(
      const ThreadTrackingLayerTreeFrameSinkClient&) = delete;
  ~BeginFrameTrackingLayerTreeFrameSinkClient() override = default;

  BeginFrameTrackingLayerTreeFrameSinkClient& operator=(
      const BeginFrameTrackingLayerTreeFrameSinkClient&) = delete;

  void SetBeginFrameSource(viz::BeginFrameSource* source) override {
    if (begin_frame_source() && observing_begin_frame_) {
      begin_frame_source()->RemoveObserver(this);
    }
    FakeLayerTreeFrameSinkClient::SetBeginFrameSource(source);
    if (begin_frame_source() && observing_begin_frame_) {
      begin_frame_source()->AddObserver(this);
    }
  }

  void SetObservingBeginFrame(bool observing) {
    if (observing_begin_frame_ == observing) {
      return;
    }
    observing_begin_frame_ = observing;
    if (begin_frame_source()) {
      if (observing_begin_frame_) {
        begin_frame_source()->AddObserver(this);
      } else {
        begin_frame_source()->RemoveObserver(this);
      }
    }
  }

  void OnBeginFrameSourcePausedChanged(bool paused) override {}

  void DidPresentCompositorFrame(
      uint32_t frame_token,
      const viz::FrameTimingDetails& details) override {
    last_frame_token_ = frame_token;
  }

  uint32_t begin_frame_count() const { return begin_frame_count_; }
  uint32_t last_frame_token() const { return last_frame_token_; }

 private:
  bool OnBeginFrameDerivedImpl(const viz::BeginFrameArgs& args) override {
    begin_frame_count_++;
    return true;
  }
  uint32_t begin_frame_count_ = 0;
  uint32_t last_frame_token_ = 0;
  bool observing_begin_frame_ = true;
};

// A CompositorFrameSink for inspecting.
class MockCompositorFrameSink : public viz::mojom::CompositorFrameSink {
 public:
  MockCompositorFrameSink(
      mojo::PendingReceiver<viz::mojom::CompositorFrameSink> receiver,
      scoped_refptr<base::SequencedTaskRunner> task_runner) {
    receiver_.Bind(std::move(receiver), task_runner);
  }

  MockCompositorFrameSink(const MockCompositorFrameSink&) = delete;
  MockCompositorFrameSink& operator=(const MockCompositorFrameSink&) = delete;

  // viz::mojom::blink::CompositorFrameSink implementation
  MOCK_METHOD1(SetParams, void(viz::mojom::CompositorFrameSinkParamsPtr));
  MOCK_METHOD1(SetNeedsBeginFrame, void(bool));
  void SubmitCompositorFrame(
      const viz::LocalSurfaceId&,
      viz::CompositorFrame frame,
      std::optional<viz::HitTestRegionList> hit_test_region_list,
      uint64_t) override {}
  MOCK_METHOD1(DidNotProduceFrame, void(const viz::BeginFrameAck&));
  MOCK_METHOD1(SetPreferredFrameInterval, void(base::TimeDelta));
  MOCK_METHOD2(BindLayerContext,
               void(viz::mojom::PendingLayerContextPtr,
                    viz::mojom::LayerContextSettingsPtr));
  MOCK_METHOD1(SetThreads, void(const std::vector<viz::Thread>&));
  MOCK_METHOD0(NotifyNewLocalSurfaceIdExpectedWhilePaused, void(void));

 private:
  mojo::Receiver<viz::mojom::CompositorFrameSink> receiver_{this};
};

}  // namespace

// Boilerplate code for simple AsyncLayerTreeFrameSink. Friend of
// AsyncLayerTreeFrameSink.
class AsyncLayerTreeFrameSinkSimpleTest : public testing::Test {
 public:
  AsyncLayerTreeFrameSinkSimpleTest()
      : task_runner_(base::MakeRefCounted<base::TestMockTimeTaskRunner>(
            base::TestMockTimeTaskRunner::Type::kStandalone)),
        display_rect_(1, 1) {
    client_to_bind_ = &layer_tree_frame_sink_client_;
  }

  void SetUp() override {
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
    layer_tree_frame_sink_->BindToClient(client_to_bind_);

    client_remote_.Bind(std::move(client), task_runner_);
    mock_compositor_frame_sink_ = std::make_unique<MockCompositorFrameSink>(
        std::move(sink_receiver), task_runner_);
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

  const viz::HitTestRegionList& GetHitTestData() const {
    return layer_tree_frame_sink_->get_last_hit_test_data_for_testing();
  }

  AsyncLayerTreeFrameSink::InitParams init_params_;

  scoped_refptr<base::TestMockTimeTaskRunner> task_runner_;
  gfx::Rect display_rect_;
  std::unique_ptr<AsyncLayerTreeFrameSink> layer_tree_frame_sink_;
  FakeLayerTreeFrameSinkClient layer_tree_frame_sink_client_;
  raw_ptr<LayerTreeFrameSinkClient> client_to_bind_;
  mojo::Remote<viz::mojom::CompositorFrameSinkClient> client_remote_;
  std::unique_ptr<MockCompositorFrameSink> mock_compositor_frame_sink_;
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

// Boilerplate code for begin frame test of AsyncLayerTreeFrameSink.
class AsyncLayerTreeFrameSinkBeginFrameTest
    : public AsyncLayerTreeFrameSinkSimpleTest {
 public:
  AsyncLayerTreeFrameSinkBeginFrameTest() {
    client_to_bind_ = &frame_tracking_client_;
  }

  void SetUp() override {
    init_params_.num_did_not_produce_frame_before_internal_begin_frame_source =
        1;
    init_params_.auto_needs_begin_frame = true;
    AsyncLayerTreeFrameSinkSimpleTest::SetUp();

    std::unique_ptr<viz::DelayBasedTimeSource> fake_source =
        std::make_unique<viz::FakeDelayBasedTimeSource>(
            task_runner_->GetMockTickClock(), task_runner_.get());
    layer_tree_frame_sink_->SetTimeSourceOfInternalBeginFrameForTesting(
        std::move(fake_source));
  }

  uint64_t last_received_begin_frame_sequence_number() const {
    return frame_tracking_client_.LastUsedBeginFrameArgs()
        .frame_id.sequence_number;
  }

  void SendCompositorFrame() {
    viz::CompositorRenderPassList pass_list;
    auto pass = viz::CompositorRenderPass::Create();
    pass->id = viz::CompositorRenderPassId{1};
    pass->output_rect = gfx::Rect(1, 1);
    pass_list.push_back(std::move(pass));
    viz::CompositorFrame frame = viz::CompositorFrameBuilder()
                                     .SetRenderPassList(std::move(pass_list))
                                     .Build();
    layer_tree_frame_sink_->SubmitCompositorFrame(std::move(frame), false);
  }

  BeginFrameTrackingLayerTreeFrameSinkClient frame_tracking_client_;
};

TEST_F(AsyncLayerTreeFrameSinkBeginFrameTest,
       OnBeginFrameFromVizWhenDisconnectForInternalBeginFrameSource) {
  // At the beginning, we have 1 internal begin frame and connect viz.
  // Then we have 4 begin frames from Viz(sequcence number 10, 11, 12, 13).
  // We will disconnect Viz after 1st frame and reconnect before the 3rd.
  // Vsync 0: internal frame (submit to connect)
  // Vsync 1: 1st viz frame (receive and didNotProduceFrame * 2 to disconnect)
  // Vsync 2: 2rd viz frame (skip when using internal frame)
  // Vsync 2: internal frame (submit to connect)
  // Vsync 2: 3rd viz frame (drop within last vsync interval)
  // Vsync 3: 4th viz frame (receive)
  base::TimeTicks start_time = task_runner_->NowTicks();
  viz::BeginFrameArgs args1 = viz::CreateBeginFrameArgsForTesting(
      BEGINFRAME_FROM_HERE, 0, 10,
      start_time + viz::BeginFrameArgs::DefaultInterval());
  viz::BeginFrameArgs args2 = viz::CreateBeginFrameArgsForTesting(
      BEGINFRAME_FROM_HERE, 0, 11,
      start_time + viz::BeginFrameArgs::DefaultInterval() * 2);
  viz::BeginFrameArgs args3 = viz::CreateBeginFrameArgsForTesting(
      BEGINFRAME_FROM_HERE, 0, 12,
      start_time + viz::BeginFrameArgs::DefaultInterval() * 2);
  viz::BeginFrameArgs args4 = viz::CreateBeginFrameArgsForTesting(
      BEGINFRAME_FROM_HERE, 0, 13,
      start_time + viz::BeginFrameArgs::DefaultInterval() * 3);

  viz::FrameTimingDetailsMap empty_details;
  viz::FrameTimingDetailsMap test_details;
  uint32_t frame_token = 1;
  test_details.emplace(frame_token, viz::FrameTimingDetails());
  uint64_t internal_sequence = 1;

  // Start with 1 internal begin frame.
  task_runner_->RunUntilIdle();
  EXPECT_TRUE(
      layer_tree_frame_sink_->use_internal_begin_frame_source_for_testing());
  if (!base::FeatureList::IsEnabled(features::kNoLateBeginFrames)) {
    EXPECT_EQ(internal_sequence, last_received_begin_frame_sequence_number());
  }

  SendCompositorFrame();
  task_runner_->FastForwardBy(viz::BeginFrameArgs::DefaultInterval());
  internal_sequence++;

  // Connected after first compositor frame.
  EXPECT_FALSE(
      layer_tree_frame_sink_->use_internal_begin_frame_source_for_testing());
  client_remote_->OnBeginFrame(args1, empty_details,
                               std::vector<viz::ReturnedResource>());
  task_runner_->RunUntilIdle();
  // Client should receive 1st viz begin frame.
  EXPECT_EQ(args1.frame_id.sequence_number,
            last_received_begin_frame_sequence_number());

  // Will disconnect after 2 DidNotProduceFrame, since here init with
  // num_did_not_produce_frame_before_internal_begin_frame_source = 1.
  layer_tree_frame_sink_->DidNotProduceFrame(viz::BeginFrameAck(args1, false),
                                             FrameSkippedReason::kNoDamage);
  layer_tree_frame_sink_->DidNotProduceFrame(viz::BeginFrameAck(args1, false),
                                             FrameSkippedReason::kNoDamage);
  task_runner_->RunUntilIdle();
  EXPECT_TRUE(
      layer_tree_frame_sink_->use_internal_begin_frame_source_for_testing());

  client_remote_->OnBeginFrame(args2, test_details,
                               std::vector<viz::ReturnedResource>());
  task_runner_->RunUntilIdle();
  // Proceed timing details.
  EXPECT_EQ(frame_tracking_client_.last_frame_token(), frame_token);
  // Client won't receive 2nd begin frame.
  EXPECT_NE(args2.frame_id.sequence_number,
            last_received_begin_frame_sequence_number());

  // Internal begin frame source should generate begin frame after disconnect.
  task_runner_->FastForwardBy(viz::BeginFrameArgs::DefaultInterval());
  internal_sequence++;
  // Client should receive begin frame from internal begin frame source.
  EXPECT_EQ(internal_sequence, last_received_begin_frame_sequence_number());

  // Connect after SubmitCompositorFrame before 3rd viz begin frame.
  SendCompositorFrame();
  task_runner_->RunUntilIdle();
  EXPECT_FALSE(
      layer_tree_frame_sink_->use_internal_begin_frame_source_for_testing());

  // Should drop 3rd begin frame within last internal begin frame's interval.
  EXPECT_CALL(*mock_compositor_frame_sink_,
              DidNotProduceFrame(viz::BeginFrameAck(args3, false)));
  client_remote_->OnBeginFrame(args3, empty_details,
                               std::vector<viz::ReturnedResource>());
  task_runner_->RunUntilIdle();
  EXPECT_EQ(
      internal_sequence,
      frame_tracking_client_.LastUsedBeginFrameArgs().frame_id.sequence_number);

  // 4th viz begin frame.
  task_runner_->FastForwardBy(viz::BeginFrameArgs::DefaultInterval());
  client_remote_->OnBeginFrame(args4, empty_details,
                               std::vector<viz::ReturnedResource>());
  task_runner_->RunUntilIdle();
  // Client should receive 4th begin frame.
  EXPECT_EQ(args4.frame_id.sequence_number,
            last_received_begin_frame_sequence_number());
  // Receive 2 from internal and 2 from viz.
  EXPECT_EQ(
      base::FeatureList::IsEnabled(features::kNoLateBeginFrames) ? 3u : 4u,
      frame_tracking_client_.begin_frame_count());
  layer_tree_frame_sink_->DetachFromClient();
}

class AsyncLayerTreeFrameSinkManualBeginFrameTest
    : public AsyncLayerTreeFrameSinkSimpleTest {
 public:
  AsyncLayerTreeFrameSinkManualBeginFrameTest() {
    client_to_bind_ = &frame_tracking_client_;
  }

  void SetUp() override {
    init_params_.manual_begin_frame = true;
    init_params_.auto_needs_begin_frame = true;
    AsyncLayerTreeFrameSinkSimpleTest::SetUp();
  }

  void TearDown() override {
    layer_tree_frame_sink_->DetachFromClient();
    AsyncLayerTreeFrameSinkSimpleTest::TearDown();
  }

  BeginFrameTrackingLayerTreeFrameSinkClient frame_tracking_client_;
};

TEST_F(AsyncLayerTreeFrameSinkManualBeginFrameTest, SendsManualBeginFrame) {
  // A manual begin frame should be sent when the client is bound.
  task_runner_->RunUntilIdle();
  EXPECT_EQ(1u, frame_tracking_client_.begin_frame_count());
  EXPECT_EQ(viz::BeginFrameArgs::kManualSourceId,
            frame_tracking_client_.LastUsedBeginFrameArgs().frame_id.source_id);
  EXPECT_EQ(
      1u,
      frame_tracking_client_.LastUsedBeginFrameArgs().frame_id.sequence_number);

  // It shouldn't send another one if needs begin frames doesn't change.
  task_runner_->RunUntilIdle();
  EXPECT_EQ(1u, frame_tracking_client_.begin_frame_count());

  // It should send another one if we stop needing begin frames and then start
  // again.
  frame_tracking_client_.SetObservingBeginFrame(false);
  task_runner_->RunUntilIdle();
  EXPECT_EQ(1u, frame_tracking_client_.begin_frame_count());

  frame_tracking_client_.SetObservingBeginFrame(true);
  task_runner_->RunUntilIdle();
  EXPECT_EQ(2u, frame_tracking_client_.begin_frame_count());
  EXPECT_EQ(viz::BeginFrameArgs::kManualSourceId,
            frame_tracking_client_.LastUsedBeginFrameArgs().frame_id.source_id);
  EXPECT_EQ(
      2u,
      frame_tracking_client_.LastUsedBeginFrameArgs().frame_id.sequence_number);
}

TEST_F(AsyncLayerTreeFrameSinkManualBeginFrameTest,
       ReceivesVizBeginFrameAfterManual) {
  // A manual begin frame should be sent when the client is bound.
  task_runner_->RunUntilIdle();
  EXPECT_EQ(1u, frame_tracking_client_.begin_frame_count());
  EXPECT_EQ(viz::BeginFrameArgs::kManualSourceId,
            frame_tracking_client_.LastUsedBeginFrameArgs().frame_id.source_id);
  EXPECT_EQ(
      1u,
      frame_tracking_client_.LastUsedBeginFrameArgs().frame_id.sequence_number);

  // Now send a real begin frame from viz.
  viz::BeginFrameArgs args = viz::CreateBeginFrameArgsForTesting(
      BEGINFRAME_FROM_HERE, 0, 10,
      frame_tracking_client_.LastUsedBeginFrameArgs().frame_time +
          base::Milliseconds(16));

  client_remote_->OnBeginFrame(args, {}, {});
  task_runner_->RunUntilIdle();

  // The client should have received the viz begin frame.
  EXPECT_EQ(2u, frame_tracking_client_.begin_frame_count());
  EXPECT_NE(viz::BeginFrameArgs::kManualSourceId,
            frame_tracking_client_.LastUsedBeginFrameArgs().frame_id.source_id);
  EXPECT_EQ(
      10u,
      frame_tracking_client_.LastUsedBeginFrameArgs().frame_id.sequence_number);
}

}  // namespace mojo_embedder
}  // namespace cc
