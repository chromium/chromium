// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/fake_content_layer_client.h"
#include "cc/test/fake_picture_layer.h"
#include "cc/test/layer_tree_test.h"
#include "cc/test/property_tree_test_utils.h"
#include "cc/trees/layer_tree_impl.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/common/surfaces/subtree_capture_id.h"

namespace cc {
namespace {

constexpr viz::SubtreeCaptureId kCaptureId(base::Token(0u, 22u));

// A base class for tests that verifies the bahvior of the layer tree when a
// sub layer has a valid viz::SubtreeCaptureId.
class LayerTreeHostCaptureTest : public LayerTreeTest {
 public:
  void SetupTree() override {
    scoped_refptr<Layer> root = FakePictureLayer::Create(&client_);
    root->SetBounds(gfx::Size(100, 100));

    child_ = FakePictureLayer::Create(&client_);
    child_->SetBounds(gfx::Size(50, 60));
    child_->SetPosition(gfx::PointF(10.f, 5.5f));
    root->AddChild(child_);

    grand_child_ = FakePictureLayer::Create(&client_);
    grand_child_->SetBounds(gfx::Size(70, 30));
    grand_child_->SetPosition(gfx::PointF(50.f, 50.f));
    child_->AddChild(grand_child_);

    layer_tree_host()->SetRootLayer(root);
    LayerTreeTest::SetupTree();
    client_.set_bounds(root->bounds());
  }

  void DrawLayersOnThread(LayerTreeHostImpl* impl) override {
    LayerImpl* root = impl->active_tree()->root_layer();
    LayerImpl* child = impl->active_tree()->LayerById(child_->id());
    LayerImpl* grand_child = impl->active_tree()->LayerById(grand_child_->id());

    VerifyLayerImpls(root, child, grand_child);
  }

 protected:
  // Lets test subclasses to verify the LayerImpls of the layers in the tree.
  virtual void VerifyLayerImpls(LayerImpl* root,
                                LayerImpl* child,
                                LayerImpl* grand_child) = 0;

  FakeContentLayerClient client_;
  scoped_refptr<Layer> child_;
  scoped_refptr<Layer> grand_child_;
};

// -----------------------------------------------------------------------------
// LayerTreeHostCaptureTestNoExtraRenderPassWhenNotCapturing:
//
// Tests that a layer tree that doesn't have a viz::SubtreeCaptureId on any of
// its layers, draw in a single root render surface, and generates a single
// compositor render pass.
class LayerTreeHostCaptureTestNoExtraRenderPassWhenNotCapturing
    : public LayerTreeHostCaptureTest {
 public:
  void BeginTest() override { PostSetNeedsCommitToMainThread(); }

  void VerifyLayerImpls(LayerImpl* root,
                        LayerImpl* child,
                        LayerImpl* grand_child) override {
    // All layers in the tree draw in the same root render surface.
    auto* root_surface = GetRenderSurface(root);
    auto* child_surface = GetRenderSurface(child);
    auto* grand_child_surface = GetRenderSurface(grand_child);
    EXPECT_EQ(root_surface, child_surface);
    EXPECT_EQ(root_surface, grand_child_surface);
    EXPECT_FALSE(root_surface->CopyOfOutputRequired());
    EXPECT_FALSE(root_surface->SubtreeCaptureId().is_valid());

    EndTest();
  }

  void DisplayReceivedCompositorFrameOnThread(
      const viz::CompositorFrame& frame) override {
    // There should be a single compositor render pass, which has no valid
    // SubtreeCaptureId.
    ASSERT_EQ(frame.render_pass_list.size(), 1u);
    EXPECT_FALSE(frame.render_pass_list.back()->subtree_capture_id.is_valid());
  }
};

SINGLE_AND_MULTI_THREAD_TEST_F(
    LayerTreeHostCaptureTestNoExtraRenderPassWhenNotCapturing);

// -----------------------------------------------------------------------------
// LayerTreeHostCaptureTestLayerWithCaptureIdElevatesToSurface
//
// Tests that a layer sub tree whose root has a valid viz::SubtreeCaptureId will
// draw into a separate render surface and a separate render pass.
class LayerTreeHostCaptureTestLayerWithCaptureIdElevatesToSurface
    : public LayerTreeHostCaptureTest {
 public:
  void BeginTest() override { child_->SetSubtreeCaptureId(kCaptureId); }

  void VerifyLayerImpls(LayerImpl* root,
                        LayerImpl* child,
                        LayerImpl* grand_child) override {
    // |child| should draw into a separate render surface from that of the root,
    // and the |grand_child| should draw into the render surface of its parent
    // (which is |child|'s).
    // The |chils|'s surface should have the expected capture ID.
    auto* root_surface = GetRenderSurface(root);
    auto* child_surface = GetRenderSurface(child);
    auto* grand_child_surface = GetRenderSurface(grand_child);
    EXPECT_NE(root_surface, child_surface);
    EXPECT_NE(root_surface, grand_child_surface);
    EXPECT_EQ(child_surface, grand_child_surface);
    EXPECT_EQ(kCaptureId, child_surface->SubtreeCaptureId());
    EXPECT_TRUE(child_surface->CopyOfOutputRequired());

    EndTest();
  }

  void DisplayReceivedCompositorFrameOnThread(
      const viz::CompositorFrame& frame) override {
    // There should be 2 render passes. The non-root render pass is associated
    // with the layer subtree rooted at |child| and should have the expected
    // capture ID.
    ASSERT_EQ(frame.render_pass_list.size(), 2u);
    EXPECT_TRUE(frame.render_pass_list.front()->subtree_capture_id.is_valid());
    EXPECT_EQ(kCaptureId, frame.render_pass_list.front()->subtree_capture_id);
    EXPECT_FALSE(frame.render_pass_list.back()->subtree_capture_id.is_valid());
  }
};

SINGLE_AND_MULTI_THREAD_TEST_F(
    LayerTreeHostCaptureTestLayerWithCaptureIdElevatesToSurface);

}  // namespace
}  // namespace cc
