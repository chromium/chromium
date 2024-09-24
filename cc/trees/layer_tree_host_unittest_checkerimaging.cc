// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/base/completion_event.h"
#include "cc/test/fake_content_layer_client.h"
#include "cc/test/fake_picture_layer.h"
#include "cc/test/layer_tree_test.h"
#include "cc/test/skia_common.h"
#include "cc/test/test_layer_tree_frame_sink.h"
#include "cc/trees/layer_tree_impl.h"
#include "components/ukm/test_ukm_recorder.h"
#include "components/viz/test/begin_frame_args_test.h"
#include "components/viz/test/fake_external_begin_frame_source.h"

namespace cc {
namespace {

class LayerTreeHostCheckerImagingTest : public LayerTreeTest {
 public:
  LayerTreeHostCheckerImagingTest()
      : url_(GURL("https://example.com")),
        ukm_source_id_(ukm::AssignNewSourceId()) {}

  void BeginTest() override {
    layer_tree_host()->SetSourceURL(ukm_source_id_, url_);
    PostSetNeedsCommitToMainThread();
  }

  void InitializeSettings(LayerTreeSettings* settings) override {
    settings->enable_checker_imaging = true;
    settings->min_image_bytes_to_checker = 512 * 1024;
  }

  void SetupTree() override {
    // Set up a content client which creates the following tiling, x denoting
    // the image to checker:
    // |---|---|---|---|
    // | x | x |   |   |
    // |---|---|---|---|
    // | x | x |   |   |
    // |---|---|---|---|
    gfx::Size layer_size(1000, 500);
    content_layer_client_.set_bounds(layer_size);
    content_layer_client_.set_fill_with_nonsolid_color(true);
    PaintImage checkerable_image =
        CreateDiscardablePaintImage(gfx::Size(450, 450));
    checkerable_image = PaintImageBuilder::WithCopy(checkerable_image)
                            .set_decoding_mode(PaintImage::DecodingMode::kAsync)
                            .TakePaintImage();
    content_layer_client_.add_draw_image(checkerable_image, gfx::Point(0, 0),
                                         SkSamplingOptions(), PaintFlags());

    layer_tree_host()->SetRootLayer(
        FakePictureLayer::Create(&content_layer_client_));
    layer_tree_host()->root_layer()->SetBounds(layer_size);
    LayerTreeTest::SetupTree();
  }

 private:
  // Accessed only on the main thread.
  FakeContentLayerClient content_layer_client_;
  GURL url_;

