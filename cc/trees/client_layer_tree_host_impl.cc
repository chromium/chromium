// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/client_layer_tree_host_impl.h"

namespace cc {

std::unique_ptr<ClientLayerTreeHostImpl> ClientLayerTreeHostImpl::Create(
    const LayerTreeSettings& settings,
    LayerTreeHostImplClient* client,
    TaskRunnerProvider* task_runner_provider,
    RenderingStatsInstrumentation* rendering_stats_instrumentation,
    TaskGraphRunner* task_graph_runner,
    std::unique_ptr<MutatorHost> mutator_host,
    RasterDarkModeFilter* dark_mode_filter,
    int id,
    scoped_refptr<base::SequencedTaskRunner> image_worker_task_runner,
    LayerTreeHostSchedulingClient* scheduling_client) {
  CHECK(!settings.trees_in_viz_in_viz_process);
  return base::WrapUnique(new ClientLayerTreeHostImpl(
      settings, client, task_runner_provider, rendering_stats_instrumentation,
      task_graph_runner, std::move(mutator_host), dark_mode_filter, id,
      std::move(image_worker_task_runner), scheduling_client));
}

ClientLayerTreeHostImpl::~ClientLayerTreeHostImpl() = default;

}  // namespace cc
