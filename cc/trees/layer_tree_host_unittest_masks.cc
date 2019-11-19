// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/layer_tree_host.h"

#include "base/time/time.h"
#include "cc/test/fake_picture_layer.h"
#include "cc/test/fake_recording_source.h"
#include "cc/test/layer_tree_test.h"
#include "cc/trees/layer_tree_impl.h"
#include "components/viz/common/quads/render_pass_draw_quad.h"

namespace cc {
namespace {

class LayerTreeTestMaskLayerForSurfaceWithContentRectNotAtOrigin
    : public LayerTreeTest {
 protected:
  void SetupTree() override {
    // The masked layer has bounds 50x50, but it has a child that causes
    // the surface bounds to be larger. It also has a parent that clips the
    // masked layer and its surface.

    SetInitialRootBounds(gfx::Size(100, 100));
    LayerTreeTest::SetupTree();
    Layer* root = layer_tree_host()->root_layer();

    scoped_refptr<FakePictureLayer> content_layer =
        FakePictureLayer::Create(&client_);

    std::unique_ptr<RecordingSource> recording_source =
        FakeRecordingSource::CreateFilledRecordingSource(gfx::Size(100, 100));
    PaintFlags paint1, paint2;
    static_cast<FakeRecordingSource*>(recording_source.get())
        ->add_draw_rect_with_flags(gfx::Rect(0, 0, 100, 90), paint1);
    static_cast<FakeRecordingSource*>(recording_source.get())
        ->add_draw_rect_with_flags(gfx::Rect(0, 90, 100, 10), paint2);
    client_.set_fill_with_nonsolid_color(true);
    static_cast<FakeRecordingSource*>(recording_source.get())->Rerecord();

    scoped_refptr<FakePictureLayer> mask_layer =
        FakePictureLayer::CreateWithRecordingSource(
            &client_, std::move(recording_source));
    content_layer->SetMaskLayer(mask_layer);

    gfx::Size layer_size(100, 100);
    content_layer->SetBounds(layer_size);

    gfx::Size mask_size(100, 100);
    mask_layer->SetBounds(mask_size);
    mask_layer_id_ = mask_layer->id();

    scoped_refptr<Layer> clip_layer = Layer::Create();
    clip_layer->SetBounds(gfx::Size(50, 50));
    clip_layer->SetMasksToBounds(true);

    scoped_refptr<Layer> scroll_layer = Layer::Create();
    scroll_layer->SetBounds(layer_size);
    scroll_layer->SetScrollable(gfx::Size(50, 50));
    scroll_layer->SetMasksToBounds(true);
    scroll_layer->SetElementId(
        LayerIdToElementIdForTesting(scroll_layer->id()));

    root->AddChild(clip_layer);
    clip_layer->AddChild(scroll_layer);
    scroll_layer->AddChild(content_layer);

    client_.set_bounds(root->bounds());
    scroll_layer->SetScrollOffset(gfx::ScrollOffset(50, 50));
  }

  void BeginTest() override { PostSetNeedsCommitToMainThread(); }

  DrawResult PrepareToDrawOnThread(LayerTreeHostImpl* host_impl,
                                   LayerTreeHostImpl::FrameData* frame_data,
                                   DrawResult draw_result) override {
    EXPECT_EQ(3u, frame_data->render_passes.size());
    viz::RenderPass* root_pass = frame_data->render_passes.back().get();
    EXPECT_EQ(2u, root_pass->quad_list.size());

    // There's a solid color quad under everything.
    EXPECT_EQ(viz::DrawQuad::Material::kSolidColor,
              root_pass->quad_list.back()->material);

    EXPECT_EQ(viz::DrawQuad::Material::kRenderPass,
              root_pass->quad_list.front()->material);
    const viz::RenderPassDrawQuad* render_pass_quad =
        viz::RenderPassDrawQuad::MaterialCast(root_pass->quad_list.front());
    gfx::Rect rect_in_target_space = MathUtil::MapEnclosingClippedRect(
        render_pass_quad->shared_quad_state->quad_to_target_transform,
        render_pass_quad->rect);
    EXPECT_EQ(gfx::Rect(0, 0, 50, 50), rect_in_target_space);

    // We use kDstIn blend mode instead of the mask feature of RenderPass.
    EXPECT_EQ(gfx::RectF(), render_pass_quad->mask_uv_rect);
    viz::RenderPass* mask_pass = frame_data->render_passes[1].get();
    EXPECT_EQ(SkBlendMode::kDstIn,
              mask_pass->quad_list.front()->shared_quad_state->blend_mode);
    EndTest();
    return draw_result;
  }

  int mask_layer_id_;
  FakeContentLayerClient client_;
};

SINGLE_AND_MULTI_THREAD_TEST_F(
    LayerTreeTestMaskLayerForSurfaceWithContentRectNotAtOrigin);

class LayerTreeTestMaskLayerForSurfaceWithContentRectNotAtOriginWithLayerList
    : public LayerTreeTest {
 protected:
  LayerTreeTestMaskLayerForSurfaceWithContentRectNotAtOriginWithLayerList() {
    SetUseLayerLists();
  }

  void SetupTree() override {
    // The masked layer has bounds 50x50, but it has a child that causes
    // the surface bounds to be larger. It also has a parent that clips the
    // masked layer and its surface.

    SetInitialRootBounds(gfx::Size(100, 100));
    LayerTreeTest::SetupTree();

    Layer* root = layer_tree_host()->root_layer();

    gfx::Size layer_size(100, 100);
    SetupViewport(root, gfx::Size(50, 50), layer_size);

    auto* scroll = layer_tree_host()->OuterViewportScrollLayerForTesting();
    SetScrollOffset(scroll, gfx::ScrollOffset(50, 50));

    client_.set_bounds(root->bounds());
    auto content_layer = FakePictureLayer::Create(&client_);
    content_layer->SetBounds(layer_size);
    CopyProperties(scroll, content_layer.get());
    root->AddChild(content_layer);

    std::unique_ptr<RecordingSource> recording_source =
        FakeRecordingSource::CreateFilledRecordingSource(gfx::Size(100, 100));
    PaintFlags paint1, paint2;
    static_cast<FakeRecordingSource*>(recording_source.get())
        ->add_draw_rect_with_flags(gfx::Rect(0, 0, 100, 90), paint1);
    static_cast<FakeRecordingSource*>(recording_source.get())
        ->add_draw_rect_with_flags(gfx::Rect(0, 90, 100, 10), paint2);
    client_.set_fill_with_nonsolid_color(true);
    static_cast<FakeRecordingSource*>(recording_source.get())->Rerecord();

    auto mask_layer = FakePictureLayer::CreateWithRecordingSource(
        &client_, std::move(recording_source));
    SetupMaskProperties(content_layer.get(), mask_layer.get());
    root->AddChild(mask_layer);

    mask_layer_id_ = mask_layer->id();
  }

  void BeginTest() override { PostSetNeedsCommitToMainThread(); }

  DrawResult PrepareToDrawOnThread(LayerTreeHostImpl* host_impl,
                                   LayerTreeHostImpl::FrameData* frame_data,
                                   DrawResult draw_result) override {
    EXPECT_EQ(1u, frame_data->render_passes.size());
    viz::RenderPass* pass = frame_data->render_passes.back().get();
    EXPECT_EQ(3u, pass->quad_list.size());

    // There's a solid color quad under everything.
    EXPECT_EQ(viz::DrawQuad::Material::kSolidColor,
              pass->quad_list.back()->material);

    EXPECT_EQ(viz::DrawQuad::Material::kTiledContent,
              pass->quad_list.ElementAt(1)->material);

    auto* mask_quad = pass->quad_list.front();
    EXPECT_EQ(viz::DrawQuad::Material::kTiledContent, mask_quad->material);
    gfx::Rect rect_in_target_space = MathUtil::MapEnclosingClippedRect(
        mask_quad->shared_quad_state->quad_to_target_transform,
        mask_quad->rect);
    EXPECT_EQ(gfx::Rect(0, 0, 50, 50), rect_in_target_space);
    // We use kDstIn blend mode for mask.
    EXPECT_EQ(SkBlendMode::kDstIn, mask_quad->shared_quad_state->blend_mode);
    EndTest();
    return draw_result;
  }

  int mask_layer_id_;
  FakeContentLayerClient client_;
};

SINGLE_AND_MULTI_THREAD_TEST_F(
    LayerTreeTestMaskLayerForSurfaceWithContentRectNotAtOriginWithLayerList);

class LayerTreeTestMaskLayerForSurfaceWithClippedLayer : public LayerTreeTest {
 protected:
  void SetupTree() override {
    // The masked layer has bounds 50x50, but it has a child that causes
    // the surface bounds to be larger. It also has a parent that clips the
    // masked layer and its surface.

    scoped_refptr<Layer> root = Layer::Create();

    scoped_refptr<Layer> clipping_layer = Layer::Create();
    root->AddChild(clipping_layer);

    scoped_refptr<FakePictureLayer> content_layer =
        FakePictureLayer::Create(&client_);
    clipping_layer->AddChild(content_layer);

    scoped_refptr<FakePictureLayer> content_child_layer =
        FakePictureLayer::Create(&client_);
    content_layer->AddChild(content_child_layer);

    std::unique_ptr<RecordingSource> recording_source =
        FakeRecordingSource::CreateFilledRecordingSource(gfx::Size(50, 50));
    PaintFlags paint1, paint2;
    static_cast<FakeRecordingSource*>(recording_source.get())
        ->add_draw_rect_with_flags(gfx::Rect(0, 0, 50, 40), paint1);
    static_cast<FakeRecordingSource*>(recording_source.get())
        ->add_draw_rect_with_flags(gfx::Rect(0, 40, 50, 10), paint2);
    client_.set_fill_with_nonsolid_color(true);
    static_cast<FakeRecordingSource*>(recording_source.get())->Rerecord();

    scoped_refptr<FakePictureLayer> mask_layer =
        FakePictureLayer::CreateWithRecordingSource(
            &client_, std::move(recording_source));
    content_layer->SetMaskLayer(mask_layer);

    gfx::Size root_size(100, 100);
    root->SetBounds(root_size);

    gfx::PointF clipping_origin(20.f, 10.f);
    gfx::Size clipping_size(10, 20);
    clipping_layer->SetBounds(clipping_size);
    clipping_layer->SetPosition(clipping_origin);
    clipping_layer->SetMasksToBounds(true);

    gfx::Size layer_size(50, 50);
    content_layer->SetBounds(layer_size);
    content_layer->SetPosition(gfx::PointF() -
                               clipping_origin.OffsetFromOrigin());

    gfx::Size child_size(50, 50);
    content_child_layer->SetBounds(child_size);
    content_child_layer->SetPosition(gfx::PointF(20.f, 0.f));

    gfx::Size mask_size(50, 50);
    mask_layer->SetBounds(mask_size);
    mask_layer_id_ = mask_layer->id();

    layer_tree_host()->SetRootLayer(root);
    LayerTreeTest::SetupTree();
    client_.set_bounds(root->bounds());
  }

