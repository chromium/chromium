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

    scoped_refptr<Layer> root = Layer::Create();

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
    content_layer->SetMaskLayer(mask_layer.get());

    gfx::Size root_size(100, 100);
    root->SetBounds(root_size);

    gfx::Size layer_size(100, 100);
    content_layer->SetBounds(layer_size);

    gfx::Size mask_size(100, 100);
    mask_layer->SetBounds(mask_size);
    mask_layer->SetLayerMaskType(Layer::LayerMaskType::MULTI_TEXTURE_MASK);
    mask_layer_id_ = mask_layer->id();

    layer_tree_host()->SetRootLayer(root);
    LayerTreeTest::SetupTree();
    scoped_refptr<Layer> outer_viewport_scroll_layer = Layer::Create();
    outer_viewport_scroll_layer->SetBounds(layer_size);
    CreateVirtualViewportLayers(root.get(), outer_viewport_scroll_layer,
                                gfx::Size(50, 50), gfx::Size(50, 50),
                                layer_tree_host());
    layer_tree_host()->outer_viewport_container_layer()->SetMasksToBounds(true);
    outer_viewport_scroll_layer->AddChild(content_layer);

    client_.set_bounds(root->bounds());
    outer_viewport_scroll_layer->SetScrollOffset(gfx::ScrollOffset(50, 50));
  }

  void BeginTest() override { PostSetNeedsCommitToMainThread(); }

  DrawResult PrepareToDrawOnThread(LayerTreeHostImpl* host_impl,
                                   LayerTreeHostImpl::FrameData* frame_data,
                                   DrawResult draw_result) override {
    EXPECT_EQ(2u, frame_data->render_passes.size());
    viz::RenderPass* root_pass = frame_data->render_passes.back().get();
    EXPECT_EQ(2u, root_pass->quad_list.size());

    // There's a solid color quad under everything.
    EXPECT_EQ(viz::DrawQuad::SOLID_COLOR,
              root_pass->quad_list.back()->material);

    EXPECT_EQ(viz::DrawQuad::RENDER_PASS,
              root_pass->quad_list.front()->material);
    const viz::RenderPassDrawQuad* render_pass_quad =
        viz::RenderPassDrawQuad::MaterialCast(root_pass->quad_list.front());
    gfx::Rect rect_in_target_space = MathUtil::MapEnclosingClippedRect(
        render_pass_quad->shared_quad_state->quad_to_target_transform,
        render_pass_quad->rect);
    EXPECT_EQ(gfx::Rect(0, 0, 50, 50).ToString(),
              rect_in_target_space.ToString());
    if (host_impl->settings().enable_mask_tiling) {
      PictureLayerImpl* mask_layer_impl = static_cast<PictureLayerImpl*>(
          host_impl->active_tree()->LayerById(mask_layer_id_));
      gfx::SizeF texture_size(
          mask_layer_impl->CalculateTileSize(mask_layer_impl->bounds()));
      EXPECT_EQ(
          gfx::RectF(50.f / texture_size.width(), 50.f / texture_size.height(),
                     50.f / texture_size.width(), 50.f / texture_size.height())
              .ToString(),
          render_pass_quad->mask_uv_rect.ToString());
    } else {
      EXPECT_EQ(gfx::ScaleRect(gfx::RectF(50.f, 50.f, 50.f, 50.f), 1.f / 100.f)
                    .ToString(),
                render_pass_quad->mask_uv_rect.ToString());
    }
    EndTest();
    return draw_result;
  }

  void AfterTest() override {}

  int mask_layer_id_;
  FakeContentLayerClient client_;
};

class LayerTreeTestMaskLayerForSurfaceWithContentRectNotAtOrigin_Untiled
    : public LayerTreeTestMaskLayerForSurfaceWithContentRectNotAtOrigin {
 public:
  void InitializeSettings(LayerTreeSettings* settings) override {
    settings->enable_mask_tiling = false;
  }
};

SINGLE_AND_MULTI_THREAD_TEST_F(
    LayerTreeTestMaskLayerForSurfaceWithContentRectNotAtOrigin_Untiled);

class LayerTreeTestMaskLayerForSurfaceWithContentRectNotAtOrigin_Tiled
    : public LayerTreeTestMaskLayerForSurfaceWithContentRectNotAtOrigin {
 public:
  void InitializeSettings(LayerTreeSettings* settings) override {
    settings->enable_mask_tiling = true;
  }
};

