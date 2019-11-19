// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/layer_tree_host.h"

#include "base/bind.h"
#include "base/time/time.h"
#include "cc/test/fake_content_layer_client.h"
#include "cc/test/fake_picture_layer.h"
#include "cc/test/fake_picture_layer_impl.h"
#include "cc/test/layer_tree_test.h"
#include "cc/trees/layer_tree_impl.h"

namespace cc {
namespace {

// These tests deal with picture layers.
class LayerTreeHostPictureTest : public LayerTreeTest {
 protected:
  void SetupTreeWithSinglePictureLayer(const gfx::Size& size) {
    scoped_refptr<Layer> root = Layer::Create();
    root->SetBounds(size);

    root_picture_layer_ = FakePictureLayer::Create(&client_);
    root_picture_layer_->SetBounds(size);
    root->AddChild(root_picture_layer_);

    layer_tree_host()->SetRootLayer(root);
    client_.set_bounds(size);
  }

  scoped_refptr<FakePictureLayer> root_picture_layer_;
  FakeContentLayerClient client_;
};

class LayerTreeHostPictureTestTwinLayer
    : public LayerTreeHostPictureTest {
  void SetupTree() override {
    SetupTreeWithSinglePictureLayer(gfx::Size(1, 1));
    picture_id1_ = root_picture_layer_->id();
    picture_id2_ = -1;
  }

  void BeginTest() override {
    // Commit and activate to produce a pending (recycled) layer and an active
    // layer.
    PostSetNeedsCommitToMainThread();
  }

  void DidCommit() override {
    switch (layer_tree_host()->SourceFrameNumber()) {
      case 1:
        // Activate reusing an existing recycled pending layer, to an already
        // existing active layer.
        layer_tree_host()->SetNeedsCommit();
        break;
      case 2:
        // Drop the picture layer from the tree so the activate will have an
        // active layer without a pending twin.
        root_picture_layer_->RemoveFromParent();
        break;
      case 3: {
        // Add a new picture layer so the activate will have a pending layer
        // without an active twin.
        scoped_refptr<FakePictureLayer> picture =
            FakePictureLayer::Create(&client_);
        picture_id2_ = picture->id();
        layer_tree_host()->root_layer()->AddChild(picture);
        break;
      }
      case 4:
        // Activate while there are pending and active twins again.
        layer_tree_host()->SetNeedsCommit();
        break;
    }
  }

  void WillActivateTreeOnThread(LayerTreeHostImpl* impl) override {
    LayerImpl* active_root_impl = impl->active_tree()->root_layer();
    int picture_id = impl->active_tree()->source_frame_number() < 2
                         ? picture_id1_
                         : picture_id2_;

    if (!impl->pending_tree()->LayerById(picture_id)) {
      EXPECT_EQ(2, activates_);
      return;
    }

    FakePictureLayerImpl* pending_picture_impl =
        static_cast<FakePictureLayerImpl*>(
            impl->pending_tree()->LayerById(picture_id));

    if (!active_root_impl) {
      EXPECT_EQ(0, activates_);
      EXPECT_EQ(nullptr, pending_picture_impl->GetPendingOrActiveTwinLayer());
      return;
    }

    if (!impl->active_tree()->LayerById(picture_id)) {
      EXPECT_EQ(3, activates_);
      EXPECT_EQ(nullptr, pending_picture_impl->GetPendingOrActiveTwinLayer());
      return;
    }

    FakePictureLayerImpl* active_picture_impl =
        static_cast<FakePictureLayerImpl*>(
            impl->active_tree()->LayerById(picture_id));

    // After the first activation, when we commit again, we'll have a pending
    // and active layer. Then we recreate a picture layer in the 4th activate
    // and the next commit will have a pending and active twin again.
    EXPECT_TRUE(activates_ == 1 || activates_ == 4) << activates_;

    EXPECT_EQ(pending_picture_impl,
              active_picture_impl->GetPendingOrActiveTwinLayer());
    EXPECT_EQ(active_picture_impl,
              pending_picture_impl->GetPendingOrActiveTwinLayer());
  }

  void DidActivateTreeOnThread(LayerTreeHostImpl* impl) override {
    int picture_id = impl->active_tree()->source_frame_number() < 3
                         ? picture_id1_
                         : picture_id2_;
    if (!impl->active_tree()->LayerById(picture_id)) {
      EXPECT_EQ(2, activates_);
    } else {
      FakePictureLayerImpl* active_picture_impl =
          static_cast<FakePictureLayerImpl*>(
              impl->active_tree()->LayerById(picture_id));
      EXPECT_EQ(nullptr, active_picture_impl->GetPendingOrActiveTwinLayer());
    }

    ++activates_;
    if (activates_ == 5)
      EndTest();
  }

  int activates_ = 0;

  int picture_id1_;
  int picture_id2_;
};

// There is no pending layers in single thread mode.
MULTI_THREAD_TEST_F(LayerTreeHostPictureTestTwinLayer);

class LayerTreeHostPictureTestResizeViewportWithGpuRaster
    : public LayerTreeHostPictureTest {
  void InitializeSettings(LayerTreeSettings* settings) override {
    settings->gpu_rasterization_forced = true;
  }

  void SetupTree() override {
    scoped_refptr<Layer> root = Layer::Create();
    root->SetBounds(gfx::Size(768, 960));
    client_.set_bounds(root->bounds());
    client_.set_fill_with_nonsolid_color(true);
    picture_ = FakePictureLayer::Create(&client_);
    picture_->SetBounds(gfx::Size(768, 960));
    root->AddChild(picture_);

    layer_tree_host()->SetRootLayer(root);
    LayerTreeHostPictureTest::SetupTree();
  }

  void BeginTest() override { PostSetNeedsCommitToMainThread(); }

  void CommitCompleteOnThread(LayerTreeHostImpl* impl) override {
    LayerImpl* child = impl->sync_tree()->LayerById(picture_->id());
    FakePictureLayerImpl* picture_impl =
        static_cast<FakePictureLayerImpl*>(child);
    gfx::Size tile_size =
        picture_impl->HighResTiling()->TileAt(0, 0)->content_rect().size();

    switch (impl->sync_tree()->source_frame_number()) {
      case 0:
        tile_size_ = tile_size;
        // GPU Raster picks a tile size based on the viewport size.
        EXPECT_EQ(gfx::Size(768, 256), tile_size);
        break;
      case 1:
        // When the viewport changed size, the new frame's tiles should change
        // along with it.
        EXPECT_NE(gfx::Size(768, 256), tile_size);
    }
  }

  void DidCommit() override {
    switch (layer_tree_host()->SourceFrameNumber()) {
      case 1:
        // Change the picture layer's size along with the viewport, so it will
        // consider picking a new tile size.
        picture_->SetBounds(gfx::Size(768, 1056));
        GenerateNewLocalSurfaceId();
        layer_tree_host()->SetViewportRectAndScale(
            gfx::Rect(768, 1056), 1.f, GetCurrentLocalSurfaceIdAllocation());
        break;
      case 2:
        EndTest();
    }
  }

  gfx::Size tile_size_;
  FakeContentLayerClient client_;
  scoped_refptr<FakePictureLayer> picture_;
};

SINGLE_AND_MULTI_THREAD_TEST_F(
    LayerTreeHostPictureTestResizeViewportWithGpuRaster);

class LayerTreeHostPictureTestChangeLiveTilesRectWithRecycleTree
    : public LayerTreeHostPictureTest {
  void SetupTree() override {
    frame_ = 0;
    did_post_commit_ = false;

    scoped_refptr<Layer> root = Layer::Create();
    root->SetBounds(gfx::Size(100, 100));
    // The layer is big enough that the live tiles rect won't cover the full
    // layer.
    client_.set_fill_with_nonsolid_color(true);
    picture_ = FakePictureLayer::Create(&client_);
    picture_->SetBounds(gfx::Size(100, 100000));
    root->AddChild(picture_);

    // picture_'s transform is going to be changing on the compositor thread, so
    // force it to have a transform node by making it scrollable.
    picture_->SetScrollable(root->bounds());

    layer_tree_host()->SetRootLayer(root);
    LayerTreeHostPictureTest::SetupTree();
    client_.set_bounds(picture_->bounds());
  }

  void BeginTest() override { PostSetNeedsCommitToMainThread(); }

  void DrawLayersOnThread(LayerTreeHostImpl* impl) override {
    LayerImpl* child = impl->active_tree()->LayerById(picture_->id());
    FakePictureLayerImpl* picture_impl =
        static_cast<FakePictureLayerImpl*>(child);
    switch (++frame_) {
      case 1: {
        PictureLayerTiling* tiling = picture_impl->HighResTiling();
        int num_tiles_y = tiling->TilingDataForTesting().num_tiles_y();

        // There should be tiles at the top of the picture layer but not at the
        // bottom.
        EXPECT_TRUE(tiling->TileAt(0, 0));
        EXPECT_FALSE(tiling->TileAt(0, num_tiles_y));

        // Make the bottom of the layer visible.
        gfx::Transform transform;
        transform.Translate(0.f, -100000.f + 100.f);
        impl->active_tree()->SetTransformMutated(picture_impl->element_id(),
                                                 transform);
        impl->SetNeedsRedraw();
        break;
      }
      case 2: {
        PictureLayerTiling* tiling = picture_impl->HighResTiling();

        // There not be tiles at the top of the layer now.
        EXPECT_FALSE(tiling->TileAt(0, 0));

        // Make the top of the layer visible again.
        impl->active_tree()->SetTransformMutated(picture_impl->element_id(),
                                                 gfx::Transform());
        impl->SetNeedsRedraw();
        break;
      }
      case 3: {
        PictureLayerTiling* tiling = picture_impl->HighResTiling();
        int num_tiles_y = tiling->TilingDataForTesting().num_tiles_y();

        // There should be tiles at the top of the picture layer again.
        EXPECT_TRUE(tiling->TileAt(0, 0));
        EXPECT_FALSE(tiling->TileAt(0, num_tiles_y));

        // Make a new main frame without changing the picture layer at all, so
        // it won't need to update or push properties.
        did_post_commit_ = true;
        PostSetNeedsCommitToMainThread();
        break;
      }
    }
  }

  void WillActivateTreeOnThread(LayerTreeHostImpl* impl) override {
    LayerImpl* child = impl->sync_tree()->LayerById(picture_->id());
    FakePictureLayerImpl* picture_impl =
        static_cast<FakePictureLayerImpl*>(child);
    PictureLayerTiling* tiling = picture_impl->HighResTiling();
    int num_tiles_y = tiling->TilingDataForTesting().num_tiles_y();

    if (!impl->active_tree()->root_layer()) {
      // If active tree doesn't have the layer, then pending tree should have
      // all needed tiles.
      EXPECT_TRUE(tiling->TileAt(0, 0));
    } else {
      // Since there was no invalidation, the pending tree shouldn't have any
      // tiles.
      EXPECT_FALSE(tiling->TileAt(0, 0));
    }
    EXPECT_FALSE(tiling->TileAt(0, num_tiles_y));

    if (did_post_commit_)
      EndTest();
  }

  int frame_;
  bool did_post_commit_;
  FakeContentLayerClient client_;
  scoped_refptr<FakePictureLayer> picture_;
};

// Multi-thread only since there is no recycle tree in single thread.
MULTI_THREAD_TEST_F(LayerTreeHostPictureTestChangeLiveTilesRectWithRecycleTree);

class LayerTreeHostPictureTestRSLLMembership : public LayerTreeHostPictureTest {
  void SetupTree() override {
    scoped_refptr<Layer> root = Layer::Create();
    root->SetBounds(gfx::Size(100, 100));
    client_.set_bounds(root->bounds());

    child_ = Layer::Create();
    root->AddChild(child_);

    // Don't be solid color so the layer has tilings/tiles.
    client_.set_fill_with_nonsolid_color(true);
    picture_ = FakePictureLayer::Create(&client_);
    picture_->SetBounds(gfx::Size(100, 100));
    child_->AddChild(picture_);

    layer_tree_host()->SetRootLayer(root);
    LayerTreeHostPictureTest::SetupTree();
  }

