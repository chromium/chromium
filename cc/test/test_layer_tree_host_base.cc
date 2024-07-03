// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/test_layer_tree_host_base.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "base/task/single_thread_task_runner.h"
#include "cc/test/fake_layer_tree_frame_sink.h"
#include "cc/test/fake_raster_source.h"
#include "cc/test/layer_test_common.h"
#include "cc/trees/layer_tree_impl.h"

namespace cc {

TestLayerTreeHostBase::TestLayerTreeHostBase()
    : task_runner_provider_(base::SingleThreadTaskRunner::GetCurrentDefault()) {
}

TestLayerTreeHostBase::~TestLayerTreeHostBase() = default;

void TestLayerTreeHostBase::SetUp() {
  layer_tree_frame_sink_ = CreateLayerTreeFrameSink();
  task_graph_runner_ = CreateTaskGraphRunner();
  host_impl_ = CreateHostImpl(CreateSettings(), &task_runner_provider_,
                              task_graph_runner_.get());
  InitializeFrameSink();
  SetInitialTreePriority();
}

void TestLayerTreeHostBase::TearDown() {
  ClearLayersAndHost();
}

void TestLayerTreeHostBase::ClearLayersAndHost() {
  pending_layer_ = nullptr;
  active_layer_ = nullptr;
  old_pending_layer_ = nullptr;
  host_impl_ = nullptr;
}

LayerTreeSettings TestLayerTreeHostBase::CreateSettings() {
  return CommitToPendingTreeLayerListSettings();
}

std::unique_ptr<LayerTreeFrameSink>
TestLayerTreeHostBase::CreateLayerTreeFrameSink() {
  return FakeLayerTreeFrameSink::Create3d();
}

std::unique_ptr<FakeLayerTreeHostImpl> TestLayerTreeHostBase::CreateHostImpl(
    const LayerTreeSettings& settings,
    TaskRunnerProvider* task_runner_provider,
    TaskGraphRunner* task_graph_runner) {
  return std::make_unique<FakeLayerTreeHostImpl>(settings, task_runner_provider,
                                                 task_graph_runner);
}

std::unique_ptr<TaskGraphRunner>
TestLayerTreeHostBase::CreateTaskGraphRunner() {
  return base::WrapUnique(new TestTaskGraphRunner);
}

void TestLayerTreeHostBase::InitializeFrameSink() {
  host_impl_->SetVisible(true);
  host_impl_->InitializeFrameSink(layer_tree_frame_sink_.get());
}

void TestLayerTreeHostBase::ResetLayerTreeFrameSink(
    std::unique_ptr<LayerTreeFrameSink> layer_tree_frame_sink) {
  host_impl()->DidLoseLayerTreeFrameSink();
  host_impl()->SetVisible(true);
  host_impl()->InitializeFrameSink(layer_tree_frame_sink.get());
  layer_tree_frame_sink_ = std::move(layer_tree_frame_sink);
}

std::unique_ptr<FakeLayerTreeHostImpl> TestLayerTreeHostBase::TakeHostImpl() {
  return std::move(host_impl_);
}

void TestLayerTreeHostBase::SetupDefaultTrees(const gfx::Size& layer_bounds) {
  scoped_refptr<FakeRasterSource> pending_raster_source =
      FakeRasterSource::CreateFilledWithText(layer_bounds);
  scoped_refptr<FakeRasterSource> active_raster_source =
      FakeRasterSource::CreateFilledWithText(layer_bounds);

  SetupTrees(std::move(pending_raster_source), std::move(active_raster_source));
}

void TestLayerTreeHostBase::SetupTrees(
    scoped_refptr<RasterSource> pending_raster_source,
    scoped_refptr<RasterSource> active_raster_source) {
  SetupSyncTree(std::move(active_raster_source));
  if (!host_impl()->CommitsToActiveTree()) {
    ActivateTree();
    SetupSyncTree(std::move(pending_raster_source));
  }
}

void TestLayerTreeHostBase::SetupSyncTree(
    scoped_refptr<RasterSource> raster_source,
    const gfx::Size& tile_size,
    const Region& invalidation) {
  if (host_impl()->CommitsToActiveTree()) {
    host_impl()->active_tree()->PushPageScaleFromMainThread(1.f, 0.00001f,
                                                            100000.f);
  } else {
    host_impl()->CreatePendingTree();
    host_impl()->pending_tree()->PushPageScaleFromMainThread(1.f, 0.00001f,
                                                             100000.f);
    host_impl()->pending_tree()->SetDeviceViewportRect(
        host_impl()->active_tree()->GetDeviceViewport());
    host_impl()->pending_tree()->SetDeviceScaleFactor(
        host_impl()->active_tree()->device_scale_factor());
  }

  LayerTreeImpl* sync_tree = host_impl()->sync_tree();
  LayerImpl* sync_root = sync_tree->root_layer();
  CHECK(!pending_layer_);
  CHECK(!sync_root || sync_root->id() == root_id_);
  FakePictureLayerImpl* sync_layer = nullptr;

  if (!sync_root) {
    sync_tree->SetRootLayerForTesting(LayerImpl::Create(sync_tree, root_id_));
    sync_root = sync_tree->root_layer();

    auto* page_scale_layer = AddLayer<LayerImpl>(sync_tree);
    sync_layer = AddLayer<FakePictureLayerImpl>(sync_tree);
    sync_layer->SetDrawsContent(true);

    sync_tree->SetElementIdsForTesting();
    SetupRootProperties(sync_root);
    CopyProperties(sync_root, page_scale_layer);
    CreateTransformNode(page_scale_layer).in_subtree_of_page_scale_layer = true;
    CopyProperties(page_scale_layer, sync_layer);
    CreateTransformNode(sync_layer);
    CreateScrollNode(sync_layer, gfx::Size(1, 1));

    auto viewport_property_ids = sync_tree->ViewportPropertyIdsForTesting();
    viewport_property_ids.page_scale_transform =
        page_scale_layer->transform_tree_index();
    sync_tree->SetViewportPropertyIds(viewport_property_ids);

    if (host_impl()->CommitsToActiveTree()) {
      active_layer_ = sync_layer;
    } else {
      pending_layer_ = sync_layer;
    }
  } else if (host_impl()->CommitsToActiveTree()) {
    sync_layer = active_layer_;
  } else {
    // Steal from the recycled tree if possible.
    pending_layer_ = old_pending_layer_;
    old_pending_layer_ = nullptr;
    sync_layer = pending_layer_;
  }

  if (!tile_size.IsEmpty()) {
    sync_layer->set_fixed_tile_size(tile_size);
  }

  // The bounds() just mirror the raster source size.
  if (raster_source) {
    sync_layer->SetBounds(raster_source->size());
    sync_layer->SetRasterSource(raster_source, invalidation);
  }
  sync_layer->SetNeedsPushProperties();

  sync_tree->set_needs_update_draw_properties();
  UpdateDrawProperties(sync_tree);
}

void TestLayerTreeHostBase::ActivateTree() {
  CHECK(!host_impl()->CommitsToActiveTree());
  UpdateDrawProperties(host_impl()->pending_tree());

  host_impl()->ActivateSyncTree();
  CHECK(!host_impl()->pending_tree());
  CHECK(host_impl()->recycle_tree());
  old_pending_layer_ = pending_layer_;
  pending_layer_ = nullptr;
  active_layer_ = static_cast<FakePictureLayerImpl*>(
      host_impl()->active_tree()->LayerById(old_pending_layer_->id()));

  UpdateDrawProperties(host_impl()->active_tree());
}

void TestLayerTreeHostBase::PerformImplSideInvalidation() {
  DCHECK(host_impl()->active_tree());
  DCHECK(!host_impl()->pending_tree());
  DCHECK(host_impl()->recycle_tree());

  host_impl()->CreatePendingTree();
  host_impl()->sync_tree()->InvalidateRegionForImages(
      host_impl()->tile_manager()->TakeImagesToInvalidateOnSyncTree());
  pending_layer_ = old_pending_layer_;
  old_pending_layer_ = nullptr;
}

void TestLayerTreeHostBase::SetInitialTreePriority() {
  GlobalStateThatImpactsTilePriority state;

  state.soft_memory_limit_in_bytes = 100 * 1000 * 1000;
  state.num_resources_limit = 10000;
  state.hard_memory_limit_in_bytes = state.soft_memory_limit_in_bytes * 2;
  state.memory_limit_policy = ALLOW_ANYTHING;
  state.tree_priority = SAME_PRIORITY_FOR_BOTH_TREES;

  host_impl_->resource_pool()->SetResourceUsageLimits(
      state.soft_memory_limit_in_bytes, state.num_resources_limit);
  host_impl_->tile_manager()->SetGlobalStateForTesting(state);
}

void TestLayerTreeHostBase::ResetTrees() {
  host_impl_->ResetTreesForTesting();
  pending_layer_ = old_pending_layer_ = active_layer_ = nullptr;
}

}  // namespace cc