  void BeginTest() override { PostSetNeedsCommitToMainThread(); }

  DrawResult PrepareToDrawOnThread(LayerTreeHostImpl* host_impl,
                                   LayerTreeHostImpl::FrameData* frame_data,
                                   DrawResult draw_result) override {
    EXPECT_EQ(3u, frame_data->render_passes.size());
    viz::RenderPass* root_pass = frame_data->render_passes.back().get();
    EXPECT_EQ(2u, root_pass->quad_list.size());

    // There's a solid color quad under everything.
    EXPECT_EQ(viz::DrawQuad::Material::kSolidColor,
              root_pass->quad_list.back()->material);

    // The surface is clipped to 10x20.
    EXPECT_EQ(viz::DrawQuad::Material::kRenderPass,
              root_pass->quad_list.front()->material);
    const viz::RenderPassDrawQuad* render_pass_quad =
        viz::RenderPassDrawQuad::MaterialCast(root_pass->quad_list.front());
    gfx::Rect rect_in_target_space = MathUtil::MapEnclosingClippedRect(
        render_pass_quad->shared_quad_state->quad_to_target_transform,
        render_pass_quad->rect);
    EXPECT_EQ(gfx::Rect(20, 10, 10, 20).ToString(),
              rect_in_target_space.ToString());

    // We use kDstIn blend mode instead of the mask feature of RenderPass.
    EXPECT_EQ(gfx::RectF(), render_pass_quad->mask_uv_rect);
    viz::RenderPass* mask_pass = frame_data->render_passes[1].get();
    EXPECT_EQ(SkBlendMode::kDstIn,
              mask_pass->quad_list.front()->shared_quad_state->blend_mode);
    EndTest();
    return draw_result;
  }

