// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/frame_sink/frame_sink_holder.h"

#include <algorithm>
#include <memory>
#include <vector>

#include "ash/frame_sink/frame_sink_holder_test_api.h"
#include "ash/frame_sink/frame_sink_host.h"
#include "ash/frame_sink/test/test_begin_frame_source.h"
#include "ash/frame_sink/test/test_layer_tree_frame_sink.h"
#include "ash/frame_sink/ui_resource_manager.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "components/viz/common/frame_sinks/begin_frame_source.h"
#include "components/viz/common/gpu/context_provider.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/common/resources/resource_id.h"
#include "components/viz/common/resources/returned_resource.h"
#include "components/viz/common/resources/transferable_resource.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace ash {
namespace {

class StubBeginFrameSource : public viz::BeginFrameSource {
 public:
  StubBeginFrameSource() : viz::BeginFrameSource(0u) {}

  void DidFinishFrame(viz::BeginFrameObserver* obs) override {}
  void AddObserver(viz::BeginFrameObserver* obs) override {}
  void RemoveObserver(viz::BeginFrameObserver* obs) override {}
  void OnGpuNoLongerBusy() override {}
};

class TestFrameFactory {
 public:
  TestFrameFactory() = default;

  TestFrameFactory(const TestFrameFactory&) = delete;
  TestFrameFactory& operator=(const TestFrameFactory&) = delete;

  ~TestFrameFactory() = default;

  std::unique_ptr<viz::CompositorFrame> CreateCompositorFrame(
      const viz::BeginFrameAck& begin_frame_ack,
      UiResourceManager& resource_manager,
      bool auto_refresh,
      const gfx::Size& last_submitted_frame_size,
      float last_submitted_frame_dsf) {
    auto frame = std::make_unique<viz::CompositorFrame>();

    frame->metadata.begin_frame_ack = begin_frame_ack;
    frame->metadata.device_scale_factor = latest_frame_dsf_;

    const viz::CompositorRenderPassId kRenderPassId{1};
    auto render_pass = viz::CompositorRenderPass::Create();
    render_pass->SetNew(kRenderPassId, gfx::Rect(latest_frame_size_),
                        gfx::Rect(), gfx::Transform());

    frame->render_pass_list.push_back(std::move(render_pass));

    for (viz::ResourceId id : latest_frame_resources_) {
      frame->resource_list.push_back(
          resource_manager.PrepareResourceForExport(id));
    }

    return frame;
  }

  void OnFirstFrameRequested() {}

  void SetFrameResources(std::vector<viz::ResourceId> frame_resource) {
    latest_frame_resources_ = std::move(frame_resource);
  }

  void SetFrameMetaData(const gfx::Size frame_size, float dsf) {
    latest_frame_size_ = frame_size;
    latest_frame_dsf_ = dsf;
  }

 private:
  std::vector<viz::ResourceId> latest_frame_resources_;
  gfx::Size latest_frame_size_;
  float latest_frame_dsf_ = 1.0;
};

MATCHER_P(IsBeginFrameAckEqual, value, "") {
  return arg.frame_id == value.frame_id && arg.trace_id == value.trace_id &&
         arg.has_damage == value.has_damage;
}

class FrameSinkHolderTest : public AshTestBase {
 public:
  FrameSinkHolderTest() = default;
  FrameSinkHolderTest(const FrameSinkHolderTest&) = delete;
  FrameSinkHolderTest& operator=(const FrameSinkHolderTest&) = delete;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();
    aura::Window* root_window =
        Shell::Get()->GetRootWindowForDisplayId(GetPrimaryDisplay().id());
    auto host_window = std::make_unique<aura::Window>(/*delegate=*/nullptr);
    host_window_ = host_window.release();
    host_window_->Init(ui::LayerType::LAYER_SOLID_COLOR);
    root_window->AddChild(host_window_);

    frame_factory_ = std::make_unique<TestFrameFactory>();

