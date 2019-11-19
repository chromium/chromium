// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/layer_tree_impl_test_base.h"

#include <stddef.h>

#include "cc/animation/animation.h"
#include "cc/animation/animation_host.h"
#include "cc/animation/animation_id_provider.h"
#include "cc/layers/append_quads_data.h"
#include "cc/test/animation_test_common.h"
#include "cc/test/fake_layer_tree_frame_sink.h"
#include "cc/test/mock_occlusion_tracker.h"
#include "cc/test/property_tree_test_utils.h"
#include "cc/trees/draw_property_utils.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/size_conversions.h"

namespace cc {

LayerTreeImplTestBase::LayerTreeImplTestBase()
    : LayerTreeImplTestBase(LayerListSettings()) {}

LayerTreeImplTestBase::LayerTreeImplTestBase(
    std::unique_ptr<LayerTreeFrameSink> layer_tree_frame_sink)
    : LayerTreeImplTestBase(LayerListSettings(),
                            std::move(layer_tree_frame_sink)) {}

LayerTreeImplTestBase::LayerTreeImplTestBase(const LayerTreeSettings& settings)
    : LayerTreeImplTestBase(settings, FakeLayerTreeFrameSink::Create3d()) {}

LayerTreeImplTestBase::LayerTreeImplTestBase(
    const LayerTreeSettings& settings,
    std::unique_ptr<LayerTreeFrameSink> layer_tree_frame_sink)
    : layer_tree_frame_sink_(std::move(layer_tree_frame_sink)),
      animation_host_(AnimationHost::CreateForTesting(ThreadInstance::MAIN)),
      host_(FakeLayerTreeHost::Create(&client_,
                                      &task_graph_runner_,
                                      animation_host_.get(),
                                      settings)),
      render_pass_(viz::RenderPass::Create()),
      layer_impl_id_(2) {
  std::unique_ptr<LayerImpl> root =
      LayerImpl::Create(host_impl()->active_tree(), 1);
  root->SetBounds(gfx::Size(1, 1));
  if (settings.use_layer_lists)
    SetupRootProperties(root.get());
  host_impl()->active_tree()->SetRootLayerForTesting(std::move(root));
  host_impl()->SetVisible(true);
  EXPECT_TRUE(host_impl()->InitializeFrameSink(layer_tree_frame_sink_.get()));

  const int timeline_id = AnimationIdProvider::NextTimelineId();
  timeline_ = AnimationTimeline::Create(timeline_id);
  animation_host_->AddAnimationTimeline(timeline_);
  // Create impl-side instance.
  animation_host_->PushPropertiesTo(host_impl()->animation_host());
  timeline_impl_ = host_impl()->animation_host()->GetTimelineById(timeline_id);
}

LayerTreeImplTestBase::~LayerTreeImplTestBase() {
  animation_host_->RemoveAnimationTimeline(timeline_);
  timeline_ = nullptr;
  host_impl()->ReleaseLayerTreeFrameSink();
}

LayerImpl* LayerTreeImplTestBase::EnsureRootLayerInPendingTree() {
  LayerTreeImpl* pending_tree = host_impl()->pending_tree();
  auto* root = pending_tree->root_layer();
  if (root)
    return root;
  pending_tree->SetRootLayerForTesting(LayerImpl::Create(pending_tree, 1));
  root = pending_tree->root_layer();
  root->SetBounds(gfx::Size(1, 1));
  if (host()->IsUsingLayerLists())
    SetupRootProperties(root);
  return root;
}

void LayerTreeImplTestBase::CalcDrawProps(const gfx::Size& viewport_size) {
  host_impl()->active_tree()->SetDeviceViewportRect(gfx::Rect(viewport_size));
  UpdateDrawProperties(host_impl()->active_tree());
}

void LayerTreeImplTestBase::AppendQuadsWithOcclusion(
    LayerImpl* layer_impl,
    const gfx::Rect& occluded) {
  AppendQuadsData data;

  render_pass_->quad_list.clear();
  render_pass_->shared_quad_state_list.clear();

  Occlusion occlusion(layer_impl->DrawTransform(),
                      SimpleEnclosedRegion(occluded), SimpleEnclosedRegion());
  layer_impl->draw_properties().occlusion_in_content_space = occlusion;

  if (layer_impl->WillDraw(DRAW_MODE_HARDWARE, resource_provider())) {
    layer_impl->AppendQuads(render_pass_.get(), &data);
    layer_impl->DidDraw(resource_provider());
  }
}

void LayerTreeImplTestBase::AppendQuadsForPassWithOcclusion(
    LayerImpl* layer_impl,
    viz::RenderPass* given_render_pass,
    const gfx::Rect& occluded) {
  AppendQuadsData data;

  given_render_pass->quad_list.clear();
  given_render_pass->shared_quad_state_list.clear();

  Occlusion occlusion(layer_impl->DrawTransform(),
                      SimpleEnclosedRegion(occluded), SimpleEnclosedRegion());
  layer_impl->draw_properties().occlusion_in_content_space = occlusion;

  layer_impl->WillDraw(DRAW_MODE_HARDWARE, resource_provider());
  layer_impl->AppendQuads(given_render_pass, &data);
  layer_impl->DidDraw(resource_provider());
}

void LayerTreeImplTestBase::AppendSurfaceQuadsWithOcclusion(
    RenderSurfaceImpl* surface_impl,
    const gfx::Rect& occluded) {
  AppendQuadsData data;

  render_pass_->quad_list.clear();
  render_pass_->shared_quad_state_list.clear();

  surface_impl->set_occlusion_in_content_space(
      Occlusion(gfx::Transform(), SimpleEnclosedRegion(occluded),
                SimpleEnclosedRegion()));
  surface_impl->AppendQuads(DRAW_MODE_HARDWARE, render_pass_.get(), &data);
}

void LayerTreeImplTestBase::UpdateActiveTreeDrawProperties(
    float device_scale_factor) {
  SetDeviceScaleAndUpdateViewportRect(host_impl()->active_tree(),
                                      device_scale_factor);
  UpdateDrawProperties(host_impl()->active_tree(), &update_layer_impl_list_);
}

void LayerTreeImplTestBase::UpdatePendingTreeDrawProperties(
    float device_scale_factor) {
  SetDeviceScaleAndUpdateViewportRect(host_impl()->pending_tree(),
                                      device_scale_factor);
  UpdateDrawProperties(host_impl()->pending_tree(), &update_layer_impl_list_);
}

}  // namespace cc
