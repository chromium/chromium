// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/damage_tracker.h"

#include <stddef.h>
#include <limits>

#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/unguessable_token.h"
#include "cc/base/math_util.h"
#include "cc/layers/layer_impl.h"
#include "cc/layers/view_transition_content_layer_impl.h"
#include "cc/paint/filter_operation.h"
#include "cc/paint/filter_operations.h"
#include "cc/test/fake_picture_layer_impl.h"
#include "cc/test/fake_raster_source.h"
#include "cc/test/layer_tree_impl_test_base.h"
#include "cc/test/property_tree_test_utils.h"
#include "cc/trees/effect_node.h"
#include "cc/trees/layer_tree_impl.h"
#include "cc/trees/single_thread_proxy.h"
#include "cc/trees/transform_node.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/quad_f.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace cc {
namespace {

class TestLayerImpl : public LayerImpl {
 public:
  static std::unique_ptr<TestLayerImpl> Create(LayerTreeImpl* tree_impl,
                                               int id) {
    return base::WrapUnique(new TestLayerImpl(tree_impl, id));
  }

  void AddDamageRect(const gfx::Rect& damage_rect);
  void SetDamageReasons(DamageReasonSet reasons);

  // LayerImpl overrides.
  gfx::Rect GetDamageRect() const override;
  DamageReasonSet GetDamageReasons() const override;
  void ResetChangeTracking() override;

 private:
  TestLayerImpl(LayerTreeImpl* tree_impl, int id);

  gfx::Rect damage_rect_;
  DamageReasonSet damage_reasons_;
};

TestLayerImpl::TestLayerImpl(LayerTreeImpl* tree_impl, int id)
    : LayerImpl(tree_impl, id) {}

void TestLayerImpl::AddDamageRect(const gfx::Rect& damage_rect) {
  damage_rect_.Union(damage_rect);
}

void TestLayerImpl::SetDamageReasons(DamageReasonSet reasons) {
  damage_reasons_ = reasons;
}

gfx::Rect TestLayerImpl::GetDamageRect() const {
  return damage_rect_;
}

DamageReasonSet TestLayerImpl::GetDamageReasons() const {
  return damage_reasons_;
}

void TestLayerImpl::ResetChangeTracking() {
  LayerImpl::ResetChangeTracking();
  damage_rect_.SetRect(0, 0, 0, 0);
}

class TestViewTransitionContentLayerImpl
    : public ViewTransitionContentLayerImpl {
 public:
  static std::unique_ptr<TestViewTransitionContentLayerImpl> Create(
      LayerTreeImpl* tree_impl,
      int id,
      const viz::ViewTransitionElementResourceId& resource_id,
      bool is_live_content_layer) {
    return base::WrapUnique(new TestViewTransitionContentLayerImpl(
        tree_impl, id, resource_id, is_live_content_layer));
  }

  void AddDamageRect(const gfx::Rect& damage_rect);

  // LayerImpl overrides.
  gfx::Rect GetDamageRect() const override;
  void ResetChangeTracking() override;

 private:
  TestViewTransitionContentLayerImpl(
      LayerTreeImpl* tree_impl,
      int id,
      const viz::ViewTransitionElementResourceId& resource_id,
      bool is_live_content_layer);

  gfx::Rect damage_rect_;
};

TestViewTransitionContentLayerImpl::TestViewTransitionContentLayerImpl(
    LayerTreeImpl* tree_impl,
    int id,
    const viz::ViewTransitionElementResourceId& resource_id,
    bool is_live_content_layer)
    : ViewTransitionContentLayerImpl(tree_impl,
                                     id,
                                     resource_id,
                                     is_live_content_layer) {}

void TestViewTransitionContentLayerImpl::AddDamageRect(
    const gfx::Rect& damage_rect) {
  damage_rect_.Union(damage_rect);
}

gfx::Rect TestViewTransitionContentLayerImpl::GetDamageRect() const {
  return damage_rect_;
}

void TestViewTransitionContentLayerImpl::ResetChangeTracking() {
  LayerImpl::ResetChangeTracking();
  damage_rect_.SetRect(0, 0, 0, 0);
}

void ClearDamageForAllSurfaces(LayerImpl* root) {
  for (auto* layer : *root->layer_tree_impl()) {
    if (GetRenderSurface(layer))
      GetRenderSurface(layer)->damage_tracker()->DidDrawDamagedArea();
    layer->ResetChangeTracking();
  }
}

void SetCopyRequest(LayerImpl* root) {
  auto* root_node =
      root->layer_tree_impl()->property_trees()->effect_tree_mutable().Node(
          root->effect_tree_index());
  root_node->has_copy_request = true;
  root->layer_tree_impl()
      ->property_trees()
      ->effect_tree_mutable()
      .set_needs_update(true);
}

class DamageTrackerTest : public LayerTreeImplTestBase, public testing::Test {
 public:
  DamageTrackerTest()
      : LayerTreeImplTestBase(CommitToActiveTreeLayerListSettings()) {}

  LayerImpl* CreateTestTreeWithOneSurface(int number_of_children) {
    ClearLayersAndProperties();

    LayerImpl* root = root_layer();
    root->SetBounds(gfx::Size(500, 500));
    root->layer_tree_impl()->SetDeviceViewportRect(gfx::Rect(root->bounds()));
    root->SetDrawsContent(true);
    SetupRootProperties(root);

    child_layers_.resize(number_of_children);
    for (int i = 0; i < number_of_children; ++i) {
      auto* child = AddLayerInActiveTree<TestLayerImpl>();
      child->SetBounds(gfx::Size(30, 30));
      child->SetDrawsContent(true);
      child_layers_[i] = child;
      CopyProperties(root, child);
      child->SetOffsetToTransformParent(gfx::Vector2dF(100.f, 100.f));
    }
    SetElementIdsForTesting();

    return root;
  }

  LayerImpl* CreateTestTreeWithTwoSurfaces() {
    // This test tree has two render surfaces: one for the root, and one for
    // child1. Additionally, the root has a second child layer, and child1 has
    // two children of its own.

    ClearLayersAndProperties();

    LayerImpl* root = root_layer();
    root->SetBounds(gfx::Size(500, 500));
    root->layer_tree_impl()->SetDeviceViewportRect(gfx::Rect(root->bounds()));
    root->SetDrawsContent(true);
    SetupRootProperties(root);

    child1_ = AddLayerInActiveTree<TestLayerImpl>();
    grand_child1_ = AddLayerInActiveTree<TestLayerImpl>();
    grand_child2_ = AddLayerInActiveTree<TestLayerImpl>();
    child2_ = AddLayerInActiveTree<TestLayerImpl>();
    SetElementIdsForTesting();

    child1_->SetBounds(gfx::Size(30, 30));
    // With a child that draws_content, opacity will cause the layer to create
    // its own RenderSurface. This layer does not draw, but is intended to
    // create its own RenderSurface.
    child1_->SetDrawsContent(false);
    CopyProperties(root, child1_);
    CreateTransformNode(child1_).post_translation =
        gfx::Vector2dF(100.f, 100.f);
    CreateEffectNode(child1_).render_surface_reason =
        RenderSurfaceReason::kTest;

    grand_child1_->SetBounds(gfx::Size(6, 8));
    grand_child1_->SetDrawsContent(true);
    CopyProperties(child1_, grand_child1_);
    grand_child1_->SetOffsetToTransformParent(gfx::Vector2dF(200.f, 200.f));

    grand_child2_->SetBounds(gfx::Size(6, 8));
    grand_child2_->SetDrawsContent(true);
    CopyProperties(child1_, grand_child2_);
    grand_child2_->SetOffsetToTransformParent(gfx::Vector2dF(190.f, 190.f));

    child2_->SetBounds(gfx::Size(18, 18));
    child2_->SetDrawsContent(true);
    CopyProperties(root, child2_);
    child2_->SetOffsetToTransformParent(gfx::Vector2dF(11.f, 11.f));

    return root;
  }

  LayerImpl* CreateTestTreeWithFourSurfaces() {
    // This test tree has four render surfaces: one each for the root, child1_,
    // grand_child3_ and grand_child4_. child1_ has four children of its own.
    // Additionally, the root has a second child layer.
    // child1_ has a screen space rect 270,270 36x38, from
    //   - grand_child1_ 300,300, 6x8
    //   - grand_child2_ 290,290, 6x8
    //   - grand_child3_ 270,270, 6x8
    //   - grand_child4_ 280,280, 15x16
    // child2_ has a screen space rect 11,11 18x18

    ClearLayersAndProperties();

    LayerImpl* root = root_layer();
    root->SetBounds(gfx::Size(500, 500));
    root->layer_tree_impl()->SetDeviceViewportRect(gfx::Rect(root->bounds()));
    root->SetDrawsContent(true);
    SetupRootProperties(root);

    child1_ = AddLayerInActiveTree<TestLayerImpl>();
    grand_child1_ = AddLayerInActiveTree<TestLayerImpl>();
    grand_child2_ = AddLayerInActiveTree<TestLayerImpl>();
    grand_child3_ = AddLayerInActiveTree<TestLayerImpl>();
    grand_child4_ = AddLayerInActiveTree<TestLayerImpl>();
    child2_ = AddLayerInActiveTree<TestLayerImpl>();
    SetElementIdsForTesting();

    child1_->SetBounds(gfx::Size(30, 30));
    // With a child that draws_content, opacity will cause the layer to create
    // its own RenderSurface. This layer does not draw, but is intended to
    // create its own RenderSurface.
    child1_->SetDrawsContent(false);
    CopyProperties(root, child1_);
    CreateTransformNode(child1_).post_translation =
        gfx::Vector2dF(100.f, 100.f);
    CreateEffectNode(child1_).render_surface_reason =
        RenderSurfaceReason::kTest;

    grand_child1_->SetBounds(gfx::Size(6, 8));
    grand_child1_->SetDrawsContent(true);
    CopyProperties(child1_, grand_child1_);
    grand_child1_->SetOffsetToTransformParent(gfx::Vector2dF(200.f, 200.f));

    grand_child2_->SetBounds(gfx::Size(6, 8));
    grand_child2_->SetDrawsContent(true);
    CopyProperties(child1_, grand_child2_);
    grand_child2_->SetOffsetToTransformParent(gfx::Vector2dF(190.f, 190.f));

    grand_child3_->SetBounds(gfx::Size(6, 8));
    grand_child3_->SetDrawsContent(true);
    CopyProperties(child1_, grand_child3_);
    CreateEffectNode(grand_child3_).render_surface_reason =
        RenderSurfaceReason::kTest;
    CreateTransformNode(grand_child3_).post_translation =
        gfx::Vector2dF(170.f, 170.f);

    grand_child4_->SetBounds(gfx::Size(15, 16));
    grand_child4_->SetDrawsContent(true);
    CopyProperties(child1_, grand_child4_);
    CreateEffectNode(grand_child4_).render_surface_reason =
        RenderSurfaceReason::kTest;
    CreateTransformNode(grand_child4_).post_translation =
        gfx::Vector2dF(180.f, 180.f);

    child2_->SetBounds(gfx::Size(18, 18));
    child2_->SetDrawsContent(true);
    CopyProperties(root, child2_);
    child2_->SetOffsetToTransformParent(gfx::Vector2dF(11.f, 11.f));

    return root;
  }

  LayerImpl* CreateAndSetUpTestTreeWithOneSurface(int number_of_children = 1) {
    LayerImpl* root = CreateTestTreeWithOneSurface(number_of_children);

    // Setup includes going past the first frame which always damages
    // everything, so that we can actually perform specific tests.
    EmulateDrawingOneFrame(root);

    return root;
  }

  LayerImpl* CreateAndSetUpTestTreeWithTwoSurfaces() {
    LayerImpl* root = CreateTestTreeWithTwoSurfaces();

    // Setup includes going past the first frame which always damages
    // everything, so that we can actually perform specific tests.
    EmulateDrawingOneFrame(root);

    return root;
  }

  LayerImpl* CreateAndSetUpTestTreeWithTwoSurfacesDrawingFullyVisible() {
    LayerImpl* root = CreateTestTreeWithTwoSurfaces();
    // Make sure render surface takes content outside visible rect into
    // consideration.
    root->layer_tree_impl()
        ->property_trees()
        ->effect_tree_mutable()
        .Node(child1_->effect_tree_index())
        ->backdrop_filters.Append(
            FilterOperation::CreateZoomFilter(2.f /* zoom */, 0 /* inset */));

    // Setup includes going past the first frame which always damages
    // everything, so that we can actually perform specific tests.
    EmulateDrawingOneFrame(root);

    return root;
  }

  LayerImpl* CreateAndSetUpTestTreeWithFourSurfaces() {
    LayerImpl* root = CreateTestTreeWithFourSurfaces();

    // Setup includes going past the first frame which always damages
    // everything, so that we can actually perform specific tests.
    EmulateDrawingOneFrame(root);

    return root;
  }

  void EmulateDrawingOneFrame(LayerImpl* root,
                              float device_scale_factor = 1.f) {
    // This emulates only steps that are relevant to testing the damage tracker:
    //   1. computing the render passes and layerlists
    //   2. updating all damage trackers in the correct order
    //   3. resetting all update_rects and property_changed flags for all layers
    //      and surfaces.

    root->layer_tree_impl()->SetDeviceScaleFactor(device_scale_factor);
    root->layer_tree_impl()->set_needs_update_draw_properties();
    UpdateDrawProperties(root->layer_tree_impl());

    DamageTracker::UpdateDamageTracking(root->layer_tree_impl());

    root->layer_tree_impl()->ResetAllChangeTracking();
  }

 protected:
  void ClearLayersAndProperties() {
    host_impl()->active_tree()->DetachLayersKeepingRootLayerForTesting();
    host_impl()->active_tree()->property_trees()->clear();
    child_layers_.clear();
    child1_ = nullptr;
    child2_ = nullptr;
    grand_child1_ = nullptr;
    grand_child2_ = nullptr;
    grand_child3_ = nullptr;
    grand_child4_ = nullptr;
  }

  // Stores result of CreateTestTreeWithOneSurface().
  std::vector<raw_ptr<TestLayerImpl, VectorExperimental>> child_layers_;