    auto layer_tree_frame_sink = std::make_unique<TestLayerTreeFrameSink>();
    layer_tree_frame_sink_ = layer_tree_frame_sink.get();

    frame_sink_holder_ = std::make_unique<FrameSinkHolder>(
        std::move(layer_tree_frame_sink),
        base::BindRepeating(&TestFrameFactory::CreateCompositorFrame,
                            base::Unretained(frame_factory_.get())),
        base::BindRepeating(&TestFrameFactory::OnFirstFrameRequested,
                            base::Unretained(frame_factory_.get())));

    holder_weak_ptr_ = frame_sink_holder_->GetWeakPtr();
  }

  UiResourceManager& GetResourceManager() {
    return frame_sink_holder_->resource_manager();
  }

  // If `frame_sink_holder_` lifetime has been extended in a unittest and the
  // holder did not schedule a delete task, it will get destroyed once we
  // delete the root_window of `host_window_`.
  std::unique_ptr<FrameSinkHolder> frame_sink_holder_;
  raw_ptr<aura::Window, DanglingUntriaged> host_window_;

  // Will be used to access the frame_sink_holder once we pass the
  // ownership of `frame_sink_holder_` to
  // `DeleteWhenLastResourceHasBeenReclaimed()` in unittests.
  base::WeakPtr<FrameSinkHolder> holder_weak_ptr_;

  // Factory to create test compositor frames.
  std::unique_ptr<TestFrameFactory> frame_factory_;

  // Keeping a reference to be used in tests.
  raw_ptr<TestLayerTreeFrameSink, DanglingUntriaged>
      layer_tree_frame_sink_;  // no owned
};

TEST_F(FrameSinkHolderTest, SubmitFrameSynchronouslyBeforeFirstFrameRequested) {
  FrameSinkHolderTestApi test_api(frame_sink_holder_.get());

  frame_factory_->SetFrameMetaData(gfx::Size(100, 100), 1.0);

  ASSERT_FALSE(test_api.IsFirstFrameRequested());
  frame_sink_holder_->SubmitCompositorFrame(/*synchronous_draw=*/true);

  // Confirm that FrameSinkHolder did not submit any frame yet.
  EXPECT_TRUE(test_api.LastSubmittedFrameSize().IsEmpty());
  EXPECT_EQ(layer_tree_frame_sink_->num_of_frames_received(), 0);

  // FrameSinkHolder has pending a frame that will be sent out asynchronously.
  EXPECT_TRUE(test_api.IsPendingFrame());

  // Asynchronous frame request.
  frame_sink_holder_->OnBeginFrame(CreateValidBeginFrameArgsForTesting());
  ASSERT_TRUE(test_api.IsFirstFrameRequested());
  EXPECT_FALSE(test_api.IsPendingFrame());
  EXPECT_TRUE(test_api.IsPendingFrameAck());

  // LayerTreeFrameSink received the frame.
  EXPECT_EQ(layer_tree_frame_sink_->num_of_frames_received(), 1);

  // Manual BeginFrameAck is used for synchronous frames only.
  EXPECT_THAT(
      layer_tree_frame_sink_->GetLatestReceivedFrame().metadata.begin_frame_ack,
      testing::Not(IsBeginFrameAckEqual(
          viz::BeginFrameAck::CreateManualAckWithDamage())));

  frame_sink_holder_->DidReceiveCompositorFrameAck();
  EXPECT_FALSE(test_api.IsPendingFrameAck());

  layer_tree_frame_sink_->ResetLatestFrameState();

  // Now that FrameSinkHolder has received the requested for the first frame, it
  // can now submit frames synchronously.
  frame_sink_holder_->SubmitCompositorFrame(/*synchronous_draw=*/true);
  EXPECT_EQ(layer_tree_frame_sink_->num_of_frames_received(), 2);
  EXPECT_EQ(test_api.LastSubmittedFrameSize(),
            layer_tree_frame_sink_->GetLatestReceivedFrame().size_in_pixels());

  // Manual BeginFrameAck is used only for synchronously submitted frames.
  EXPECT_THAT(
      layer_tree_frame_sink_->GetLatestReceivedFrame().metadata.begin_frame_ack,
      IsBeginFrameAckEqual(viz::BeginFrameAck::CreateManualAckWithDamage()));
}

