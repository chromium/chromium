// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/layer_tree_host.h"

#include "cc/layers/render_surface_impl.h"
#include "cc/layers/video_layer.h"
#include "cc/layers/video_layer_impl.h"
#include "cc/test/fake_video_frame_provider.h"
#include "cc/test/layer_test_common.h"
#include "cc/test/layer_tree_test.h"
#include "cc/trees/damage_tracker.h"
#include "cc/trees/layer_tree_impl.h"

namespace cc {
namespace {

constexpr auto kTestTransform =
    media::VideoTransformation(media::VIDEO_ROTATION_90, /*mirrored=*/true);

// These tests deal with compositing video.
class LayerTreeHostVideoTest : public LayerTreeTest {};

class LayerTreeHostVideoTestSetNeedsDisplay
    : public LayerTreeHostVideoTest {
 public:
  void SetupTree() override {
    scoped_refptr<Layer> root = Layer::Create();
    root->SetBounds(gfx::Size(10, 10));
    root->SetIsDrawable(true);

    scoped_refptr<VideoLayer> video =
        VideoLayer::Create(&video_frame_provider_, kTestTransform);
    video->SetPosition(gfx::PointF(3.f, 3.f));
    video->SetBounds(gfx::Size(4, 5));
    video->SetIsDrawable(true);
    root->AddChild(video);
    video_layer_id_ = video->id();

    layer_tree_host()->SetRootLayer(root);
    SetInitialDeviceScaleFactor(2.f);
    LayerTreeHostVideoTest::SetupTree();
  }

  void BeginTest() override {
    num_draws_ = 0;
    PostSetNeedsCommitToMainThread();
  }

  DrawResult PrepareToDrawOnThread(LayerTreeHostImpl* host_impl,
                                   LayerTreeHostImpl::FrameData* frame,
                                   DrawResult draw_result) override {
    LayerImpl* root_layer = host_impl->active_tree()->root_layer();
    RenderSurfaceImpl* root_surface = GetRenderSurface(root_layer);
    gfx::Rect damage_rect;
    EXPECT_TRUE(
        root_surface->damage_tracker()->GetDamageRectIfValid(&damage_rect));

    switch (num_draws_) {
      case 0:
        // First frame the whole viewport is damaged.
        EXPECT_EQ(gfx::Rect(0, 0, 20, 20), damage_rect);
        break;
      case 1:
        // Second frame the video layer is damaged.
        EXPECT_EQ(gfx::Rect(6, 6, 8, 10), damage_rect);
        EndTest();
        break;
    }

    EXPECT_EQ(DrawResult::kSuccess, draw_result);
    return draw_result;
  }

  void DrawLayersOnThread(LayerTreeHostImpl* host_impl) override {
    VideoLayerImpl* video = static_cast<VideoLayerImpl*>(
        host_impl->active_tree()->LayerById(video_layer_id_));

    EXPECT_EQ(kTestTransform, video->video_transform_for_testing());

    if (num_draws_ == 0)
      video->SetNeedsRedraw();

    ++num_draws_;
  }

  void AfterTest() override { EXPECT_EQ(2, num_draws_); }

 private:
  int num_draws_;
  int video_layer_id_;

  FakeVideoFrameProvider video_frame_provider_;
};

SINGLE_AND_MULTI_THREAD_TEST_F(LayerTreeHostVideoTestSetNeedsDisplay);

}  // namespace
}  // namespace cc
