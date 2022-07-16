// Copyright 2015 The Chromium Authors. All rights reserved.
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
  auto animation_host = AnimationHost::CreateForTesting(ThreadInstance::MAIN);
  std::unique_ptr<FakeLayerTreeHost> host = FakeLayerTreeHost::Create(
      &host_client, &task_graph_runner, animation_host.get(), settings,
      CompositorMode::SINGLE_THREADED);
  EXPECT_FALSE(host->pending_commit_state()->needs_gpu_rasterization_histogram);
  host->CreateFakeLayerTreeHostImpl();
  host->WillCommit(/*completion_event=*/nullptr, /*has_updates=*/true);
  EXPECT_FALSE(host->pending_commit_state()->needs_gpu_rasterization_histogram);
  {
    DebugScopedSetImplThread impl(host->GetTaskRunnerProvider());
    EXPECT_FALSE(
        host->active_commit_state()->needs_gpu_rasterization_histogram);
    host->RecordGpuRasterizationHistogram(host->host_impl());
  }
  host->CommitComplete();
  EXPECT_FALSE(host->pending_commit_state()->needs_gpu_rasterization_histogram);
}

TEST(LayerTreeHostRecordGpuHistogramTest, Threaded) {
  FakeLayerTreeHostClient host_client;
  TestTaskGraphRunner task_graph_runner;
  LayerTreeSettings settings;
  auto animation_host = AnimationHost::CreateForTesting(ThreadInstance::MAIN);
  std::unique_ptr<FakeLayerTreeHost> host = FakeLayerTreeHost::Create(
      &host_client, &task_graph_runner, animation_host.get(), settings,
      CompositorMode::THREADED);
  EXPECT_TRUE(host->pending_commit_state()->needs_gpu_rasterization_histogram);
  host->CreateFakeLayerTreeHostImpl();
  host->WillCommit(/*completion_event=*/nullptr, /*has_updates=*/true);
  EXPECT_FALSE(host->pending_commit_state()->needs_gpu_rasterization_histogram);
  {
    DebugScopedSetImplThread impl(host->GetTaskRunnerProvider());
    EXPECT_TRUE(host->active_commit_state()->needs_gpu_rasterization_histogram);
    host->RecordGpuRasterizationHistogram(host->host_impl());
  }
  host->CommitComplete();
  EXPECT_FALSE(host->pending_commit_state()->needs_gpu_rasterization_histogram);
}

}  // namespace

}  // namespace cc