TEST_F(FrameSinkHolderTest, ObserveBeginFrameSourceOnDemand) {
  FrameSinkHolderTestApi test_api(frame_sink_holder_.get());

  StubBeginFrameSource source;
  frame_sink_holder_->SetBeginFrameSource(&source);

  // FrameSinkHolder should be observing the source when it is set.
  EXPECT_TRUE(test_api.IsObservingBeginFrameSource());

  // After consecutively not producing frames for a certain number of
  // BeginFrames, FrameSinkHolder should stop observing the source.
  for (int i = 0; i < 4; i++) {
    frame_sink_holder_->OnBeginFrame(CreateValidBeginFrameArgsForTesting());
  }

  frame_sink_holder_->SubmitCompositorFrame(/*synchronous_draw=*/true);
  frame_sink_holder_->OnBeginFrame(CreateValidBeginFrameArgsForTesting());
  EXPECT_TRUE(test_api.IsObservingBeginFrameSource());

  for (int i = 0; i < 5; i++) {
    frame_sink_holder_->OnBeginFrame(CreateValidBeginFrameArgsForTesting());
  }

  EXPECT_FALSE(test_api.IsObservingBeginFrameSource());

  // However, if there is request to submit a new frame, FrameSinkHolder should
  // start observing the source again.
  frame_sink_holder_->SubmitCompositorFrame(/*synchronous_draw=*/true);
  EXPECT_TRUE(test_api.IsObservingBeginFrameSource());

  frame_sink_holder_->SetBeginFrameSource(nullptr);
}

TEST_F(FrameSinkHolderTest, ObserveBeginFrameSourceOnDemand_AutoUpdate) {
  FrameSinkHolderTestApi test_api(frame_sink_holder_.get());

  frame_sink_holder_->SetAutoUpdateMode(true);

  StubBeginFrameSource source;
  frame_sink_holder_->SetBeginFrameSource(&source);

  // FrameSinkHolder should be observing BeginFrameSource when it is set.
  EXPECT_TRUE(test_api.IsObservingBeginFrameSource());

  // When auto update mode is on, we should not stop observation of
  // BeginFrameSource.
  for (int i = 0; i < 10; i++) {
    frame_sink_holder_->OnBeginFrame(CreateValidBeginFrameArgsForTesting());
  }

  EXPECT_TRUE(test_api.IsObservingBeginFrameSource());

  // However once the auto update mode is turned off, we should stop observing
  // the BeginFrameSource as needed.
  frame_sink_holder_->SetAutoUpdateMode(false);

  for (int i = 0; i < 5; i++) {
    frame_sink_holder_->OnBeginFrame(CreateValidBeginFrameArgsForTesting());
  }

  EXPECT_FALSE(test_api.IsObservingBeginFrameSource());

  // Renablending the auto update mode should start the observation of
  // BeginFrameSource.
  frame_sink_holder_->SetAutoUpdateMode(true);
  EXPECT_TRUE(test_api.IsObservingBeginFrameSource());

  frame_sink_holder_->SetBeginFrameSource(nullptr);
}