  int mask_layer_id_;
  FakeContentLayerClient client_;
};

SINGLE_AND_MULTI_THREAD_TEST_F(
    LayerTreeTestMaskLayerForSurfaceWithClippedLayer);

class LayerTreeTestMaskLayerForSurfaceWithDifferentScale
    : public LayerTreeTest {
 protected:
  void SetupTree() override {
    // The masked layer has bounds 50x50, but it has a child that causes
    // the surface bounds to be larger. It also has a parent that clips the
    // masked layer and its surface.

    scoped_refptr<Layer> root = Layer::Create();

    scoped_refptr<Layer> clipping_scaling_layer = Layer::Create();
    root->AddChild(clipping_scaling_layer);

    scoped_refptr<FakePictureLayer> content_layer =
        FakePictureLayer::Create(&client_);
    clipping_scaling_layer->AddChild(content_layer);

    scoped_refptr<FakePictureLayer> content_child_layer =
        FakePictureLayer::Create(&client_);
    content_layer->AddChild(content_child_layer);

    std::unique_ptr<RecordingSource> recording_source =
        FakeRecordingSource::CreateFilledRecordingSource(gfx::Size(50, 50));
    PaintFlags paint1, paint2;
    static_cast<FakeRecordingSource*>(recording_source.get())
        ->add_draw_rect_with_flags(gfx::Rect(0, 0, 50, 40), paint1);
    static_cast<FakeRecordingSource*>(recording_source.get())
        ->add_draw_rect_with_flags(gfx::Rect(0, 40, 50, 10), paint2);
    client_.set_fill_with_nonsolid_color(true);
    static_cast<FakeRecordingSource*>(recording_source.get())->Rerecord();

    scoped_refptr<FakePictureLayer> mask_layer =
        FakePictureLayer::CreateWithRecordingSource(
            &client_, std::move(recording_source));
    content_layer->SetMaskLayer(mask_layer);

    gfx::Size root_size(100, 100);
    root->SetBounds(root_size);

    gfx::Transform scale;
    scale.Scale(2, 2);
    gfx::PointF clipping_origin(20.f, 10.f);
    gfx::Size clipping_size(10, 20);
    clipping_scaling_layer->SetBounds(clipping_size);
    clipping_scaling_layer->SetPosition(clipping_origin);
    // This changes scale between contributing layer and render surface to 2.
    clipping_scaling_layer->SetTransform(scale);
    clipping_scaling_layer->SetMasksToBounds(true);

    gfx::Size layer_size(50, 50);
    content_layer->SetBounds(layer_size);
    content_layer->SetPosition(gfx::PointF() -
                               clipping_origin.OffsetFromOrigin());

    gfx::Size child_size(50, 50);
    content_child_layer->SetBounds(child_size);
    content_child_layer->SetPosition(gfx::PointF(20.f, 0.f));

    gfx::Size mask_size(50, 50);
    mask_layer->SetBounds(mask_size);
    // Setting will change transform on mask layer will make it not adjust
    // raster scale, which will remain 1. This means the mask_layer and render
    // surface will have a scale of 2 during draw time.
    mask_layer->SetHasWillChangeTransformHint(true);
    mask_layer_id_ = mask_layer->id();

    layer_tree_host()->SetRootLayer(root);
    LayerTreeTest::SetupTree();
    client_.set_bounds(root->bounds());
  }

  void BeginTest() override { PostSetNeedsCommitToMainThread(); }

  DrawResult PrepareToDrawOnThread(LayerTreeHostImpl* host_impl,
                                   LayerTreeHostImpl::FrameData* frame_data,
                                   DrawResult draw_result) override {
    EXPECT_EQ(3u, frame_data->render_passes.size());
    viz::RenderPass* root_pass = frame_data->render_passes.back().get();
    EXPECT_EQ(2u, root_pass->quad_list.size());

    // There's a solid color quad under everything.
    EXPECT_EQ(viz::DrawQuad::Material::kSolidColor,
              root_pass->quad_list.back()->material);

    // The surface is clipped to 10x20, and then scaled by 2, which ends up
    // being 20x40.
    EXPECT_EQ(viz::DrawQuad::Material::kRenderPass,
              root_pass->quad_list.front()->material);
    const viz::RenderPassDrawQuad* render_pass_quad =
        viz::RenderPassDrawQuad::MaterialCast(root_pass->quad_list.front());
    gfx::Rect rect_in_target_space = MathUtil::MapEnclosingClippedRect(
        render_pass_quad->shared_quad_state->quad_to_target_transform,
        render_pass_quad->rect);
    EXPECT_EQ(gfx::Rect(20, 10, 20, 40).ToString(),
              rect_in_target_space.ToString());
    gfx::Rect visible_rect_in_target_space = MathUtil::MapEnclosingClippedRect(
        render_pass_quad->shared_quad_state->quad_to_target_transform,
        render_pass_quad->visible_rect);
    EXPECT_EQ(gfx::Rect(20, 10, 20, 40).ToString(),
              visible_rect_in_target_space.ToString());

    // We use kDstIn blend mode instead of the mask feature of RenderPass.
    EXPECT_EQ(gfx::RectF(), render_pass_quad->mask_uv_rect);
    viz::RenderPass* mask_pass = frame_data->render_passes[1].get();
    EXPECT_EQ(SkBlendMode::kDstIn,
              mask_pass->quad_list.front()->shared_quad_state->blend_mode);
    EndTest();
    return draw_result;
  }

  int mask_layer_id_;
  FakeContentLayerClient client_;
};

SINGLE_AND_MULTI_THREAD_TEST_F(
    LayerTreeTestMaskLayerForSurfaceWithDifferentScale);

class LayerTreeTestMaskLayerWithScaling : public LayerTreeTest {
 protected:
  void SetupTree() override {
    // Root
    //  |
    //  +-- Scaling Layer (adds a 2x scale)
    //       |
    //       +-- Content Layer
    //             +--Mask

    scoped_refptr<Layer> root = Layer::Create();

    scoped_refptr<Layer> scaling_layer = Layer::Create();
    root->AddChild(scaling_layer);

    scoped_refptr<FakePictureLayer> content_layer =
        FakePictureLayer::Create(&client_);
    scaling_layer->AddChild(content_layer);

    std::unique_ptr<RecordingSource> recording_source =
        FakeRecordingSource::CreateFilledRecordingSource(gfx::Size(100, 100));
    PaintFlags paint1, paint2;
    static_cast<FakeRecordingSource*>(recording_source.get())
        ->add_draw_rect_with_flags(gfx::Rect(0, 0, 100, 10), paint1);
    static_cast<FakeRecordingSource*>(recording_source.get())
        ->add_draw_rect_with_flags(gfx::Rect(0, 10, 100, 90), paint2);
    client_.set_fill_with_nonsolid_color(true);
    static_cast<FakeRecordingSource*>(recording_source.get())->Rerecord();

    scoped_refptr<FakePictureLayer> mask_layer =
        FakePictureLayer::CreateWithRecordingSource(
            &client_, std::move(recording_source));
    content_layer->SetMaskLayer(mask_layer);

    gfx::Size root_size(100, 100);
    root->SetBounds(root_size);

    gfx::Size scaling_layer_size(50, 50);
    scaling_layer->SetBounds(scaling_layer_size);
    gfx::Transform scale;
    scale.Scale(2.f, 2.f);
    scaling_layer->SetTransform(scale);

    content_layer->SetBounds(scaling_layer_size);
    mask_layer->SetBounds(scaling_layer_size);

    layer_tree_host()->SetRootLayer(root);
    LayerTreeTest::SetupTree();
    client_.set_bounds(root->bounds());
  }

