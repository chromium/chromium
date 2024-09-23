// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_FAKE_LAYER_TREE_HOST_IMPL_H_
#define CC_TEST_FAKE_LAYER_TREE_HOST_IMPL_H_

#include "base/task/sequenced_task_runner.h"
#include "cc/test/fake_layer_tree_host_impl_client.h"
#include "cc/test/fake_rendering_stats_instrumentation.h"
#include "cc/trees/layer_tree_host_impl.h"
#include "cc/trees/single_thread_proxy.h"

namespace cc {

class AnimationHost;

// Note: If you're creating this as a pair with a FakeLayerTreeHost, consider
// creating it via FakeLayerTreeHost::InitializeSingleThreaded if your test
// will use a Proxy or FakeLayerTreeHost::CreateFakeLayerTreeHostImpl if it
// doesn't use a Proxy. These will ensure we're not accidentally creating
// multiple HostImpls.
class FakeLayerTreeHostImpl : public LayerTreeHostImpl {
 public:
  FakeLayerTreeHostImpl(TaskRunnerProvider* task_runner_provider,
                        TaskGraphRunner* task_graph_runner);
  FakeLayerTreeHostImpl(const LayerTreeSettings& settings,
                        TaskRunnerProvider* task_runner_provider,
                        TaskGraphRunner* task_graph_runner);
  FakeLayerTreeHostImpl(
      const LayerTreeSettings& settings,
      TaskRunnerProvider* task_runner_provider,
      TaskGraphRunner* task_graph_runner,
      scoped_refptr<base::SequencedTaskRunner> image_worker_task_runner);
  ~FakeLayerTreeHostImpl() override;

  void ForcePrepareToDraw() {
    LayerTreeHostImpl::FrameData frame_data;
    PrepareToDraw(&frame_data);
    DidDrawAllLayers(frame_data);
  }

  void CreatePendingTree() override;
  void EnsureSyncTree();

  void NotifyTileStateChanged(const Tile* tile) override;
  const viz::BeginFrameArgs& CurrentBeginFrameArgs() const override;
  void AdvanceToNextFrame(base::TimeDelta advance_by);
  TargetColorParams GetTargetColorParams(
      gfx::ContentColorUsage content_color_usage) const override;

  using LayerTreeHostImpl::ActivateSyncTree;
  using LayerTreeHostImpl::prepare_tiles_needed;
  using LayerTreeHostImpl::is_likely_to_require_a_draw;
  using LayerTreeHostImpl::RemoveRenderPasses;

  bool notify_tile_state_changed_called() const {
    return notify_tile_state_changed_called_;
  }
  void set_notify_tile_state_changed_called(bool called) {
    notify_tile_state_changed_called_ = called;
  }
  void set_target_color_params(
      std::optional<TargetColorParams> target_color_params) {
    target_color_params_ = target_color_params;
  }

  AnimationHost* animation_host() const;

  FakeLayerTreeHostImplClient* client() { return &client_; }

 private:
  FakeLayerTreeHostImplClient client_;
  FakeRenderingStatsInstrumentation stats_instrumentation_;
  bool notify_tile_state_changed_called_;
  std::optional<TargetColorParams> target_color_params_;
};

}  // namespace cc

#endif  // CC_TEST_FAKE_LAYER_TREE_HOST_IMPL_H_
