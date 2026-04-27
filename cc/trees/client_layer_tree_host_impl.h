// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_CLIENT_LAYER_TREE_HOST_IMPL_H_
#define CC_TREES_CLIENT_LAYER_TREE_HOST_IMPL_H_

#include "cc/cc_export.h"
#include "cc/trees/layer_tree_host_impl.h"

namespace cc {

class CC_EXPORT ClientLayerTreeHostImpl : public LayerTreeHostImpl {
 public:
  static std::unique_ptr<ClientLayerTreeHostImpl> Create(
      const LayerTreeSettings& settings,
      LayerTreeHostImplClient* client,
      TaskRunnerProvider* task_runner_provider,
      RenderingStatsInstrumentation* rendering_stats_instrumentation,
      TaskGraphRunner* task_graph_runner,
      std::unique_ptr<MutatorHost> mutator_host,
      RasterDarkModeFilter* dark_mode_filter,
      int id,
      scoped_refptr<base::SequencedTaskRunner> image_worker_task_runner,
      LayerTreeHostSchedulingClient* scheduling_client);

  using LayerTreeHostImpl::LayerTreeHostImpl;
  ~ClientLayerTreeHostImpl() override;
};

}  // namespace cc

#endif  // CC_TREES_CLIENT_LAYER_TREE_HOST_IMPL_H_