TEST_F(FrameSinkHolderTest, SubmitFrameSynchronouslyWhilePendingFrameAck) {
  FrameSinkHolderTestApi test_api(frame_sink_holder_.get());

  frame_factory_->SetFrameMetaData(gfx::Size(100, 100), 1.0);

  // Call OnBeginFrame so that FrameSinkHolder can know that it can submit
  // frames synchronously.
  frame_sink_holder_->OnBeginFrame(CreateValidBeginFrameArgsForTesting());

  frame_sink_holder_->SubmitCompositorFrame(/*synchronous_draw=*/true);
  EXPECT_TRUE(test_api.IsPendingFrameAck());
  EXPECT_EQ(layer_tree_frame_sink_->num_of_frames_received(), 1);

  frame_factory_->SetFrameMetaData(gfx::Size(200, 200), 1.0);
  frame_sink_holder_->SubmitCompositorFrame(/*synchronous_draw=*/true);

  // This confirms that FrameSinkHolder did not submit frame synchronously,
  // since it has not received frame ack for the last frame.
  EXPECT_EQ(layer_tree_frame_sink_->GetLatestReceivedFrame().size_in_pixels(),
            gfx::Size(100, 100));
  EXPECT_EQ(layer_tree_frame_sink_->num_of_frames_received(), 1);

  // FrameSinkHolder fell to asynchronous frame submission.
  EXPECT_TRUE(test_api.IsPendingFrame());
}

TEST_F(FrameSinkHolderTest, HandlingAsynchronousFrameRequests_NoAutoUpdate) {
  FrameSinkHolderTestApi test_api(frame_sink_holder_.get());

  frame_sink_holder_->OnBeginFrame(CreateValidBeginFrameArgsForTesting());

  // FrameSinkHolder has no request to submit a frame.
  auto skipped_reason = layer_tree_frame_sink_->GetLatestFrameSkippedReason();
  ASSERT_TRUE(skipped_reason.has_value());
  EXPECT_EQ(skipped_reason, cc::FrameSkippedReason::kNoDamage);

  frame_factory_->SetFrameMetaData(gfx::Size(100, 100), 1.0);

  frame_sink_holder_->SubmitCompositorFrame(/*synchronous_draw=*/false);
  EXPECT_TRUE(test_api.IsPendingFrame());

  // This time FrameSinkHolder has a request to submit frame asynchronously.
  frame_sink_holder_->OnBeginFrame(CreateValidBeginFrameArgsForTesting());
  EXPECT_EQ(layer_tree_frame_sink_->num_of_frames_received(), 1);

  // Asynchronously submitted frames will not have manual BeginFrameAck.
  EXPECT_THAT(
      layer_tree_frame_sink_->GetLatestReceivedFrame().metadata.begin_frame_ack,
      testing::Not(IsBeginFrameAckEqual(
          viz::BeginFrameAck::CreateManualAckWithDamage())));
  EXPECT_FALSE(test_api.IsPendingFrame());
  EXPECT_TRUE(test_api.IsPendingFrameAck());

  layer_tree_frame_sink_->ResetLatestFrameState();

  // FrameSinkHolder did not submit a frame since it is still waiting for ack.
  frame_sink_holder_->OnBeginFrame(CreateValidBeginFrameArgsForTesting());

  skipped_reason = layer_tree_frame_sink_->GetLatestFrameSkippedReason();
  ASSERT_TRUE(skipped_reason.has_value());
  EXPECT_EQ(skipped_reason, cc::FrameSkippedReason::kWaitingOnMain);

  EXPECT_EQ(layer_tree_frame_sink_->num_of_frames_received(), 1);

  // Received ack.
  frame_sink_holder_->DidReceiveCompositorFrameAck();
  EXPECT_FALSE(test_api.IsPendingFrameAck());

  layer_tree_frame_sink_->ResetLatestFrameState();

  // FrameSinkHolder did not submit anything because it did not have any pending
  // request.
  frame_sink_holder_->OnBeginFrame(CreateValidBeginFrameArgsForTesting());

  skipped_reason = layer_tree_frame_sink_->GetLatestFrameSkippedReason();
  ASSERT_TRUE(skipped_reason.has_value());
  EXPECT_EQ(skipped_reason, cc::FrameSkippedReason::kNoDamage);

  EXPECT_EQ(layer_tree_frame_sink_->num_of_frames_received(), 1);

  // FrameSinkHolder has an async request again.
  frame_sink_holder_->SubmitCompositorFrame(/*synchronous_draw=*/false);
  EXPECT_TRUE(test_api.IsPendingFrame());
  EXPECT_FALSE(test_api.IsPendingFrameAck());

  layer_tree_frame_sink_->ResetLatestFrameState();

  // FrameSinkHolder should now submit a new frame.
  frame_sink_holder_->OnBeginFrame(CreateValidBeginFrameArgsForTesting());

  // Asynchronously submitted frames will not have manual begin_frame_ack.
  EXPECT_THAT(
      layer_tree_frame_sink_->GetLatestReceivedFrame().metadata.begin_frame_ack,
      testing::Not(IsBeginFrameAckEqual(
          viz::BeginFrameAck::CreateManualAckWithDamage())));
  EXPECT_EQ(layer_tree_frame_sink_->num_of_frames_received(), 2);
  EXPECT_FALSE(test_api.IsPendingFrame());
  EXPECT_TRUE(test_api.IsPendingFrameAck());
}