SINGLE_AND_MULTI_THREAD_TEST_F(
    LayerTreeTestMaskLayerForSurfaceWithContentRectNotAtOrigin_Tiled);

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
    content_layer->SetMaskLayer(mask_layer.get());

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
    mask_layer->SetLayerMaskType(Layer::LayerMaskType::MULTI_TEXTURE_MASK);
    mask_layer_id_ = mask_layer->id();

    layer_tree_host()->SetRootLayer(root);
    LayerTreeTest::SetupTree();
    client_.set_bounds(root->bounds());
  }

  void BeginTest() override { PostSetNeedsCommitToMainThread(); }

  DrawResult PrepareToDrawOnThread(LayerTreeHostImpl* host_impl,
                                   LayerTreeHostImpl::FrameData* frame_data,
                                   DrawResult draw_result) override {
    EXPECT_EQ(2u, frame_data->render_passes.size());
    viz::RenderPass* root_pass = frame_data->render_passes.back().get();
    EXPECT_EQ(2u, root_pass->quad_list.size());

    // There's a solid color quad under everything.
    EXPECT_EQ(viz::DrawQuad::SOLID_COLOR,
              root_pass->quad_list.back()->material);

    // The surface is clipped to 10x20.
    EXPECT_EQ(viz::DrawQuad::RENDER_PASS,
              root_pass->quad_list.front()->material);
    const viz::RenderPassDrawQuad* render_pass_quad =
        viz::RenderPassDrawQuad::MaterialCast(root_pass->quad_list.front());
    gfx::Rect rect_in_target_space = MathUtil::MapEnclosingClippedRect(
        render_pass_quad->shared_quad_state->quad_to_target_transform,
        render_pass_quad->rect);
    EXPECT_EQ(gfx::Rect(20, 10, 10, 20).ToString(),
              rect_in_target_space.ToString());
    // The masked layer is 50x50, but the surface size is 10x20. So the texture
    // coords in the mask are scaled by 10/50 and 20/50.
    // The surface is clipped to (20,10) so the mask texture coords are offset
    // by 20/50 and 10/50
    if (host_impl->settings().enable_mask_tiling) {
      PictureLayerImpl* mask_layer_impl = static_cast<PictureLayerImpl*>(
          host_impl->active_tree()->LayerById(mask_layer_id_));
      gfx::SizeF texture_size(
          mask_layer_impl->CalculateTileSize(mask_layer_impl->bounds()));
      EXPECT_EQ(
          gfx::RectF(20.f / texture_size.width(), 10.f / texture_size.height(),
                     10.f / texture_size.width(), 20.f / texture_size.height())
              .ToString(),
          render_pass_quad->mask_uv_rect.ToString());
    } else {
      EXPECT_EQ(gfx::ScaleRect(gfx::RectF(20.f, 10.f, 10.f, 20.f), 1.f / 50.f)
                    .ToString(),
                render_pass_quad->mask_uv_rect.ToString());
    }
    EndTest();
    return draw_result;
  }

  void AfterTest() override {}

  int mask_layer_id_;
  FakeContentLayerClient client_;
};

class LayerTreeTestMaskLayerForSurfaceWithClippedLayer_Untiled
    : public LayerTreeTestMaskLayerForSurfaceWithClippedLayer {
 public:
  void InitializeSettings(LayerTreeSettings* settings) override {
    settings->enable_mask_tiling = false;
  }
};

SINGLE_AND_MULTI_THREAD_TEST_F(
    LayerTreeTestMaskLayerForSurfaceWithClippedLayer_Untiled);

class LayerTreeTestMaskLayerForSurfaceWithClippedLayer_Tiled
    : public LayerTreeTestMaskLayerForSurfaceWithClippedLayer {
 public:
  void InitializeSettings(LayerTreeSettings* settings) override {
    settings->enable_mask_tiling = true;
  }
};

SINGLE_AND_MULTI_THREAD_TEST_F(
    LayerTreeTestMaskLayerForSurfaceWithClippedLayer_Tiled);

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
    content_layer->SetMaskLayer(mask_layer.get());

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
    mask_layer->SetLayerMaskType(Layer::LayerMaskType::MULTI_TEXTURE_MASK);
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
    EXPECT_EQ(2u, frame_data->render_passes.size());
    viz::RenderPass* root_pass = frame_data->render_passes.back().get();
    EXPECT_EQ(2u, root_pass->quad_list.size());

    // There's a solid color quad under everything.
    EXPECT_EQ(viz::DrawQuad::SOLID_COLOR,
              root_pass->quad_list.back()->material);

    // The surface is clipped to 10x20, and then scaled by 2, which ends up
    // being 20x40.
    EXPECT_EQ(viz::DrawQuad::RENDER_PASS,
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
    // The masked layer is 50x50, but the surface size is 10x20. So the texture
    // coords in the mask are scaled by 10/50 and 20/50.
    // The surface is clipped to (20,10) so the mask texture coords are offset
    // by 20/50 and 10/50
    if (host_impl->settings().enable_mask_tiling) {
      PictureLayerImpl* mask_layer_impl = static_cast<PictureLayerImpl*>(
          host_impl->active_tree()->LayerById(mask_layer_id_));
      gfx::SizeF texture_size(
          mask_layer_impl->CalculateTileSize(mask_layer_impl->bounds()));
      EXPECT_EQ(
          gfx::RectF(20.f / texture_size.width(), 10.f / texture_size.height(),
                     10.f / texture_size.width(), 20.f / texture_size.height())
              .ToString(),
          render_pass_quad->mask_uv_rect.ToString());
    } else {
      EXPECT_EQ(gfx::ScaleRect(gfx::RectF(20.f, 10.f, 10.f, 20.f), 1.f / 50.f)
                    .ToString(),
                render_pass_quad->mask_uv_rect.ToString());
    }
    EndTest();
    return draw_result;
  }

  void AfterTest() override {}

  int mask_layer_id_;
  FakeContentLayerClient client_;
};