  void BeginTest() override { PostSetNeedsCommitToMainThread(); }

  DrawResult PrepareToDrawOnThread(LayerTreeHostImpl* host_impl,
                                   LayerTreeHostImpl::FrameData* frame_data,
                                   DrawResult draw_result) override {
    EXPECT_EQ(3u, frame_data->render_passes.size());
    viz::RenderPass* root_pass = frame_data->render_passes.back().get();
    EXPECT_EQ(2u, root_pass->quad_list.size());

    // There's a solid color quad under everything.
    EXPECT_EQ(viz::DrawQuad::Material::kSolidColor,
              root_pass->quad_list.back()->material);

    EXPECT_EQ(viz::DrawQuad::Material::kRenderPass,
              root_pass->quad_list.front()->material);
    const viz::RenderPassDrawQuad* render_pass_quad =
        viz::RenderPassDrawQuad::MaterialCast(root_pass->quad_list.front());
    gfx::Rect rect_in_target_space = MathUtil::MapEnclosingClippedRect(
        render_pass_quad->shared_quad_state->quad_to_target_transform,
        render_pass_quad->rect);

    // We use kDstIn blend mode instead of the mask feature of RenderPass.
    EXPECT_EQ(gfx::RectF(), render_pass_quad->mask_uv_rect);
    viz::RenderPass* mask_pass = frame_data->render_passes[1].get();
    EXPECT_EQ(SkBlendMode::kDstIn,
              mask_pass->quad_list.front()->shared_quad_state->blend_mode);

    switch (host_impl->active_tree()->source_frame_number()) {
      case 0:
        // Check that the tree scaling is correctly taken into account for the
        // mask, that should fully map onto the quad.
        EXPECT_EQ(gfx::Rect(0, 0, 100, 100).ToString(),
                  rect_in_target_space.ToString());
        break;
      case 1:
        // Applying a DSF should change the render surface size, but won't
        // affect which part of the mask is used.
        EXPECT_EQ(gfx::Rect(0, 0, 200, 200).ToString(),
                  rect_in_target_space.ToString());
        EndTest();
        break;
    }
    return draw_result;
  }