TEST_F(FrameSinkHolderTest, DontSubmitNewFramesWhenWaitingToDeleteSinkHolder) {
  FrameSinkHolderTestApi test_api(frame_sink_holder_.get());
  base::RunLoop loop;

  viz::ResourceId id_1 =
      GetResourceManager().OfferResource(std::make_unique<UiResource>());

  frame_factory_->SetFrameResources({id_1});
  frame_factory_->SetFrameMetaData(gfx::Size(100, 100), 1.0);

  // Call OnBeginFrame so that FrameSinkHolder can know that it can submit
  // frames synchronously.
  frame_sink_holder_->OnBeginFrame(CreateValidBeginFrameArgsForTesting());
  frame_sink_holder_->SubmitCompositorFrame(/*synchronous_draw=*/true);
  EXPECT_EQ(layer_tree_frame_sink_->num_of_frames_received(), 1);

  // The lifetime of frame_sink_holder has been extended since there are still
  // some exported resources.
  EXPECT_FALSE(FrameSinkHolder::DeleteWhenLastResourceHasBeenReclaimed(
      std::move(frame_sink_holder_), host_window_));

  ASSERT_TRUE(holder_weak_ptr_);

  // During deletion FrameSinkHolder submits a empty frame.
  EXPECT_EQ(layer_tree_frame_sink_->num_of_frames_received(), 2);

  layer_tree_frame_sink_->ResetLatestFrameState();

  holder_weak_ptr_->OnBeginFrame(CreateValidBeginFrameArgsForTesting());

  // Confirms that FrameSinkHolder did not submit a new frame on asynchronous
  // request.
  EXPECT_EQ(layer_tree_frame_sink_->num_of_frames_received(), 2);
}

TEST_F(FrameSinkHolderTest,
       DeleteSinkHolderImmediatelyWhenNoFramesIsSubmitted) {
  FrameSinkHolderTestApi test_api(frame_sink_holder_.get());

  // Confirms that FrameSinkHolder has not submitted any frames yet.
  EXPECT_TRUE(test_api.LastSubmittedFrameSize().IsEmpty());
  EXPECT_EQ(layer_tree_frame_sink_->num_of_frames_received(), 0);

  // FrameSinkHolder will get deleted straight away since it has not submitted
  // any resources to the display compositor.
  EXPECT_TRUE(FrameSinkHolder::DeleteWhenLastResourceHasBeenReclaimed(
      std::move(frame_sink_holder_), host_window_));

  // Since FrameSinkHolder is deleted immediately, we expect the weak_ptr to be
  // not valid.
  EXPECT_FALSE(holder_weak_ptr_);
}