class LayerTreeTestMaskLayerForSurfaceWithDifferentScale_Untiled
    : public LayerTreeTestMaskLayerForSurfaceWithDifferentScale {
 public:
  void InitializeSettings(LayerTreeSettings* settings) override {
    settings->enable_mask_tiling = false;
  }
};

SINGLE_AND_MULTI_THREAD_TEST_F(
    LayerTreeTestMaskLayerForSurfaceWithDifferentScale_Untiled);

class LayerTreeTestMaskLayerForSurfaceWithDifferentScale_Tiled
    : public LayerTreeTestMaskLayerForSurfaceWithDifferentScale {
 public:
  void InitializeSettings(LayerTreeSettings* settings) override {
    settings->enable_mask_tiling = true;
  }
};

SINGLE_AND_MULTI_THREAD_TEST_F(
    LayerTreeTestMaskLayerForSurfaceWithDifferentScale_Tiled);

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
    content_layer->SetMaskLayer(mask_layer.get());

    gfx::Size root_size(100, 100);
    root->SetBounds(root_size);

    gfx::Size scaling_layer_size(50, 50);
    scaling_layer->SetBounds(scaling_layer_size);
    gfx::Transform scale;
    scale.Scale(2.f, 2.f);
    scaling_layer->SetTransform(scale);

    content_layer->SetBounds(scaling_layer_size);

    mask_layer->SetBounds(scaling_layer_size);
    mask_layer->SetLayerMaskType(Layer::LayerMaskType::MULTI_TEXTURE_MASK);

    layer_tree_host()->SetRootLayer(root);
    LayerTreeTest::SetupTree();
    client_.set_bounds(root->bounds());
  }

  void BeginTest() override { PostSetNeedsCommitToMainThread(); }

  DrawResult PrepareToDrawOnThread(LayerTreeHostImpl* host_impl,
                                   LayerTreeHostImpl::FrameData* frame_data,
                                   DrawResult draw_result) override {
    EXPECT_EQ(2u, frame_data->render_passes.size());
    viz::RenderPass* root_pass = frame_data->render_passes.back().get();
    EXPECT_EQ(2u, root_pass->quad_list.size());

    // There's a solid color quad under everything.
    EXPECT_EQ(viz::DrawQuad::SOLID_COLOR,
              root_pass->quad_list.back()->material);

    EXPECT_EQ(viz::DrawQuad::RENDER_PASS,
              root_pass->quad_list.front()->material);
    const viz::RenderPassDrawQuad* render_pass_quad =
        viz::RenderPassDrawQuad::MaterialCast(root_pass->quad_list.front());
    switch (host_impl->active_tree()->source_frame_number()) {
      case 0:
        // Check that the tree scaling is correctly taken into account for the
        // mask, that should fully map onto the quad.
        EXPECT_EQ(gfx::Rect(0, 0, 100, 100).ToString(),
                  render_pass_quad->rect.ToString());
        if (host_impl->settings().enable_mask_tiling) {
          EXPECT_EQ(
              gfx::RectF(0.f, 0.f, 100.f / 128.f, 100.f / 128.f).ToString(),
              render_pass_quad->mask_uv_rect.ToString());
        } else {
          EXPECT_EQ(gfx::RectF(0.f, 0.f, 1.f, 1.f).ToString(),
                    render_pass_quad->mask_uv_rect.ToString());
        }
        break;
      case 1:
        // Applying a DSF should change the render surface size, but won't
        // affect which part of the mask is used.
        EXPECT_EQ(gfx::Rect(0, 0, 200, 200).ToString(),
                  render_pass_quad->rect.ToString());
        if (host_impl->settings().enable_mask_tiling) {
          EXPECT_EQ(
              gfx::RectF(0.f, 0.f, 100.f / 128.f, 100.f / 128.f).ToString(),
              render_pass_quad->mask_uv_rect.ToString());
        } else {
          EXPECT_EQ(gfx::RectF(0.f, 0.f, 1.f, 1.f).ToString(),
                    render_pass_quad->mask_uv_rect.ToString());
        }
        EndTest();
        break;
    }
    return draw_result;
  }

  void DidCommit() override {
    switch (layer_tree_host()->SourceFrameNumber()) {
      case 1:
        gfx::Size double_root_size(200, 200);
        layer_tree_host()->SetViewportSizeAndScale(
            double_root_size, 2.f, viz::LocalSurfaceId(), base::TimeTicks());
        break;
    }
  }

  void AfterTest() override {}

  FakeContentLayerClient client_;
};