  void BeginTest() override { PostSetNeedsCommitToMainThread(); }

  void CommitCompleteOnThread(LayerTreeHostImpl* impl) override {
    LayerImpl* gchild = impl->sync_tree()->LayerById(picture_->id());
    FakePictureLayerImpl* picture = static_cast<FakePictureLayerImpl*>(gchild);

    switch (impl->active_tree()->source_frame_number()) {
      case 0:
        // On 1st commit the layer has tilings.
        EXPECT_GT(picture->tilings()->num_tilings(), 0u);
        break;
      case 1:
        // On 2nd commit, the layer is transparent, but its tilings are left
        // there.
        EXPECT_GT(picture->tilings()->num_tilings(), 0u);
        break;
      case 2:
        // On 3rd commit, the layer is visible again, so has tilings.
        EXPECT_GT(picture->tilings()->num_tilings(), 0u);
    }
  }

  void DidActivateTreeOnThread(LayerTreeHostImpl* impl) override {
    LayerImpl* gchild = impl->active_tree()->LayerById(picture_->id());
    FakePictureLayerImpl* picture = static_cast<FakePictureLayerImpl*>(gchild);

    switch (impl->active_tree()->source_frame_number()) {
      case 0:
        // On 1st commit the layer has tilings.
        EXPECT_GT(picture->tilings()->num_tilings(), 0u);
        break;
      case 1:
        // On 2nd commit, the layer is transparent, but its tilings are left
        // there.
        EXPECT_GT(picture->tilings()->num_tilings(), 0u);
        break;
      case 2:
        // On 3rd commit, the layer is visible again, so has tilings.
        EXPECT_GT(picture->tilings()->num_tilings(), 0u);
        EndTest();
    }
  }