  void DidCommit() override {
    switch (layer_tree_host()->SourceFrameNumber()) {
      case 1:
        gfx::Size double_root_size(200, 200);
        GenerateNewLocalSurfaceId();
        layer_tree_host()->SetViewportRectAndScale(
            gfx::Rect(double_root_size), 2.f,
            GetCurrentLocalSurfaceIdAllocation());
        break;
    }
  }

  FakeContentLayerClient client_;
};

SINGLE_AND_MULTI_THREAD_TEST_F(LayerTreeTestMaskLayerWithScaling);

class LayerTreeTestMaskWithNonExactTextureSize : public LayerTreeTest {
 protected:
  void SetupTree() override {
    // The masked layer has bounds 100x100, but is allocated a 120x150 texture.

    scoped_refptr<Layer> root = Layer::Create();

    scoped_refptr<FakePictureLayer> content_layer =
        FakePictureLayer::Create(&client_);
    root->AddChild(content_layer);

    std::unique_ptr<RecordingSource> recording_source =
        FakeRecordingSource::CreateFilledRecordingSource(gfx::Size(100, 100));
    PaintFlags paint1, paint2;
    static_cast<FakeRecordingSource*>(recording_source.get())
        ->add_draw_rect_with_flags(gfx::Rect(0, 0, 100, 90), paint1);
    static_cast<FakeRecordingSource*>(recording_source.get())
        ->add_draw_rect_with_flags(gfx::Rect(0, 90, 100, 10), paint2);
    client_.set_fill_with_nonsolid_color(true);
    static_cast<FakeRecordingSource*>(recording_source.get())->Rerecord();

    scoped_refptr<FakePictureLayer> mask_layer =
        FakePictureLayer::CreateWithRecordingSource(
            &client_, std::move(recording_source));
    content_layer->SetMaskLayer(mask_layer);

    gfx::Size root_size(100, 100);
    root->SetBounds(root_size);

    gfx::Size layer_size(100, 100);
    content_layer->SetBounds(layer_size);

    gfx::Size mask_size(100, 100);
    gfx::Size mask_texture_size(120, 150);
    mask_layer->SetBounds(mask_size);
    mask_layer->set_fixed_tile_size(mask_texture_size);

    layer_tree_host()->SetRootLayer(root);
    LayerTreeTest::SetupTree();
    client_.set_bounds(root->bounds());
  }