class LayerTreeTestMaskLayerWithScaling_Untiled
    : public LayerTreeTestMaskLayerWithScaling {
 public:
  void InitializeSettings(LayerTreeSettings* settings) override {
    settings->enable_mask_tiling = false;
    settings->layer_transforms_should_scale_layer_contents = true;
  }
};

SINGLE_AND_MULTI_THREAD_TEST_F(LayerTreeTestMaskLayerWithScaling_Untiled);

class LayerTreeTestMaskLayerWithScaling_Tiled
    : public LayerTreeTestMaskLayerWithScaling {
 public:
  void InitializeSettings(LayerTreeSettings* settings) override {
    settings->enable_mask_tiling = true;
    settings->layer_transforms_should_scale_layer_contents = true;
  }
};

SINGLE_AND_MULTI_THREAD_TEST_F(LayerTreeTestMaskLayerWithScaling_Tiled);

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
    content_layer->SetMaskLayer(mask_layer.get());

    gfx::Size root_size(100, 100);
    root->SetBounds(root_size);

    gfx::Size layer_size(100, 100);
    content_layer->SetBounds(layer_size);

    gfx::Size mask_size(100, 100);
    gfx::Size mask_texture_size(120, 150);
    mask_layer->SetBounds(mask_size);
    mask_layer->SetLayerMaskType(Layer::LayerMaskType::SINGLE_TEXTURE_MASK);
    mask_layer->set_fixed_tile_size(mask_texture_size);

    layer_tree_host()->SetRootLayer(root);
    LayerTreeTest::SetupTree();
    client_.set_bounds(root->bounds());
  }

  void BeginTest() override { PostSetNeedsCommitToMainThread(); }

  DrawResult PrepareToDrawOnThread(LayerTreeHostImpl* host_impl,
                                   LayerTreeHostImpl::FrameData* frame_data,
                                   DrawResult draw_result) override {
    EXPECT_EQ(2u, frame_data->render_passes.size());
    viz::RenderPass* root_pass = frame_data->render_passes.back().get();
    EXPECT_EQ(2u, root_pass->quad_list.size());

    // There's a solid color quad under everything.
    EXPECT_EQ(viz::DrawQuad::SOLID_COLOR,
              root_pass->quad_list.back()->material);

    // The surface is 100x100
    EXPECT_EQ(viz::DrawQuad::RENDER_PASS,
              root_pass->quad_list.front()->material);
    const viz::RenderPassDrawQuad* render_pass_quad =
        viz::RenderPassDrawQuad::MaterialCast(root_pass->quad_list.front());
    EXPECT_EQ(gfx::Rect(0, 0, 100, 100).ToString(),
              render_pass_quad->rect.ToString());
    // The mask layer is 100x100, but is backed by a 120x150 image.
    EXPECT_EQ(gfx::RectF(0.0f, 0.0f, 100.f / 120.0f, 100.f / 150.0f).ToString(),
              render_pass_quad->mask_uv_rect.ToString());
    EndTest();
    return draw_result;
  }

  void AfterTest() override {}

  int mask_layer_id_;
  FakeContentLayerClient client_;
};

class LayerTreeTestMaskWithNonExactTextureSize_Untiled
    : public LayerTreeTestMaskWithNonExactTextureSize {
 public:
  void InitializeSettings(LayerTreeSettings* settings) override {
    settings->enable_mask_tiling = false;
  }
};

SINGLE_AND_MULTI_THREAD_TEST_F(
    LayerTreeTestMaskWithNonExactTextureSize_Untiled);

class LayerTreeTestMaskWithNonExactTextureSize_Tiled
    : public LayerTreeTestMaskWithNonExactTextureSize {
 public:
  void InitializeSettings(LayerTreeSettings* settings) override {
    settings->enable_mask_tiling = true;
  }
};

SINGLE_AND_MULTI_THREAD_TEST_F(LayerTreeTestMaskWithNonExactTextureSize_Tiled);

}  // namespace
}  // namespace cc
