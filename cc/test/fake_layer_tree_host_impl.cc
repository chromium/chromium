// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/fake_layer_tree_host_impl.h"

#include <stddef.h>

#include <utility>

#include "base/task/sequenced_task_runner.h"
#include "cc/animation/animation_host.h"
#include "cc/test/layer_test_common.h"
#include "cc/trees/layer_tree_impl.h"
#include "components/viz/test/begin_frame_args_test.h"

namespace cc {

FakeLayerTreeHostImpl::FakeLayerTreeHostImpl(
    TaskRunnerProvider* task_runner_provider,
    TaskGraphRunner* task_graph_runner)
    : FakeLayerTreeHostImpl(CommitToPendingTreeLayerListSettings(),
                            task_runner_provider,
                            task_graph_runner) {}

FakeLayerTreeHostImpl::FakeLayerTreeHostImpl(
    const LayerTreeSettings& settings,
    TaskRunnerProvider* task_runner_provider,
    TaskGraphRunner* task_graph_runner)
    : FakeLayerTreeHostImpl(settings,
                            task_runner_provider,
                            task_graph_runner,
                            nullptr) {}

FakeLayerTreeHostImpl::FakeLayerTreeHostImpl(
    const LayerTreeSettings& settings,
    TaskRunnerProvider* task_runner_provider,
    TaskGraphRunner* task_graph_runner,
    scoped_refptr<base::SequencedTaskRunner> image_worker_task_runner)
    : LayerTreeHostImpl(settings,
                        &client_,
                        task_runner_provider,
                        &stats_instrumentation_,
                        task_graph_runner,
                        AnimationHost::CreateForTesting(ThreadInstance::kImpl),
                        nullptr,
                        0,
                        std::move(image_worker_task_runner),
                        /*scheduling_client=*/nullptr),
      notify_tile_state_changed_called_(false) {
  // Explicitly clear all debug settings.
  SetDebugState(LayerTreeDebugState());
  active_tree()->SetDeviceViewportRect(gfx::Rect(100, 100));

  // Start an impl frame so tests have a valid frame_time to work with.
  base::TimeTicks time_ticks = base::TimeTicks() + base::Microseconds(1);
  WillBeginImplFrame(viz::CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE,
                                                         0, 1, time_ticks));
}

FakeLayerTreeHostImpl::~FakeLayerTreeHostImpl() {
  ReleaseLayerTreeFrameSink();
}

void FakeLayerTreeHostImpl::CreatePendingTree() {
  LayerTreeHostImpl::CreatePendingTree();
  float arbitrary_large_page_scale = 100000.f;
  pending_tree()->PushPageScaleFromMainThread(
      1.f, 1.f / arbitrary_large_page_scale, arbitrary_large_page_scale);
  // Normally a pending tree will not be fully painted until the commit has
  // happened and any PaintWorklets have been resolved. However many of the
  // unittests never actually commit the pending trees that they create, so to
  // enable them to still treat the tree as painted we forcibly override the
  // state here. Note that this marks a distinct departure from reality in the
  // name of easier testing.
  set_pending_tree_fully_painted_for_testing(true);
}

void FakeLayerTreeHostImpl::EnsureSyncTree() {
  if (!CommitsToActiveTree() && !pending_tree()) {
    CreatePendingTree();
  }
  CHECK(sync_tree());
}

void FakeLayerTreeHostImpl::NotifyTileStateChanged(const Tile* tile) {
  LayerTreeHostImpl::NotifyTileStateChanged(tile);
  notify_tile_state_changed_called_ = true;
}

TargetColorParams FakeLayerTreeHostImpl::GetTargetColorParams(
    gfx::ContentColorUsage content_color_usage) const {
  return target_color_params_.value_or(
      LayerTreeHostImpl::GetTargetColorParams(content_color_usage));
}

const viz::BeginFrameArgs& FakeLayerTreeHostImpl::CurrentBeginFrameArgs()
    const {
  return current_begin_frame_tracker_.DangerousMethodCurrentOrLast();
}

void FakeLayerTreeHostImpl::AdvanceToNextFrame(base::TimeDelta advance_by) {
  viz::BeginFrameArgs next_begin_frame_args =
      current_begin_frame_tracker_.Current();
  next_begin_frame_args.frame_time += advance_by;
  DidFinishImplFrame(current_begin_frame_tracker_.Current());
  WillBeginImplFrame(next_begin_frame_args);
}

AnimationHost* FakeLayerTreeHostImpl::animation_host() const {
  return static_cast<AnimationHost*>(mutator_host());
}

}  // namespace cc