  void BeginTest() override { PostSetNeedsCommitToMainThread(); }

  DrawResult PrepareToDrawOnThread(LayerTreeHostImpl* host_impl,
                                   LayerTreeHostImpl::FrameData* frame_data,
                                   DrawResult draw_result) override {
    EXPECT_EQ(3u, frame_data->render_passes.size());
    viz::RenderPass* root_pass = frame_data->render_passes.back().get();
    EXPECT_EQ(2u, root_pass->quad_list.size());

    // There's a solid color quad under everything.
    EXPECT_EQ(viz::DrawQuad::Material::kSolidColor,
              root_pass->quad_list.back()->material);

    // The surface is 100x100
    EXPECT_EQ(viz::DrawQuad::Material::kRenderPass,
              root_pass->quad_list.front()->material);
    const viz::RenderPassDrawQuad* render_pass_quad =
        viz::RenderPassDrawQuad::MaterialCast(root_pass->quad_list.front());
    EXPECT_EQ(gfx::Rect(0, 0, 100, 100).ToString(),
              render_pass_quad->rect.ToString());

    // We use kDstIn blend mode instead of the mask feature of RenderPass.
    EXPECT_EQ(gfx::RectF(), render_pass_quad->mask_uv_rect);
    viz::RenderPass* mask_pass = frame_data->render_passes[1].get();
    EXPECT_EQ(SkBlendMode::kDstIn,
              mask_pass->quad_list.front()->shared_quad_state->blend_mode);
    EndTest();
    return draw_result;
  }

  int mask_layer_id_;
  FakeContentLayerClient client_;
};

SINGLE_AND_MULTI_THREAD_TEST_F(LayerTreeTestMaskWithNonExactTextureSize);

}  // namespace
}  // namespace cc