  // Accessed on the impl thread.
  const ukm::SourceId ukm_source_id_;
};

class LayerTreeHostCheckerImagingTestMergeWithMainFrame
    : public LayerTreeHostCheckerImagingTest {
  void BeginMainFrame(const viz::BeginFrameArgs& args) override {
    if (layer_tree_host()->SourceFrameNumber() == 1) {
      // The first commit has happened, invalidate a tile outside the region
      // for the image to ensure that the final invalidation on the pending
      // tree is the union of this and impl-side invalidation.
      layer_tree_host()->root_layer()->SetNeedsDisplayRect(
          gfx::Rect(600, 0, 50, 500));
      layer_tree_host()->SetNeedsCommit();
    }
  }

  void DidReceiveImplSideInvalidationRequest(
      LayerTreeHostImpl* host_impl) override {
    if (invalidation_requested_)
      return;
    invalidation_requested_ = true;

    // Request a commit.
    host_impl->SetNeedsCommit();
  }

  void CommitCompleteOnThread(LayerTreeHostImpl* host_impl) override {
    switch (++num_of_commits_) {
      case 1:
        // Block notifying the scheduler of this request until we've had a
        // chance to make sure that the decode work was scheduled, flushed and
        // the commit requested after it is received.
        host_impl->BlockImplSideInvalidationRequestsForTesting(true);
        break;
      case 2: {
        // Ensure that the expected tiles are invalidated on the sync tree.
        PictureLayerImpl* sync_layer_impl = static_cast<PictureLayerImpl*>(
            host_impl->sync_tree()->root_layer());
        PictureLayerTiling* sync_tiling =
            sync_layer_impl->picture_layer_tiling_set()
                ->FindTilingWithResolution(TileResolution::HIGH_RESOLUTION);

        for (int i = 0; i < 4; i++) {
          SCOPED_TRACE(i);
          for (int j = 0; j < 2; j++) {
            SCOPED_TRACE(j);
            Tile* tile =
                sync_tiling->TileAt(i, j) ? sync_tiling->TileAt(i, j) : nullptr;

            // If this is the pending tree, then only the invalidated tiles
            // exist and have a raster task. If its the active tree, then only
            // the invalidated tiles have a raster task.
            if (i < 3) {
              ASSERT_TRUE(tile);
              EXPECT_TRUE(tile->HasRasterTask());
            } else if (host_impl->pending_tree()) {
              EXPECT_EQ(tile, nullptr);
            } else {
              ASSERT_TRUE(tile);
              EXPECT_FALSE(tile->HasRasterTask());
            }
          }
        }

        // Insetting of image is included in the update rect.
        gfx::Rect expected_update_rect(-1, -1, 452, 452);
        expected_update_rect.Union(gfx::Rect(600, 0, 50, 500));
        EXPECT_EQ(sync_layer_impl->update_rect(), expected_update_rect);

        EndTest();
      } break;
      default:
        NOTREACHED();
    }
  }

  void AfterTest() override { EXPECT_EQ(num_of_commits_, 2); }

  // Use only on impl thread.
  int num_of_commits_ = 0;
  bool invalidation_requested_ = false;
};

// Checkering of content is only done on the pending tree which does not exist
// in single-threaded mode.
MULTI_THREAD_TEST_F(LayerTreeHostCheckerImagingTestMergeWithMainFrame);

class LayerTreeHostCheckerImagingTestImplSideTree
    : public LayerTreeHostCheckerImagingTest {
  void DidInvalidateContentOnImplSide(LayerTreeHostImpl* host_impl) override {
    ++num_of_impl_side_invalidations_;

    // The source_frame_number of the sync tree should be from the first main
    // frame, since this is an impl-side sync tree.
    EXPECT_EQ(host_impl->sync_tree()->source_frame_number(), 0);

    // Ensure that the expected tiles are invalidated on the sync tree.
    PictureLayerImpl* sync_layer_impl =
        static_cast<PictureLayerImpl*>(host_impl->sync_tree()->root_layer());
    PictureLayerTiling* sync_tiling =
        sync_layer_impl->picture_layer_tiling_set()->FindTilingWithResolution(
            TileResolution::HIGH_RESOLUTION);

    for (int i = 0; i < 4; i++) {
      for (int j = 0; j < 2; j++) {
        Tile* tile =
            sync_tiling->TileAt(i, j) ? sync_tiling->TileAt(i, j) : nullptr;

        // If this is the pending tree, then only the invalidated tiles
        // exist and have a raster task. If its the active tree, then only
        // the invalidated tiles have a raster task.
        if (i < 2) {
          ASSERT_TRUE(tile);
          EXPECT_TRUE(tile->HasRasterTask());
        } else if (host_impl->pending_tree()) {
          EXPECT_EQ(tile, nullptr);
        } else {
          ASSERT_TRUE(tile);
          EXPECT_FALSE(tile->HasRasterTask());
        }
      }
    }

    // Insetting of image is included in the update rect.
    EXPECT_EQ(sync_layer_impl->update_rect(), gfx::Rect(-1, -1, 452, 452));
  }

  void CommitCompleteOnThread(LayerTreeHostImpl* host_impl) override {
    num_of_commits_++;
  }

  void DidActivateTreeOnThread(LayerTreeHostImpl* host_impl) override {
    num_of_activations_++;
    if (num_of_activations_ == 2) {
      EndTest();
    }
  }

  void AfterTest() override {
    EXPECT_EQ(num_of_activations_, 2);
    EXPECT_EQ(num_of_commits_, 1);
    EXPECT_EQ(num_of_impl_side_invalidations_, 1);
  }

  int num_of_activations_ = 0;
  int num_of_commits_ = 0;
  int num_of_impl_side_invalidations_ = 0;
};

// Checkering of content is only done on the pending tree which does not exist
// in single-threaded mode.
MULTI_THREAD_TEST_F(LayerTreeHostCheckerImagingTestImplSideTree);

}  // namespace
}  // namespace cc