  void DidCommit() override {
    switch (layer_tree_host()->SourceFrameNumber()) {
      case 1:
        // For the 2nd commit, change opacity to 0 so that the layer will not be
        // part of the visible frame.
        child_->SetOpacity(0.f);
        break;
      case 2:
        // For the 3rd commit, change opacity to 1 so that the layer will again
        // be part of the visible frame.
        child_->SetOpacity(1.f);
    }
  }

  FakeContentLayerClient client_;
  scoped_refptr<Layer> child_;
  scoped_refptr<FakePictureLayer> picture_;
};

SINGLE_THREAD_TEST_F(LayerTreeHostPictureTestRSLLMembership);

class LayerTreeHostPictureTestRSLLMembershipWithScale
    : public LayerTreeHostPictureTest {
  void SetupTree() override {
    scoped_refptr<Layer> root_clip = Layer::Create();
    root_clip->SetBounds(gfx::Size(100, 100));
    scoped_refptr<Layer> page_scale_layer = Layer::Create();
    page_scale_layer->SetBounds(gfx::Size(100, 100));

    pinch_ = Layer::Create();
    pinch_->SetBounds(gfx::Size(500, 500));
    pinch_->SetScrollable(root_clip->bounds());
    page_scale_layer->AddChild(pinch_);
    root_clip->AddChild(page_scale_layer);

    // Don't be solid color so the layer has tilings/tiles.
    client_.set_fill_with_nonsolid_color(true);
    picture_ = FakePictureLayer::Create(&client_);
    picture_->SetBounds(gfx::Size(100, 100));
    pinch_->AddChild(picture_);

    layer_tree_host()->SetPageScaleFactorAndLimits(1.f, 1.f, 4.f);
    layer_tree_host()->SetRootLayer(root_clip);
    LayerTreeHostPictureTest::SetupTree();
    client_.set_bounds(picture_->bounds());
  }

  void BeginTest() override {
    frame_ = 0;
    draws_in_frame_ = 0;
    last_frame_drawn_ = -1;
    ready_to_draw_ = false;
    PostSetNeedsCommitToMainThread();
  }

  void WillActivateTreeOnThread(LayerTreeHostImpl* impl) override {
    LayerImpl* gchild = impl->sync_tree()->LayerById(picture_->id());
    FakePictureLayerImpl* picture = static_cast<FakePictureLayerImpl*>(gchild);
    ready_to_draw_ = false;

    switch (frame_) {
      case 0:
        // On 1st commit the pending layer has tilings.
        ASSERT_EQ(1u, picture->tilings()->num_tilings());
        EXPECT_EQ(gfx::AxisTransform2d(),
                  picture->tilings()->tiling_at(0)->raster_transform());
        break;
      case 1:
        // On 2nd commit, the pending layer is transparent, so has a stale
        // value.
        ASSERT_EQ(1u, picture->tilings()->num_tilings());
        EXPECT_EQ(gfx::AxisTransform2d(),
                  picture->tilings()->tiling_at(0)->raster_transform());
        break;
      case 2:
        // On 3rd commit, the pending layer is visible again, so has tilings and
        // is updated for the pinch.
        ASSERT_EQ(1u, picture->tilings()->num_tilings());
        EXPECT_EQ(gfx::AxisTransform2d(2.f, gfx::Vector2dF()),
                  picture->tilings()->tiling_at(0)->raster_transform());
    }
  }

  void DrawLayersOnThread(LayerTreeHostImpl* impl) override {
    LayerImpl* gchild = impl->active_tree()->LayerById(picture_->id());
    FakePictureLayerImpl* picture = static_cast<FakePictureLayerImpl*>(gchild);

    if (frame_ != last_frame_drawn_)
      draws_in_frame_ = 0;
    ++draws_in_frame_;
    last_frame_drawn_ = frame_;

    switch (frame_) {
      case 0:
        if (draws_in_frame_ == 1) {
          // On 1st commit the layer has tilings.
          EXPECT_GT(picture->tilings()->num_tilings(), 0u);
          EXPECT_EQ(gfx::AxisTransform2d(),
                    picture->HighResTiling()->raster_transform());

          // Pinch zoom in to change the scale on the active tree.
          impl->PinchGestureBegin();
          impl->PinchGestureUpdate(2.f, gfx::Point(1, 1));
          impl->PinchGestureEnd(gfx::Point(1, 1), true);
        } else if (picture->tilings()->num_tilings() == 1) {
          // If the pinch gesture caused a commit we could get here with a
          // pending tree.
          EXPECT_FALSE(impl->pending_tree());
          EXPECT_EQ(gfx::AxisTransform2d(2.f, gfx::Vector2dF()),
                    picture->HighResTiling()->raster_transform());

          // Need to wait for ready to draw here so that the pinch is
          // entirely complete, otherwise another draw might come in before
          // the commit occurs.
          if (ready_to_draw_) {
            ++frame_;
            MainThreadTaskRunner()->PostTask(
                FROM_HERE,
                base::BindOnce(
                    &LayerTreeHostPictureTestRSLLMembershipWithScale::NextStep,
                    base::Unretained(this)));
          }
        }
        break;
      case 1:
        EXPECT_EQ(1, draws_in_frame_);
        // On 2nd commit, this active layer is transparent, so does not update
        // tilings.  It has the high res scale=2 from the previous frame, and
        // also a scale=1 copied from the pending layer's stale value during
        // activation.
        EXPECT_EQ(2u, picture->picture_layer_tiling_set()->num_tilings());

        ++frame_;
        MainThreadTaskRunner()->PostTask(
            FROM_HERE,
            base::BindOnce(
                &LayerTreeHostPictureTestRSLLMembershipWithScale::NextStep,
                base::Unretained(this)));
        break;
      case 2:
        EXPECT_EQ(1, draws_in_frame_);
        // On 3rd commit, the layer is visible again, so has tilings.
        EXPECT_GT(picture->tilings()->num_tilings(), 0u);
        EndTest();
    }
  }

  void NextStep() {
    switch (frame_) {
      case 1:
        // For the 2nd commit, change opacity to 0 so that the layer will not be
        // part of the visible frame.
        pinch_->SetOpacity(0.f);
        break;
      case 2:
        // For the 3rd commit, change opacity to 1 so that the layer will again
        // be part of the visible frame.
        pinch_->SetOpacity(1.f);
        break;
    }
  }

  void NotifyReadyToDrawOnThread(LayerTreeHostImpl* impl) override {
    ready_to_draw_ = true;
    if (frame_ == 0) {
      // The ready to draw can race with a draw in which everything is
      // actually ready.  Therefore, just issue one more extra draw
      // here to force notify->draw ordering.
      impl->SetNeedsRedraw();
    }
  }

  FakeContentLayerClient client_;
  scoped_refptr<Layer> pinch_;
  scoped_refptr<FakePictureLayer> picture_;
  int frame_;
  int draws_in_frame_;
  int last_frame_drawn_;
  bool ready_to_draw_;
};

// Multi-thread only because in single thread you can't pinch zoom on the
// compositor thread.
// TODO(https://crbug.com/997866): Flaky on several platforms.
// MULTI_THREAD_TEST_F(LayerTreeHostPictureTestRSLLMembershipWithScale);

class LayerTreeHostPictureTestForceRecalculateScales
    : public LayerTreeHostPictureTest {
  void SetupTree() override {
    gfx::Size size(100, 100);
    scoped_refptr<Layer> root = Layer::Create();
    root->SetBounds(size);
    top_layer_ = Layer::Create();
    top_layer_->SetBounds(size);
    root->AddChild(top_layer_);

    will_change_layer_ = FakePictureLayer::Create(&client_);
    will_change_layer_->SetHasWillChangeTransformHint(true);
    will_change_layer_->SetBounds(size);
    top_layer_->AddChild(will_change_layer_);

    normal_layer_ = FakePictureLayer::Create(&client_);
    normal_layer_->SetBounds(size);
    top_layer_->AddChild(normal_layer_);

    layer_tree_host()->SetRootLayer(root);
    layer_tree_host()->SetViewportRectAndScale(gfx::Rect(size), 1.f,
                                               viz::LocalSurfaceIdAllocation());

    client_.set_fill_with_nonsolid_color(true);
    client_.set_bounds(size);
  }

  void BeginTest() override { PostSetNeedsCommitToMainThread(); }

  void DrawLayersOnThread(LayerTreeHostImpl* impl) override {
    FakePictureLayerImpl* will_change_layer =
        static_cast<FakePictureLayerImpl*>(
            impl->active_tree()->LayerById(will_change_layer_->id()));
    FakePictureLayerImpl* normal_layer = static_cast<FakePictureLayerImpl*>(
        impl->active_tree()->LayerById(normal_layer_->id()));

    switch (impl->active_tree()->source_frame_number()) {
      case 0:
        // On first commit, both layers are at the default scale.
        ASSERT_EQ(1u, will_change_layer->tilings()->num_tilings());
        EXPECT_EQ(
            gfx::AxisTransform2d(),
            will_change_layer->tilings()->tiling_at(0)->raster_transform());
        ASSERT_EQ(1u, normal_layer->tilings()->num_tilings());
        EXPECT_EQ(gfx::AxisTransform2d(),
                  normal_layer->tilings()->tiling_at(0)->raster_transform());

        MainThreadTaskRunner()->PostTask(
            FROM_HERE,
            base::BindOnce(
                &LayerTreeHostPictureTestForceRecalculateScales::ScaleUp,
                base::Unretained(this)));
        break;
      case 1:
        // On 2nd commit after scaling up to 2, the normal layer will adjust its
        // scale and the will change layer should not (as it is will change.
        ASSERT_EQ(1u, will_change_layer->tilings()->num_tilings());
        EXPECT_EQ(
            gfx::AxisTransform2d(),
            will_change_layer->tilings()->tiling_at(0)->raster_transform());
        ASSERT_EQ(1u, normal_layer->tilings()->num_tilings());
        EXPECT_EQ(gfx::AxisTransform2d(2.f, gfx::Vector2dF()),
                  normal_layer->tilings()->tiling_at(0)->raster_transform());

        MainThreadTaskRunner()->PostTask(
            FROM_HERE,
            base::BindOnce(&LayerTreeHostPictureTestForceRecalculateScales::
                               ScaleUpAndRecalculateScales,
                           base::Unretained(this)));
        break;
      case 2:
        // On 3rd commit, both layers should adjust scales due to forced
        // recalculating.
        ASSERT_EQ(1u, will_change_layer->tilings()->num_tilings());
        EXPECT_EQ(
            gfx::AxisTransform2d(4.f, gfx::Vector2dF()),
            will_change_layer->tilings()->tiling_at(0)->raster_transform());
        ASSERT_EQ(1u, normal_layer->tilings()->num_tilings());
        EXPECT_EQ(gfx::AxisTransform2d(4.f, gfx::Vector2dF()),
                  normal_layer->tilings()->tiling_at(0)->raster_transform());
        EndTest();
        break;
    }
  }

  void ScaleUp() {
    gfx::Transform transform;
    transform.Scale(2, 2);
    top_layer_->SetTransform(transform);
  }

  void ScaleUpAndRecalculateScales() {
    gfx::Transform transform;
    transform.Scale(4, 4);
    top_layer_->SetTransform(transform);
    layer_tree_host()->SetNeedsRecalculateRasterScales();
  }

  scoped_refptr<Layer> top_layer_;
  scoped_refptr<FakePictureLayer> will_change_layer_;
  scoped_refptr<FakePictureLayer> normal_layer_;
};

SINGLE_THREAD_TEST_F(LayerTreeHostPictureTestForceRecalculateScales);

}  // namespace
}  // namespace cc
