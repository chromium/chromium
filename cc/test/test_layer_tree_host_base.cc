// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/test_layer_tree_host_base.h"

#include "base/memory/ptr_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "cc/test/fake_layer_tree_frame_sink.h"
#include "cc/test/fake_raster_source.h"
#include "cc/test/layer_test_common.h"
#include "cc/trees/layer_tree_impl.h"

namespace cc {

TestLayerTreeHostBase::TestLayerTreeHostBase()
    : task_runner_provider_(base::ThreadTaskRunnerHandle::Get()),
      pending_layer_(nullptr),
      active_layer_(nullptr),
      old_pending_layer_(nullptr),
      root_id_(1),
      next_layer_id_(2) {}

TestLayerTreeHostBase::~TestLayerTreeHostBase() = default;

void TestLayerTreeHostBase::SetUp() {
  layer_tree_frame_sink_ = CreateLayerTreeFrameSink();
  task_graph_runner_ = CreateTaskGraphRunner();
  host_impl_ = CreateHostImpl(CreateSettings(), &task_runner_provider_,
                              task_graph_runner_.get());
  InitializeFrameSink();
  SetInitialTreePriority();
}

LayerTreeSettings TestLayerTreeHostBase::CreateSettings() {
  return LayerListSettings();
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
      FakeRasterSource::CreateFilled(layer_bounds);
  scoped_refptr<FakeRasterSource> active_raster_source =
      FakeRasterSource::CreateFilled(layer_bounds);

  SetupTrees(std::move(pending_raster_source), std::move(active_raster_source));
}

void TestLayerTreeHostBase::SetupTrees(
    scoped_refptr<RasterSource> pending_raster_source,
    scoped_refptr<RasterSource> active_raster_source) {
  SetupPendingTree(std::move(active_raster_source));
  ActivateTree();
  SetupPendingTree(std::move(pending_raster_source));
}

void TestLayerTreeHostBase::SetupPendingTree(
    scoped_refptr<RasterSource> raster_source) {
  SetupPendingTree(std::move(raster_source), gfx::Size(), Region());
}

void TestLayerTreeHostBase::SetupPendingTree(
    scoped_refptr<RasterSource> raster_source,
    const gfx::Size& tile_size,
    const Region& invalidation) {
  host_impl()->CreatePendingTree();
  host_impl()->pending_tree()->PushPageScaleFromMainThread(1.f, 0.00001f,
                                                           100000.f);
  LayerTreeImpl* pending_tree = host_impl()->pending_tree();
  pending_tree->SetDeviceViewportRect(
      host_impl()->active_tree()->GetDeviceViewport());
  pending_tree->SetDeviceScaleFactor(
      host_impl()->active_tree()->device_scale_factor());

  // Steal from the recycled tree if possible.
  LayerImpl* pending_root = pending_tree->root_layer();
  DCHECK(!pending_layer_);
  DCHECK(!pending_root || pending_root->id() == root_id_);

  if (!pending_root) {
    pending_tree->SetRootLayerForTesting(
        LayerImpl::Create(pending_tree, root_id_));
    pending_root = pending_tree->root_layer();

    auto* page_scale_layer = AddLayer<LayerImpl>(pending_tree);
    pending_layer_ = AddLayer<FakePictureLayerImpl>(pending_tree);
    pending_layer_->SetDrawsContent(true);
    pending_layer_->SetScrollable(gfx::Size(1, 1));

    pending_tree->SetElementIdsForTesting();
    SetupRootProperties(pending_root);
    CopyProperties(pending_root, page_scale_layer);
    CreateTransformNode(page_scale_layer).in_subtree_of_page_scale_layer = true;
    CopyProperties(page_scale_layer, pending_layer_);
    CreateTransformNode(pending_layer_);
    CreateScrollNode(pending_layer_);

    auto viewport_property_ids = pending_tree->ViewportPropertyIdsForTesting();
    viewport_property_ids.page_scale_transform =
        page_scale_layer->transform_tree_index();
    pending_tree->SetViewportPropertyIds(viewport_property_ids);
  } else {
    pending_layer_ = old_pending_layer_;
    old_pending_layer_ = nullptr;
  }

  if (!tile_size.IsEmpty())
    pending_layer_->set_fixed_tile_size(tile_size);

  // The bounds() just mirror the raster source size.
  if (raster_source) {
    pending_layer_->SetBounds(raster_source->GetSize());
    pending_layer_->SetRasterSource(raster_source, invalidation);
  }

  host_impl()->pending_tree()->set_needs_update_draw_properties();
  UpdateDrawProperties(host_impl()->pending_tree());
}

void TestLayerTreeHostBase::ActivateTree() {
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