  // Store result of CreateTestTreeWithTwoSurfaces().
  raw_ptr<TestLayerImpl> child1_ = nullptr;
  raw_ptr<TestLayerImpl> child2_ = nullptr;
  raw_ptr<TestLayerImpl, DanglingUntriaged> grand_child1_ = nullptr;
  raw_ptr<TestLayerImpl, DanglingUntriaged> grand_child2_ = nullptr;
  raw_ptr<TestLayerImpl> grand_child3_ = nullptr;
  raw_ptr<TestLayerImpl> grand_child4_ = nullptr;
};

TEST_F(DamageTrackerTest, SanityCheckTestTreeWithOneSurface) {
  // Sanity check that the simple test tree will actually produce the expected
  // render surfaces.

  LayerImpl* root = CreateAndSetUpTestTreeWithOneSurface();
  LayerImpl* child = child_layers_[0];

  EXPECT_EQ(2, GetRenderSurface(root)->num_contributors());
  EXPECT_TRUE(root->contributes_to_drawn_render_surface());
  EXPECT_TRUE(child->contributes_to_drawn_render_surface());

  gfx::Rect root_damage_rect;
  EXPECT_TRUE(GetRenderSurface(root)->damage_tracker()->GetDamageRectIfValid(
      &root_damage_rect));

  EXPECT_EQ(gfx::Rect(500, 500).ToString(), root_damage_rect.ToString());
  EXPECT_TRUE(GetRenderSurface(root)
                  ->damage_tracker()
                  ->has_damage_from_contributing_content());
}

TEST_F(DamageTrackerTest, SanityCheckTestTreeWithTwoSurfaces) {
  // Sanity check that the complex test tree will actually produce the expected
  // render surfaces.

  LayerImpl* root = CreateAndSetUpTestTreeWithTwoSurfaces();

  gfx::Rect child_damage_rect;
  EXPECT_TRUE(GetRenderSurface(child1_)->damage_tracker()->GetDamageRectIfValid(
      &child_damage_rect));
  gfx::Rect root_damage_rect;
  EXPECT_TRUE(GetRenderSurface(root)->damage_tracker()->GetDamageRectIfValid(
      &root_damage_rect));

  EXPECT_NE(GetRenderSurface(child1_), GetRenderSurface(root));
  EXPECT_EQ(GetRenderSurface(child2_), GetRenderSurface(root));
  EXPECT_EQ(3, GetRenderSurface(root)->num_contributors());
  EXPECT_EQ(2, GetRenderSurface(child1_)->num_contributors());

  // The render surface for child1_ only has a content_rect that encloses
  // grand_child1_ and grand_child2_, because child1_ does not draw content.
  EXPECT_EQ(gfx::Rect(190, 190, 16, 18).ToString(),
            child_damage_rect.ToString());
  EXPECT_EQ(gfx::Rect(500, 500).ToString(), root_damage_rect.ToString());

  EXPECT_TRUE(GetRenderSurface(root)
                  ->damage_tracker()
                  ->has_damage_from_contributing_content());
  EXPECT_TRUE(GetRenderSurface(child1_)
                  ->damage_tracker()
                  ->has_damage_from_contributing_content());
}

TEST_F(DamageTrackerTest, VerifyDamageForUpdateRects) {
  LayerImpl* root = CreateAndSetUpTestTreeWithOneSurface();
  LayerImpl* child = child_layers_[0];

  // CASE 1: Setting the update rect should cause the corresponding damage to
  //         the surface.
  ClearDamageForAllSurfaces(root);
  child->UnionUpdateRect(gfx::Rect(10, 11, 12, 13));
  EmulateDrawingOneFrame(root);

  // Damage position on the surface should be: position of update_rect (10, 11)
  // relative to the child (100, 100).
  gfx::Rect root_damage_rect;
  EXPECT_TRUE(GetRenderSurface(root)->damage_tracker()->GetDamageRectIfValid(
      &root_damage_rect));
  EXPECT_EQ(gfx::Rect(110, 111, 12, 13).ToString(),
            root_damage_rect.ToString());

  // CASE 2: The same update rect twice in a row still produces the same
  //         damage.
  ClearDamageForAllSurfaces(root);
  child->UnionUpdateRect(gfx::Rect(10, 11, 12, 13));
  EmulateDrawingOneFrame(root);
  EXPECT_TRUE(GetRenderSurface(root)->damage_tracker()->GetDamageRectIfValid(
      &root_damage_rect));
  EXPECT_EQ(gfx::Rect(110, 111, 12, 13).ToString(),
            root_damage_rect.ToString());

  // CASE 3: Setting a different update rect should cause damage on the new
  //         update region, but no additional exposed old region.
  ClearDamageForAllSurfaces(root);
  child->UnionUpdateRect(gfx::Rect(20, 25, 1, 2));
  EmulateDrawingOneFrame(root);

  // Damage position on the surface should be: position of update_rect (20, 25)
  // relative to the child (100, 100).
  EXPECT_TRUE(GetRenderSurface(root)->damage_tracker()->GetDamageRectIfValid(
      &root_damage_rect));
  EXPECT_EQ(gfx::Rect(120, 125, 1, 2).ToString(), root_damage_rect.ToString());
  EXPECT_TRUE(GetRenderSurface(root)
                  ->damage_tracker()
                  ->has_damage_from_contributing_content());
}

TEST_F(DamageTrackerTest, VerifyDamageForLayerDamageRects) {
  LayerImpl* root = CreateAndSetUpTestTreeWithOneSurface();
  TestLayerImpl* child = child_layers_[0];

  // CASE 1: Adding the layer damage rect should cause the corresponding damage
  // to the surface.
  ClearDamageForAllSurfaces(root);
  child->AddDamageRect(gfx::Rect(10, 11, 12, 13));
  child->SetDamageReasons({DamageReason::kAnimatedImage});
  EmulateDrawingOneFrame(root);

  // Damage position on the surface should be: position of layer damage_rect
  // (10, 11) relative to the child (100, 100).
  gfx::Rect root_damage_rect;
  EXPECT_TRUE(GetRenderSurface(root)->damage_tracker()->GetDamageRectIfValid(
      &root_damage_rect));
  EXPECT_EQ(true, root_damage_rect.Contains(gfx::Rect(110, 111, 12, 13)));
  EXPECT_EQ(GetRenderSurface(root)->damage_tracker()->GetDamageReasons(),
            DamageReasonSet{DamageReason::kAnimatedImage});

  // CASE 2: The same layer damage rect twice in a row still produces the same
  // damage.
  ClearDamageForAllSurfaces(root);
  child->AddDamageRect(gfx::Rect(10, 11, 12, 13));
  child->SetDamageReasons({DamageReason::kScrollbarFadeOutAnimation});
  EmulateDrawingOneFrame(root);
  EXPECT_TRUE(GetRenderSurface(root)->damage_tracker()->GetDamageRectIfValid(
      &root_damage_rect));
  EXPECT_EQ(true, root_damage_rect.Contains(gfx::Rect(110, 111, 12, 13)));
  EXPECT_EQ(GetRenderSurface(root)->damage_tracker()->GetDamageReasons(),
            DamageReasonSet{DamageReason::kScrollbarFadeOutAnimation});

  // CASE 3: Adding a different layer damage rect should cause damage on the
  // new damaged region, but no additional exposed old region.
  ClearDamageForAllSurfaces(root);
  child->AddDamageRect(gfx::Rect(20, 25, 1, 2));
  child->SetDamageReasons({DamageReason::kAnimatedImage});
  EmulateDrawingOneFrame(root);

  // Damage position on the surface should be: position of layer damage_rect
  // (20, 25) relative to the child (100, 100).
  EXPECT_TRUE(GetRenderSurface(root)->damage_tracker()->GetDamageRectIfValid(
      &root_damage_rect));
  EXPECT_EQ(true, root_damage_rect.Contains(gfx::Rect(120, 125, 1, 2)));
  EXPECT_EQ(GetRenderSurface(root)->damage_tracker()->GetDamageReasons(),
            DamageReasonSet{DamageReason::kAnimatedImage});

  // CASE 4: Adding multiple layer damage rects should cause a unified
  // damage on root damage rect.
  ClearDamageForAllSurfaces(root);
  child->AddDamageRect(gfx::Rect(20, 25, 1, 2));
  child->AddDamageRect(gfx::Rect(10, 15, 3, 4));
  child->SetDamageReasons(
      {DamageReason::kAnimatedImage, DamageReason::kUntracked});
  EmulateDrawingOneFrame(root);

  // Damage position on the surface should be: position of layer damage_rect
  // (20, 25) relative to the child (100, 100).
  EXPECT_TRUE(GetRenderSurface(root)->damage_tracker()->GetDamageRectIfValid(
      &root_damage_rect));
  EXPECT_EQ(true, root_damage_rect.Contains(gfx::Rect(120, 125, 1, 2)));
  EXPECT_EQ(true, root_damage_rect.Contains(gfx::Rect(110, 115, 3, 4)));
  EXPECT_TRUE(GetRenderSurface(root)
                  ->damage_tracker()
                  ->has_damage_from_contributing_content());
  EXPECT_EQ(GetRenderSurface(root)->damage_tracker()->GetDamageReasons(),
            (DamageReasonSet{DamageReason::kAnimatedImage,
                             DamageReason::kUntracked}));
}

TEST_F(DamageTrackerTest, VerifyDamageForLayerUpdateAndDamageRects) {
  LayerImpl* root = CreateAndSetUpTestTreeWithOneSurface();
  TestLayerImpl* child = child_layers_[0];

  // CASE 1: Adding the layer damage rect and update rect should cause the
  // corresponding damage to the surface.
  ClearDamageForAllSurfaces(root);
  child->AddDamageRect(gfx::Rect(5, 6, 12, 13));
  child->UnionUpdateRect(gfx::Rect(15, 16, 14, 10));
  child->SetDamageReasons({DamageReason::kAnimatedImage});
  EmulateDrawingOneFrame(root);

  // Damage position on the surface should be: position of unified layer
  // damage_rect and update rect (5, 6)
  // relative to the child (100, 100).
  gfx::Rect root_damage_rect;
  EXPECT_TRUE(GetRenderSurface(root)->damage_tracker()->GetDamageRectIfValid(
      &root_damage_rect));
  EXPECT_EQ(true, root_damage_rect.Contains(gfx::Rect(105, 106, 24, 20)));
  EXPECT_EQ(GetRenderSurface(root)->damage_tracker()->GetDamageReasons(),
            DamageReasonSet{DamageReason::kAnimatedImage});

  // CASE 2: The same layer damage rect and update rect twice in a row still
  // produces the same damage.
  ClearDamageForAllSurfaces(root);
  child->AddDamageRect(gfx::Rect(10, 11, 12, 13));
  child->UnionUpdateRect(gfx::Rect(10, 11, 14, 15));
  child->SetDamageReasons({DamageReason::kAnimatedImage});
  EmulateDrawingOneFrame(root);
  EXPECT_TRUE(GetRenderSurface(root)->damage_tracker()->GetDamageRectIfValid(
      &root_damage_rect));
  EXPECT_EQ(true, root_damage_rect.Contains(gfx::Rect(110, 111, 14, 15)));
  EXPECT_EQ(GetRenderSurface(root)->damage_tracker()->GetDamageReasons(),
            DamageReasonSet{DamageReason::kAnimatedImage});

  // CASE 3: Adding a different layer damage rect and update rect should cause
  // damage on the new damaged region, but no additional exposed old region.
  ClearDamageForAllSurfaces(root);
  child->AddDamageRect(gfx::Rect(20, 25, 2, 3));
  child->UnionUpdateRect(gfx::Rect(5, 10, 7, 8));
  child->SetDamageReasons({DamageReason::kAnimatedImage});
  EmulateDrawingOneFrame(root);

  // Damage position on the surface should be: position of unified layer damage
  // rect and update rect (5, 10) relative to the child (100, 100).
  EXPECT_TRUE(GetRenderSurface(root)->damage_tracker()->GetDamageRectIfValid(
      &root_damage_rect));
  EXPECT_EQ(true, root_damage_rect.Contains(gfx::Rect(105, 110, 17, 18)));
  EXPECT_TRUE(GetRenderSurface(root)
                  ->damage_tracker()
                  ->has_damage_from_contributing_content());
  EXPECT_EQ(GetRenderSurface(root)->damage_tracker()->GetDamageReasons(),
            DamageReasonSet{DamageReason::kAnimatedImage});
}

TEST_F(DamageTrackerTest, VerifyDamageForPropertyChanges) {
  LayerImpl* root = CreateAndSetUpTestTreeWithOneSurface();
  TestLayerImpl* child = child_layers_[0];

  // CASE 1: The layer's property changed flag takes priority over update rect.
  //
  CreateEffectNode(child).render_surface_reason = RenderSurfaceReason::kTest;
  EmulateDrawingOneFrame(root);
  ClearDamageForAllSurfaces(root);
  child->UnionUpdateRect(gfx::Rect(10, 11, 12, 13));
  root->layer_tree_impl()->SetOpacityMutated(child->element_id(), 0.5f);
  child->SetDamageReasons({DamageReason::kAnimatedImage});
  EmulateDrawingOneFrame(root);

  ASSERT_EQ(2, GetRenderSurface(root)->num_contributors());
  EXPECT_EQ(GetRenderSurface(root)->damage_tracker()->GetDamageReasons(),
            DamageReasonSet{DamageReason::kAnimatedImage});

  // Damage should be the entire child layer in target_surface space.
  gfx::Rect expected_rect = gfx::Rect(100, 100, 30, 30);
  gfx::Rect root_damage_rect;
  EXPECT_TRUE(GetRenderSurface(root)->damage_tracker()->GetDamageRectIfValid(
      &root_damage_rect));
  EXPECT_EQ(expected_rect.ToString(), root_damage_rect.ToString());
  EXPECT_TRUE(GetRenderSurface(root)
                  ->damage_tracker()
                  ->has_damage_from_contributing_content());
  EXPECT_TRUE(GetRenderSurface(child)
                  ->damage_tracker()
                  ->has_damage_from_contributing_content());

  // CASE 2: If a layer moves due to property change, it damages both the new
  //         location and the old (exposed) location. The old location is the
  //         entire old layer, not just the update_rect.

  // Cycle one frame of no change, just to sanity check that the next rect is
  // not because of the old damage state.
  ClearDamageForAllSurfaces(root);
  EmulateDrawingOneFrame(root);
  EXPECT_TRUE(GetRenderSurface(root)->damage_tracker()->GetDamageRectIfValid(
      &root_damage_rect));
  EXPECT_TRUE(root_damage_rect.IsEmpty());
  EXPECT_EQ(GetRenderSurface(root)->damage_tracker()->GetDamageReasons(),
            DamageReasonSet{});

  // Then, test the actual layer movement.
  ClearDamageForAllSurfaces(root);
  CreateTransformNode(child).post_translation =
      child->offset_to_transform_parent();
  child->SetOffsetToTransformParent(gfx::Vector2dF());
  gfx::Transform translation;
  translation.Translate(100.f, 130.f);
  root->layer_tree_impl()->SetTransformMutated(child->element_id(),
                                               translation);
  child->SetDamageReasons({DamageReason::kAnimatedImage});
  EmulateDrawingOneFrame(root);

  // Expect damage to be the combination of the previous one and the new one.
  expected_rect.Union(gfx::Rect(200, 230, 30, 30));
  EXPECT_TRUE(GetRenderSurface(root)->damage_tracker()->GetDamageRectIfValid(
      &root_damage_rect));
  EXPECT_EQ(expected_rect, root_damage_rect);
  EXPECT_TRUE(GetRenderSurface(root)
                  ->damage_tracker()
                  ->has_damage_from_contributing_content());
  // TODO(crbug.com/40646366): Transform from browser animation should not be
  // considered as damage from contributing layer since it is applied to the
  // whole layer which has a render surface.
  EXPECT_TRUE(GetRenderSurface(child)
                  ->damage_tracker()
                  ->has_damage_from_contributing_content());
  // SurfacePropertyChanged causes an extra kUntracked to be added. Verify
  // kAnimatedImage is not dropped.
  EXPECT_TRUE(GetRenderSurface(root)->damage_tracker()->GetDamageReasons().Has(
      DamageReason::kAnimatedImage));
}

TEST_F(DamageTrackerTest,
       VerifyDamageForPropertyChangesFromContributingContents) {
  LayerImpl* root = CreateAndSetUpTestTreeWithTwoSurfaces();

  // CASE 1: child1_'s opacity changed.
  ClearDamageForAllSurfaces(root);
  root->layer_tree_impl()->SetOpacityMutated(child1_->element_id(), 0.5f);
  EmulateDrawingOneFrame(root);
  EXPECT_TRUE(GetRenderSurface(root)
                  ->damage_tracker()
                  ->has_damage_from_contributing_content());
  EXPECT_FALSE(GetRenderSurface(child1_)
                   ->damage_tracker()
                   ->has_damage_from_contributing_content());

  // CASE 2: layer2_'s opacity changed.
  CreateEffectNode(child2_).render_surface_reason = RenderSurfaceReason::kTest;
  EmulateDrawingOneFrame(root);
  ClearDamageForAllSurfaces(root);
  root->layer_tree_impl()->SetOpacityMutated(child2_->element_id(), 0.5f);
  EmulateDrawingOneFrame(root);
  EXPECT_TRUE(GetRenderSurface(root)
                  ->damage_tracker()
                  ->has_damage_from_contributing_content());
  EXPECT_FALSE(GetRenderSurface(child1_)
                   ->damage_tracker()
                   ->has_damage_from_contributing_content());

  // CASE 3: grand_child1_'s opacity changed.
  CreateEffectNode(grand_child1_).render_surface_reason =
      RenderSurfaceReason::kTest;
  EmulateDrawingOneFrame(root);
  ClearDamageForAllSurfaces(root);
  root->layer_tree_impl()->SetOpacityMutated(grand_child1_->element_id(), 0.5f);
  EmulateDrawingOneFrame(root);
  EXPECT_TRUE(GetRenderSurface(root)
                  ->damage_tracker()
                  ->has_damage_from_contributing_content());
  EXPECT_TRUE(GetRenderSurface(child1_)
                  ->damage_tracker()
                  ->has_damage_from_contributing_content());
}

// Regression test for http://crbug.com/923794
TEST_F(DamageTrackerTest, EffectPropertyChangeNoSurface) {
  LayerImpl* root = CreateAndSetUpTestTreeWithOneSurface();
  LayerImpl* child = child_layers_[0];

  // Create a separate effect node for the child, but no render surface.
  auto& effect_node = CreateEffectNode(child);
  effect_node.opacity = 0.5;
  effect_node.has_potential_opacity_animation = true;
  EmulateDrawingOneFrame(root);

  EXPECT_EQ(root->transform_tree_index(), child->transform_tree_index());
  EXPECT_NE(root->effect_tree_index(), child->effect_tree_index());

  // Change the child's opacity, which should damage its target.
  ClearDamageForAllSurfaces(root);
  root->layer_tree_impl()->SetOpacityMutated(child->element_id(), 0.4f);
  EmulateDrawingOneFrame(root);
  EXPECT_TRUE(GetRenderSurface(root)
                  ->damage_tracker()
                  ->has_damage_from_contributing_content());
}

// Regression test for http://crbug.com/923794
TEST_F(DamageTrackerTest, TransformPropertyChangeNoSurface) {
  LayerImpl* root = CreateAndSetUpTestTreeWithOneSurface();
  LayerImpl* child = child_layers_[0];

  // Create a separate transform node for the child, but no render surface.
  gfx::Transform trans1;
  trans1.Scale(2, 1);
  CreateTransformNode(child).local = trans1;
  EmulateDrawingOneFrame(root);

  EXPECT_NE(root->transform_tree_index(), child->transform_tree_index());
  EXPECT_EQ(root->effect_tree_index(), child->effect_tree_index());

  // Change the child's transform , which should damage its target.
  ClearDamageForAllSurfaces(root);
  gfx::Transform trans2;
  trans2.Scale(1, 2);
  root->layer_tree_impl()->SetTransformMutated(child->element_id(), trans2);
  EmulateDrawingOneFrame(root);
  EXPECT_TRUE(GetRenderSurface(root)
                  ->damage_tracker()
                  ->has_damage_from_contributing_content());
}

TEST_F(DamageTrackerTest,
       VerifyDamageForUpdateAndDamageRectsFromContributingContents) {
  LayerImpl* root = CreateAndSetUpTestTreeWithTwoSurfaces();

  // CASE 1: Adding child1_'s damage rect and update rect should cause the
  // corresponding damage to the surface.
  child1_->SetDrawsContent(true);
  ClearDamageForAllSurfaces(root);
  child1_->AddDamageRect(gfx::Rect(105, 106, 12, 15));
  child1_->UnionUpdateRect(gfx::Rect(115, 116, 12, 15));
  EmulateDrawingOneFrame(root);
  EXPECT_TRUE(GetRenderSurface(root)
                  ->damage_tracker()
                  ->has_damage_from_contributing_content());
  EXPECT_TRUE(GetRenderSurface(child1_)
                  ->damage_tracker()
                  ->has_damage_from_contributing_content());

  // CASE 2: Adding child2_'s damage rect and update rect should cause the
  // corresponding damage to the surface.
  ClearDamageForAllSurfaces(root);
  child2_->AddDamageRect(gfx::Rect(11, 11, 12, 15));
  child2_->UnionUpdateRect(gfx::Rect(12, 12, 12, 15));
  EmulateDrawingOneFrame(root);
  EXPECT_TRUE(GetRenderSurface(root)
                  ->damage_tracker()
                  ->has_damage_from_contributing_content());
  EXPECT_FALSE(GetRenderSurface(child1_)
                   ->damage_tracker()
                   ->has_damage_from_contributing_content());

  // CASE 3: Adding grand_child1_'s damage rect and update rect should cause
  // the corresponding damage to the surface.
  ClearDamageForAllSurfaces(root);
  grand_child1_->AddDamageRect(gfx::Rect(1, 0, 2, 5));
  grand_child1_->UnionUpdateRect(gfx::Rect(2, 1, 2, 5));
  EmulateDrawingOneFrame(root);
  EXPECT_TRUE(GetRenderSurface(root)
                  ->damage_tracker()
                  ->has_damage_from_contributing_content());
  EXPECT_TRUE(GetRenderSurface(child1_)
                  ->damage_tracker()
                  ->has_damage_from_contributing_content());
}

TEST_F(DamageTrackerTest, VerifyDamageWhenSurfaceRemoved) {
  LayerImpl* root = CreateAndSetUpTestTreeWithTwoSurfaces();
  LayerImpl* surface = child1_;
  LayerImpl* child = grand_child1_;
  child->SetDrawsContent(true);
  EmulateDrawingOneFrame(root);
  ClearDamageForAllSurfaces(root);

  SetRenderSurfaceReason(surface, RenderSurfaceReason::kNone);
  child->SetDrawsContent(false);
  EmulateDrawingOneFrame(root);
  gfx::Rect root_damage_rect;
  EXPECT_TRUE(GetRenderSurface(root)->damage_tracker()->GetDamageRectIfValid(
      &root_damage_rect));
  EXPECT_EQ(gfx::Rect(290, 290, 16, 18).ToString(),
            root_damage_rect.ToString());
  EXPECT_TRUE(GetRenderSurface(root)
                  ->damage_tracker()
                  ->has_damage_from_contributing_content());
}

TEST_F(DamageTrackerTest, VerifyDamageForTransformedLayer) {
  // If a layer is transformed, the damage rect should still enclose the entire
  // transformed layer.

  LayerImpl* root = CreateAndSetUpTestTreeWithOneSurface();
  LayerImpl* child = child_layers_[0];

  gfx::Transform rotation;
  rotation.Rotate(45.0);

  ClearDamageForAllSurfaces(root);
  auto& transform_node = CreateTransformNode(child);
  transform_node.origin = gfx::Point3F(child->bounds().width() * 0.5f,
                                       child->bounds().height() * 0.5f, 0.f);
  transform_node.post_translation = gfx::Vector2dF(85.f, 85.f);
  child->SetOffsetToTransformParent(gfx::Vector2dF());
  CreateEffectNode(child).render_surface_reason = RenderSurfaceReason::kTest;

  child->NoteLayerPropertyChanged();
  EmulateDrawingOneFrame(root);

  // Sanity check that the layer actually moved to (85, 85), damaging its old
  // location and new location.
  gfx::Rect root_damage_rect;
  EXPECT_TRUE(GetRenderSurface(root)->damage_tracker()->GetDamageRectIfValid(
      &root_damage_rect));
  EXPECT_EQ(gfx::Rect(85, 85, 45, 45).ToString(), root_damage_rect.ToString());
  EXPECT_TRUE(GetRenderSurface(root)
                  ->damage_tracker()
                  ->has_damage_from_contributing_content());
  // Layer's layer_property_changed_not_from_property_trees_ should be
  // considered as damage to render surface.
  EXPECT_TRUE(GetRenderSurface(child)
                  ->damage_tracker()
                  ->has_damage_from_contributing_content());

  // With the anchor on the layer's center, now we can test the rotation more
  // intuitively, since it applies about the layer's anchor.
  ClearDamageForAllSurfaces(root);
  root->layer_tree_impl()->SetTransformMutated(child->element_id(), rotation);
  EmulateDrawingOneFrame(root);

  // Since the child layer is square, rotation by 45 degrees about the center
  // should increase the size of the expected rect by sqrt(2), centered around
  // (100, 100). The old exposed region should be fully contained in the new
  // region.
  float expected_width = 30.f * sqrt(2.f);
  float expected_position = 100.f - 0.5f * expected_width;
  gfx::Rect expected_rect = gfx::ToEnclosingRect(gfx::RectF(
      expected_position, expected_position, expected_width, expected_width));
  EXPECT_TRUE(GetRenderSurface(root)->damage_tracker()->GetDamageRectIfValid(
      &root_damage_rect));
  EXPECT_EQ(expected_rect.ToString(), root_damage_rect.ToString());
  EXPECT_TRUE(GetRenderSurface(root)
                  ->damage_tracker()
                  ->has_damage_from_contributing_content());
  // Transform from browser animation should not be considered as damage from
  // contributing layer since it is applied to the whole layer which has a
  // render surface.
  EXPECT_FALSE(GetRenderSurface(child)
                   ->damage_tracker()
                   ->has_damage_from_contributing_content());
}

TEST_F(DamageTrackerTest, VerifyDamageForPerspectiveClippedLayer) {
  // If a layer has a perspective transform that causes w < 0, then not
  // clipping the layer can cause an invalid damage rect. This test checks that
  // the w < 0 case is tracked properly.
  //
  // The transform is constructed so that if w < 0 clipping is not performed,
  // the incorrect rect will be very small, specifically: position (500.972504,
  // 498.544617) and size 0.056610 x 2.910767.  Instead, the correctly
  // transformed rect should actually be very huge (i.e. in theory, -infinity
  // on the left), and positioned so that the right-most bound rect will be
  // approximately 501 units in root surface space.
  //

  LayerImpl* root = CreateAndSetUpTestTreeWithOneSurface();
  LayerImpl* child = child_layers_[0];

  gfx::Transform transform;
  transform.Translate3d(550.0, 500.0, 0.0);
  transform.ApplyPerspectiveDepth(1.0);
  transform.RotateAboutYAxis(45.0);
  transform.Translate3d(-50.0, -50.0, 0.0);

  // Set up the child
  child->SetBounds(gfx::Size(100, 100));
  CreateTransformNode(child).local = transform;
  child->SetOffsetToTransformParent(gfx::Vector2dF());
  EmulateDrawingOneFrame(root);

  // Sanity check that the child layer's bounds would actually get clipped by
  // w < 0, otherwise this test is not actually testing the intended scenario.
  gfx::RectF test_rect(gfx::SizeF(child->bounds()));
  bool clipped = false;
  MathUtil::MapQuad(transform, gfx::QuadF(test_rect), &clipped);
  EXPECT_TRUE(clipped);

  // Damage the child without moving it.
  CreateEffectNode(child).render_surface_reason = RenderSurfaceReason::kTest;
  EmulateDrawingOneFrame(root);
  ClearDamageForAllSurfaces(root);
  root->layer_tree_impl()->SetOpacityMutated(child->element_id(), 0.5f);
  EmulateDrawingOneFrame(root);

  // The expected damage should cover the entire root surface (500x500), but we
  // don't care whether the damage rect was clamped or is larger than the
  // surface for this test.
  gfx::Rect root_damage_rect;
  EXPECT_TRUE(GetRenderSurface(root)->damage_tracker()->GetDamageRectIfValid(
      &root_damage_rect));
  gfx::Rect damage_we_care_about = gfx::Rect(gfx::Size(500, 500));
  EXPECT_TRUE(root_damage_rect.Contains(damage_we_care_about));
  EXPECT_TRUE(GetRenderSurface(root)
                  ->damage_tracker()
                  ->has_damage_from_contributing_content());
  EXPECT_FALSE(GetRenderSurface(child)
                   ->damage_tracker()
                   ->has_damage_from_contributing_content());
}

TEST_F(DamageTrackerTest, VerifyDamageForBlurredSurface) {
  LayerImpl* root = CreateAndSetUpTestTreeWithTwoSurfaces();
  LayerImpl* surface = child1_;
  LayerImpl* child = grand_child1_;

  FilterOperations filters;
  filters.Append(FilterOperation::CreateBlurFilter(5.f));

  // TODO(crbug.com/40646366): Setting the filter on an existing render surface
  // should not damage the conrresponding render surface.
  ClearDamageForAllSurfaces(root);
  SetFilter(surface, filters);
  EmulateDrawingOneFrame(root);
  EXPECT_TRUE(GetRenderSurface(root)
                  ->damage_tracker()
                  ->has_damage_from_contributing_content());
  EXPECT_FALSE(GetRenderSurface(surface)
                   ->damage_tracker()
                   ->has_damage_from_contributing_content());

  // Setting the update rect should cause the corresponding damage to the
  // surface, blurred based on the size of the blur filter.
  ClearDamageForAllSurfaces(root);
  child->UnionUpdateRect(gfx::Rect(1, 2, 3, 4));
  EmulateDrawingOneFrame(root);

  // Damage position on the surface should be: position of update_rect (1, 2)
  // relative to the child (300, 300), but expanded by the blur outsets
  // (15, since the blur radius is 5).
  gfx::Rect root_damage_rect;
  EXPECT_TRUE(GetRenderSurface(root)->damage_tracker()->GetDamageRectIfValid(
      &root_damage_rect));
  EXPECT_EQ(gfx::Rect(286, 287, 33, 34), root_damage_rect);
  EXPECT_TRUE(GetRenderSurface(root)
                  ->damage_tracker()
                  ->has_damage_from_contributing_content());
  EXPECT_TRUE(GetRenderSurface(surface)
                  ->damage_tracker()
                  ->has_damage_from_contributing_content());
}

TEST_F(DamageTrackerTest, VerifyDamageForImageFilter) {
  LayerImpl* root = CreateAndSetUpTestTreeWithOneSurface();
  LayerImpl* child = child_layers_[0];
  gfx::Rect root_damage_rect, child_damage_rect;

  // Allow us to set damage on child too.
  child->SetDrawsContent(true);

  FilterOperations filters;
  filters.Append(FilterOperation::CreateReferenceFilter(
      sk_make_sp<BlurPaintFilter>(2, 2, SkTileMode::kDecal, nullptr)));

  // Setting the filter will damage the whole surface.
  CreateTransformNode(child).post_translation =
      child->offset_to_transform_parent();
  child->SetOffsetToTransformParent(gfx::Vector2dF());
  CreateEffectNode(child).render_surface_reason = RenderSurfaceReason::kTest;
  EmulateDrawingOneFrame(root);
  ClearDamageForAllSurfaces(root);
  child->layer_tree_impl()->SetFilterMutated(child->element_id(), filters);
  EmulateDrawingOneFrame(root);
  EXPECT_TRUE(GetRenderSurface(root)->damage_tracker()->GetDamageRectIfValid(
      &root_damage_rect));
  EXPECT_TRUE(GetRenderSurface(child)->damage_tracker()->GetDamageRectIfValid(
      &child_damage_rect));

  // gfx::Rect(100, 100, 30, 30), expanded by 6px for the 2px blur filter.
  EXPECT_EQ(gfx::Rect(94, 94, 42, 42), root_damage_rect);

  // gfx::Rect(0, 0, 30, 30), expanded by 6px for the 2px blur filter.
  EXPECT_EQ(gfx::Rect(-6, -6, 42, 42), child_damage_rect);

  EXPECT_TRUE(GetRenderSurface(root)
                  ->damage_tracker()
                  ->has_damage_from_contributing_content());
  EXPECT_FALSE(GetRenderSurface(child)
                   ->damage_tracker()
                   ->has_damage_from_contributing_content());

  // CASE 1: Setting the update rect should damage the whole surface (for now)
  ClearDamageForAllSurfaces(root);
  child->UnionUpdateRect(gfx::Rect(1, 1));
  EmulateDrawingOneFrame(root);

  EXPECT_TRUE(GetRenderSurface(root)->damage_tracker()->GetDamageRectIfValid(
      &root_damage_rect));
  EXPECT_TRUE(GetRenderSurface(child)->damage_tracker()->GetDamageRectIfValid(
      &child_damage_rect));

  // gfx::Rect(100, 100, 1, 1), expanded by 6px for the 2px blur filter.
  EXPECT_EQ(gfx::Rect(94, 94, 13, 13), root_damage_rect);

  // gfx::Rect(0, 0, 1, 1), expanded by 6px for the 2px blur filter.
  EXPECT_EQ(gfx::Rect(-6, -6, 13, 13), child_damage_rect);

  EXPECT_TRUE(GetRenderSurface(root)
                  ->damage_tracker()
                  ->has_damage_from_contributing_content());
  EXPECT_TRUE(GetRenderSurface(child)
                  ->damage_tracker()
                  ->has_damage_from_contributing_content());

  // CASE 2: No changes, so should not damage the surface.
  ClearDamageForAllSurfaces(root);
  EmulateDrawingOneFrame(root);

  EXPECT_TRUE(GetRenderSurface(root)->damage_tracker()->GetDamageRectIfValid(
      &root_damage_rect));
  EXPECT_TRUE(GetRenderSurface(child)->damage_tracker()->GetDamageRectIfValid(
      &child_damage_rect));

  // Should not be expanded by the blur filter.
  EXPECT_EQ(gfx::Rect(), root_damage_rect);
  EXPECT_EQ(gfx::Rect(), child_damage_rect);

  EXPECT_FALSE(GetRenderSurface(root)
                   ->damage_tracker()
                   ->has_damage_from_contributing_content());
  EXPECT_FALSE(GetRenderSurface(child)
                   ->damage_tracker()
                   ->has_damage_from_contributing_content());
}

TEST_F(DamageTrackerTest, VerifyDamageForTransformedImageFilter) {
  LayerImpl* root = CreateAndSetUpTestTreeWithOneSurface();
  LayerImpl* child = child_layers_[0];
  gfx::Rect root_damage_rect, child_damage_rect;

  // Allow us to set damage on child too.
  child->SetDrawsContent(true);

  FilterOperations filters;
  filters.Append(FilterOperation::CreateReferenceFilter(
      sk_make_sp<BlurPaintFilter>(2, 2, SkTileMode::kDecal, nullptr)));

  // Setting the filter will damage the whole surface.
  gfx::Transform transform;
  transform.RotateAboutYAxis(60);
  ClearDamageForAllSurfaces(root);
  auto& transform_node = CreateTransformNode(child);
  transform_node.local = transform;
  transform_node.post_translation = child->offset_to_transform_parent();
  child->SetOffsetToTransformParent(gfx::Vector2dF());
  auto& effect_node = CreateEffectNode(child);
  effect_node.render_surface_reason = RenderSurfaceReason::kFilter;
  effect_node.has_potential_filter_animation = true;

  EmulateDrawingOneFrame(root);
  child->layer_tree_impl()->SetFilterMutated(child->element_id(), filters);
  EmulateDrawingOneFrame(root);
  EXPECT_TRUE(GetRenderSurface(root)->damage_tracker()->GetDamageRectIfValid(
      &root_damage_rect));
  EXPECT_TRUE(GetRenderSurface(child)->damage_tracker()->GetDamageRectIfValid(
      &child_damage_rect));

  // Blur outset is 6px for a 2px blur.
  int blur_outset = 6;
  int rotated_outset_left = blur_outset / 2;
  int expected_rotated_width = (30 + 2 * blur_outset) / 2;
  gfx::Rect expected_root_damage(100 - rotated_outset_left, 100 - blur_outset,
                                 expected_rotated_width, 30 + 2 * blur_outset);
  expected_root_damage.Union(gfx::Rect(100, 100, 30, 30));
  EXPECT_EQ(expected_root_damage, root_damage_rect);
  EXPECT_EQ(gfx::Rect(-blur_outset, -blur_outset, 30 + 2 * blur_outset,
                      30 + 2 * blur_outset),
            child_damage_rect);

  // Setting the update rect should damage the whole surface (for now)
  ClearDamageForAllSurfaces(root);
  child->UnionUpdateRect(gfx::Rect(30, 30));
  EmulateDrawingOneFrame(root);

  EXPECT_TRUE(GetRenderSurface(root)->damage_tracker()->GetDamageRectIfValid(
      &root_damage_rect));
  EXPECT_TRUE(GetRenderSurface(child)->damage_tracker()->GetDamageRectIfValid(
      &child_damage_rect));

  int expect_width = 30 + 2 * blur_outset;
  int expect_height = 30 + 2 * blur_outset;
  EXPECT_EQ(gfx::Rect(100 - blur_outset / 2, 100 - blur_outset,
                      expect_width / 2, expect_height),
            root_damage_rect);
  EXPECT_EQ(gfx::Rect(-blur_outset, -blur_outset, expect_width, expect_height),
            child_damage_rect);
}

TEST_F(DamageTrackerTest, VerifyDamageForHighDPIImageFilter) {
  LayerImpl* root = CreateAndSetUpTestTreeWithOneSurface();
  LayerImpl* child = child_layers_[0];
  gfx::Rect root_damage_rect, child_damage_rect;

  ClearDamageForAllSurfaces(root);
  int device_scale_factor = 2;
  EmulateDrawingOneFrame(root, device_scale_factor);

  // Allow us to set damage on child too.
  child->SetDrawsContent(true);

  FilterOperations filters;
  filters.Append(FilterOperation::CreateBlurFilter(3.f));

  // Setting the filter and creating a new render surface will damage the whole
  // surface.
  ClearDamageForAllSurfaces(root);
  CreateTransformNode(child).post_translation =
      child->offset_to_transform_parent();
  child->SetOffsetToTransformParent(gfx::Vector2dF());
  CreateEffectNode(child).render_surface_reason = RenderSurfaceReason::kTest;
  ClearDamageForAllSurfaces(root);
  child->layer_tree_impl()->SetFilterMutated(child->element_id(), filters);
  EmulateDrawingOneFrame(root, device_scale_factor);
  EXPECT_TRUE(GetRenderSurface(root)->damage_tracker()->GetDamageRectIfValid(
      &root_damage_rect));
  EXPECT_TRUE(GetRenderSurface(child)->damage_tracker()->GetDamageRectIfValid(
      &child_damage_rect));

  // Blur outset is 9px for a 3px blur, scaled up by DSF.
  int blur_outset = 9 * device_scale_factor;
  gfx::Rect expected_child_damage_rect(60, 60);
  expected_child_damage_rect.Inset(-blur_outset);
  gfx::Rect expected_root_damage_rect(child_damage_rect);
  expected_root_damage_rect.Offset(200, 200);
  EXPECT_EQ(expected_root_damage_rect, root_damage_rect);
  EXPECT_EQ(expected_child_damage_rect, child_damage_rect);

  // Setting the update rect should damage only the affected area (original,
  // outset by 3 * blur sigma * DSF).
  ClearDamageForAllSurfaces(root);
  child->UnionUpdateRect(gfx::Rect(30, 30));
  EmulateDrawingOneFrame(root, device_scale_factor);

  EXPECT_TRUE(GetRenderSurface(root)->damage_tracker()->GetDamageRectIfValid(
      &root_damage_rect));
  EXPECT_TRUE(GetRenderSurface(child)->damage_tracker()->GetDamageRectIfValid(
      &child_damage_rect));

  EXPECT_EQ(expected_root_damage_rect, root_damage_rect);
  EXPECT_EQ(expected_child_damage_rect, child_damage_rect);
}

TEST_F(DamageTrackerTest, VerifyDamageForAddingAndRemovingLayer) {
  LayerImpl* root = CreateAndSetUpTestTreeWithOneSurface();
  LayerImpl* child1 = child_layers_[0];

  // CASE 1: Adding a new layer should cause the appropriate damage.
  //
  ClearDamageForAllSurfaces(root);

  LayerImpl* child2 = AddLayerInActiveTree<LayerImpl>();
  child2->SetBounds(gfx::Size(6, 8));
  child2->SetDrawsContent(true);
  CopyProperties(root, child2);
  child2->SetOffsetToTransformParent(gfx::Vector2dF(400.f, 380.f));
  EmulateDrawingOneFrame(root);

  // Sanity check - all 3 layers should be on the same render surface; render
  // surfaces are tested elsewhere.
  ASSERT_EQ(3, GetRenderSurface(root)->num_contributors());

  gfx::Rect root_damage_rect;
  EXPECT_TRUE(GetRenderSurface(root)->damage_tracker()->GetDamageRectIfValid(
      &root_damage_rect));
  EXPECT_EQ(gfx::Rect(400, 380, 6, 8).ToString(), root_damage_rect.ToString());
  EXPECT_TRUE(GetRenderSurface(root)
                  ->damage_tracker()
                  ->has_damage_from_contributing_content());

  // CASE 2: If the layer is removed, its entire old layer becomes exposed, not
  //         just the last update rect.

  // Advance one frame without damage so that we know the damage rect is not
  // leftover from the previous case.
  ClearDamageForAllSurfaces(root);
  EmulateDrawingOneFrame(root);

  EXPECT_TRUE(GetRenderSurface(root)->damage_tracker()->GetDamageRectIfValid(
      &root_damage_rect));
  EXPECT_TRUE(root_damage_rect.IsEmpty());

  // Then, test removing child1_.
  {
    OwnedLayerImplList layers =
        host_impl()->active_tree()->DetachLayersKeepingRootLayerForTesting();
    ASSERT_EQ(3u, layers.size());
    ASSERT_EQ(child1, layers[1].get());
    host_impl()->active_tree()->AddLayer(std::move(layers[2]));
  }
  EmulateDrawingOneFrame(root);

  EXPECT_TRUE(GetRenderSurface(root)->damage_tracker()->GetDamageRectIfValid(
      &root_damage_rect));
  EXPECT_EQ(gfx::Rect(100, 100, 30, 30).ToString(),
            root_damage_rect.ToString());
  EXPECT_TRUE(GetRenderSurface(root)
                  ->damage_tracker()
                  ->has_damage_from_contributing_content());
}

TEST_F(DamageTrackerTest, VerifyDamageForNewUnchangedLayer) {
  // If child2_ is added to the layer tree, but it doesn't have any explicit
  // damage of its own, it should still indeed damage the target surface.

  LayerImpl* root = CreateAndSetUpTestTreeWithOneSurface();

  ClearDamageForAllSurfaces(root);

  LayerImpl* child2 = AddLayerInActiveTree<LayerImpl>();
  child2->SetBounds(gfx::Size(6, 8));
  child2->SetDrawsContent(true);
  CopyProperties(root, child2);
  child2->SetOffsetToTransformParent(gfx::Vector2dF(400.f, 380.f));
  host_impl()->active_tree()->ResetAllChangeTracking();
  // Sanity check the initial conditions of the test, if these asserts
  // trigger, it means the test no longer actually covers the intended
  // scenario.
  ASSERT_FALSE(child2->LayerPropertyChanged());
  ASSERT_TRUE(child2->update_rect().IsEmpty());

  EmulateDrawingOneFrame(root);

  // Sanity check - all 3 layers should be on the same render surface; render
  // surfaces are tested elsewhere.
  ASSERT_EQ(3, GetRenderSurface(root)->num_contributors());

  gfx::Rect root_damage_rect;
  EXPECT_TRUE(GetRenderSurface(root)->damage_tracker()->GetDamageRectIfValid(
      &root_damage_rect));
  EXPECT_EQ(gfx::Rect(400, 380, 6, 8).ToString(), root_damage_rect.ToString());
  EXPECT_TRUE(GetRenderSurface(root)
                  ->damage_tracker()
                  ->has_damage_from_contributing_content());
}

TEST_F(DamageTrackerTest, VerifyDamageForMultipleLayers) {
  LayerImpl* root = CreateAndSetUpTestTreeWithOneSurface();
  LayerImpl* child1 = child_layers_[0];

  // In this test we don't want the above tree manipulation to be considered
  // part of the same frame.
  ClearDamageForAllSurfaces(root);
  LayerImpl* child2 = AddLayerInActiveTree<LayerImpl>();
  child2->SetBounds(gfx::Size(6, 8));
  child2->SetDrawsContent(true);
  CopyProperties(root, child2);
  child2->SetOffsetToTransformParent(gfx::Vector2dF(400.f, 380.f));
  EmulateDrawingOneFrame(root);

  // Damaging two layers simultaneously should cause combined damage.
  // - child1_ update rect in surface space: gfx::Rect(100, 100, 1, 2);
  // - child2_ update rect in surface space: gfx::Rect(400, 380, 3, 4);
  ClearDamageForAllSurfaces(root);
  child1->UnionUpdateRect(gfx::Rect(1, 2));
  child2->UnionUpdateRect(gfx::Rect(3, 4));
  EmulateDrawingOneFrame(root);
  gfx::Rect root_damage_rect;
  EXPECT_TRUE(GetRenderSurface(root)->damage_tracker()->GetDamageRectIfValid(
      &root_damage_rect));
  EXPECT_EQ(gfx::Rect(100, 100, 303, 284).ToString(),
            root_damage_rect.ToString());
  EXPECT_TRUE(GetRenderSurface(root)
                  ->damage_tracker()
                  ->has_damage_from_contributing_content());
}

TEST_F(DamageTrackerTest, VerifyDamageForNestedSurfaces) {
  LayerImpl* root = CreateAndSetUpTestTreeWithTwoSurfaces();
  CreateEffectNode(child2_).render_surface_reason = RenderSurfaceReason::kTest;
  CreateEffectNode(grand_child1_).render_surface_reason =
      RenderSurfaceReason::kTest;
  EmulateDrawingOneFrame(root);
  gfx::Rect child_damage_rect;
  gfx::Rect root_damage_rect;

  // CASE 1: Damage to a descendant surface should propagate properly to
  //         ancestor surface.
  ClearDamageForAllSurfaces(root);
  root->layer_tree_impl()->SetOpacityMutated(grand_child1_->element_id(), 0.5f);
  EmulateDrawingOneFrame(root);
  EXPECT_TRUE(GetRenderSurface(child1_)->damage_tracker()->GetDamageRectIfValid(
      &child_damage_rect));
  EXPECT_TRUE(GetRenderSurface(root)->damage_tracker()->GetDamageRectIfValid(
      &root_damage_rect));
  EXPECT_EQ(gfx::Rect(200, 200, 6, 8).ToString(), child_damage_rect.ToString());
  EXPECT_EQ(gfx::Rect(300, 300, 6, 8).ToString(), root_damage_rect.ToString());
  EXPECT_TRUE(GetRenderSurface(root)
                  ->damage_tracker()
                  ->has_damage_from_contributing_content());
  EXPECT_TRUE(GetRenderSurface(child1_)
                  ->damage_tracker()
                  ->has_damage_from_contributing_content());
  EXPECT_FALSE(GetRenderSurface(child2_)
                   ->damage_tracker()
                   ->has_damage_from_contributing_content());
  EXPECT_FALSE(GetRenderSurface(grand_child1_)
                   ->damage_tracker()
                   ->has_damage_from_contributing_content());

  // CASE 2: Same as previous case, but with additional damage elsewhere that
  //         should be properly unioned.
  // - child1_ surface damage in root surface space:
  //   gfx::Rect(300, 300, 6, 8);
  // - child2_ damage in root surface space:
  //   gfx::Rect(11, 11, 18, 18);
  ClearDamageForAllSurfaces(root);
  root->layer_tree_impl()->SetOpacityMutated(grand_child1_->element_id(), 0.7f);
  root->layer_tree_impl()->SetOpacityMutated(child2_->element_id(), 0.7f);
  EmulateDrawingOneFrame(root);
  EXPECT_TRUE(GetRenderSurface(child1_)->damage_tracker()->GetDamageRectIfValid(
      &child_damage_rect));
  EXPECT_TRUE(GetRenderSurface(root)->damage_tracker()->GetDamageRectIfValid(
      &root_damage_rect));
  EXPECT_EQ(gfx::Rect(200, 200, 6, 8).ToString(), child_damage_rect.ToString());
  EXPECT_EQ(gfx::Rect(11, 11, 295, 297).ToString(),
            root_damage_rect.ToString());
  EXPECT_TRUE(GetRenderSurface(root)
                  ->damage_tracker()
                  ->has_damage_from_contributing_content());
  EXPECT_TRUE(GetRenderSurface(child1_)
                  ->damage_tracker()
                  ->has_damage_from_contributing_content());
  EXPECT_FALSE(GetRenderSurface(child2_)
                   ->damage_tracker()
                   ->has_damage_from_contributing_content());
  EXPECT_FALSE(GetRenderSurface(grand_child1_)
                   ->damage_tracker()
                   ->has_damage_from_contributing_content());
}

TEST_F(DamageTrackerTest, VerifyDamageForSurfaceChangeFromDescendantLayer) {
  // If descendant layer changes and affects the content bounds of the render
  // surface, then the entire descendant surface should be damaged, and it
  // should damage its ancestor surface with the old and new surface regions.

  // This is a tricky case, since only the first grand_child changes, but the
  // entire surface should be marked dirty.

  LayerImpl* root = CreateAndSetUpTestTreeWithTwoSurfaces();
  gfx::Rect child_damage_rect;
  gfx::Rect root_damage_rect;

  ClearDamageForAllSurfaces(root);
  grand_child1_->SetOffsetToTransformParent(gfx::Vector2dF(195.f, 205.f));
  EmulateDrawingOneFrame(root);
  EXPECT_TRUE(GetRenderSurface(child1_)->damage_tracker()->GetDamageRectIfValid(
      &child_damage_rect));
  EXPECT_TRUE(GetRenderSurface(root)->damage_tracker()->GetDamageRectIfValid(
      &root_damage_rect));

  // The new surface bounds should be damaged entirely, even though only one of
  // the layers changed.
  EXPECT_EQ(gfx::Rect(190, 190, 11, 23).ToString(),
            child_damage_rect.ToString());

  // Damage to the root surface should be the union of child1_'s *entire* render
  // surface (in target space), and its old exposed area (also in target
  // space).
  EXPECT_EQ(gfx::Rect(290, 290, 16, 23).ToString(),
            root_damage_rect.ToString());

  EXPECT_TRUE(GetRenderSurface(root)
                  ->damage_tracker()
                  ->has_damage_from_contributing_content());
  EXPECT_TRUE(GetRenderSurface(child1_)
                  ->damage_tracker()
                  ->has_damage_from_contributing_content());
}

TEST_F(DamageTrackerTest, VerifyDamageForSurfaceChangeFromDescendantSurface) {
  // CASE 1: If descendant surface changes position, the ancestor surface should
  //         be damaged with the old and new descendant surface regions.

  LayerImpl* root = CreateAndSetUpTestTreeWithTwoSurfaces();
  child1_->SetDrawsContent(true);
  EmulateDrawingOneFrame(root);

  gfx::Rect child_damage_rect;
  gfx::Rect root_damage_rect;

  ClearDamageForAllSurfaces(root);
  SetPostTranslation(child1_.get(), gfx::Vector2dF(105.f, 107.f));
  child1_->NoteLayerPropertyChanged();
  EmulateDrawingOneFrame(root);
  EXPECT_TRUE(GetRenderSurface(child1_)->damage_tracker()->GetDamageRectIfValid(
      &child_damage_rect));
  EXPECT_TRUE(GetRenderSurface(root)->damage_tracker()->GetDamageRectIfValid(
      &root_damage_rect));

  // Damage to the root surface should be the union of child1_'s *entire* render
  // surface (in target space), and its old exposed area (also in target
  // space).
  EXPECT_EQ(gfx::UnionRects(gfx::Rect(100, 100, 206, 208),
                            gfx::Rect(105, 107, 206, 208))
                .ToString(),
            root_damage_rect.ToString());
  // The child surface should also be damaged.
  EXPECT_EQ(gfx::Rect(0, 0, 206, 208).ToString(), child_damage_rect.ToString());
  EXPECT_TRUE(GetRenderSurface(root)
                  ->damage_tracker()
                  ->has_damage_from_contributing_content());
  EXPECT_TRUE(GetRenderSurface(child1_)
                  ->damage_tracker()
                  ->has_damage_from_contributing_content());

  // CASE 2: Change render surface content rect should make
  //         |has_damage_from_contributing_content| true.
  ClearDamageForAllSurfaces(root);
  CreateEffectNode(child2_).render_surface_reason = RenderSurfaceReason::kTest;
  EmulateDrawingOneFrame(root);

  // Surface property changed only from descendant.
  child2_->SetBounds(gfx::Size(120, 140));
  EmulateDrawingOneFrame(root);

  EXPECT_TRUE(GetRenderSurface(root)
                  ->damage_tracker()
                  ->has_damage_from_contributing_content());

  EXPECT_TRUE(GetRenderSurface(child2_)
                  ->damage_tracker()
                  ->has_damage_from_contributing_content());

  // Surface property changed from both parent and descendant.
  child2_->SetBounds(gfx::Size(220, 240));
  root->layer_tree_impl()->SetOpacityMutated(child2_->element_id(), 0.5f);
  EmulateDrawingOneFrame(root);

  EXPECT_TRUE(GetRenderSurface(root)
                  ->damage_tracker()
                  ->has_damage_from_contributing_content());

  EXPECT_TRUE(GetRenderSurface(child2_)
                  ->damage_tracker()
                  ->has_damage_from_contributing_content());
}

TEST_F(DamageTrackerTest, VerifyDamageForSurfaceChangeFromViewTransitionLayer) {
  ClearLayersAndProperties();

  blink::ViewTransitionToken transition_token;

  LayerImpl* root = root_layer();
  root->SetBounds(gfx::Size(500, 500));
  root->layer_tree_impl()->SetDeviceViewportRect(gfx::Rect(root->bounds()));
  root->SetDrawsContent(true);
  SetupRootProperties(root);

  LayerImpl* child1 = AddLayerInActiveTree<TestLayerImpl>();
  LayerImpl* grand_child1 = AddLayerInActiveTree<TestLayerImpl>();
  LayerImpl* child2 = AddLayerInActiveTree<TestViewTransitionContentLayerImpl>(
      viz::ViewTransitionElementResourceId(transition_token, 3), false);

  // child 1 of the root - live render surface.
  child1->SetBounds(gfx::Size(80, 80));
  child1->SetDrawsContent(true);
  CopyProperties(root, child1);
  CreateTransformNode(child1).post_translation = gfx::Vector2dF(100.f, 100.f);
  CreateEffectNode(child1).render_surface_reason = RenderSurfaceReason::kTest;

  // grandchild 1 - child of the child1
  grand_child1->SetBounds(gfx::Size(10, 20));
  grand_child1->SetDrawsContent(true);
  CopyProperties(child1, grand_child1);
  grand_child1->SetOffsetToTransformParent(gfx::Vector2dF(30.f, 30.f));

  // child2 of the root - Shared element layer
  child2->SetBounds(gfx::Size(160, 160));
  child2->SetDrawsContent(true);
  CopyProperties(root, child2);
  child2->SetOffsetToTransformParent(gfx::Vector2dF(100.f, 100.f));

  SetElementIdsForTesting();
  EmulateDrawingOneFrame(root);

  // Assign the same element resource id to child 1.
  GetRenderSurface(child1)
      ->OwningEffectNodeMutableForTest()
      ->view_transition_element_resource_id =
      child2->ViewTransitionResourceId();
  EmulateDrawingOneFrame(root);

  gfx::Rect child1_damage_rect;
  gfx::Rect root_damage_rect;

  // Next frame
  ClearDamageForAllSurfaces(root);
  grand_child1->NoteLayerPropertyChanged();
  EmulateDrawingOneFrame(root);

  EXPECT_TRUE(GetRenderSurface(child1)->damage_tracker()->GetDamageRectIfValid(
      &child1_damage_rect));
  EXPECT_TRUE(GetRenderSurface(root)->damage_tracker()->GetDamageRectIfValid(
      &root_damage_rect));

  EXPECT_EQ(gfx::Rect(30, 30, 10, 20).ToString(),
            child1_damage_rect.ToString());

  // The damage from the shared content render surface should contributes to
  // the view transition layer's parent surface.
  EXPECT_EQ(gfx::Rect(130, 130, 50, 70).ToString(),
            root_damage_rect.ToString());

  EXPECT_TRUE(GetRenderSurface(root)
                  ->damage_tracker()
                  ->has_damage_from_contributing_content());
  EXPECT_TRUE(GetRenderSurface(child1)
                  ->damage_tracker()
                  ->has_damage_from_contributing_content());
}

TEST_F(DamageTrackerTest, VerifyDamageForSurfaceChangeFromAncestorLayer) {
  // An ancestor/owning layer changes that affects the position/transform of
  // the render surface. Note that in this case, the layer_property_changed flag
  // already propagates to the subtree (tested in LayerImpltest), which damages
  // the entire child1_ surface, but the damage tracker still needs the correct
  // logic to compute the exposed region on the root surface.

  // TODO(shawnsingh): the expectations of this test case should change when we
  // add support for a unique scissor_rect per RenderSurface. In that case, the
  // child1_ surface should be completely unchanged, since we are only
  // transforming it, while the root surface would be damaged appropriately.

  LayerImpl* root = CreateAndSetUpTestTreeWithTwoSurfaces();
  gfx::Rect child_damage_rect;
  gfx::Rect root_damage_rect;

  ClearDamageForAllSurfaces(root);
  gfx::Transform translation;
  translation.Translate(-50.f, -50.f);
  root->layer_tree_impl()->SetTransformMutated(child1_->element_id(),
                                               translation);
  EmulateDrawingOneFrame(root);
  EXPECT_TRUE(GetRenderSurface(child1_)->damage_tracker()->GetDamageRectIfValid(
      &child_damage_rect));
  EXPECT_TRUE(GetRenderSurface(root)->damage_tracker()->GetDamageRectIfValid(
      &root_damage_rect));

  // The new surface bounds should be damaged entirely.
  EXPECT_EQ(gfx::Rect(190, 190, 16, 18).ToString(),
            child_damage_rect.ToString());

  // The entire child1_ surface and the old exposed child1_ surface should
  // damage the root surface.
  //  - old child1_ surface in target space: gfx::Rect(290, 290, 16, 18)
  //  - new child1_ surface in target space: gfx::Rect(240, 240, 16, 18)
  EXPECT_EQ(gfx::Rect(240, 240, 66, 68).ToString(),
            root_damage_rect.ToString());

  EXPECT_TRUE(GetRenderSurface(root)
                  ->damage_tracker()
                  ->has_damage_from_contributing_content());
  EXPECT_FALSE(GetRenderSurface(child1_)
                   ->damage_tracker()
                   ->has_damage_from_contributing_content());
}

TEST_F(DamageTrackerTest, VerifyDamageForAddingAndRemovingRenderSurfaces) {
  LayerImpl* root = CreateAndSetUpTestTreeWithTwoSurfaces();
  gfx::Rect child_damage_rect;
  gfx::Rect root_damage_rect;

  // CASE 1: If a descendant surface disappears, its entire old area becomes
  //         exposed.
  ClearDamageForAllSurfaces(root);
  SetRenderSurfaceReason(child1_.get(), RenderSurfaceReason::kNone);
  EmulateDrawingOneFrame(root);

  // Sanity check that there is only one surface now.
  ASSERT_EQ(GetRenderSurface(child1_), GetRenderSurface(root));
  ASSERT_EQ(4, GetRenderSurface(root)->num_contributors());

  EXPECT_TRUE(GetRenderSurface(root)->damage_tracker()->GetDamageRectIfValid(
      &root_damage_rect));
  EXPECT_EQ(gfx::Rect(290, 290, 16, 18).ToString(),
            root_damage_rect.ToString());

  EXPECT_TRUE(GetRenderSurface(root)
                  ->damage_tracker()
                  ->has_damage_from_contributing_content());

  // CASE 2: If a descendant surface appears, its entire old area becomes
  //         exposed.

  // Cycle one frame of no change, just to sanity check that the next rect is
  // not because of the old damage state.
  ClearDamageForAllSurfaces(root);
  EmulateDrawingOneFrame(root);
  EXPECT_TRUE(GetRenderSurface(root)->damage_tracker()->GetDamageRectIfValid(
      &root_damage_rect));
  EXPECT_TRUE(root_damage_rect.IsEmpty());

  // Then change the tree so that the render surface is added back.
  ClearDamageForAllSurfaces(root);
  SetRenderSurfaceReason(child1_.get(), RenderSurfaceReason::kTest);

  EmulateDrawingOneFrame(root);

  // Sanity check that there is a new surface now.
  ASSERT_TRUE(GetRenderSurface(child1_));
  EXPECT_EQ(3, GetRenderSurface(root)->num_contributors());
  EXPECT_EQ(2, GetRenderSurface(child1_)->num_contributors());

  EXPECT_TRUE(GetRenderSurface(child1_)->damage_tracker()->GetDamageRectIfValid(
      &child_damage_rect));
  EXPECT_TRUE(GetRenderSurface(root)->damage_tracker()->GetDamageRectIfValid(
      &root_damage_rect));
  EXPECT_EQ(gfx::Rect(190, 190, 16, 18).ToString(),
            child_damage_rect.ToString());
  EXPECT_EQ(gfx::Rect(290, 290, 16, 18).ToString(),
            root_damage_rect.ToString());
  EXPECT_TRUE(GetRenderSurface(root)
                  ->damage_tracker()
                  ->has_damage_from_contributing_content());
}

TEST_F(DamageTrackerTest, VerifyNoDamageWhenNothingChanged) {
  LayerImpl* root = CreateAndSetUpTestTreeWithTwoSurfaces();
  gfx::Rect child_damage_rect;
  gfx::Rect root_damage_rect;

  // CASE 1: If nothing changes, the damage rect should be empty.
  //
  ClearDamageForAllSurfaces(root);
  EmulateDrawingOneFrame(root);
  EXPECT_TRUE(GetRenderSurface(child1_)->damage_tracker()->GetDamageRectIfValid(
      &child_damage_rect));
  EXPECT_TRUE(GetRenderSurface(root)->damage_tracker()->GetDamageRectIfValid(
      &root_damage_rect));
  EXPECT_TRUE(child_damage_rect.IsEmpty());
  EXPECT_TRUE(root_damage_rect.IsEmpty());
  EXPECT_FALSE(GetRenderSurface(root)
                   ->damage_tracker()
                   ->has_damage_from_contributing_content());

  // CASE 2: If nothing changes twice in a row, the damage rect should still be
  //         empty.
  //
  ClearDamageForAllSurfaces(root);
  EmulateDrawingOneFrame(root);
  EXPECT_TRUE(GetRenderSurface(child1_)->damage_tracker()->GetDamageRectIfValid(
      &child_damage_rect));
  EXPECT_TRUE(GetRenderSurface(root)->damage_tracker()->GetDamageRectIfValid(
      &root_damage_rect));
  EXPECT_TRUE(child_damage_rect.IsEmpty());
  EXPECT_TRUE(root_damage_rect.IsEmpty());
  EXPECT_FALSE(GetRenderSurface(root)
                   ->damage_tracker()
                   ->has_damage_from_contributing_content());
}

TEST_F(DamageTrackerTest, VerifyNoDamageForUpdateRectThatDoesNotDrawContent) {
  LayerImpl* root = CreateAndSetUpTestTreeWithTwoSurfaces();
  gfx::Rect child_damage_rect;
  gfx::Rect root_damage_rect;

  // In our specific tree, the update rect of child1_ should not cause any
  // damage to any surface because it does not actually draw content.
  ClearDamageForAllSurfaces(root);
  child1_->UnionUpdateRect(gfx::Rect(1, 2));
  EmulateDrawingOneFrame(root);
  EXPECT_TRUE(GetRenderSurface(child1_)->damage_tracker()->GetDamageRectIfValid(
      &child_damage_rect));
  EXPECT_TRUE(GetRenderSurface(root)->damage_tracker()->GetDamageRectIfValid(
      &root_damage_rect));
  EXPECT_TRUE(child_damage_rect.IsEmpty());
  EXPECT_TRUE(root_damage_rect.IsEmpty());
  EXPECT_FALSE(GetRenderSurface(root)
                   ->damage_tracker()
                   ->has_damage_from_contributing_content());
}

TEST_F(DamageTrackerTest, VerifyDamageForMask) {
  LayerImpl* root = CreateAndSetUpTestTreeWithOneSurface();
  LayerImpl* child = child_layers_[0];

  // In the current implementation of the damage tracker, changes to mask
  // layers should damage the entire corresponding surface.

  ClearDamageForAllSurfaces(root);

  CreateTransformNode(child).post_translation =
      child->offset_to_transform_parent();
  child->SetOffsetToTransformParent(gfx::Vector2dF());

  // Set up the mask layer.
  CreateEffectNode(child);
  auto* mask_layer = AddLayerInActiveTree<FakePictureLayerImpl>();
  SetupMaskProperties(child, mask_layer);
  Region empty_invalidation;
  mask_layer->UpdateRasterSource(
      FakeRasterSource::CreateFilled(child->bounds()), &empty_invalidation);

  // Add opacity and a grand_child so that the render surface persists even
  // after we remove the mask.
  LayerImpl* grand_child = AddLayerInActiveTree<LayerImpl>();
  grand_child->SetBounds(gfx::Size(2, 2));
  grand_child->SetDrawsContent(true);
  CopyProperties(child, grand_child);
  grand_child->SetOffsetToTransformParent(gfx::Vector2dF(2.f, 2.f));
  EmulateDrawingOneFrame(root);

  EXPECT_EQ(2, GetRenderSurface(root)->num_contributors());
  EXPECT_TRUE(root->contributes_to_drawn_render_surface());
  EXPECT_EQ(3, GetRenderSurface(child)->num_contributors());
  EXPECT_TRUE(child->contributes_to_drawn_render_surface());
  EXPECT_EQ(GetRenderSurface(child), GetRenderSurface(mask_layer));
  EXPECT_TRUE(mask_layer->contributes_to_drawn_render_surface());

  // CASE 1: the update_rect on a mask layer should damage the rect.
  ClearDamageForAllSurfaces(root);
  mask_layer->UnionUpdateRect(gfx::Rect(1, 2, 3, 4));
  EmulateDrawingOneFrame(root);
  gfx::Rect child_damage_rect;
  EXPECT_TRUE(GetRenderSurface(child)->damage_tracker()->GetDamageRectIfValid(
      &child_damage_rect));
  EXPECT_EQ(gfx::Rect(1, 2, 3, 4), child_damage_rect);

  EXPECT_TRUE(GetRenderSurface(root)
                  ->damage_tracker()
                  ->has_damage_from_contributing_content());
  EXPECT_TRUE(GetRenderSurface(child)
                  ->damage_tracker()
                  ->has_damage_from_contributing_content());

  // CASE 2: a property change on the mask layer should damage the entire
  //         target surface.

  // Advance one frame without damage so that we know the damage rect is not
  // leftover from the previous case.
  ClearDamageForAllSurfaces(root);
  EmulateDrawingOneFrame(root);
  EXPECT_TRUE(GetRenderSurface(child)->damage_tracker()->GetDamageRectIfValid(
      &child_damage_rect));
  EXPECT_TRUE(child_damage_rect.IsEmpty());

  // Then test the property change.
  ClearDamageForAllSurfaces(root);
  mask_layer->NoteLayerPropertyChanged();

  EmulateDrawingOneFrame(root);
  EXPECT_TRUE(GetRenderSurface(child)->damage_tracker()->GetDamageRectIfValid(
      &child_damage_rect));
  EXPECT_EQ(gfx::Rect(30, 30), child_damage_rect);

  EXPECT_TRUE(GetRenderSurface(root)
                  ->damage_tracker()
                  ->has_damage_from_contributing_content());
  EXPECT_TRUE(GetRenderSurface(child)
                  ->damage_tracker()
                  ->has_damage_from_contributing_content());

  // CASE 3: removing the mask also damages the entire target surface.
  //

  // Advance one frame without damage so that we know the damage rect is not
  // leftover from the previous case.
  ClearDamageForAllSurfaces(root);
  EmulateDrawingOneFrame(root);
  EXPECT_TRUE(GetRenderSurface(child)->damage_tracker()->GetDamageRectIfValid(
      &child_damage_rect));
  EXPECT_TRUE(child_damage_rect.IsEmpty());

  // Then test mask removal.
  ClearDamageForAllSurfaces(root);
  auto layers =
      root->layer_tree_impl()->DetachLayersKeepingRootLayerForTesting();
  ASSERT_EQ(layers[1].get(), child);
  root->layer_tree_impl()->AddLayer(std::move(layers[1]));
  ASSERT_EQ(layers[2].get(), mask_layer);
  ASSERT_EQ(layers[3].get(), grand_child);
  root->layer_tree_impl()->AddLayer(std::move(layers[3]));
  CopyProperties(root, child);
  CreateEffectNode(child).render_surface_reason = RenderSurfaceReason::kTest;
  CopyProperties(child, grand_child);

  EmulateDrawingOneFrame(root);

  // Sanity check that a render surface still exists.
  ASSERT_TRUE(GetRenderSurface(child));

  EXPECT_TRUE(GetRenderSurface(child)->damage_tracker()->GetDamageRectIfValid(
      &child_damage_rect));
  EXPECT_EQ(gfx::Rect(30, 30), child_damage_rect);

  EXPECT_TRUE(GetRenderSurface(root)
                  ->damage_tracker()
                  ->has_damage_from_contributing_content());
}

TEST_F(DamageTrackerTest, DamageWhenAddedExternally) {
  LayerImpl* root = CreateAndSetUpTestTreeWithOneSurface();
  LayerImpl* child = child_layers_[0];

  // Case 1: This test ensures that when the tracker is given damage, that
  //         it is included with any other partial damage.
  //
  ClearDamageForAllSurfaces(root);
  child->UnionUpdateRect(gfx::Rect(10, 11, 12, 13));
  GetRenderSurface(root)->damage_tracker()->AddDamageNextUpdate(
      gfx::Rect(15, 16, 32, 33));
  EmulateDrawingOneFrame(root);
  gfx::Rect root_damage_rect;
  EXPECT_TRUE(GetRenderSurface(root)->damage_tracker()->GetDamageRectIfValid(
      &root_damage_rect));
  EXPECT_EQ(gfx::UnionRects(gfx::Rect(15, 16, 32, 33),
                            gfx::Rect(100 + 10, 100 + 11, 12, 13)).ToString(),
            root_damage_rect.ToString());
  EXPECT_TRUE(GetRenderSurface(root)
                  ->damage_tracker()
                  ->has_damage_from_contributing_content());

  // Case 2: An additional sanity check that adding damage works even when
  //         nothing on the layer tree changed.
  //
  ClearDamageForAllSurfaces(root);
  GetRenderSurface(root)->damage_tracker()->AddDamageNextUpdate(
      gfx::Rect(30, 31, 14, 15));
  EmulateDrawingOneFrame(root);
  EXPECT_TRUE(GetRenderSurface(root)->damage_tracker()->GetDamageRectIfValid(
      &root_damage_rect));
  EXPECT_EQ(gfx::Rect(30, 31, 14, 15).ToString(), root_damage_rect.ToString());
  EXPECT_FALSE(GetRenderSurface(root)
                   ->damage_tracker()
                   ->has_damage_from_contributing_content());
}

TEST_F(DamageTrackerTest, VerifyDamageWithNoContributingLayers) {
  LayerImpl* root = root_layer();
  ClearDamageForAllSurfaces(root);

  LayerImpl* empty_surface = AddLayerInActiveTree<LayerImpl>();
  CopyProperties(root, empty_surface);
  CreateEffectNode(empty_surface).render_surface_reason =
      RenderSurfaceReason::kTest;
  EmulateDrawingOneFrame(root);

  DCHECK_EQ(GetRenderSurface(empty_surface), empty_surface->render_target());
  const RenderSurfaceImpl* target_surface = GetRenderSurface(empty_surface);
  gfx::Rect damage_rect;
  EXPECT_TRUE(
      target_surface->damage_tracker()->GetDamageRectIfValid(&damage_rect));
  EXPECT_TRUE(damage_rect.IsEmpty());
  EXPECT_FALSE(
      target_surface->damage_tracker()->has_damage_from_contributing_content());
}

TEST_F(DamageTrackerTest, VerifyDamageAccumulatesUntilReset) {
  // If damage is not cleared, it should accumulate.

  LayerImpl* root = CreateAndSetUpTestTreeWithOneSurface();
  LayerImpl* child = child_layers_[0];

  ClearDamageForAllSurfaces(root);
  child->UnionUpdateRect(gfx::Rect(10.f, 11.f, 1.f, 2.f));
  EmulateDrawingOneFrame(root);

  // Sanity check damage after the first frame; this isnt the actual test yet.
  gfx::Rect root_damage_rect;
  EXPECT_TRUE(GetRenderSurface(root)->damage_tracker()->GetDamageRectIfValid(
      &root_damage_rect));
  EXPECT_EQ(gfx::Rect(110, 111, 1, 2).ToString(), root_damage_rect.ToString());
  EXPECT_TRUE(GetRenderSurface(root)
                  ->damage_tracker()
                  ->has_damage_from_contributing_content());

  // New damage, without having cleared the previous damage, should be unioned
  // to the previous one.
  child->UnionUpdateRect(gfx::Rect(20, 25, 1, 2));
  EmulateDrawingOneFrame(root);
  EXPECT_TRUE(GetRenderSurface(root)->damage_tracker()->GetDamageRectIfValid(
      &root_damage_rect));
  EXPECT_EQ(gfx::Rect(110, 111, 11, 16).ToString(),
            root_damage_rect.ToString());
  EXPECT_TRUE(GetRenderSurface(root)
                  ->damage_tracker()
                  ->has_damage_from_contributing_content());

  // If we notify the damage tracker that we drew the damaged area, then damage
  // should be emptied.
  GetRenderSurface(root)->damage_tracker()->DidDrawDamagedArea();
  EXPECT_TRUE(GetRenderSurface(root)->damage_tracker()->GetDamageRectIfValid(
      &root_damage_rect));
  EXPECT_TRUE(root_damage_rect.IsEmpty());
  EXPECT_FALSE(GetRenderSurface(root)
                   ->damage_tracker()
                   ->has_damage_from_contributing_content());

  // Damage should remain empty even after one frame, since there's yet no new
  // damage.
  EmulateDrawingOneFrame(root);
  EXPECT_TRUE(GetRenderSurface(root)->damage_tracker()->GetDamageRectIfValid(
      &root_damage_rect));
  EXPECT_TRUE(root_damage_rect.IsEmpty());
  EXPECT_FALSE(GetRenderSurface(root)
                   ->damage_tracker()
                   ->has_damage_from_contributing_content());
}

TEST_F(DamageTrackerTest, HugeDamageRect) {
  // This number is so large that we start losing floating point accuracy.
  const int kBigNumber = 900000000;
  // Walk over a range to find floating point inaccuracy boundaries that move
  // toward the wrong direction.
  const int kRange = 5000;

  for (int i = 0; i < kRange; ++i) {
    LayerImpl* root = CreateAndSetUpTestTreeWithOneSurface();
    LayerImpl* child = child_layers_[0];
    // Set copy request to damage the entire layer.
    SetCopyRequest(root);

    gfx::Transform transform;
    transform.Translate(-kBigNumber, -kBigNumber);

    // The child layer covers (0, 0, i, i) of the viewport,
    // but has a huge negative position.
    child->SetBounds(gfx::Size(kBigNumber + i, kBigNumber + i));
    auto& transform_node = CreateTransformNode(child);
    transform_node.local = transform;
    transform_node.post_translation = child->offset_to_transform_parent();
    child->SetOffsetToTransformParent(gfx::Vector2dF());

    float device_scale_factor = 1.f;
    // Visible rects computed from combining clips in target space and root
    // space don't match because of the loss in floating point accuracy. So, we
    // skip verify_clip_tree_calculations.
    EmulateDrawingOneFrame(root, device_scale_factor);

    // The expected damage should cover the visible part of the child layer,
    // which is (0, 0, i, i) in the viewport.
    gfx::Rect root_damage_rect;
    EXPECT_TRUE(GetRenderSurface(root)->damage_tracker()->GetDamageRectIfValid(
        &root_damage_rect));
    gfx::Rect damage_we_care_about = gfx::Rect(i, i);
    EXPECT_LE(damage_we_care_about.right(), root_damage_rect.right());
    EXPECT_LE(damage_we_care_about.bottom(), root_damage_rect.bottom());
    EXPECT_TRUE(GetRenderSurface(root)
                    ->damage_tracker()
                    ->has_damage_from_contributing_content());
  }
}

TEST_F(DamageTrackerTest, DamageRectTooBig) {
  LayerImpl* root = CreateAndSetUpTestTreeWithOneSurface(2);
  LayerImpl* child1 = child_layers_[0];
  LayerImpl* child2 = child_layers_[1];

  // Set copy request to damage the entire layer.
  SetCopyRequest(root);

  // Really far left.
  child1->SetOffsetToTransformParent(gfx::Vector2dF(
      static_cast<float>(std::numeric_limits<int>::min() + 100), 0));
  child1->SetBounds(gfx::Size(1, 1));

  // Really far right.
  child2->SetOffsetToTransformParent(gfx::Vector2dF(
      static_cast<float>(std::numeric_limits<int>::max() - 100), 0));
  child2->SetBounds(gfx::Size(1, 1));

  EmulateDrawingOneFrame(root, 1.f);

  // The expected damage would be too large to store in a gfx::Rect, so we
  // should damage everything (ie, we don't have a valid rect).
  gfx::Rect damage_rect;
  EXPECT_FALSE(GetRenderSurface(root)->damage_tracker()->GetDamageRectIfValid(
      &damage_rect));
  EXPECT_EQ(GetRenderSurface(root)->content_rect(),
            GetRenderSurface(root)->GetDamageRect());
  EXPECT_TRUE(GetRenderSurface(root)
                  ->damage_tracker()
                  ->has_damage_from_contributing_content());
}

TEST_F(DamageTrackerTest, DamageRectTooBigWithFilter) {
  LayerImpl* root = CreateAndSetUpTestTreeWithOneSurface(2);
  LayerImpl* child1 = child_layers_[0];
  LayerImpl* child2 = child_layers_[1];

  // Set copy request to damage the entire layer.
  SetCopyRequest(root);

  FilterOperations filters;
  filters.Append(FilterOperation::CreateBlurFilter(5.f));
  root->SetDrawsContent(true);
  SetBackdropFilter(root, filters);

  // Really far left.
  child1->SetOffsetToTransformParent(gfx::Vector2dF(
      static_cast<float>(std::numeric_limits<int>::min() + 100), 0));
  child1->SetBounds(gfx::Size(1, 1));

  // Really far right.
  child2->SetOffsetToTransformParent(gfx::Vector2dF(
      static_cast<float>(std::numeric_limits<int>::max() - 100), 0));
  child2->SetBounds(gfx::Size(1, 1));

  float device_scale_factor = 1.f;
  EmulateDrawingOneFrame(root, device_scale_factor);

  // The expected damage would be too large to store in a gfx::Rect, so we
  // should damage everything (ie, we don't have a valid rect).
  gfx::Rect damage_rect;
  EXPECT_FALSE(GetRenderSurface(root)->damage_tracker()->GetDamageRectIfValid(
      &damage_rect));
  EXPECT_EQ(GetRenderSurface(root)->content_rect(),
            GetRenderSurface(root)->GetDamageRect());
  EXPECT_TRUE(GetRenderSurface(root)
                  ->damage_tracker()
                  ->has_damage_from_contributing_content());
}

TEST_F(DamageTrackerTest, DamageRectTooBigInRenderSurface) {
  LayerImpl* root = CreateAndSetUpTestTreeWithTwoSurfacesDrawingFullyVisible();

  // Set copy request to damage the entire layer.
  SetCopyRequest(root);

  // Really far left.
  grand_child1_->SetOffsetToTransformParent(gfx::Vector2dF(
      static_cast<float>(std::numeric_limits<int>::min() + 500), 0));
  grand_child1_->SetBounds(gfx::Size(1, 1));
  grand_child1_->SetDrawsContent(true);

  // Really far right.
  grand_child2_->SetOffsetToTransformParent(gfx::Vector2dF(
      static_cast<float>(std::numeric_limits<int>::max() - 500), 0));
  grand_child2_->SetBounds(gfx::Size(1, 1));
  grand_child2_->SetDrawsContent(true);

  UpdateDrawProperties(host_impl()->active_tree());
  // Avoid the descendant-only property change path that skips unioning damage
  // from descendant layers.
  GetRenderSurface(child1_)->NoteAncestorPropertyChanged();
  DamageTracker::UpdateDamageTracking(host_impl()->active_tree());

  // The expected damage would be too large to store in a gfx::Rect, so we
  // should damage everything on child1_.
  gfx::Rect damage_rect;
  EXPECT_FALSE(
      GetRenderSurface(child1_)->damage_tracker()->GetDamageRectIfValid(
          &damage_rect));
  EXPECT_EQ(GetRenderSurface(child1_)->content_rect(),
            GetRenderSurface(child1_)->GetDamageRect());

  // However, the root should just use the child1_ render surface's content rect
  // as damage.
  ASSERT_TRUE(GetRenderSurface(root)->damage_tracker()->GetDamageRectIfValid(
      &damage_rect));
  EXPECT_TRUE(damage_rect.Contains(GetRenderSurface(root)->content_rect()));
  EXPECT_TRUE(damage_rect.Contains(
      gfx::ToEnclosingRect(GetRenderSurface(child1_)->DrawableContentRect())));
  EXPECT_EQ(damage_rect, GetRenderSurface(root)->GetDamageRect());

  EXPECT_TRUE(GetRenderSurface(root)
                  ->damage_tracker()
                  ->has_damage_from_contributing_content());
  EXPECT_TRUE(GetRenderSurface(child1_)
                  ->damage_tracker()
                  ->has_damage_from_contributing_content());

  // Add new damage, without changing properties, which goes down a different
  // path in the damage tracker.
  root->layer_tree_impl()->ResetAllChangeTracking();
  grand_child1_->AddDamageRect(gfx::Rect(grand_child1_->bounds()));
  grand_child2_->AddDamageRect(gfx::Rect(grand_child1_->bounds()));

  // Recompute all damage / properties.
  UpdateDrawProperties(host_impl()->active_tree());
  DamageTracker::UpdateDamageTracking(host_impl()->active_tree());

  // Child1 should still not have a valid rect, since the union of the damage of
  // its children is not representable by a single rect.
  EXPECT_FALSE(
      GetRenderSurface(child1_)->damage_tracker()->GetDamageRectIfValid(
          &damage_rect));
  EXPECT_EQ(GetRenderSurface(child1_)->content_rect(),
            GetRenderSurface(child1_)->GetDamageRect());

  // Root should have valid damage and contain both its content rect and the
  // drawable content rect of child1_.
  ASSERT_TRUE(GetRenderSurface(root)->damage_tracker()->GetDamageRectIfValid(
      &damage_rect));
  EXPECT_TRUE(damage_rect.Contains(GetRenderSurface(root)->content_rect()));
  EXPECT_TRUE(damage_rect.Contains(
      gfx::ToEnclosingRect(GetRenderSurface(child1_)->DrawableContentRect())));
  EXPECT_EQ(damage_rect, GetRenderSurface(root)->GetDamageRect());

  EXPECT_TRUE(GetRenderSurface(root)
                  ->damage_tracker()
                  ->has_damage_from_contributing_content());
  EXPECT_TRUE(GetRenderSurface(child1_)
                  ->damage_tracker()
                  ->has_damage_from_contributing_content());
}

TEST_F(DamageTrackerTest, DamageRectTooBigInRenderSurfaceWithFilter) {
  LayerImpl* root = CreateAndSetUpTestTreeWithTwoSurfaces();

  // Set copy request to damage the entire layer.
  SetCopyRequest(root);

  // Set up a moving pixels filter on the child.
  FilterOperations filters;
  filters.Append(FilterOperation::CreateBlurFilter(5.f));
  child1_->SetDrawsContent(true);
  SetBackdropFilter(child1_.get(), filters);

  // Really far left.
  grand_child1_->SetOffsetToTransformParent(gfx::Vector2dF(
      static_cast<float>(std::numeric_limits<int>::min() + 500), 0));
  grand_child1_->SetBounds(gfx::Size(1, 1));
  grand_child1_->SetDrawsContent(true);

  // Really far right.
  grand_child2_->SetOffsetToTransformParent(gfx::Vector2dF(
      static_cast<float>(std::numeric_limits<int>::max() - 500), 0));
  grand_child2_->SetBounds(gfx::Size(1, 1));
  grand_child2_->SetDrawsContent(true);

  UpdateDrawProperties(host_impl()->active_tree());
  // Avoid the descendant-only property change path that skips unioning damage
  // from descendant layers.
  GetRenderSurface(child1_)->NoteAncestorPropertyChanged();
  DamageTracker::UpdateDamageTracking(host_impl()->active_tree());

  // The expected damage would be too large to store in a gfx::Rect, so we
  // should damage everything on child1_.
  gfx::Rect damage_rect;
  EXPECT_FALSE(
      GetRenderSurface(child1_)->damage_tracker()->GetDamageRectIfValid(
          &damage_rect));
  EXPECT_EQ(GetRenderSurface(child1_)->content_rect(),
            GetRenderSurface(child1_)->GetDamageRect());

  // However, the root should just use the child1_ render surface's content rect
  // as damage.
  ASSERT_TRUE(GetRenderSurface(root)->damage_tracker()->GetDamageRectIfValid(
      &damage_rect));
  EXPECT_TRUE(damage_rect.Contains(GetRenderSurface(root)->content_rect()));
  EXPECT_TRUE(damage_rect.Contains(
      gfx::ToEnclosingRect(GetRenderSurface(child1_)->DrawableContentRect())));
  EXPECT_EQ(damage_rect, GetRenderSurface(root)->GetDamageRect());

  EXPECT_TRUE(GetRenderSurface(root)
                  ->damage_tracker()
                  ->has_damage_from_contributing_content());
  EXPECT_TRUE(GetRenderSurface(child1_)
                  ->damage_tracker()
                  ->has_damage_from_contributing_content());

  // Add new damage, without changing properties, which goes down a different
  // path in the damage tracker.
  root->layer_tree_impl()->ResetAllChangeTracking();
  grand_child1_->AddDamageRect(gfx::Rect(grand_child1_->bounds()));
  grand_child2_->AddDamageRect(gfx::Rect(grand_child1_->bounds()));

  // Recompute all damage / properties.
  UpdateDrawProperties(host_impl()->active_tree());
  DamageTracker::UpdateDamageTracking(host_impl()->active_tree());

  // Child1 should still not have a valid rect, since the union of the damage of
  // its children is not representable by a single rect.
  EXPECT_FALSE(
      GetRenderSurface(child1_)->damage_tracker()->GetDamageRectIfValid(
          &damage_rect));
  EXPECT_EQ(GetRenderSurface(child1_)->content_rect(),
            GetRenderSurface(child1_)->GetDamageRect());

  // Root should have valid damage and contain both its content rect and the
  // drawable content rect of child1_.
  ASSERT_TRUE(GetRenderSurface(root)->damage_tracker()->GetDamageRectIfValid(
      &damage_rect));
  EXPECT_TRUE(damage_rect.Contains(GetRenderSurface(root)->content_rect()));
  EXPECT_TRUE(damage_rect.Contains(
      gfx::ToEnclosingRect(GetRenderSurface(child1_)->DrawableContentRect())));
  EXPECT_EQ(damage_rect, GetRenderSurface(root)->GetDamageRect());

  EXPECT_TRUE(GetRenderSurface(root)
                  ->damage_tracker()
                  ->has_damage_from_contributing_content());
  EXPECT_TRUE(GetRenderSurface(child1_)
                  ->damage_tracker()
                  ->has_damage_from_contributing_content());
}

TEST_F(DamageTrackerTest, CanUseCachedBackdropFilterResultTest) {
  LayerImpl* root = CreateAndSetUpTestTreeWithFourSurfaces();

  // child1_ has a screen space rect of 270,270 36x38, from
  //   - grand_child1_ 300,300, 6x8
  //   - grand_child2_ 290,290, 6x8
  //   - grand_child3_ 270,270, 6x8
  //   - grand_child4_ 280,280, 15x16
  // child2_ has a screen space rect 11,11 18x18

  ClearDamageForAllSurfaces(root);

  // Add a backdrop blur filter onto child1_
  FilterOperations filters;
  filters.Append(FilterOperation::CreateBlurFilter(2.f));
  SetBackdropFilter(child1_.get(), filters);
  child1_->NoteLayerPropertyChanged();
  // intersects_damage_under_ is false by default.
  EXPECT_TRUE(GetRenderSurface(child1_)->intersects_damage_under());

  EmulateDrawingOneFrame(root);
  // child1_'s render target has changed its surface property.
  EXPECT_TRUE(GetRenderSurface(child1_)->intersects_damage_under());

  // Let run for one update and there should be no damage left.
  EmulateDrawingOneFrame(root);
  EXPECT_FALSE(GetRenderSurface(child1_)->intersects_damage_under());

  // CASE 1.1: Setting a non-intersecting update rect on the root
  // doesn't invalidate child1_'s cached backdrop-filtered result.
  // Damage rect at 0,0 20x20 doesn't intersect 270,270 36x38.
  root->UnionUpdateRect(gfx::Rect(0, 0, 20, 20));
  EmulateDrawingOneFrame(root);
  EXPECT_FALSE(GetRenderSurface(child1_)->intersects_damage_under());

  // CASE 1.2: Setting an intersecting update rect on the root invalidates
  // child1_'s cached backdrop-filtered result.
  // Damage rect at 260,260 20x20 intersects 270,270 36x38.
  ClearDamageForAllSurfaces(root);
  root->UnionUpdateRect(gfx::Rect(260, 260, 20, 20));
  EmulateDrawingOneFrame(root);
  EXPECT_TRUE(GetRenderSurface(child1_)->intersects_damage_under());

  // CASE 1.3: Damage on layers above the surface with the backdrop filter
  // doesn't invalidate cached backdrop-filtered result. Move child2_ to overlap
  // with child1_.
  ClearDamageForAllSurfaces(root);
  child2_->SetOffsetToTransformParent(gfx::Vector2dF(180.f, 180.f));
  EmulateDrawingOneFrame(root);
  EXPECT_FALSE(GetRenderSurface(grand_child1_)->intersects_damage_under());

  // CASE 2: Adding or removing a backdrop filter would invalidate cached
  // backdrop-filtered result of the corresponding render surfaces.
  ClearDamageForAllSurfaces(root);
  // Remove the backdrop filter on child1_
  SetBackdropFilter(child1_.get(), FilterOperations());
  grand_child1_->NoteLayerPropertyChanged();
  // Add a backdrop blur filtre to grand_child4_
  SetBackdropFilter(grand_child4_.get(), filters);
  grand_child4_->NoteLayerPropertyChanged();
  EmulateDrawingOneFrame(root);
  EXPECT_TRUE(GetRenderSurface(grand_child4_)->intersects_damage_under());
  EXPECT_TRUE(GetRenderSurface(grand_child1_)->intersects_damage_under());

  // Let run for one update and there should be no damage left.
  EmulateDrawingOneFrame(root);
  EXPECT_FALSE(GetRenderSurface(grand_child4_)->intersects_damage_under());

  // CASE 3.1: Adding a non-intersecting damage rect to a sibling layer under
  // the render surface with the backdrop filter doesn't invalidate cached
  // backdrop-filtered result. Damage rect on grand_child1_ at 302,302 1x1
  // doesn't intersect 280,280 15x16.
  ClearDamageForAllSurfaces(root);
  grand_child1_->AddDamageRect(gfx::Rect(2, 2, 1.f, 1.f));
  EmulateDrawingOneFrame(root);
  EXPECT_FALSE(GetRenderSurface(grand_child4_)->intersects_damage_under());

  // CASE 3.2: Adding an intersecting damage rect to a sibling layer under the
  // render surface with the backdrop filter invalidates cached
  // backdrop-filtered result. Damage rect on grand_child2_ at 290,290 1x1
  // intersects 280,280 15x16.
  ClearDamageForAllSurfaces(root);
  grand_child2_->AddDamageRect(gfx::Rect(0, 0, 1.f, 1.f));
  EmulateDrawingOneFrame(root);
  EXPECT_TRUE(GetRenderSurface(grand_child4_)->intersects_damage_under());

  // CASE 4.1: Non-intersecting damage rect on a sibling surface under the
  // render surface with the backdrop filter doesn't invalidate cached
  // backdrop-filtered result.
  ClearDamageForAllSurfaces(root);
  grand_child3_->AddDamageRect(gfx::Rect(0, 0, 1.f, 1.f));
  EmulateDrawingOneFrame(root);
  gfx::Rect damage_rect;
  EXPECT_TRUE(GetRenderSurface(grand_child3_)
                  ->damage_tracker()
                  ->GetDamageRectIfValid(&damage_rect));
  EXPECT_EQ(gfx::Rect(170, 170, 1.f, 1.f), damage_rect);
  // Damage rect at 170,170 1x1 in render target local space doesn't intersect
  // 180,180 15x16.
  EXPECT_FALSE(GetRenderSurface(grand_child4_)->intersects_damage_under());

  // CASE 4.2: Intersecting damage rect on a sibling surface under the render
  // surface with the backdrop filter invalidates cached backdrop-filtered
  // result.
  ClearDamageForAllSurfaces(root);
  grand_child3_->SetBounds(gfx::Size(11.f, 11.f));
  EmulateDrawingOneFrame(root);
  EXPECT_TRUE(GetRenderSurface(grand_child3_)
                  ->damage_tracker()
                  ->GetDamageRectIfValid(&damage_rect));
  EXPECT_EQ(gfx::Rect(170, 170, 11.f, 11.f), damage_rect);
  // Damage rect at 170,170 11x11 in render target local space intersects
  // 180,180 15x16
  EXPECT_TRUE(GetRenderSurface(grand_child4_)->intersects_damage_under());

  // CASE 5.1: Removing a non-intersecting sibling layer under the render
  // surface with the backdrop filter doesn't invalidate cached
  // backdrop-filtered result. Removing grand_child1_ at 300,300 6x8 which
  // doesn't intersect 280,280 15x16.
  ClearDamageForAllSurfaces(root);
  OwnedLayerImplList layers =
      host_impl()->active_tree()->DetachLayersKeepingRootLayerForTesting();
  ASSERT_EQ(7u, layers.size());
  ASSERT_EQ(child1_, layers[1].get());
  ASSERT_EQ(grand_child1_, layers[2].get());
  ASSERT_EQ(grand_child2_, layers[3].get());
  ASSERT_EQ(grand_child3_, layers[4].get());
  ASSERT_EQ(grand_child4_, layers[5].get());
  ASSERT_EQ(child2_, layers[6].get());
  host_impl()->active_tree()->AddLayer(std::move(layers[1]));
  host_impl()->active_tree()->AddLayer(std::move(layers[3]));
  host_impl()->active_tree()->AddLayer(std::move(layers[4]));
  host_impl()->active_tree()->AddLayer(std::move(layers[5]));
  host_impl()->active_tree()->AddLayer(std::move(layers[6]));
  EmulateDrawingOneFrame(root);
  EXPECT_FALSE(GetRenderSurface(grand_child4_)->intersects_damage_under());

  // CASE 5.2: Removing an intersecting sibling layer under the render surface
  // with the backdrop filter invalidates cached backdrop-filtered result.
  // Removing grand_child2_ at 290,290 6x8 which intersects 280,280 15x16.
  ClearDamageForAllSurfaces(root);
  layers = host_impl()->active_tree()->DetachLayersKeepingRootLayerForTesting();
  ASSERT_EQ(6u, layers.size());
  ASSERT_EQ(child1_, layers[1].get());
  ASSERT_EQ(grand_child2_, layers[2].get());
  ASSERT_EQ(grand_child3_, layers[3].get());
  ASSERT_EQ(grand_child4_, layers[4].get());
  ASSERT_EQ(child2_, layers[5].get());
  host_impl()->active_tree()->AddLayer(std::move(layers[1]));
  host_impl()->active_tree()->AddLayer(std::move(layers[3]));
  host_impl()->active_tree()->AddLayer(std::move(layers[4]));
  host_impl()->active_tree()->AddLayer(std::move(layers[5]));

  EmulateDrawingOneFrame(root);
  EXPECT_TRUE(GetRenderSurface(grand_child4_)->intersects_damage_under());

  // Let run for one update and there should be no damage left.
  ClearDamageForAllSurfaces(root);
  EmulateDrawingOneFrame(root);
  EXPECT_FALSE(GetRenderSurface(grand_child4_)->intersects_damage_under());

  // CASE 6: Removing a intersecting sibling surface under the render
  // surface with the backdrop filter invalidate cached backdrop-filtered
  // result.
  ClearDamageForAllSurfaces(root);
  SetRenderSurfaceReason(grand_child3_.get(), RenderSurfaceReason::kNone);
  grand_child3_->SetDrawsContent(false);
  EmulateDrawingOneFrame(root);
  EXPECT_TRUE(GetRenderSurface(grand_child4_)->intersects_damage_under());
}

TEST_F(DamageTrackerTest, DamageRectOnlyVisibleContentsMoveToOutside) {
  LayerImpl* root = CreateAndSetUpTestTreeWithOneSurface(2);
  ClearDamageForAllSurfaces(root);

  LayerImpl* child1 = child_layers_[0];
  LayerImpl* child2 = child_layers_[1];
  gfx::Rect origin_damage = child1->visible_drawable_content_rect();
  origin_damage.Union(child2->visible_drawable_content_rect());

  // Really far left.
  child1->SetOffsetToTransformParent(gfx::Vector2dF(
      static_cast<float>(std::numeric_limits<int>::min() + 100), 0));
  child1->SetBounds(gfx::Size(1, 1));

  // Really far right.
  child2->SetOffsetToTransformParent(gfx::Vector2dF(
      static_cast<float>(std::numeric_limits<int>::max() - 100), 0));
  child2->SetBounds(gfx::Size(1, 1));
  EmulateDrawingOneFrame(root, 1.f);

  // Above damages should be excludebe because they're outside of
  // the root surface.
  gfx::Rect damage_rect;
  EXPECT_TRUE(GetRenderSurface(root)->damage_tracker()->GetDamageRectIfValid(
      &damage_rect));
  EXPECT_EQ(origin_damage, damage_rect);
  EXPECT_TRUE(GetRenderSurface(root)->content_rect().Contains(damage_rect));
  EXPECT_TRUE(GetRenderSurface(root)
                  ->damage_tracker()
                  ->has_damage_from_contributing_content());
}

TEST_F(DamageTrackerTest, DamageRectOnlyVisibleContentsLargeTwoContents) {
  LayerImpl* root = CreateAndSetUpTestTreeWithOneSurface(2);
  ClearDamageForAllSurfaces(root);

  LayerImpl* child1 = child_layers_[0];
  LayerImpl* child2 = child_layers_[1];

  gfx::Rect expected_damage = child1->visible_drawable_content_rect();
  expected_damage.Union(child2->visible_drawable_content_rect());
  expected_damage.set_x(0);
  expected_damage.set_width(GetRenderSurface(root)->content_rect().width());

  // Really far left.
  child1->SetOffsetToTransformParent(gfx::Vector2dF(
      static_cast<float>(std::numeric_limits<int>::min() + 100), 100));
  child1->SetBounds(
      gfx::Size(std::numeric_limits<int>::max(), child1->bounds().height()));

  // Really far right.
  child2->SetOffsetToTransformParent(gfx::Vector2dF(100, 100));
  child2->SetBounds(
      gfx::Size(std::numeric_limits<int>::max(), child2->bounds().height()));
  EmulateDrawingOneFrame(root, 1.f);

  // Above damages should be excluded because they're outside of
  // the root surface.
  gfx::Rect damage_rect;
  EXPECT_TRUE(GetRenderSurface(root)->damage_tracker()->GetDamageRectIfValid(
      &damage_rect));
  EXPECT_EQ(expected_damage, damage_rect);
  EXPECT_TRUE(GetRenderSurface(root)->content_rect().Contains(damage_rect));
  EXPECT_TRUE(GetRenderSurface(root)
                  ->damage_tracker()
                  ->has_damage_from_contributing_content());
}

TEST_F(DamageTrackerTest,
       DamageRectOnlyVisibleContentsHugeContentPartiallyVisible) {
  LayerImpl* root = CreateAndSetUpTestTreeWithOneSurface(1);
  int content_width = GetRenderSurface(root)->content_rect().width();

  ClearDamageForAllSurfaces(root);

  LayerImpl* child1 = child_layers_[0];
  int y = child1->offset_to_transform_parent().y();
  int offset = 100;
  int expected_width = offset + child1->bounds().width();
  // Huge content that exceeds on both side.
  child1->SetOffsetToTransformParent(
      gfx::Vector2dF(std::numeric_limits<int>::min() + offset, y));
  child1->SetBounds(
      gfx::Size(std::numeric_limits<int>::max(), child1->bounds().height()));

  EmulateDrawingOneFrame(root);

  gfx::Rect expected_damage_rect1(0, y, expected_width,
                                  child1->bounds().height());

  // Above damages should be excludebe because they're outside of
  // the root surface.
  gfx::Rect damage_rect;
  EXPECT_TRUE(GetRenderSurface(root)->damage_tracker()->GetDamageRectIfValid(
      &damage_rect));
  EXPECT_EQ(expected_damage_rect1, damage_rect);
  EXPECT_TRUE(GetRenderSurface(root)
                  ->damage_tracker()
                  ->has_damage_from_contributing_content());

  ClearDamageForAllSurfaces(root);

  // Now move the huge layer to the right, keeping offset visible.
  child1->SetOffsetToTransformParent(gfx::Vector2dF(content_width - offset, y));
  child1->NoteLayerPropertyChanged();

  EmulateDrawingOneFrame(root);

  // The damaged rect should be "letter boxed" region.
  gfx::Rect expected_damage_rect2(0, y, content_width,
                                  child1->bounds().height());
  EXPECT_TRUE(GetRenderSurface(root)->damage_tracker()->GetDamageRectIfValid(
      &damage_rect));
  EXPECT_EQ(expected_damage_rect2, damage_rect);
  EXPECT_TRUE(GetRenderSurface(root)
                  ->damage_tracker()
                  ->has_damage_from_contributing_content());
}

TEST_F(DamageTrackerTest, VerifyDamageExpansionWithBackdropBlurFilters) {
  LayerImpl* root = CreateAndSetUpTestTreeWithTwoSurfaces();

  // Allow us to set damage on child1_.
  child1_->SetDrawsContent(true);

  FilterOperations filters;
  filters.Append(FilterOperation::CreateBlurFilter(2.f));

  // Setting the filter will damage the whole surface.
  ClearDamageForAllSurfaces(root);
  SetBackdropFilter(child1_.get(), filters);
  child1_->NoteLayerPropertyChanged();
  EmulateDrawingOneFrame(root);

  ClearDamageForAllSurfaces(root);
  root->UnionUpdateRect(gfx::Rect(297, 297, 2, 2));
  EmulateDrawingOneFrame(root);

  // child1_'s render surface has a size of 206x208 due to the contributions
  // from grand_child1_ and grand_child2_. The blur filter on child1_ intersects
  // the damage from root and expands it to (100,100 206x208).
  gfx::Rect expected_damage_rect = gfx::Rect(100, 100, 206, 208);
  gfx::Rect root_damage_rect;
  EXPECT_TRUE(GetRenderSurface(root)->damage_tracker()->GetDamageRectIfValid(
      &root_damage_rect));
  EXPECT_EQ(expected_damage_rect, root_damage_rect);

  ClearDamageForAllSurfaces(root);
  gfx::Rect damage_rect(97, 97, 2, 2);
  root->UnionUpdateRect(damage_rect);
  EmulateDrawingOneFrame(root);

  // The blur filter on child1_ doesn't intersect the damage from root so the
  // damage remains unchanged.
  EXPECT_TRUE(GetRenderSurface(root)->damage_tracker()->GetDamageRectIfValid(
      &root_damage_rect));
  EXPECT_EQ(damage_rect, root_damage_rect);
}

}  // namespace
}  // namespace cc
