// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_FAKE_LAYER_TREE_HOST_H_
#define CC_TEST_FAKE_LAYER_TREE_HOST_H_

#include <memory>

#include "cc/benchmarks/micro_benchmark_controller.h"
#include "cc/test/fake_impl_task_runner_provider.h"
#include "cc/test/fake_layer_tree_host_client.h"
#include "cc/test/fake_layer_tree_host_impl.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/layer_tree_impl.h"
#include "cc/trees/tree_synchronizer.h"

namespace cc {

class MutatorHost;
class TestTaskGraphRunner;

// FakeImplTaskRunnerProvider uses DebugScopedSetImplThread to set its task
// runner to look like the impl thread. This means it needs to outlive the
// LayerTreeHost so that the task runner remains impl-thread bound. This helper
// class is inherited before LayerTreeHost to ensure it's constructed first and
// destructed last.
class TaskRunnerProviderHolder {
 public:
  FakeImplTaskRunnerProvider& task_runner_provider() {
    return task_runner_provider_;
  }

 private:
  FakeImplTaskRunnerProvider task_runner_provider_;
};

class FakeLayerTreeHost : private TaskRunnerProviderHolder,
                          public LayerTreeHost {
 public:
  static std::unique_ptr<FakeLayerTreeHost> Create(
      FakeLayerTreeHostClient* client,
      TestTaskGraphRunner* task_graph_runner,
      MutatorHost* mutator_host);
  static std::unique_ptr<FakeLayerTreeHost> Create(
      FakeLayerTreeHostClient* client,
      TestTaskGraphRunner* task_graph_runner,
      MutatorHost* mutator_host,
      const LayerTreeSettings& settings);
  static std::unique_ptr<FakeLayerTreeHost> Create(
      FakeLayerTreeHostClient* client,
      TestTaskGraphRunner* task_graph_runner,
      MutatorHost* mutator_host,
      const LayerTreeSettings& settings,
      CompositorMode mode);
  static std::unique_ptr<FakeLayerTreeHost> Create(
      FakeLayerTreeHostClient* client,
      TestTaskGraphRunner* task_graph_runner,
      MutatorHost* mutator_host,
      const LayerTreeSettings& settings,
      CompositorMode mode,
      InitParams params);
  ~FakeLayerTreeHost() override;

  void SetNeedsCommit() override;
  void SetNeedsUpdateLayers() override {}

  std::unique_ptr<LayerTreeHostImpl> CreateLayerTreeHostImpl(
      LayerTreeHostImplClient* client) override;

  // This method is exposed for tests that don't use a Proxy (the
  // initialization of which would call the overridden CreateLayerTreeHostImpl
  // above). This method will create a FakeLayerTreeHostImpl which is owned by
  // this object and can be accessed via host_impl(). This classs ensures that
  // the FakeLayerTreeHost doesn't then try to create a second
  // FakeLayerTreeHostImpl via a proxy.
  void CreateFakeLayerTreeHostImpl();

  LayerImpl* CommitAndCreateLayerImplTree();
  LayerImpl* CommitAndCreatePendingTree();

  FakeLayerTreeHostImpl* host_impl() { return host_impl_; }
  LayerTreeImpl* active_tree() {
    DCHECK(host_impl_);
    return host_impl_->active_tree();
  }
  LayerTreeImpl* pending_tree() {
    DCHECK(host_impl_);
    return host_impl_->pending_tree();
  }

  using LayerTreeHost::ScheduleMicroBenchmark;
  using LayerTreeHost::SendMessageToMicroBenchmark;
  using LayerTreeHost::InitializeSingleThreaded;
  using LayerTreeHost::InitializeForTesting;
  using LayerTreeHost::RecordGpuRasterizationHistogram;
  using LayerTreeHost::SetUIResourceManagerForTesting;

  void UpdateLayers() { LayerTreeHost::UpdateLayers(); }

  MicroBenchmarkController* GetMicroBenchmarkController() {
    return &micro_benchmark_controller_;
  }

  bool needs_commit() { return needs_commit_; }
  void reset_needs_commit() { needs_commit_ = false; }

  FakeLayerTreeHost(FakeLayerTreeHostClient* client,
                    LayerTreeHost::InitParams params,
                    CompositorMode mode);

 protected:
  FakeLayerTreeHostClient* client_ = nullptr;
  FakeLayerTreeHostImpl* host_impl_ = nullptr;

  bool needs_commit_ = false;

 private:
  // Used only if created via CreateFakeLayerTreeHostImpl to provide ownership
  // and lifetime management. Normally the Proxy object owns the LTHI which
  // will be the case if the test initializes a Proxy and the Impl is created
  // via the overriden CreateLayerTreeHostImpl. Tests without a Proxy that
  // manually create an Impl will call CreateFakeLayerTreeHostImpl and this
  // class will own that object. Calls should be made on the |host_impl_|
  // pointer as that'll always be set to the correct object.
  std::unique_ptr<FakeLayerTreeHostImpl> owned_host_impl_;
};

}  // namespace cc

#endif  // CC_TEST_FAKE_LAYER_TREE_HOST_H_
