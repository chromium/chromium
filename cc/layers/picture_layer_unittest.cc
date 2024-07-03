// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/picture_layer.h"

#include <stddef.h>

#include <utility>

#include "base/task/single_thread_task_runner.h"
#include "cc/animation/animation_host.h"
#include "cc/base/completion_event.h"
#include "cc/layers/append_quads_data.h"
#include "cc/layers/content_layer_client.h"
#include "cc/layers/picture_layer_impl.h"
#include "cc/paint/paint_flags.h"
#include "cc/test/fake_content_layer_client.h"
#include "cc/test/fake_layer_tree_frame_sink.h"
#include "cc/test/fake_layer_tree_host.h"
#include "cc/test/fake_picture_layer.h"
#include "cc/test/fake_picture_layer_impl.h"
#include "cc/test/fake_proxy.h"
#include "cc/test/layer_test_common.h"
#include "cc/test/property_tree_test_utils.h"
#include "cc/test/skia_common.h"
#include "cc/test/stub_layer_tree_host_single_thread_client.h"
#include "cc/test/test_task_graph_runner.h"
#include "cc/trees/single_thread_proxy.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkRefCnt.h"

namespace cc {

namespace {

TEST(PictureLayerTest, NoTilesIfEmptyBounds) {
  FakeContentLayerClient client;
  client.set_bounds(gfx::Size());
  scoped_refptr<PictureLayer> layer = PictureLayer::Create(&client);
  layer->SetBounds(gfx::Size(10, 10));

  FakeLayerTreeHostClient host_client;
  TestTaskGraphRunner task_graph_runner;
  auto animation_host = AnimationHost::CreateForTesting(ThreadInstance::kMain);
  std::unique_ptr<FakeLayerTreeHost> host = FakeLayerTreeHost::Create(
      &host_client, &task_graph_runner, animation_host.get());
  host->SetRootLayer(layer);
  layer->SetIsDrawable(true);
  layer->Update();

  EXPECT_EQ(0, host->SourceFrameNumber());
  host->WillCommit(/*completion=*/nullptr, /*has_updates=*/false);
  EXPECT_EQ(1, host->SourceFrameNumber());
  host->CommitComplete(host->SourceFrameNumber(),
                       {base::TimeTicks(), base::TimeTicks::Now()});
  EXPECT_EQ(1, host->SourceFrameNumber());

  layer->SetBounds(gfx::Size(0, 0));
  // Intentionally skipping Update since it would normally be skipped on
  // a layer with empty bounds.

  FakeImplTaskRunnerProvider impl_task_runner_provider;

  std::unique_ptr<FakeLayerTreeFrameSink> layer_tree_frame_sink =
      FakeLayerTreeFrameSink::CreateSoftware();
  FakeLayerTreeHostImpl host_impl(CommitToPendingTreeLayerTreeSettings(),
                                  &impl_task_runner_provider,
                                  &task_graph_runner);
  host_impl.InitializeFrameSink(layer_tree_frame_sink.get());
  host_impl.CreatePendingTree();
  std::unique_ptr<FakePictureLayerImpl> layer_impl =
      FakePictureLayerImpl::Create(host_impl.pending_tree(), 1);

  // Here and elsewhere: when doing a full commit, we would call
  // layer_tree_host_->ActivateCommitState() and the second argument would come
  // from layer_tree_host_->active_commit_state(); we use pending_commit_state()
  // just to keep the test code simple.
  layer->PushPropertiesTo(layer_impl.get(), *host->GetPendingCommitState(),
                          host->GetThreadUnsafeCommitState());
  EXPECT_FALSE(layer_impl->CanHaveTilings());
  EXPECT_TRUE(layer_impl->bounds() == gfx::Size(0, 0));
  EXPECT_EQ(gfx::Size(), layer_impl->raster_source()->size());
  EXPECT_FALSE(layer_impl->raster_source()->HasRecordings());
}

TEST(PictureLayerTest, InvalidateRasterAfterUpdate) {
  gfx::Size layer_size(50, 50);
  FakeContentLayerClient client;
  client.set_bounds(layer_size);
  scoped_refptr<PictureLayer> layer = PictureLayer::Create(&client);
  layer->SetBounds(gfx::Size(50, 50));

  FakeLayerTreeHostClient host_client;
  TestTaskGraphRunner task_graph_runner;
  auto animation_host = AnimationHost::CreateForTesting(ThreadInstance::kMain);
  std::unique_ptr<FakeLayerTreeHost> host = FakeLayerTreeHost::Create(
      &host_client, &task_graph_runner, animation_host.get());
  host->SetRootLayer(layer);
  layer->SetIsDrawable(true);

  gfx::Rect invalidation_bounds(layer_size);

  // The important two lines are the following:
  layer->SetNeedsDisplayRect(invalidation_bounds);
  layer->Update();

  FakeImplTaskRunnerProvider impl_task_runner_provider;
  std::unique_ptr<LayerTreeFrameSink> layer_tree_frame_sink(
      FakeLayerTreeFrameSink::Create3d());
  FakeLayerTreeHostImpl host_impl(CommitToPendingTreeLayerTreeSettings(),
                                  &impl_task_runner_provider,
                                  &task_graph_runner);
  host_impl.SetVisible(true);
  host_impl.InitializeFrameSink(layer_tree_frame_sink.get());
  host_impl.CreatePendingTree();
  host_impl.pending_tree()->SetRootLayerForTesting(
      FakePictureLayerImpl::Create(host_impl.pending_tree(), 1));
  FakePictureLayerImpl* layer_impl = static_cast<FakePictureLayerImpl*>(
      host_impl.pending_tree()->root_layer());
  layer->PushPropertiesTo(layer_impl, *host->GetPendingCommitState(),
                          host->GetThreadUnsafeCommitState());

  EXPECT_EQ(invalidation_bounds,
            layer_impl->GetPendingInvalidation()->bounds());
}

TEST(PictureLayerTest, InvalidateRasterWithoutUpdate) {
  gfx::Size layer_size(50, 50);
  FakeContentLayerClient client;
  client.set_bounds(layer_size);
  scoped_refptr<PictureLayer> layer = PictureLayer::Create(&client);
  layer->SetBounds(gfx::Size(50, 50));

  FakeLayerTreeHostClient host_client;
  TestTaskGraphRunner task_graph_runner;
  auto animation_host = AnimationHost::CreateForTesting(ThreadInstance::kMain);
  std::unique_ptr<FakeLayerTreeHost> host = FakeLayerTreeHost::Create(
      &host_client, &task_graph_runner, animation_host.get());
  host->SetRootLayer(layer);
  layer->SetIsDrawable(true);

  gfx::Rect invalidation_bounds(layer_size);

  // The important line is the following (note that we do not call Update):
  layer->SetNeedsDisplayRect(invalidation_bounds);

  FakeImplTaskRunnerProvider impl_task_runner_provider;
  std::unique_ptr<LayerTreeFrameSink> layer_tree_frame_sink(
      FakeLayerTreeFrameSink::Create3d());
  FakeLayerTreeHostImpl host_impl(CommitToPendingTreeLayerTreeSettings(),
                                  &impl_task_runner_provider,
                                  &task_graph_runner);
  host_impl.SetVisible(true);
  host_impl.InitializeFrameSink(layer_tree_frame_sink.get());
  host_impl.CreatePendingTree();
  host_impl.pending_tree()->set_source_frame_number(host->SourceFrameNumber());
  host_impl.pending_tree()->SetRootLayerForTesting(
      FakePictureLayerImpl::Create(host_impl.pending_tree(), 1));
  FakePictureLayerImpl* layer_impl = static_cast<FakePictureLayerImpl*>(
      host_impl.pending_tree()->root_layer());
  layer->PushPropertiesTo(layer_impl, *host->GetPendingCommitState(),
                          host->GetThreadUnsafeCommitState());

  EXPECT_EQ(gfx::Rect(), layer_impl->GetPendingInvalidation()->bounds());
}

TEST(PictureLayerTest, ClearVisibleRectWhenNoTiling) {
  gfx::Size layer_size(50, 50);
  FakeContentLayerClient client;
  client.set_bounds(layer_size);
  client.add_draw_image(CreateDiscardablePaintImage(layer_size), gfx::Point());
  scoped_refptr<PictureLayer> layer = PictureLayer::Create(&client);
  layer->SetBounds(gfx::Size(10, 10));

  FakeLayerTreeHostClient host_client;
  TestTaskGraphRunner task_graph_runner;
  auto animation_host = AnimationHost::CreateForTesting(ThreadInstance::kMain);
  std::unique_ptr<FakeLayerTreeHost> host = FakeLayerTreeHost::Create(
      &host_client, &task_graph_runner, animation_host.get());
  host->SetRootLayer(layer);
  layer->SetIsDrawable(true);
  layer->Update();

  EXPECT_EQ(0, host->SourceFrameNumber());
  host->WillCommit(/*completion=*/nullptr, /*has_updates=*/false);
  host->CommitComplete(host->SourceFrameNumber(),
                       {base::TimeTicks(), base::TimeTicks::Now()});
  EXPECT_EQ(1, host->SourceFrameNumber());

  layer->Update();
  host->BuildPropertyTreesForTesting();

  FakeImplTaskRunnerProvider impl_task_runner_provider;

  std::unique_ptr<LayerTreeFrameSink> layer_tree_frame_sink(
      FakeLayerTreeFrameSink::Create3d());
  FakeLayerTreeHostImpl host_impl(CommitToPendingTreeLayerListSettings(),
                                  &impl_task_runner_provider,
                                  &task_graph_runner);
  host_impl.SetVisible(true);
  EXPECT_TRUE(host_impl.InitializeFrameSink(layer_tree_frame_sink.get()));

  host_impl.CreatePendingTree();
  host_impl.pending_tree()->SetRootLayerForTesting(
      FakePictureLayerImpl::Create(host_impl.pending_tree(), 1));
  FakePictureLayerImpl* layer_impl = static_cast<FakePictureLayerImpl*>(
      host_impl.pending_tree()->root_layer());
  SetupRootProperties(layer_impl);
  UpdateDrawProperties(host_impl.pending_tree());

  const auto& unsafe_state = host->GetThreadUnsafeCommitState();
  std::unique_ptr<CommitState> commit_state =
      host->WillCommit(/*completion=*/nullptr, /*has_updates=*/true);
  layer->PushPropertiesTo(layer_impl, *commit_state, unsafe_state);
  host->CommitComplete(commit_state->source_frame_number,
                       {base::TimeTicks(), base::TimeTicks::Now()});

  EXPECT_EQ(2, host->SourceFrameNumber());

  host_impl.ActivateSyncTree();

  // By updating the draw proprties on the active tree, we will set the viewport
  // rect for tile priorities to something non-empty.
  UpdateDrawProperties(host_impl.active_tree());

  layer->SetBounds(gfx::Size(11, 11));

  host_impl.CreatePendingTree();
  layer_impl = static_cast<FakePictureLayerImpl*>(
      host_impl.pending_tree()->root_layer());

  // We should now have invalid contents and should therefore clear the
  // recording source.
  layer->PushPropertiesTo(layer_impl, *host->GetPendingCommitState(),
                          host->GetThreadUnsafeCommitState());
  UpdateDrawProperties(host_impl.pending_tree());

  host_impl.ActivateSyncTree();

  auto render_pass = viz::CompositorRenderPass::Create();
  AppendQuadsData data;
  host_impl.active_tree()->root_layer()->WillDraw(DRAW_MODE_SOFTWARE, nullptr);
  host_impl.active_tree()->root_layer()->AppendQuads(render_pass.get(), &data);
  host_impl.active_tree()->root_layer()->DidDraw(nullptr);
}

// PicturePile uses the source frame number as a unit for measuring invalidation
// frequency. When a pile moves between compositors, the frame number increases
// non-monotonically. This executes that code path under this scenario allowing
// for the code to verify correctness with DCHECKs.
TEST(PictureLayerTest, NonMonotonicSourceFrameNumber) {
  LayerTreeSettings settings = LayerTreeSettings();
  settings.single_thread_proxy_scheduler = false;
  settings.use_zero_copy = true;

  StubLayerTreeHostSingleThreadClient single_thread_client;
  FakeLayerTreeHostClient host_client1;
  FakeLayerTreeHostClient host_client2;
  TestTaskGraphRunner task_graph_runner;

  FakeContentLayerClient client;
  client.set_bounds(gfx::Size());
  scoped_refptr<FakePictureLayer> layer = FakePictureLayer::Create(&client);

  auto animation_host = AnimationHost::CreateForTesting(ThreadInstance::kMain);

  LayerTreeHost::InitParams params;
  params.client = &host_client1;
  params.settings = &settings;
  params.task_graph_runner = &task_graph_runner;
  params.main_task_runner = base::SingleThreadTaskRunner::GetCurrentDefault();
  params.mutator_host = animation_host.get();
  std::unique_ptr<LayerTreeHost> host1 = LayerTreeHost::CreateSingleThreaded(
      &single_thread_client, std::move(params));
  host1->SetVisible(true);
  host_client1.SetLayerTreeHost(host1.get());

  auto animation_host2 = AnimationHost::CreateForTesting(ThreadInstance::kMain);

  LayerTreeHost::InitParams params2;
  params2.client = &host_client1;
  params2.settings = &settings;
  params2.task_graph_runner = &task_graph_runner;
  params2.main_task_runner = base::SingleThreadTaskRunner::GetCurrentDefault();
  params2.client = &host_client2;
  params2.mutator_host = animation_host2.get();
  std::unique_ptr<LayerTreeHost> host2 = LayerTreeHost::CreateSingleThreaded(
      &single_thread_client, std::move(params2));
  host2->SetVisible(true);
  host_client2.SetLayerTreeHost(host2.get());

  // The PictureLayer is put in one LayerTreeHost.
  host1->SetRootLayer(layer);
  // Do a main frame, record the picture layers.
  EXPECT_EQ(0, layer->update_count());
  layer->SetNeedsDisplay();
  host1->CompositeForTest(base::TimeTicks::Now(), false, base::OnceClosure());
  EXPECT_EQ(1, layer->update_count());
  EXPECT_EQ(1, host1->SourceFrameNumber());

  // The source frame number in |host1| is now higher than host2.
  layer->SetNeedsDisplay();
  host1->CompositeForTest(base::TimeTicks::Now(), false, base::OnceClosure());
  EXPECT_EQ(2, layer->update_count());
  EXPECT_EQ(2, host1->SourceFrameNumber());

  // Then moved to another LayerTreeHost.
  host1->SetRootLayer(nullptr);
  host2->SetRootLayer(layer);

  // Do a main frame, record the picture layers. The frame number has changed
  // non-monotonically.
  layer->SetNeedsDisplay();
  host2->CompositeForTest(base::TimeTicks::Now(), false, base::OnceClosure());
  EXPECT_EQ(3, layer->update_count());
  EXPECT_EQ(1, host2->SourceFrameNumber());

  animation_host->SetMutatorHostClient(nullptr);
  animation_host2->SetMutatorHostClient(nullptr);

  host_client1.SetLayerTreeHost(nullptr);
  host_client2.SetLayerTreeHost(nullptr);
}

// Verify that PictureLayer::DropRecordingSourceContentIfInvalid does not
// assert when changing frames.
TEST(PictureLayerTest, ChangingHostsWithCollidingFrames) {
  LayerTreeSettings settings = LayerTreeSettings();
  settings.single_thread_proxy_scheduler = false;

  StubLayerTreeHostSingleThreadClient single_thread_client;
  FakeLayerTreeHostClient host_client1;
  FakeLayerTreeHostClient host_client2;
  TestTaskGraphRunner task_graph_runner;

  FakeContentLayerClient client;
  client.set_bounds(gfx::Size());
  scoped_refptr<FakePictureLayer> layer = FakePictureLayer::Create(&client);

  auto animation_host = AnimationHost::CreateForTesting(ThreadInstance::kMain);

  LayerTreeHost::InitParams params;
  params.client = &host_client1;
  params.settings = &settings;
  params.task_graph_runner = &task_graph_runner;
  params.main_task_runner = base::SingleThreadTaskRunner::GetCurrentDefault();
  params.mutator_host = animation_host.get();
  std::unique_ptr<LayerTreeHost> host1 = LayerTreeHost::CreateSingleThreaded(
      &single_thread_client, std::move(params));
  host1->SetVisible(true);
  host_client1.SetLayerTreeHost(host1.get());

  auto animation_host2 = AnimationHost::CreateForTesting(ThreadInstance::kMain);

  LayerTreeHost::InitParams params2;
  params2.client = &host_client1;
  params2.settings = &settings;
  params2.task_graph_runner = &task_graph_runner;
  params2.main_task_runner = base::SingleThreadTaskRunner::GetCurrentDefault();
  params2.client = &host_client2;
  params2.mutator_host = animation_host2.get();
  std::unique_ptr<LayerTreeHost> host2 = LayerTreeHost::CreateSingleThreaded(
      &single_thread_client, std::move(params2));
  host2->SetVisible(true);
  host_client2.SetLayerTreeHost(host2.get());

  // The PictureLayer is put in one LayerTreeHost.
  host1->SetRootLayer(layer);
  // Do a main frame, record the picture layers.
  EXPECT_EQ(0, layer->update_count());
  layer->SetBounds(gfx::Size(500, 500));
  host1->CompositeForTest(base::TimeTicks::Now(), false, base::OnceClosure());
  EXPECT_EQ(1, layer->update_count());
  EXPECT_EQ(1, host1->SourceFrameNumber());
  EXPECT_EQ(gfx::Size(500, 500), layer->bounds());

  // Then moved to another LayerTreeHost.
  host1->SetRootLayer(nullptr);
  scoped_refptr<Layer> root = Layer::Create();
  host2->SetRootLayer(root);
  root->AddChild(layer);

  // Make the layer not update.
  layer->SetHideLayerAndSubtree(true);
  EXPECT_EQ(gfx::Size(500, 500), layer->GetRecordingSourceForTesting().size());

  // Change its bounds while it's in a state that can't update.
  layer->SetBounds(gfx::Size(600, 600));
  host2->CompositeForTest(base::TimeTicks::Now(), false, base::OnceClosure());

  // This layer should not have been updated because it is invisible.
  EXPECT_EQ(1, layer->update_count());
  EXPECT_EQ(1, host2->SourceFrameNumber());

  // This layer should also drop its recording source because it was resized
  // and not recorded.
  EXPECT_EQ(gfx::Size(), layer->GetRecordingSourceForTesting().size());

  host_client1.SetLayerTreeHost(nullptr);
  host_client2.SetLayerTreeHost(nullptr);
}

TEST(PictureLayerTest, RecordingScaleIsCorrectlySet) {
  gfx::Size layer_bounds(400, 400);
  float recording_scale = 1.5f;

  FakeContentLayerClient client;
  client.set_bounds(layer_bounds);
  // Fill layer with solid color.
  PaintFlags solid_flags;
  SkColor4f solid_color{0.1f, 0.15f, 0.2f, 1.0f};
  solid_flags.setColor(solid_color);
  client.add_draw_rect(
      gfx::ScaleToEnclosingRect(gfx::Rect(layer_bounds), recording_scale),
      solid_flags);

  // Add 1 pixel of non solid color.
  SkColor4f non_solid_color{0.25f, 0.3f, 0.35f, 0.5f};
  PaintFlags non_solid_flags;
  non_solid_flags.setColor(non_solid_color);
  client.add_draw_rect(gfx::Rect(std::round(390 * recording_scale),
                                 std::round(390 * recording_scale), 1, 1),
                       non_solid_flags);

  scoped_refptr<FakePictureLayer> layer = FakePictureLayer::Create(&client);
  layer->SetBounds(layer_bounds);

  FakeLayerTreeHostClient host_client;
  TestTaskGraphRunner task_graph_runner;
  auto animation_host = AnimationHost::CreateForTesting(ThreadInstance::kMain);
  std::unique_ptr<FakeLayerTreeHost> host = FakeLayerTreeHost::Create(
      &host_client, &task_graph_runner, animation_host.get());
  host->SetRootLayer(layer);

  gfx::Rect invalidation_bounds(layer_bounds);
  layer->SetIsDrawable(true);
  layer->SetNeedsDisplayRect(invalidation_bounds);
  layer->Update();

  // Solid color analysis will return true since the layer tree host has the
  // recording scale set to its default value of 1. The non solid pixel is
  // out of bounds for the unscaled layer size in this particular case.
  EXPECT_TRUE(layer->GetRecordingSourceForTesting().is_solid_color());

  host->SetRecordingScaleFactor(recording_scale);
  layer->SetNeedsDisplayRect(invalidation_bounds);
  layer->Update();

  // Once the recording scale is set and propagated to the recording source,
  // the solid color analysis should work as expected and return false.
  EXPECT_FALSE(layer->GetRecordingSourceForTesting().is_solid_color());
}

}  // namespace
}  // namespace cc