TEST_F(FrameSinkHolderTest, ExtendLifeTimeOfHolderToRootWindow) {
  FrameSinkHolderTestApi test_api(frame_sink_holder_.get());

  viz::ResourceId id_1 =
      GetResourceManager().OfferResource(std::make_unique<UiResource>());
  viz::ResourceId id_2 =
      GetResourceManager().OfferResource(std::make_unique<UiResource>());
  viz::ResourceId id_3 =
      GetResourceManager().OfferResource(std::make_unique<UiResource>());

  frame_factory_->SetFrameResources({id_1, id_2, id_3});
  frame_factory_->SetFrameMetaData(gfx::Size(100, 100), 1.0);

  // Call OnBeginFrame so that FrameSinkHolder can know that it can submit
  // frames synchronously.
  frame_sink_holder_->OnBeginFrame(CreateValidBeginFrameArgsForTesting());
  frame_sink_holder_->SubmitCompositorFrame(/*synchronous_draw=*/true);

  // Confirms that FrameSinkHolder has not submitted any frames.
  EXPECT_FALSE(test_api.LastSubmittedFrameSize().IsEmpty());
  EXPECT_EQ(layer_tree_frame_sink_->num_of_frames_received(), 1);

  // Since FrameSinkHolder has not received the resources back from display
  // compositor, it extend its lifetime.
  EXPECT_FALSE(FrameSinkHolder::DeleteWhenLastResourceHasBeenReclaimed(
      std::move(frame_sink_holder_), host_window_));

  // Since FrameSinkHolder lifetime is extend, we expect the weak_ptr to be
  // valid.
  EXPECT_TRUE(holder_weak_ptr_);
}

TEST_F(FrameSinkHolderTest, KeepSubmittingFrameWhenAutoUpdateIsOn) {
  FrameSinkHolderTestApi test_api(frame_sink_holder_.get());

  frame_factory_->SetFrameMetaData(gfx::Size(100, 100), 1.0);

  // Request a frame.
  frame_sink_holder_->OnBeginFrame(CreateValidBeginFrameArgsForTesting());

  // Since auto_fresh_mode is off, FrameSinkHolder did not submit any frame as
  // there was not request for a frame submission.
  EXPECT_EQ(layer_tree_frame_sink_->num_of_frames_received(), 0);

  // Request a frame again. FrameSinkHolder should not submit a frame.
  frame_sink_holder_->OnBeginFrame(CreateValidBeginFrameArgsForTesting());
  EXPECT_EQ(layer_tree_frame_sink_->num_of_frames_received(), 0);

  frame_sink_holder_->SetAutoUpdateMode(/*mode=*/true);

  // After auto_fresh_mode on, when compositor requests for a frame,
  // FrameSinkHolder should submit a frame.
  frame_sink_holder_->OnBeginFrame(CreateValidBeginFrameArgsForTesting());
  EXPECT_EQ(layer_tree_frame_sink_->num_of_frames_received(), 1);

  // FrameSinkHolder should not submit a new frame sas it has no received an
  // ack from the compositor,
  frame_sink_holder_->OnBeginFrame(CreateValidBeginFrameArgsForTesting());
  EXPECT_EQ(layer_tree_frame_sink_->num_of_frames_received(), 1);

  // Receive an ack.
  frame_sink_holder_->DidReceiveCompositorFrameAck();

  // In auto_fresh mode, FrameSinkHolder will keep on submitting frames
  // asynchronously.
  frame_sink_holder_->OnBeginFrame(CreateValidBeginFrameArgsForTesting());
  EXPECT_EQ(layer_tree_frame_sink_->num_of_frames_received(), 2);
  frame_sink_holder_->DidReceiveCompositorFrameAck();
  frame_sink_holder_->OnBeginFrame(CreateValidBeginFrameArgsForTesting());
  EXPECT_EQ(layer_tree_frame_sink_->num_of_frames_received(), 3);
}

