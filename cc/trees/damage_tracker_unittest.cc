// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/damage_tracker.h"

#include <stddef.h>

#include "cc/base/math_util.h"
#include "cc/layers/layer_impl.h"
#include "cc/paint/filter_operation.h"
#include "cc/paint/filter_operations.h"
#include "cc/test/fake_impl_task_runner_provider.h"
#include "cc/test/fake_layer_tree_host_impl.h"
#include "cc/test/geometry_test_utils.h"
#include "cc/test/layer_test_common.h"
#include "cc/test/test_task_graph_runner.h"
#include "cc/trees/layer_tree_host_common.h"
#include "cc/trees/layer_tree_impl.h"
#include "cc/trees/single_thread_proxy.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/quad_f.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace cc {
namespace {

void ExecuteCalculateDrawProperties(LayerImpl* root,
                                    float device_scale_factor,
                                    RenderSurfaceList* render_surface_list) {
  // Sanity check: The test itself should create the root layer's render
  //               surface, so that the surface (and its damage tracker) can
  //               persist across multiple calls to this function.
  ASSERT_FALSE(render_surface_list->size());

  LayerTreeHostCommon::CalcDrawPropsImplInputsForTesting inputs(
      root, root->bounds(), device_scale_factor, render_surface_list);
  LayerTreeHostCommon::CalculateDrawPropertiesForTesting(&inputs);
  ASSERT_TRUE(GetRenderSurface(root));
}

void ClearDamageForAllSurfaces(LayerImpl* root) {
  for (auto* layer : *root->layer_tree_impl()) {
    if (GetRenderSurface(layer))
      GetRenderSurface(layer)->damage_tracker()->DidDrawDamagedArea();
  }
}

void EmulateDrawingOneFrame(LayerImpl* root, float device_scale_factor = 1.f) {
  // This emulates only steps that are relevant to testing the damage tracker:
  //   1. computing the render passes and layerlists
  //   2. updating all damage trackers in the correct order
  //   3. resetting all update_rects and property_changed flags for all layers
  //      and surfaces.

  RenderSurfaceList render_surface_list;
  ExecuteCalculateDrawProperties(root, device_scale_factor,
                                 &render_surface_list);

  DamageTracker::UpdateDamageTracking(root->layer_tree_impl(),
                                      render_surface_list);

  root->layer_tree_impl()->ResetAllChangeTracking();
}

class DamageTrackerTest : public testing::Test {
 public:
  DamageTrackerTest()
      : host_impl_(&task_runner_provider_, &task_graph_runner_) {}

  LayerImpl* CreateTestTreeWithOneSurface(int number_of_children) {
    host_impl_.active_tree()->DetachLayers();
    std::unique_ptr<LayerImpl> root =
        LayerImpl::Create(host_impl_.active_tree(), 1);

    root->SetPosition(gfx::PointF());
    root->SetBounds(gfx::Size(500, 500));
    root->SetDrawsContent(true);
    root->test_properties()->force_render_surface = true;

    for (int i = 0; i < number_of_children; ++i) {
      std::unique_ptr<LayerImpl> child =
          LayerImpl::Create(host_impl_.active_tree(), 2 + i);
      child->SetPosition(gfx::PointF(100.f, 100.f));
      child->SetBounds(gfx::Size(30, 30));
      child->SetDrawsContent(true);
      root->test_properties()->AddChild(std::move(child));
    }
    host_impl_.active_tree()->SetRootLayerForTesting(std::move(root));
    host_impl_.active_tree()->SetElementIdsForTesting();

    return host_impl_.active_tree()->root_layer_for_testing();
  }

  LayerImpl* CreateTestTreeWithTwoSurfaces() {
    // This test tree has two render surfaces: one for the root, and one for
    // child1. Additionally, the root has a second child layer, and child1 has
    // two children of its own.

    host_impl_.active_tree()->DetachLayers();
    std::unique_ptr<LayerImpl> root =
        LayerImpl::Create(host_impl_.active_tree(), 1);
    std::unique_ptr<LayerImpl> child1 =
        LayerImpl::Create(host_impl_.active_tree(), 2);
    std::unique_ptr<LayerImpl> child2 =
        LayerImpl::Create(host_impl_.active_tree(), 3);
    std::unique_ptr<LayerImpl> grand_child1 =
        LayerImpl::Create(host_impl_.active_tree(), 4);
    std::unique_ptr<LayerImpl> grand_child2 =
        LayerImpl::Create(host_impl_.active_tree(), 5);

    root->SetPosition(gfx::PointF());
    root->SetBounds(gfx::Size(500, 500));
    root->SetDrawsContent(true);
    root->test_properties()->force_render_surface = true;

    child1->SetPosition(gfx::PointF(100.f, 100.f));
    child1->SetBounds(gfx::Size(30, 30));
    // With a child that draws_content, opacity will cause the layer to create
    // its own RenderSurface. This layer does not draw, but is intended to
    // create its own RenderSurface.
    child1->SetDrawsContent(false);
    child1->test_properties()->force_render_surface = true;

    child2->SetPosition(gfx::PointF(11.f, 11.f));
    child2->SetBounds(gfx::Size(18, 18));
    child2->SetDrawsContent(true);

    grand_child1->SetPosition(gfx::PointF(200.f, 200.f));
    grand_child1->SetBounds(gfx::Size(6, 8));
    grand_child1->SetDrawsContent(true);

    grand_child2->SetPosition(gfx::PointF(190.f, 190.f));
    grand_child2->SetBounds(gfx::Size(6, 8));
    grand_child2->SetDrawsContent(true);

    child1->test_properties()->AddChild(std::move(grand_child1));
    child1->test_properties()->AddChild(std::move(grand_child2));
    root->test_properties()->AddChild(std::move(child1));
    root->test_properties()->AddChild(std::move(child2));
    host_impl_.active_tree()->SetRootLayerForTesting(std::move(root));
    host_impl_.active_tree()->SetElementIdsForTesting();

    return host_impl_.active_tree()->root_layer_for_testing();
  }

  LayerImpl* CreateAndSetUpTestTreeWithOneSurface(int number_of_children = 1) {
    LayerImpl* root = CreateTestTreeWithOneSurface(number_of_children);

    // Setup includes going past the first frame which always damages
    // everything, so that we can actually perform specific tests.
    root->layer_tree_impl()->property_trees()->needs_rebuild = true;
    EmulateDrawingOneFrame(root);

    return root;
  }

  LayerImpl* CreateAndSetUpTestTreeWithTwoSurfaces() {
    LayerImpl* root = CreateTestTreeWithTwoSurfaces();

    // Setup includes going past the first frame which always damages
    // everything, so that we can actually perform specific tests.
    root->layer_tree_impl()->property_trees()->needs_rebuild = true;
    EmulateDrawingOneFrame(root);

    return root;
  }

