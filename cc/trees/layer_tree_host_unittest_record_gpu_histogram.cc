// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/animation/animation_host.h"
#include "cc/test/fake_layer_tree_host.h"
#include "cc/test/fake_layer_tree_host_client.h"
#include "cc/test/test_task_graph_runner.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {

namespace {

TEST(LayerTreeHostRecordGpuHistogramTest, SingleThreaded) {
  FakeLayerTreeHostClient host_client;
  TestTaskGraphRunner task_graph_runner;
  LayerTreeSettings settings;
  auto animation_host = AnimationHost::CreateForTesting(ThreadInstance::kMain);
  std::unique_ptr<FakeLayerTreeHost> host = FakeLayerTreeHost::Create(
      &host_client, &task_graph_runner, animation_host.get(), settings,
      CompositorMode::SINGLE_THREADED);
  EXPECT_FALSE(
      host->GetPendingCommitState()->needs_gpu_rasterization_histogram);
  host->CreateFakeLayerTreeHostImpl();
  auto commit_state =
      host->WillCommit(/*completion=*/nullptr, /*has_updates=*/true);
  EXPECT_FALSE(commit_state->needs_gpu_rasterization_histogram);
  EXPECT_FALSE(
      host->GetPendingCommitState()->needs_gpu_rasterization_histogram);
  host->CommitComplete(commit_state->source_frame_number,
                       {base::TimeTicks(), base::TimeTicks::Now()});
  EXPECT_FALSE(
      host->GetPendingCommitState()->needs_gpu_rasterization_histogram);
}

TEST(LayerTreeHostRecordGpuHistogramTest, Threaded) {
  FakeLayerTreeHostClient host_client;
  TestTaskGraphRunner task_graph_runner;
  LayerTreeSettings settings;
  auto animation_host = AnimationHost::CreateForTesting(ThreadInstance::kMain);
  std::unique_ptr<FakeLayerTreeHost> host = FakeLayerTreeHost::Create(
      &host_client, &task_graph_runner, animation_host.get(), settings,
      CompositorMode::THREADED);
  EXPECT_TRUE(host->GetPendingCommitState()->needs_gpu_rasterization_histogram);
  host->CreateFakeLayerTreeHostImpl();
  auto commit_state =
      host->WillCommit(/*completion=*/nullptr, /*has_updates=*/true);
  EXPECT_TRUE(commit_state->needs_gpu_rasterization_histogram);
  EXPECT_FALSE(
      host->GetPendingCommitState()->needs_gpu_rasterization_histogram);
  {
    DebugScopedSetImplThread impl(host->GetTaskRunnerProvider());
    host->host_impl()->RecordGpuRasterizationHistogram();
  }
  host->CommitComplete(commit_state->source_frame_number,
                       {base::TimeTicks(), base::TimeTicks::Now()});
  commit_state.reset();
  EXPECT_FALSE(
      host->GetPendingCommitState()->needs_gpu_rasterization_histogram);
}

}  // namespace

}  // namespace cc