TEST_F(FrameSinkHolderTest, DeleteHolderAfterReclaimingAllResources) {
  FrameSinkHolderTestApi test_api(frame_sink_holder_.get());
  base::RunLoop loop;

  viz::ResourceId id_1 =
      GetResourceManager().OfferResource(std::make_unique<UiResource>());
  viz::ResourceId id_2 =
      GetResourceManager().OfferResource(std::make_unique<UiResource>());

  frame_factory_->SetFrameResources({id_1, id_2});
  frame_factory_->SetFrameMetaData(gfx::Size(100, 100), 1.0);

  // Call OnBeginFrame so that FrameSinkHolder can know that it can submit
  // frames synchronously.
  frame_sink_holder_->OnBeginFrame(CreateValidBeginFrameArgsForTesting());
  frame_sink_holder_->SubmitCompositorFrame(/*synchronous_draw=*/true);

  EXPECT_FALSE(FrameSinkHolder::DeleteWhenLastResourceHasBeenReclaimed(
      std::move(frame_sink_holder_), host_window_));

  // The lifetime of frame_sink_holder has been extended since there are still
  // some exported resources.
  ASSERT_TRUE(holder_weak_ptr_);

  std::vector<viz::ReturnedResource> to_be_returned_resources;
  layer_tree_frame_sink_->GetFrameResourcesToReturn(to_be_returned_resources);

  // Reclaim the exported resources.
  holder_weak_ptr_->ReclaimResources(std::move(to_be_returned_resources));

  // Wait for the deletion task to be completed.
  loop.RunUntilIdle();

  ASSERT_FALSE(holder_weak_ptr_);
}

TEST_F(FrameSinkHolderTest, LayerTreeFrameSinkLost) {
  FrameSinkHolderTestApi test_api(frame_sink_holder_.get());

  viz::ResourceId id_1 =
      GetResourceManager().OfferResource(std::make_unique<UiResource>());

  frame_factory_->SetFrameResources({id_1});
  frame_factory_->SetFrameMetaData(gfx::Size(100, 100), 1.0);

  // Call OnBeginFrame so that FrameSinkHolder can know that it can submit
  // frames synchronously.
  frame_sink_holder_->OnBeginFrame(CreateValidBeginFrameArgsForTesting());
  frame_sink_holder_->SubmitCompositorFrame(/*synchronous_draw=*/true);

  EXPECT_EQ(GetResourceManager().exported_resources_count(), 1u);

  frame_sink_holder_->DidLoseLayerTreeFrameSink();

  // When FrameSinkHolder loses the LayerTreeFrameSink, it marks all the
  // exported resources as lost.
  EXPECT_EQ(GetResourceManager().exported_resources_count(), 0u);
}

TEST_F(FrameSinkHolderTest,
       LayerTreeFrameSinkLostWhenWaitingToDeleteSinkHolder) {
  FrameSinkHolderTestApi test_api(frame_sink_holder_.get());
  base::RunLoop loop;

  viz::ResourceId id_1 =
      GetResourceManager().OfferResource(std::make_unique<UiResource>());

  frame_factory_->SetFrameResources({id_1});
  frame_factory_->SetFrameMetaData(gfx::Size(100, 100), 1.0);

  // Call OnBeginFrame so that FrameSinkHolder can know that it can submit
  // frames synchronously.
  frame_sink_holder_->OnBeginFrame(CreateValidBeginFrameArgsForTesting());
  frame_sink_holder_->SubmitCompositorFrame(/*synchronous_draw=*/true);

  EXPECT_FALSE(FrameSinkHolder::DeleteWhenLastResourceHasBeenReclaimed(
      std::move(frame_sink_holder_), host_window_));

  // The lifetime of frame_sink_holder has been extended since there are still
  // some exported resources.
  ASSERT_TRUE(holder_weak_ptr_);

  holder_weak_ptr_->DidLoseLayerTreeFrameSink();

  // Since FrameSinkHolder cannot reclaim back exported resources, it schedules
  // to delete itself.
  // Wait for deletion task to complete.
  loop.RunUntilIdle();

  ASSERT_FALSE(holder_weak_ptr_);
}