 protected:
  FakeImplTaskRunnerProvider task_runner_provider_;
  TestTaskGraphRunner task_graph_runner_;
  FakeLayerTreeHostImpl host_impl_;
};

TEST_F(DamageTrackerTest, SanityCheckTestTreeWithOneSurface) {
  // Sanity check that the simple test tree will actually produce the expected
  // render surfaces.

  LayerImpl* root = CreateAndSetUpTestTreeWithOneSurface();
  LayerImpl* child = root->test_properties()->children[0];

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

  LayerImpl* child1 = root->test_properties()->children[0];
  LayerImpl* child2 = root->test_properties()->children[1];

  gfx::Rect child_damage_rect;
  EXPECT_TRUE(GetRenderSurface(child1)->damage_tracker()->GetDamageRectIfValid(
      &child_damage_rect));
  gfx::Rect root_damage_rect;
  EXPECT_TRUE(GetRenderSurface(root)->damage_tracker()->GetDamageRectIfValid(
      &root_damage_rect));

  EXPECT_NE(GetRenderSurface(child1), GetRenderSurface(root));
  EXPECT_EQ(GetRenderSurface(child2), GetRenderSurface(root));
  EXPECT_EQ(3, GetRenderSurface(root)->num_contributors());
  EXPECT_EQ(2, GetRenderSurface(child1)->num_contributors());

  // The render surface for child1 only has a content_rect that encloses
  // grand_child1 and grand_child2, because child1 does not draw content.
  EXPECT_EQ(gfx::Rect(190, 190, 16, 18).ToString(),
            child_damage_rect.ToString());
  EXPECT_EQ(gfx::Rect(500, 500).ToString(), root_damage_rect.ToString());

  EXPECT_TRUE(GetRenderSurface(root)
                  ->damage_tracker()
                  ->has_damage_from_contributing_content());
  EXPECT_TRUE(GetRenderSurface(child1)
                  ->damage_tracker()
                  ->has_damage_from_contributing_content());
}

TEST_F(DamageTrackerTest, VerifyDamageForUpdateRects) {
  LayerImpl* root = CreateAndSetUpTestTreeWithOneSurface();
  LayerImpl* child = root->test_properties()->children[0];

  // CASE 1: Setting the update rect should cause the corresponding damage to
  //         the surface.
  ClearDamageForAllSurfaces(root);
  child->SetUpdateRect(gfx::Rect(10, 11, 12, 13));
  root->layer_tree_impl()->property_trees()->needs_rebuild = true;
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
  child->SetUpdateRect(gfx::Rect(10, 11, 12, 13));
  root->layer_tree_impl()->property_trees()->needs_rebuild = true;
  EmulateDrawingOneFrame(root);
  EXPECT_TRUE(GetRenderSurface(root)->damage_tracker()->GetDamageRectIfValid(
      &root_damage_rect));
  EXPECT_EQ(gfx::Rect(110, 111, 12, 13).ToString(),
            root_damage_rect.ToString());

  // CASE 3: Setting a different update rect should cause damage on the new
  //         update region, but no additional exposed old region.
  ClearDamageForAllSurfaces(root);
  child->SetUpdateRect(gfx::Rect(20, 25, 1, 2));
  root->layer_tree_impl()->property_trees()->needs_rebuild = true;
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
  LayerImpl* child = root->test_properties()->children[0];

  // CASE 1: Adding the layer damage rect should cause the corresponding damage
  // to the surface.
  ClearDamageForAllSurfaces(root);
  child->AddDamageRect(gfx::Rect(10, 11, 12, 13));
  root->layer_tree_impl()->property_trees()->needs_rebuild = true;
  EmulateDrawingOneFrame(root);

  // Damage position on the surface should be: position of layer damage_rect
  // (10, 11) relative to the child (100, 100).
  gfx::Rect root_damage_rect;
  EXPECT_TRUE(GetRenderSurface(root)->damage_tracker()->GetDamageRectIfValid(
      &root_damage_rect));
  EXPECT_EQ(true, root_damage_rect.Contains(gfx::Rect(110, 111, 12, 13)));

  // CASE 2: The same layer damage rect twice in a row still produces the same
  // damage.
  ClearDamageForAllSurfaces(root);
  child->AddDamageRect(gfx::Rect(10, 11, 12, 13));
  root->layer_tree_impl()->property_trees()->needs_rebuild = true;
  EmulateDrawingOneFrame(root);
  EXPECT_TRUE(GetRenderSurface(root)->damage_tracker()->GetDamageRectIfValid(
      &root_damage_rect));
  EXPECT_EQ(true, root_damage_rect.Contains(gfx::Rect(110, 111, 12, 13)));

  // CASE 3: Adding a different layer damage rect should cause damage on the
  // new damaged region, but no additional exposed old region.
  ClearDamageForAllSurfaces(root);
  child->AddDamageRect(gfx::Rect(20, 25, 1, 2));
  root->layer_tree_impl()->property_trees()->needs_rebuild = true;
  EmulateDrawingOneFrame(root);

  // Damage position on the surface should be: position of layer damage_rect
  // (20, 25) relative to the child (100, 100).
  EXPECT_TRUE(GetRenderSurface(root)->damage_tracker()->GetDamageRectIfValid(
      &root_damage_rect));
  EXPECT_EQ(true, root_damage_rect.Contains(gfx::Rect(120, 125, 1, 2)));

  // CASE 4: Adding multiple layer damage rects should cause a unified
  // damage on root damage rect.
  ClearDamageForAllSurfaces(root);
  child->AddDamageRect(gfx::Rect(20, 25, 1, 2));
  child->AddDamageRect(gfx::Rect(10, 15, 3, 4));
  root->layer_tree_impl()->property_trees()->needs_rebuild = true;
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
}

TEST_F(DamageTrackerTest, VerifyDamageForLayerUpdateAndDamageRects) {
  LayerImpl* root = CreateAndSetUpTestTreeWithOneSurface();
  LayerImpl* child = root->test_properties()->children[0];

  // CASE 1: Adding the layer damage rect and update rect should cause the
  // corresponding damage to the surface.
  ClearDamageForAllSurfaces(root);
  child->AddDamageRect(gfx::Rect(5, 6, 12, 13));
  child->SetUpdateRect(gfx::Rect(15, 16, 14, 10));
  root->layer_tree_impl()->property_trees()->needs_rebuild = true;
  EmulateDrawingOneFrame(root);

  // Damage position on the surface should be: position of unified layer
  // damage_rect and update rect (5, 6)
  // relative to the child (100, 100).
  gfx::Rect root_damage_rect;
  EXPECT_TRUE(GetRenderSurface(root)->damage_tracker()->GetDamageRectIfValid(
      &root_damage_rect));
  EXPECT_EQ(true, root_damage_rect.Contains(gfx::Rect(105, 106, 24, 20)));

  // CASE 2: The same layer damage rect and update rect twice in a row still
  // produces the same damage.
  ClearDamageForAllSurfaces(root);
  child->AddDamageRect(gfx::Rect(10, 11, 12, 13));
  child->SetUpdateRect(gfx::Rect(10, 11, 14, 15));
  root->layer_tree_impl()->property_trees()->needs_rebuild = true;
  EmulateDrawingOneFrame(root);
  EXPECT_TRUE(GetRenderSurface(root)->damage_tracker()->GetDamageRectIfValid(
      &root_damage_rect));
  EXPECT_EQ(true, root_damage_rect.Contains(gfx::Rect(110, 111, 14, 15)));

  // CASE 3: Adding a different layer damage rect and update rect should cause
  // damage on the new damaged region, but no additional exposed old region.
  ClearDamageForAllSurfaces(root);
  child->AddDamageRect(gfx::Rect(20, 25, 2, 3));
  child->SetUpdateRect(gfx::Rect(5, 10, 7, 8));
  root->layer_tree_impl()->property_trees()->needs_rebuild = true;
  EmulateDrawingOneFrame(root);

  // Damage position on the surface should be: position of unified layer damage
  // rect and update rect (5, 10) relative to the child (100, 100).
  EXPECT_TRUE(GetRenderSurface(root)->damage_tracker()->GetDamageRectIfValid(
      &root_damage_rect));
  EXPECT_EQ(true, root_damage_rect.Contains(gfx::Rect(105, 110, 17, 18)));
  EXPECT_TRUE(GetRenderSurface(root)
                  ->damage_tracker()
                  ->has_damage_from_contributing_content());
}

TEST_F(DamageTrackerTest, VerifyDamageForPropertyChanges) {
  LayerImpl* root = CreateAndSetUpTestTreeWithOneSurface();
  LayerImpl* child = root->test_properties()->children[0];

  // CASE 1: The layer's property changed flag takes priority over update rect.
  //
  child->test_properties()->force_render_surface = true;
  root->layer_tree_impl()->property_trees()->needs_rebuild = true;
  EmulateDrawingOneFrame(root);
  ClearDamageForAllSurfaces(root);
  child->SetUpdateRect(gfx::Rect(10, 11, 12, 13));
  root->layer_tree_impl()->SetOpacityMutated(child->element_id(), 0.5f);
  EmulateDrawingOneFrame(root);

  ASSERT_EQ(2, GetRenderSurface(root)->num_contributors());

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
  root->layer_tree_impl()->property_trees()->needs_rebuild = true;
  EmulateDrawingOneFrame(root);
  EXPECT_TRUE(GetRenderSurface(root)->damage_tracker()->GetDamageRectIfValid(
      &root_damage_rect));
  EXPECT_TRUE(root_damage_rect.IsEmpty());

  // Then, test the actual layer movement.
  ClearDamageForAllSurfaces(root);
  gfx::Transform translation;
  translation.Translate(100.f, 130.f);
  root->layer_tree_impl()->SetTransformMutated(child->element_id(),
                                               translation);
  EmulateDrawingOneFrame(root);

  // Expect damage to be the combination of the previous one and the new one.
  expected_rect.Union(gfx::Rect(200, 230, 30, 30));
  EXPECT_TRUE(GetRenderSurface(root)->damage_tracker()->GetDamageRectIfValid(
      &root_damage_rect));
  EXPECT_FLOAT_RECT_EQ(expected_rect, root_damage_rect);
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

TEST_F(DamageTrackerTest,
       VerifyDamageForPropertyChangesFromContributingContents) {
  LayerImpl* root = CreateAndSetUpTestTreeWithTwoSurfaces();
  LayerImpl* child1 = root->test_properties()->children[0];
  LayerImpl* child2 = root->test_properties()->children[1];
  LayerImpl* grandchild1 = child1->test_properties()->children[0];

  // CASE 1: The child1's opacity changed.
  ClearDamageForAllSurfaces(root);
  root->layer_tree_impl()->SetOpacityMutated(child1->element_id(), 0.5f);
  EmulateDrawingOneFrame(root);
  EXPECT_TRUE(GetRenderSurface(root)
                  ->damage_tracker()
                  ->has_damage_from_contributing_content());
  EXPECT_FALSE(GetRenderSurface(child1)
                   ->damage_tracker()
                   ->has_damage_from_contributing_content());

  // CASE 2: The layer2's opacity changed.
  child2->test_properties()->force_render_surface = true;
  root->layer_tree_impl()->property_trees()->needs_rebuild = true;
  EmulateDrawingOneFrame(root);
  ClearDamageForAllSurfaces(root);
  root->layer_tree_impl()->SetOpacityMutated(child2->element_id(), 0.5f);
  EmulateDrawingOneFrame(root);
  EXPECT_TRUE(GetRenderSurface(root)
                  ->damage_tracker()
                  ->has_damage_from_contributing_content());
  EXPECT_FALSE(GetRenderSurface(child1)
                   ->damage_tracker()
                   ->has_damage_from_contributing_content());

  // CASE 3: The grandchild1's opacity changed.
  grandchild1->test_properties()->force_render_surface = true;
  root->layer_tree_impl()->property_trees()->needs_rebuild = true;
  EmulateDrawingOneFrame(root);
  ClearDamageForAllSurfaces(root);
  root->layer_tree_impl()->SetOpacityMutated(grandchild1->element_id(), 0.5f);
  EmulateDrawingOneFrame(root);
  EXPECT_TRUE(GetRenderSurface(root)
                  ->damage_tracker()
                  ->has_damage_from_contributing_content());
  EXPECT_TRUE(GetRenderSurface(child1)
                  ->damage_tracker()
                  ->has_damage_from_contributing_content());
}

TEST_F(DamageTrackerTest,
       VerifyDamageForUpdateAndDamageRectsFromContributingContents) {
  LayerImpl* root = CreateAndSetUpTestTreeWithTwoSurfaces();
  LayerImpl* child1 = root->test_properties()->children[0];
  LayerImpl* child2 = root->test_properties()->children[1];
  LayerImpl* grandchild1 = child1->test_properties()->children[0];

  // CASE 1: Adding the layer1's damage rect and update rect should cause the
  // corresponding damage to the surface.
  child1->SetDrawsContent(true);
  ClearDamageForAllSurfaces(root);
  child1->AddDamageRect(gfx::Rect(105, 106, 12, 15));
  child1->SetUpdateRect(gfx::Rect(115, 116, 12, 15));
  root->layer_tree_impl()->property_trees()->needs_rebuild = true;
  EmulateDrawingOneFrame(root);
  EXPECT_TRUE(GetRenderSurface(root)
                  ->damage_tracker()
                  ->has_damage_from_contributing_content());
  EXPECT_TRUE(GetRenderSurface(child1)
                  ->damage_tracker()
                  ->has_damage_from_contributing_content());

  // CASE 2: Adding the layer2's damage rect and update rect should cause the
  // corresponding damage to the surface.
  ClearDamageForAllSurfaces(root);
  child2->AddDamageRect(gfx::Rect(11, 11, 12, 15));
  child2->SetUpdateRect(gfx::Rect(12, 12, 12, 15));
  EmulateDrawingOneFrame(root);
  EXPECT_TRUE(GetRenderSurface(root)
                  ->damage_tracker()
                  ->has_damage_from_contributing_content());
  EXPECT_FALSE(GetRenderSurface(child1)
                   ->damage_tracker()
                   ->has_damage_from_contributing_content());

  // CASE 3: Adding the grandchild1's damage rect and update rect should cause
  // the corresponding damage to the surface.
  ClearDamageForAllSurfaces(root);
  grandchild1->AddDamageRect(gfx::Rect(1, 0, 2, 5));
  grandchild1->SetUpdateRect(gfx::Rect(2, 1, 2, 5));
  EmulateDrawingOneFrame(root);
  EXPECT_TRUE(GetRenderSurface(root)
                  ->damage_tracker()
                  ->has_damage_from_contributing_content());
  EXPECT_TRUE(GetRenderSurface(child1)
                  ->damage_tracker()
                  ->has_damage_from_contributing_content());
}

TEST_F(DamageTrackerTest, VerifyDamageWhenSurfaceRemoved) {
  LayerImpl* root = CreateAndSetUpTestTreeWithTwoSurfaces();
  LayerImpl* surface = root->test_properties()->children[0];
  LayerImpl* child = surface->test_properties()->children[0];
  child->SetDrawsContent(true);
  EmulateDrawingOneFrame(root);
  ClearDamageForAllSurfaces(root);

  surface->test_properties()->force_render_surface = false;
  child->SetDrawsContent(false);
  root->layer_tree_impl()->property_trees()->needs_rebuild = true;
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
  LayerImpl* child = root->test_properties()->children[0];
  child->test_properties()->force_render_surface = true;

  gfx::Transform rotation;
  rotation.Rotate(45.0);

  ClearDamageForAllSurfaces(root);
  child->test_properties()->transform_origin = gfx::Point3F(
      child->bounds().width() * 0.5f, child->bounds().height() * 0.5f, 0.f);
  child->SetPosition(gfx::PointF(85.f, 85.f));
  child->NoteLayerPropertyChanged();
  root->layer_tree_impl()->property_trees()->needs_rebuild = true;
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
  LayerImpl* child = root->test_properties()->children[0];

  gfx::Transform transform;
  transform.Translate3d(550.0, 500.0, 0.0);
  transform.ApplyPerspectiveDepth(1.0);
  transform.RotateAboutYAxis(45.0);
  transform.Translate3d(-50.0, -50.0, 0.0);

  // Set up the child
  child->SetPosition(gfx::PointF(0.f, 0.f));
  child->SetBounds(gfx::Size(100, 100));
  child->test_properties()->transform = transform;
  root->layer_tree_impl()->property_trees()->needs_rebuild = true;
  EmulateDrawingOneFrame(root);

  // Sanity check that the child layer's bounds would actually get clipped by
  // w < 0, otherwise this test is not actually testing the intended scenario.
  gfx::RectF test_rect(child->position(), gfx::SizeF(child->bounds()));
  bool clipped = false;
  MathUtil::MapQuad(transform, gfx::QuadF(test_rect), &clipped);
  EXPECT_TRUE(clipped);

  // Damage the child without moving it.
  child->test_properties()->force_render_surface = true;
  root->layer_tree_impl()->property_trees()->needs_rebuild = true;
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
  LayerImpl* surface = root->test_properties()->children[0];
  LayerImpl* child = surface->test_properties()->children[0];

  FilterOperations filters;
  filters.Append(FilterOperation::CreateBlurFilter(5.f));

  // Setting the filter should not damage the conrresponding render surface.
  ClearDamageForAllSurfaces(root);
  surface->test_properties()->filters = filters;
  root->layer_tree_impl()->property_trees()->needs_rebuild = true;
  EmulateDrawingOneFrame(root);
  EXPECT_FALSE(GetRenderSurface(root)
                   ->damage_tracker()
                   ->has_damage_from_contributing_content());
  EXPECT_FALSE(GetRenderSurface(surface)
                   ->damage_tracker()
                   ->has_damage_from_contributing_content());

  // Setting the update rect should cause the corresponding damage to the
  // surface, blurred based on the size of the blur filter.
  ClearDamageForAllSurfaces(root);
  child->SetUpdateRect(gfx::Rect(1, 2, 3, 4));
  root->layer_tree_impl()->property_trees()->needs_rebuild = true;
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
  LayerImpl* child = root->test_properties()->children[0];
  gfx::Rect root_damage_rect, child_damage_rect;

  // Allow us to set damage on child too.
  child->SetDrawsContent(true);

  FilterOperations filters;
  filters.Append(
      FilterOperation::CreateReferenceFilter(sk_make_sp<BlurPaintFilter>(
          2, 2, BlurPaintFilter::TileMode::kClampToBlack_TileMode, nullptr)));

  // Setting the filter will damage the whole surface.
  child->test_properties()->force_render_surface = true;
  root->layer_tree_impl()->property_trees()->needs_rebuild = true;
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
  child->SetUpdateRect(gfx::Rect(1, 1));
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
  LayerImpl* child = root->test_properties()->children[0];
  gfx::Rect root_damage_rect, child_damage_rect;

  // Allow us to set damage on child too.
  child->SetDrawsContent(true);

  FilterOperations filters;
  filters.Append(
      FilterOperation::CreateReferenceFilter(sk_make_sp<BlurPaintFilter>(
          2, 2, BlurPaintFilter::TileMode::kClampToBlack_TileMode, nullptr)));

  // Setting the filter will damage the whole surface.
  gfx::Transform transform;
  transform.RotateAboutYAxis(60);
  ClearDamageForAllSurfaces(root);
  child->test_properties()->force_render_surface = true;
  child->test_properties()->transform = transform;
  root->layer_tree_impl()->property_trees()->needs_rebuild = true;
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
  child->SetUpdateRect(gfx::Rect(30, 30));
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
  LayerImpl* child = root->test_properties()->children[0];
  gfx::Rect root_damage_rect, child_damage_rect;

  // Allow us to set damage on child too.
  child->SetDrawsContent(true);

  FilterOperations filters;
  filters.Append(FilterOperation::CreateBlurFilter(3.f));

  // Setting the filter will damage the whole surface.
  ClearDamageForAllSurfaces(root);
  child->test_properties()->force_render_surface = true;
  root->layer_tree_impl()->property_trees()->needs_rebuild = true;
  int device_scale_factor = 2;
  EmulateDrawingOneFrame(root, device_scale_factor);
  child->layer_tree_impl()->SetFilterMutated(child->element_id(), filters);
  EmulateDrawingOneFrame(root, device_scale_factor);
  EXPECT_TRUE(GetRenderSurface(root)->damage_tracker()->GetDamageRectIfValid(
      &root_damage_rect));
  EXPECT_TRUE(GetRenderSurface(child)->damage_tracker()->GetDamageRectIfValid(
      &child_damage_rect));

  // Blur outset is 9px for a 3px blur, scaled up by DSF.
  int blur_outset = 9 * device_scale_factor;
  gfx::Rect original_rect(100, 100, 100, 100);
  gfx::Rect expected_child_damage_rect(60, 60);
  expected_child_damage_rect.Inset(-blur_outset, -blur_outset);
  gfx::Rect expected_root_damage_rect(child_damage_rect);
  expected_root_damage_rect.Offset(200, 200);
  gfx::Rect expected_total_damage_rect = expected_root_damage_rect;
  expected_total_damage_rect.Union(original_rect);
  EXPECT_EQ(expected_total_damage_rect, root_damage_rect);
  EXPECT_EQ(expected_child_damage_rect, child_damage_rect);

  // Setting the update rect should damage only the affected area (original,
  // outset by 3 * blur sigma * DSF).
  ClearDamageForAllSurfaces(root);
  child->SetUpdateRect(gfx::Rect(30, 30));
  EmulateDrawingOneFrame(root, device_scale_factor);

  EXPECT_TRUE(GetRenderSurface(root)->damage_tracker()->GetDamageRectIfValid(
      &root_damage_rect));
  EXPECT_TRUE(GetRenderSurface(child)->damage_tracker()->GetDamageRectIfValid(
      &child_damage_rect));

  EXPECT_EQ(expected_root_damage_rect, root_damage_rect);
  EXPECT_EQ(expected_child_damage_rect, child_damage_rect);
}

TEST_F(DamageTrackerTest, VerifyDamageForBackdropBlurredChild) {
  LayerImpl* root = CreateAndSetUpTestTreeWithTwoSurfaces();
  LayerImpl* child1 = root->test_properties()->children[0];
  LayerImpl* child2 = root->test_properties()->children[1];

  // Allow us to set damage on child1 too.
  child1->SetDrawsContent(true);

  FilterOperations filters;
  filters.Append(FilterOperation::CreateBlurFilter(2.f));

  // Setting the filter will damage the whole surface.
  ClearDamageForAllSurfaces(root);
  child1->test_properties()->backdrop_filters = filters;
  child1->NoteLayerPropertyChanged();
  root->layer_tree_impl()->property_trees()->needs_rebuild = true;
  EmulateDrawingOneFrame(root);

  // CASE 1: Setting the update rect should cause the corresponding damage to
  //         the surface, blurred based on the size of the child's backdrop
  //         blur filter. Note that child1's render surface has a size of
  //         206x208 due to contributions from grand_child1 and grand_child2.
  ClearDamageForAllSurfaces(root);
  root->SetUpdateRect(gfx::Rect(297, 297, 2, 2));
  root->layer_tree_impl()->property_trees()->needs_rebuild = true;
  EmulateDrawingOneFrame(root);

  gfx::Rect root_damage_rect;
  EXPECT_TRUE(GetRenderSurface(root)->damage_tracker()->GetDamageRectIfValid(
      &root_damage_rect));
  // Damage position on the surface should be a composition of the damage on
  // the root and on child2.  Damage on the root should be: position of
  // update_rect (297, 297), but expanded by the blur outsets.
  gfx::Rect expected_damage_rect = gfx::Rect(297, 297, 2, 2);

  // 6px spread for a 2px blur.
  expected_damage_rect.Inset(-6, -6, -6, -6);
  EXPECT_EQ(expected_damage_rect.ToString(), root_damage_rect.ToString());

  // CASE 2: Setting the update rect should cause the corresponding damage to
  //         the surface, blurred based on the size of the child's backdrop
  //         blur filter. Since the damage extends to the right/bottom outside
  //         of the blurred layer, only the left/top should end up expanded.
  ClearDamageForAllSurfaces(root);
  root->SetUpdateRect(gfx::Rect(297, 297, 30, 30));
  root->layer_tree_impl()->property_trees()->needs_rebuild = true;
  EmulateDrawingOneFrame(root);

  EXPECT_TRUE(GetRenderSurface(root)->damage_tracker()->GetDamageRectIfValid(
      &root_damage_rect));
  // Damage position on the surface should be a composition of the damage on
  // the root and on child2.  Damage on the root should be: position of
  // update_rect (297, 297), but expanded on the left/top by the blur outsets.
  expected_damage_rect = gfx::Rect(297, 297, 30, 30);

  // 6px spread for a 2px blur.
  expected_damage_rect.Inset(-6, -6, 0, 0);
  EXPECT_EQ(expected_damage_rect.ToString(), root_damage_rect.ToString());

  // CASE 3: Setting this update rect outside the blurred content_bounds of the
  //         blurred child1 will not cause it to be expanded.
  ClearDamageForAllSurfaces(root);
  root->SetUpdateRect(gfx::Rect(30, 30, 2, 2));
  root->layer_tree_impl()->property_trees()->needs_rebuild = true;
  EmulateDrawingOneFrame(root);

  EXPECT_TRUE(GetRenderSurface(root)->damage_tracker()->GetDamageRectIfValid(
      &root_damage_rect));
  // Damage on the root should be: position of update_rect (30, 30), not
  // expanded.
  expected_damage_rect = gfx::Rect(30, 30, 2, 2);

  EXPECT_EQ(expected_damage_rect.ToString(), root_damage_rect.ToString());

  // CASE 4: Setting this update rect inside the blurred content_bounds but
  //         outside the original content_bounds of the blurred child1 will
  //         cause it to be expanded.
  ClearDamageForAllSurfaces(root);
  root->SetUpdateRect(gfx::Rect(99, 99, 1, 1));
  root->layer_tree_impl()->property_trees()->needs_rebuild = true;
  EmulateDrawingOneFrame(root);

  EXPECT_TRUE(GetRenderSurface(root)->damage_tracker()->GetDamageRectIfValid(
      &root_damage_rect));
  // Damage on the root should be: the originally damaged rect (99,99 1x1)
  // plus the rect that can influence with a 2px blur (93,93 13x13) intersected
  // with the surface rect (100,100 206x208). So no additional damage occurs
  // above or to the left, but there is additional damage within the blurred
  // area.
  expected_damage_rect = gfx::Rect(99, 99, 7, 7);
  EXPECT_EQ(expected_damage_rect.ToString(), root_damage_rect.ToString());

  // CASE 5: Setting the update rect on child2, which is above child1, will
  // not get blurred by child1, so it does not need to get expanded.
  ClearDamageForAllSurfaces(root);
  child2->SetUpdateRect(gfx::Rect(1, 1));
  root->layer_tree_impl()->property_trees()->needs_rebuild = true;
  EmulateDrawingOneFrame(root);

  EXPECT_TRUE(GetRenderSurface(root)->damage_tracker()->GetDamageRectIfValid(
      &root_damage_rect));
  // Damage on child2 should be: position of update_rect offset by the child's
  // position (11, 11), and not expanded by anything.
  expected_damage_rect = gfx::Rect(11, 11, 1, 1);

  EXPECT_EQ(expected_damage_rect.ToString(), root_damage_rect.ToString());

  // CASE 6: Setting the update rect on child1 will also blur the damage, so
  //         that any pixels needed for the blur are redrawn in the current
  //         frame.
  ClearDamageForAllSurfaces(root);
  child1->SetUpdateRect(gfx::Rect(1, 1));
  root->layer_tree_impl()->property_trees()->needs_rebuild = true;
  EmulateDrawingOneFrame(root);

  EXPECT_TRUE(GetRenderSurface(root)->damage_tracker()->GetDamageRectIfValid(
      &root_damage_rect));
  // Damage on child1 should be: position of update_rect offset by the child's
  // position (100, 100), and expanded by the damage.

  // Damage should be (0,0 1x1), offset by the 100,100 offset of child1 in
  // root, and expanded 6px for the 2px blur (i.e., 94,94 13x13), but there
  // should be no damage outside child1 (i.e. none above or to the left of
  // 100,100.
  expected_damage_rect = gfx::Rect(100, 100, 7, 7);
  EXPECT_EQ(expected_damage_rect.ToString(), root_damage_rect.ToString());

  // CASE 7: No changes, so should not damage the surface.
  ClearDamageForAllSurfaces(root);
  // We want to make sure that the backdrop filter doesn't cause empty damage
  // to get expanded. We position child1 so that an expansion of the empty rect
  // would have non-empty intersection with child1 in its target space (root
  // space).
  child1->SetPosition(gfx::PointF());
  root->layer_tree_impl()->property_trees()->needs_rebuild = true;
  EmulateDrawingOneFrame(root);

  gfx::Rect child_damage_rect;
  EXPECT_TRUE(GetRenderSurface(root)->damage_tracker()->GetDamageRectIfValid(
      &root_damage_rect));
  EXPECT_TRUE(GetRenderSurface(child1)->damage_tracker()->GetDamageRectIfValid(
      &child_damage_rect));

  // Should not be expanded by the blur filter.
  EXPECT_EQ(gfx::Rect(), root_damage_rect);
  EXPECT_EQ(gfx::Rect(), child_damage_rect);
}

TEST_F(DamageTrackerTest, VerifyDamageForAddingAndRemovingLayer) {
  LayerImpl* root = CreateAndSetUpTestTreeWithOneSurface();
  LayerImpl* child1 = root->test_properties()->children[0];

  // CASE 1: Adding a new layer should cause the appropriate damage.
  //
  ClearDamageForAllSurfaces(root);
  {
    std::unique_ptr<LayerImpl> child2 =
        LayerImpl::Create(host_impl_.active_tree(), 3);
    child2->SetPosition(gfx::PointF(400.f, 380.f));
    child2->SetBounds(gfx::Size(6, 8));
    child2->SetDrawsContent(true);
    root->test_properties()->AddChild(std::move(child2));
  }
  root->layer_tree_impl()->property_trees()->needs_rebuild = true;
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
  root->layer_tree_impl()->property_trees()->needs_rebuild = true;
  EmulateDrawingOneFrame(root);

  EXPECT_TRUE(GetRenderSurface(root)->damage_tracker()->GetDamageRectIfValid(
      &root_damage_rect));
  EXPECT_TRUE(root_damage_rect.IsEmpty());

  // Then, test removing child1.
  root->test_properties()->RemoveChild(child1);
  child1 = nullptr;
  root->layer_tree_impl()->property_trees()->needs_rebuild = true;
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
  // If child2 is added to the layer tree, but it doesn't have any explicit
  // damage of its own, it should still indeed damage the target surface.

  LayerImpl* root = CreateAndSetUpTestTreeWithOneSurface();

  ClearDamageForAllSurfaces(root);
  {
    std::unique_ptr<LayerImpl> child2 =
        LayerImpl::Create(host_impl_.active_tree(), 3);
    child2->SetPosition(gfx::PointF(400.f, 380.f));
    child2->SetBounds(gfx::Size(6, 8));
    child2->SetDrawsContent(true);
    root->test_properties()->AddChild(std::move(child2));
    root->layer_tree_impl()->BuildLayerListForTesting();
    host_impl_.active_tree()->ResetAllChangeTracking();
    LayerImpl* child2_ptr = host_impl_.active_tree()->LayerById(3);
    // Sanity check the initial conditions of the test, if these asserts
    // trigger, it means the test no longer actually covers the intended
    // scenario.
    ASSERT_FALSE(child2_ptr->LayerPropertyChanged());
    ASSERT_TRUE(child2_ptr->update_rect().IsEmpty());
  }
  root->layer_tree_impl()->property_trees()->needs_rebuild = true;
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
  LayerImpl* child1 = root->test_properties()->children[0];

  // In this test we don't want the above tree manipulation to be considered
  // part of the same frame.
  ClearDamageForAllSurfaces(root);
  {
    std::unique_ptr<LayerImpl> child2 =
        LayerImpl::Create(host_impl_.active_tree(), 3);
    child2->SetPosition(gfx::PointF(400.f, 380.f));
    child2->SetBounds(gfx::Size(6, 8));
    child2->SetDrawsContent(true);
    root->test_properties()->AddChild(std::move(child2));
  }
  LayerImpl* child2 = root->test_properties()->children[1];
  root->layer_tree_impl()->property_trees()->needs_rebuild = true;
  EmulateDrawingOneFrame(root);

  // Damaging two layers simultaneously should cause combined damage.
  // - child1 update rect in surface space: gfx::Rect(100, 100, 1, 2);
  // - child2 update rect in surface space: gfx::Rect(400, 380, 3, 4);
  ClearDamageForAllSurfaces(root);
  child1->SetUpdateRect(gfx::Rect(1, 2));
  child2->SetUpdateRect(gfx::Rect(3, 4));
  root->layer_tree_impl()->property_trees()->needs_rebuild = true;
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
  LayerImpl* child1 = root->test_properties()->children[0];
  LayerImpl* child2 = root->test_properties()->children[1];
  LayerImpl* grand_child1 =
      root->test_properties()->children[0]->test_properties()->children[0];
  child2->test_properties()->force_render_surface = true;
  grand_child1->test_properties()->force_render_surface = true;
  root->layer_tree_impl()->property_trees()->needs_rebuild = true;
  EmulateDrawingOneFrame(root);
  gfx::Rect child_damage_rect;
  gfx::Rect root_damage_rect;

  // CASE 1: Damage to a descendant surface should propagate properly to
  //         ancestor surface.
  ClearDamageForAllSurfaces(root);
  root->layer_tree_impl()->SetOpacityMutated(grand_child1->element_id(), 0.5f);
  EmulateDrawingOneFrame(root);
  EXPECT_TRUE(GetRenderSurface(child1)->damage_tracker()->GetDamageRectIfValid(
      &child_damage_rect));
  EXPECT_TRUE(GetRenderSurface(root)->damage_tracker()->GetDamageRectIfValid(
      &root_damage_rect));
  EXPECT_EQ(gfx::Rect(200, 200, 6, 8).ToString(), child_damage_rect.ToString());
  EXPECT_EQ(gfx::Rect(300, 300, 6, 8).ToString(), root_damage_rect.ToString());
  EXPECT_TRUE(GetRenderSurface(root)
                  ->damage_tracker()
                  ->has_damage_from_contributing_content());
  EXPECT_TRUE(GetRenderSurface(child1)
                  ->damage_tracker()
                  ->has_damage_from_contributing_content());
  EXPECT_FALSE(GetRenderSurface(child2)
                   ->damage_tracker()
                   ->has_damage_from_contributing_content());
  EXPECT_FALSE(GetRenderSurface(grand_child1)
                   ->damage_tracker()
                   ->has_damage_from_contributing_content());

  // CASE 2: Same as previous case, but with additional damage elsewhere that
  //         should be properly unioned.
  // - child1 surface damage in root surface space:
  //   gfx::Rect(300, 300, 6, 8);
  // - child2 damage in root surface space:
  //   gfx::Rect(11, 11, 18, 18);
  ClearDamageForAllSurfaces(root);
  root->layer_tree_impl()->SetOpacityMutated(grand_child1->element_id(), 0.7f);
  root->layer_tree_impl()->SetOpacityMutated(child2->element_id(), 0.7f);
  EmulateDrawingOneFrame(root);
  EXPECT_TRUE(GetRenderSurface(child1)->damage_tracker()->GetDamageRectIfValid(
      &child_damage_rect));
  EXPECT_TRUE(GetRenderSurface(root)->damage_tracker()->GetDamageRectIfValid(
      &root_damage_rect));
  EXPECT_EQ(gfx::Rect(200, 200, 6, 8).ToString(), child_damage_rect.ToString());
  EXPECT_EQ(gfx::Rect(11, 11, 295, 297).ToString(),
            root_damage_rect.ToString());
  EXPECT_TRUE(GetRenderSurface(root)
                  ->damage_tracker()
                  ->has_damage_from_contributing_content());
  EXPECT_TRUE(GetRenderSurface(child1)
                  ->damage_tracker()
                  ->has_damage_from_contributing_content());
  EXPECT_FALSE(GetRenderSurface(child2)
                   ->damage_tracker()
                   ->has_damage_from_contributing_content());
  EXPECT_FALSE(GetRenderSurface(grand_child1)
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
  LayerImpl* child1 = root->test_properties()->children[0];
  LayerImpl* grand_child1 =
      root->test_properties()->children[0]->test_properties()->children[0];
  gfx::Rect child_damage_rect;
  gfx::Rect root_damage_rect;

  ClearDamageForAllSurfaces(root);
  grand_child1->SetPosition(gfx::PointF(195.f, 205.f));
  root->layer_tree_impl()->property_trees()->needs_rebuild = true;
  EmulateDrawingOneFrame(root);
  EXPECT_TRUE(GetRenderSurface(child1)->damage_tracker()->GetDamageRectIfValid(
      &child_damage_rect));
  EXPECT_TRUE(GetRenderSurface(root)->damage_tracker()->GetDamageRectIfValid(
      &root_damage_rect));

  // The new surface bounds should be damaged entirely, even though only one of
  // the layers changed.
  EXPECT_EQ(gfx::Rect(190, 190, 11, 23).ToString(),
            child_damage_rect.ToString());

  // Damage to the root surface should be the union of child1's *entire* render
  // surface (in target space), and its old exposed area (also in target
  // space).
  EXPECT_EQ(gfx::Rect(290, 290, 16, 23).ToString(),
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
  // the entire child1 surface, but the damage tracker still needs the correct
  // logic to compute the exposed region on the root surface.

  // TODO(shawnsingh): the expectations of this test case should change when we
  // add support for a unique scissor_rect per RenderSurface. In that case, the
  // child1 surface should be completely unchanged, since we are only
  // transforming it, while the root surface would be damaged appropriately.

  LayerImpl* root = CreateAndSetUpTestTreeWithTwoSurfaces();
  LayerImpl* child1 = root->test_properties()->children[0];
  gfx::Rect child_damage_rect;
  gfx::Rect root_damage_rect;

  ClearDamageForAllSurfaces(root);
  gfx::Transform translation;
  translation.Translate(-50.f, -50.f);
  root->layer_tree_impl()->SetTransformMutated(child1->element_id(),
                                               translation);
  EmulateDrawingOneFrame(root);
  EXPECT_TRUE(GetRenderSurface(child1)->damage_tracker()->GetDamageRectIfValid(
      &child_damage_rect));
  EXPECT_TRUE(GetRenderSurface(root)->damage_tracker()->GetDamageRectIfValid(
      &root_damage_rect));

  // The new surface bounds should be damaged entirely.
  EXPECT_EQ(gfx::Rect(190, 190, 16, 18).ToString(),
            child_damage_rect.ToString());

  // The entire child1 surface and the old exposed child1 surface should damage
  // the root surface.
  //  - old child1 surface in target space: gfx::Rect(290, 290, 16, 18)
  //  - new child1 surface in target space: gfx::Rect(240, 240, 16, 18)
  EXPECT_EQ(gfx::Rect(240, 240, 66, 68).ToString(),
            root_damage_rect.ToString());

  EXPECT_TRUE(GetRenderSurface(root)
                  ->damage_tracker()
                  ->has_damage_from_contributing_content());
  EXPECT_FALSE(GetRenderSurface(child1)
                   ->damage_tracker()
                   ->has_damage_from_contributing_content());
}

TEST_F(DamageTrackerTest, VerifyDamageForAddingAndRemovingRenderSurfaces) {
  LayerImpl* root = CreateAndSetUpTestTreeWithTwoSurfaces();
  LayerImpl* child1 = root->test_properties()->children[0];
  gfx::Rect child_damage_rect;
  gfx::Rect root_damage_rect;

  // CASE 1: If a descendant surface disappears, its entire old area becomes
  //         exposed.
  ClearDamageForAllSurfaces(root);
  child1->test_properties()->force_render_surface = false;
  root->layer_tree_impl()->property_trees()->needs_rebuild = true;
  EmulateDrawingOneFrame(root);

  // Sanity check that there is only one surface now.
  ASSERT_EQ(GetRenderSurface(child1), GetRenderSurface(root));
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
  root->layer_tree_impl()->property_trees()->needs_rebuild = true;
  EmulateDrawingOneFrame(root);
  EXPECT_TRUE(GetRenderSurface(root)->damage_tracker()->GetDamageRectIfValid(
      &root_damage_rect));
  EXPECT_TRUE(root_damage_rect.IsEmpty());

  // Then change the tree so that the render surface is added back.
  ClearDamageForAllSurfaces(root);
  child1->test_properties()->force_render_surface = true;

  root->layer_tree_impl()->property_trees()->needs_rebuild = true;
  EmulateDrawingOneFrame(root);

  // Sanity check that there is a new surface now.
  ASSERT_TRUE(GetRenderSurface(child1));
  EXPECT_EQ(3, GetRenderSurface(root)->num_contributors());
  EXPECT_EQ(2, GetRenderSurface(child1)->num_contributors());

  EXPECT_TRUE(GetRenderSurface(child1)->damage_tracker()->GetDamageRectIfValid(
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
  LayerImpl* child1 = root->test_properties()->children[0];
  gfx::Rect child_damage_rect;
  gfx::Rect root_damage_rect;

  // CASE 1: If nothing changes, the damage rect should be empty.
  //
  ClearDamageForAllSurfaces(root);
  root->layer_tree_impl()->property_trees()->needs_rebuild = true;
  EmulateDrawingOneFrame(root);
  EXPECT_TRUE(GetRenderSurface(child1)->damage_tracker()->GetDamageRectIfValid(
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
  root->layer_tree_impl()->property_trees()->needs_rebuild = true;
  EmulateDrawingOneFrame(root);
  EXPECT_TRUE(GetRenderSurface(child1)->damage_tracker()->GetDamageRectIfValid(
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
  LayerImpl* child1 = root->test_properties()->children[0];
  gfx::Rect child_damage_rect;
  gfx::Rect root_damage_rect;

  // In our specific tree, the update rect of child1 should not cause any
  // damage to any surface because it does not actually draw content.
  ClearDamageForAllSurfaces(root);
  child1->SetUpdateRect(gfx::Rect(1, 2));
  root->layer_tree_impl()->property_trees()->needs_rebuild = true;
  EmulateDrawingOneFrame(root);
  EXPECT_TRUE(GetRenderSurface(child1)->damage_tracker()->GetDamageRectIfValid(
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
  LayerImpl* child = root->test_properties()->children[0];

  // In the current implementation of the damage tracker, changes to mask
  // layers should damage the entire corresponding surface.

  ClearDamageForAllSurfaces(root);

  // Set up the mask layer.
  {
    std::unique_ptr<LayerImpl> mask_layer =
        LayerImpl::Create(host_impl_.active_tree(), 3);
    mask_layer->SetPosition(child->position());
    mask_layer->SetBounds(child->bounds());
    child->test_properties()->SetMaskLayer(std::move(mask_layer));
    child->test_properties()->force_render_surface = true;
  }
  LayerImpl* mask_layer = child->test_properties()->mask_layer;

  // Add opacity and a grand_child so that the render surface persists even
  // after we remove the mask.
  {
    std::unique_ptr<LayerImpl> grand_child =
        LayerImpl::Create(host_impl_.active_tree(), 4);
    grand_child->SetPosition(gfx::PointF(2.f, 2.f));
    grand_child->SetBounds(gfx::Size(2, 2));
    grand_child->SetDrawsContent(true);
    child->test_properties()->AddChild(std::move(grand_child));
  }
  root->layer_tree_impl()->property_trees()->needs_rebuild = true;
  EmulateDrawingOneFrame(root);

  // CASE 1: the update_rect on a mask layer should damage the entire target
  //         surface.
  ClearDamageForAllSurfaces(root);
  mask_layer->SetUpdateRect(gfx::Rect(1, 2, 3, 4));
  root->layer_tree_impl()->property_trees()->needs_rebuild = true;
  EmulateDrawingOneFrame(root);
  gfx::Rect child_damage_rect;
  EXPECT_TRUE(GetRenderSurface(child)->damage_tracker()->GetDamageRectIfValid(
      &child_damage_rect));
  EXPECT_EQ(gfx::Rect(30, 30).ToString(), child_damage_rect.ToString());

  EXPECT_TRUE(GetRenderSurface(root)
                  ->damage_tracker()
                  ->has_damage_from_contributing_content());
  EXPECT_FALSE(GetRenderSurface(child)
                   ->damage_tracker()
                   ->has_damage_from_contributing_content());

  // CASE 2: a property change on the mask layer should damage the entire
  //         target surface.

  // Advance one frame without damage so that we know the damage rect is not
  // leftover from the previous case.
  ClearDamageForAllSurfaces(root);
  root->layer_tree_impl()->property_trees()->needs_rebuild = true;
  EmulateDrawingOneFrame(root);
  EXPECT_TRUE(GetRenderSurface(child)->damage_tracker()->GetDamageRectIfValid(
      &child_damage_rect));
  EXPECT_TRUE(child_damage_rect.IsEmpty());

  // Then test the property change.
  ClearDamageForAllSurfaces(root);
  mask_layer->NoteLayerPropertyChanged();

  root->layer_tree_impl()->property_trees()->needs_rebuild = true;
  EmulateDrawingOneFrame(root);
  EXPECT_TRUE(GetRenderSurface(child)->damage_tracker()->GetDamageRectIfValid(
      &child_damage_rect));
  EXPECT_EQ(gfx::Rect(30, 30).ToString(), child_damage_rect.ToString());

  EXPECT_TRUE(GetRenderSurface(root)
                  ->damage_tracker()
                  ->has_damage_from_contributing_content());
  EXPECT_FALSE(GetRenderSurface(child)
                   ->damage_tracker()
                   ->has_damage_from_contributing_content());

  // CASE 3: removing the mask also damages the entire target surface.
  //

  // Advance one frame without damage so that we know the damage rect is not
  // leftover from the previous case.
  ClearDamageForAllSurfaces(root);
  root->layer_tree_impl()->property_trees()->needs_rebuild = true;
  EmulateDrawingOneFrame(root);
  EXPECT_TRUE(GetRenderSurface(child)->damage_tracker()->GetDamageRectIfValid(
      &child_damage_rect));
  EXPECT_TRUE(child_damage_rect.IsEmpty());

  // Then test mask removal.
  ClearDamageForAllSurfaces(root);
  child->test_properties()->SetMaskLayer(nullptr);
  child->NoteLayerPropertyChanged();
  ASSERT_TRUE(child->LayerPropertyChanged());
  root->layer_tree_impl()->property_trees()->needs_rebuild = true;
  EmulateDrawingOneFrame(root);

  // Sanity check that a render surface still exists.
  ASSERT_TRUE(GetRenderSurface(child));

  EXPECT_TRUE(GetRenderSurface(child)->damage_tracker()->GetDamageRectIfValid(
      &child_damage_rect));
  EXPECT_EQ(gfx::Rect(30, 30).ToString(), child_damage_rect.ToString());

  EXPECT_TRUE(GetRenderSurface(root)
                  ->damage_tracker()
                  ->has_damage_from_contributing_content());
}

TEST_F(DamageTrackerTest, DamageWhenAddedExternally) {
  LayerImpl* root = CreateAndSetUpTestTreeWithOneSurface();
  LayerImpl* child = root->test_properties()->children[0];

  // Case 1: This test ensures that when the tracker is given damage, that
  //         it is included with any other partial damage.
  //
  ClearDamageForAllSurfaces(root);
  child->SetUpdateRect(gfx::Rect(10, 11, 12, 13));
  GetRenderSurface(root)->damage_tracker()->AddDamageNextUpdate(
      gfx::Rect(15, 16, 32, 33));
  root->layer_tree_impl()->property_trees()->needs_rebuild = true;
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
  root->layer_tree_impl()->property_trees()->needs_rebuild = true;
  EmulateDrawingOneFrame(root);
  EXPECT_TRUE(GetRenderSurface(root)->damage_tracker()->GetDamageRectIfValid(
      &root_damage_rect));
  EXPECT_EQ(gfx::Rect(30, 31, 14, 15).ToString(), root_damage_rect.ToString());
  EXPECT_FALSE(GetRenderSurface(root)
                   ->damage_tracker()
                   ->has_damage_from_contributing_content());
}

TEST_F(DamageTrackerTest, VerifyDamageWithNoContributingLayers) {
  std::unique_ptr<LayerImpl> root =
      LayerImpl::Create(host_impl_.active_tree(), 1);
  root->test_properties()->force_render_surface = true;
  host_impl_.active_tree()->SetRootLayerForTesting(std::move(root));
  LayerImpl* root_ptr = host_impl_.active_tree()->root_layer_for_testing();
  root_ptr->layer_tree_impl()->property_trees()->needs_rebuild = true;
  EmulateDrawingOneFrame(root_ptr);

  DCHECK_EQ(GetRenderSurface(root_ptr), root_ptr->render_target());
  RenderSurfaceImpl* target_surface = GetRenderSurface(root_ptr);
  gfx::Rect damage_rect;
  EXPECT_TRUE(
      target_surface->damage_tracker()->GetDamageRectIfValid(&damage_rect));
  EXPECT_TRUE(damage_rect.IsEmpty());
  EXPECT_FALSE(GetRenderSurface(root_ptr)
                   ->damage_tracker()
                   ->has_damage_from_contributing_content());
}

TEST_F(DamageTrackerTest, VerifyDamageAccumulatesUntilReset) {
  // If damage is not cleared, it should accumulate.

  LayerImpl* root = CreateAndSetUpTestTreeWithOneSurface();
  LayerImpl* child = root->test_properties()->children[0];

  ClearDamageForAllSurfaces(root);
  child->SetUpdateRect(gfx::Rect(10.f, 11.f, 1.f, 2.f));
  root->layer_tree_impl()->property_trees()->needs_rebuild = true;
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
  child->SetUpdateRect(gfx::Rect(20, 25, 1, 2));
  root->layer_tree_impl()->property_trees()->needs_rebuild = true;
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
  root->layer_tree_impl()->property_trees()->needs_rebuild = true;
  EmulateDrawingOneFrame(root);
  EXPECT_TRUE(GetRenderSurface(root)->damage_tracker()->GetDamageRectIfValid(
      &root_damage_rect));
  EXPECT_TRUE(root_damage_rect.IsEmpty());
  EXPECT_FALSE(GetRenderSurface(root)
                   ->damage_tracker()
                   ->has_damage_from_contributing_content());
}

TEST_F(DamageTrackerTest, HugeDamageRect) {
  // This number is so large that we start losting floating point accuracy.
  const int kBigNumber = 900000000;
  // Walk over a range to find floating point inaccuracy boundaries that move
  // toward the wrong direction.
  const int kRange = 5000;

  for (int i = 0; i < kRange; ++i) {
    LayerImpl* root = CreateAndSetUpTestTreeWithOneSurface();
    LayerImpl* child = root->test_properties()->children[0];

    gfx::Transform transform;
    transform.Translate(-kBigNumber, -kBigNumber);

    // The child layer covers (0, 0, i, i) of the viewport,
    // but has a huge negative position.
    child->SetPosition(gfx::PointF());
    child->SetBounds(gfx::Size(kBigNumber + i, kBigNumber + i));
    child->test_properties()->transform = transform;
    root->layer_tree_impl()->property_trees()->needs_rebuild = true;
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
  LayerImpl* child1 = root->test_properties()->children[0];
  LayerImpl* child2 = root->test_properties()->children[1];

  // Really far left.
  child1->SetPosition(gfx::PointF(std::numeric_limits<int>::min() + 100, 0));
  child1->SetBounds(gfx::Size(1, 1));

  // Really far right.
  child2->SetPosition(gfx::PointF(std::numeric_limits<int>::max() - 100, 0));
  child2->SetBounds(gfx::Size(1, 1));

  root->layer_tree_impl()->property_trees()->needs_rebuild = true;
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

TEST_F(DamageTrackerTest, DamageRectTooBigWithFilter) {
  LayerImpl* root = CreateAndSetUpTestTreeWithOneSurface(2);
  LayerImpl* child1 = root->test_properties()->children[0];
  LayerImpl* child2 = root->test_properties()->children[1];

  FilterOperations filters;
  filters.Append(FilterOperation::CreateBlurFilter(5.f));
  root->SetDrawsContent(true);
  root->test_properties()->backdrop_filters = filters;

  // Really far left.
  child1->SetPosition(gfx::PointF(std::numeric_limits<int>::min() + 100, 0));
  child1->SetBounds(gfx::Size(1, 1));

  // Really far right.
  child2->SetPosition(gfx::PointF(std::numeric_limits<int>::max() - 100, 0));
  child2->SetBounds(gfx::Size(1, 1));

  root->layer_tree_impl()->property_trees()->needs_rebuild = true;
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
  LayerImpl* root = CreateAndSetUpTestTreeWithTwoSurfaces();
  LayerImpl* child1 = root->test_properties()->children[0];
  LayerImpl* grandchild1 = child1->test_properties()->children[0];
  LayerImpl* grandchild2 = child1->test_properties()->children[1];

  // Really far left.
  grandchild1->SetPosition(
      gfx::PointF(std::numeric_limits<int>::min() + 500, 0));
  grandchild1->SetBounds(gfx::Size(1, 1));
  grandchild1->SetDrawsContent(true);

  // Really far right.
  grandchild2->SetPosition(
      gfx::PointF(std::numeric_limits<int>::max() - 500, 0));
  grandchild2->SetBounds(gfx::Size(1, 1));
  grandchild2->SetDrawsContent(true);

  root->layer_tree_impl()->property_trees()->needs_rebuild = true;
  float device_scale_factor = 1.f;
  RenderSurfaceList render_surface_list;
  ExecuteCalculateDrawProperties(root, device_scale_factor,
                                 &render_surface_list);
  // Avoid the descendant-only property change path that skips unioning damage
  // from descendant layers.
  GetRenderSurface(child1)->NoteAncestorPropertyChanged();
  DamageTracker::UpdateDamageTracking(host_impl_.active_tree(),
                                      render_surface_list);

  // The expected damage would be too large to store in a gfx::Rect, so we
  // should damage everything on child1.
  gfx::Rect damage_rect;
  EXPECT_FALSE(GetRenderSurface(child1)->damage_tracker()->GetDamageRectIfValid(
      &damage_rect));
  EXPECT_EQ(GetRenderSurface(child1)->content_rect(),
            GetRenderSurface(child1)->GetDamageRect());

  // However, the root should just use the child1 render surface's content rect
  // as damage.
  ASSERT_TRUE(GetRenderSurface(root)->damage_tracker()->GetDamageRectIfValid(
      &damage_rect));
  EXPECT_TRUE(damage_rect.Contains(GetRenderSurface(root)->content_rect()));
  EXPECT_TRUE(damage_rect.Contains(
      gfx::ToEnclosingRect(GetRenderSurface(child1)->DrawableContentRect())));
  EXPECT_EQ(damage_rect, GetRenderSurface(root)->GetDamageRect());

  EXPECT_TRUE(GetRenderSurface(root)
                  ->damage_tracker()
                  ->has_damage_from_contributing_content());
  EXPECT_TRUE(GetRenderSurface(child1)
                  ->damage_tracker()
                  ->has_damage_from_contributing_content());

  // Add new damage, without changing properties, which goes down a different
  // path in the damage tracker.
  root->layer_tree_impl()->ResetAllChangeTracking();
  grandchild1->AddDamageRect(gfx::Rect(grandchild1->bounds()));
  grandchild2->AddDamageRect(gfx::Rect(grandchild1->bounds()));

  // Recompute all damage / properties.
  render_surface_list.clear();
  ExecuteCalculateDrawProperties(root, device_scale_factor,
                                 &render_surface_list);
  DamageTracker::UpdateDamageTracking(host_impl_.active_tree(),
                                      render_surface_list);

  // Child1 should still not have a valid rect, since the union of the damage of
  // its children is not representable by a single rect.
  EXPECT_FALSE(GetRenderSurface(child1)->damage_tracker()->GetDamageRectIfValid(
      &damage_rect));
  EXPECT_EQ(GetRenderSurface(child1)->content_rect(),
            GetRenderSurface(child1)->GetDamageRect());

  // Root should have valid damage and contain both its content rect and the
  // drawable content rect of child1.
  ASSERT_TRUE(GetRenderSurface(root)->damage_tracker()->GetDamageRectIfValid(
      &damage_rect));
  EXPECT_TRUE(damage_rect.Contains(GetRenderSurface(root)->content_rect()));
  EXPECT_TRUE(damage_rect.Contains(
      gfx::ToEnclosingRect(GetRenderSurface(child1)->DrawableContentRect())));
  EXPECT_EQ(damage_rect, GetRenderSurface(root)->GetDamageRect());

  EXPECT_TRUE(GetRenderSurface(root)
                  ->damage_tracker()
                  ->has_damage_from_contributing_content());
  EXPECT_TRUE(GetRenderSurface(child1)
                  ->damage_tracker()
                  ->has_damage_from_contributing_content());
}

TEST_F(DamageTrackerTest, DamageRectTooBigInRenderSurfaceWithFilter) {
  LayerImpl* root = CreateAndSetUpTestTreeWithTwoSurfaces();
  LayerImpl* child1 = root->test_properties()->children[0];
  LayerImpl* grandchild1 = child1->test_properties()->children[0];
  LayerImpl* grandchild2 = child1->test_properties()->children[1];

  // Set up a moving pixels filter on the child.
  FilterOperations filters;
  filters.Append(FilterOperation::CreateBlurFilter(5.f));
  child1->SetDrawsContent(true);
  child1->test_properties()->backdrop_filters = filters;

  // Really far left.
  grandchild1->SetPosition(
      gfx::PointF(std::numeric_limits<int>::min() + 500, 0));
  grandchild1->SetBounds(gfx::Size(1, 1));
  grandchild1->SetDrawsContent(true);

  // Really far right.
  grandchild2->SetPosition(
      gfx::PointF(std::numeric_limits<int>::max() - 500, 0));
  grandchild2->SetBounds(gfx::Size(1, 1));
  grandchild2->SetDrawsContent(true);

  root->layer_tree_impl()->property_trees()->needs_rebuild = true;
  float device_scale_factor = 1.f;
  RenderSurfaceList render_surface_list;
  ExecuteCalculateDrawProperties(root, device_scale_factor,
                                 &render_surface_list);
  // Avoid the descendant-only property change path that skips unioning damage
  // from descendant layers.
  GetRenderSurface(child1)->NoteAncestorPropertyChanged();
  DamageTracker::UpdateDamageTracking(host_impl_.active_tree(),
                                      render_surface_list);

  // The expected damage would be too large to store in a gfx::Rect, so we
  // should damage everything on child1.
  gfx::Rect damage_rect;
  EXPECT_FALSE(GetRenderSurface(child1)->damage_tracker()->GetDamageRectIfValid(
      &damage_rect));
  EXPECT_EQ(GetRenderSurface(child1)->content_rect(),
            GetRenderSurface(child1)->GetDamageRect());

  // However, the root should just use the child1 render surface's content rect
  // as damage.
  ASSERT_TRUE(GetRenderSurface(root)->damage_tracker()->GetDamageRectIfValid(
      &damage_rect));
  EXPECT_TRUE(damage_rect.Contains(GetRenderSurface(root)->content_rect()));
  EXPECT_TRUE(damage_rect.Contains(
      gfx::ToEnclosingRect(GetRenderSurface(child1)->DrawableContentRect())));
  EXPECT_EQ(damage_rect, GetRenderSurface(root)->GetDamageRect());

  EXPECT_TRUE(GetRenderSurface(root)
                  ->damage_tracker()
                  ->has_damage_from_contributing_content());
  EXPECT_TRUE(GetRenderSurface(child1)
                  ->damage_tracker()
                  ->has_damage_from_contributing_content());

  // Add new damage, without changing properties, which goes down a different
  // path in the damage tracker.
  root->layer_tree_impl()->ResetAllChangeTracking();
  grandchild1->AddDamageRect(gfx::Rect(grandchild1->bounds()));
  grandchild2->AddDamageRect(gfx::Rect(grandchild1->bounds()));

  // Recompute all damage / properties.
  render_surface_list.clear();
  ExecuteCalculateDrawProperties(root, device_scale_factor,
                                 &render_surface_list);
  DamageTracker::UpdateDamageTracking(host_impl_.active_tree(),
                                      render_surface_list);

  // Child1 should still not have a valid rect, since the union of the damage of
  // its children is not representable by a single rect.
  EXPECT_FALSE(GetRenderSurface(child1)->damage_tracker()->GetDamageRectIfValid(
      &damage_rect));
  EXPECT_EQ(GetRenderSurface(child1)->content_rect(),
            GetRenderSurface(child1)->GetDamageRect());

  // Root should have valid damage and contain both its content rect and the
  // drawable content rect of child1.
  ASSERT_TRUE(GetRenderSurface(root)->damage_tracker()->GetDamageRectIfValid(
      &damage_rect));
  EXPECT_TRUE(damage_rect.Contains(GetRenderSurface(root)->content_rect()));
  EXPECT_TRUE(damage_rect.Contains(
      gfx::ToEnclosingRect(GetRenderSurface(child1)->DrawableContentRect())));
  EXPECT_EQ(damage_rect, GetRenderSurface(root)->GetDamageRect());

  EXPECT_TRUE(GetRenderSurface(root)
                  ->damage_tracker()
                  ->has_damage_from_contributing_content());
  EXPECT_TRUE(GetRenderSurface(child1)
                  ->damage_tracker()
                  ->has_damage_from_contributing_content());
}

}  // namespace
}  // namespace cc