TEST_F(FrameSinkHolderTest,
       DeleteSinkHolderWithExportedResources_DuringShutdown) {
  FrameSinkHolderTestApi test_api(frame_sink_holder_.get());

  viz::ResourceId id_1 =
      GetResourceManager().OfferResource(std::make_unique<UiResource>());

  frame_factory_->SetFrameResources({id_1});
  frame_factory_->SetFrameMetaData(gfx::Size(100, 100), 1.0);

  // Call OnBeginFrame so that FrameSinkHolder can know that it can submit
  // frames synchronously.
  frame_sink_holder_->OnBeginFrame(CreateValidBeginFrameArgsForTesting());
  frame_sink_holder_->SubmitCompositorFrame(/*synchronous_draw=*/true);

  // Confirms we have an exported resource.
  EXPECT_EQ(frame_sink_holder_->resource_manager().exported_resources_count(),
            1u);

  // During shutdown, root_window can be null. We can replicate it by
  // removing the host window from the window hierarchy.
  aura::Window* root_window =
      Shell::Get()->GetRootWindowForDisplayId(GetPrimaryDisplay().id());
  root_window->RemoveChild(host_window_);

  // Since `host_window_` is removed from window tree hierarchy, wrapping it in
  // a unique_ptr to delete this object as it goes out of scope and stop it
  // from leaking memory.
  auto host_window = base::WrapUnique<aura::Window>(host_window_);

  // Since FrameSinkHolder cannot extend its lifetime, it marks the resources
  // as lost and deletes itself immediately.
  EXPECT_TRUE(FrameSinkHolder::DeleteWhenLastResourceHasBeenReclaimed(
      std::move(frame_sink_holder_), host_window_));

  // Since FrameSinkHolder is deleted immediately, we expect the weak_ptr to be
  // not valid.
  EXPECT_FALSE(holder_weak_ptr_);
}

TEST_F(FrameSinkHolderTest,
       DeleteSinkHolderImmediatelyWhenNoExportedResources) {
  FrameSinkHolderTestApi test_api(frame_sink_holder_.get());

  viz::ResourceId id_1 =
      GetResourceManager().OfferResource(std::make_unique<UiResource>());

  frame_factory_->SetFrameResources({id_1});
  frame_factory_->SetFrameMetaData(gfx::Size(100, 100), 1.0);

  // Call OnBeginFrame so that FrameSinkHolder can know that it can submit
  // frames synchronously.
  frame_sink_holder_->OnBeginFrame(CreateValidBeginFrameArgsForTesting());
  frame_sink_holder_->SubmitCompositorFrame(/*synchronous_draw=*/true);

  // Confirms that FrameSinkHolder has submitted a frame.
  EXPECT_FALSE(test_api.LastSubmittedFrameSize().IsEmpty());
  EXPECT_EQ(layer_tree_frame_sink_->num_of_frames_received(), 1);

  std::vector<viz::ReturnedResource> to_be_returned_resources;
  layer_tree_frame_sink_->GetFrameResourcesToReturn(to_be_returned_resources);

  frame_sink_holder_->ReclaimResources(std::move(to_be_returned_resources));

  // We can delete the holder straight way since we have no exported resources.
  ASSERT_EQ(GetResourceManager().exported_resources_count(), 0u);

  EXPECT_TRUE(FrameSinkHolder::DeleteWhenLastResourceHasBeenReclaimed(
      std::move(frame_sink_holder_), host_window_));

  // Since FrameSinkHolder is deleted immediately, we expect the weak_ptr to be
  // not valid.
  EXPECT_FALSE(holder_weak_ptr_);
}

}  // namespace
}  // namespace ash
