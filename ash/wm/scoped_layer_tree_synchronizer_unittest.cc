// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/scoped_layer_tree_synchronizer.h"

#include <cmath>
#include <memory>
#include <tuple>
#include <utility>

#include "ash/test/ash_test_base.h"
#include "base/functional/callback_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/test/test_windows.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/geometry/rrect_f.h"
#include "ui/wm/core/window_util.h"

namespace ash {
namespace {

constexpr gfx::Rect kRootBounds(0, 0, 50, 50);
constexpr gfx::RoundedCornersF kRootLayerRadii(10);
constexpr gfx::RoundedCornersF kZeroRadii(0);

std::unique_ptr<ui::Layer> CreateLayer() {
  return std::make_unique<ui::Layer>();
}

using BoundsWithRoundedCorners = std::pair<gfx::Rect, gfx::RoundedCornersF>;
using UpdatingScopedLayerTreeSynchronizerTestParam =
    std::tuple</*root_layer_bounds=*/BoundsWithRoundedCorners,
               /*child_layer_bounds=*/BoundsWithRoundedCorners,
               /*expected_child_layer_radii=*/gfx::RoundedCornersF>;

class UpdatingScopedLayerTreeSynchronizerTest
    : public testing::TestWithParam<
          UpdatingScopedLayerTreeSynchronizerTestParam> {
 public:
  UpdatingScopedLayerTreeSynchronizerTest() = default;

  UpdatingScopedLayerTreeSynchronizerTest(
      const UpdatingScopedLayerTreeSynchronizerTest&) = delete;
  UpdatingScopedLayerTreeSynchronizerTest& operator=(
      const UpdatingScopedLayerTreeSynchronizerTest&) = delete;

  ~UpdatingScopedLayerTreeSynchronizerTest() override = default;

 protected:
  gfx::Rect root_layer_bounds() const { return std::get<0>(GetParam()).first; }
  gfx::RoundedCornersF root_layer_radii() const {
    return std::get<0>(GetParam()).second;
  }

  gfx::Rect child_layer_bounds() const { return std::get<1>(GetParam()).first; }
  gfx::RoundedCornersF child_layer_radii() const {
    return std::get<1>(GetParam()).second;
  }
  gfx::RoundedCornersF expected_child_layer_radii() const {
    return std::get<2>(GetParam());
  }
};

TEST_P(UpdatingScopedLayerTreeSynchronizerTest, OverlappingCorners) {
  // Layer Tree:
  // +root
  // +--child_layer
  std::unique_ptr<ui::Layer> root = CreateLayer();
  std::unique_ptr<ui::Layer> child_layer = CreateLayer();

  // Set up layer tree.
  root->Add(child_layer.get());

  // Set up layer bounds.
  root->SetBounds(root_layer_bounds());
  child_layer->SetBounds(child_layer_bounds());

  // Set layer properties.
  root->SetRoundedCornerRadius(root_layer_radii());
  root->SetIsFastRoundedCorner(true);

  child_layer->SetRoundedCornerRadius(child_layer_radii());
  child_layer->SetIsFastRoundedCorner(true);

  auto layer_tree_synchronizer = std::make_unique<ScopedLayerTreeSynchronizer>(
      root.get(), /*restore_tree=*/false);

  ASSERT_EQ(child_layer->rounded_corner_radii(), child_layer_radii());

  layer_tree_synchronizer->SynchronizeRoundedCorners(
      root.get(),
      gfx::RRectF(gfx::RectF(root_layer_bounds()), root_layer_radii()));

  // Root layer bounds and radii should be unaffected.
  ASSERT_EQ(root->rounded_corner_radii(), root_layer_radii());
  ASSERT_EQ(root->bounds(), root_layer_bounds());
  EXPECT_EQ(child_layer->rounded_corner_radii(), expected_child_layer_radii());
}

UpdatingScopedLayerTreeSynchronizerTestParam MakeTestParams(
    const gfx::Rect& root_layer_bounds,
    const gfx::RoundedCornersF& root_layer_radii,
    const gfx::Rect& child_layer_bounds,
    const gfx::RoundedCornersF& child_layer_radii,
    const gfx::RoundedCornersF& expected_child_layer_radii) {
  return std::make_tuple(std::make_pair(root_layer_bounds, root_layer_radii),
                         std::make_pair(child_layer_bounds, child_layer_radii),
                         expected_child_layer_radii);
}

INSTANTIATE_TEST_SUITE_P(
    SanityTests,
    UpdatingScopedLayerTreeSynchronizerTest,
    testing::Values(
        // Child layer has zero bounds.
        MakeTestParams(kRootBounds,
                       kZeroRadii,
                       gfx::Rect(),
                       kZeroRadii,
                       kZeroRadii),
        // Parent layer has zero bounds.
        MakeTestParams(gfx::Rect(),
                       kZeroRadii,
                       gfx::Rect(5, 5, 10, 10),
                       kZeroRadii,
                       kZeroRadii),
        // If child layer has no rounded corners, we do not update those layers.
        MakeTestParams(kRootBounds,
                       gfx::RoundedCornersF(5),
                       gfx::Rect(0, 0, 50, 50),
                       kZeroRadii,
                       kZeroRadii)));

INSTANTIATE_TEST_SUITE_P(
    RootLayerHasNoRoundedCorners,
    UpdatingScopedLayerTreeSynchronizerTest,
    testing::Values(
        // Child layer is the same size as the root layer.
        MakeTestParams(kRootBounds,
                       kZeroRadii,
                       gfx::Rect(0, 0, 50, 50),
                       kZeroRadii,
                       kZeroRadii),
        // Child layer (has rounded corners) is the same size as the root layer.
        MakeTestParams(kRootBounds,
                       kZeroRadii,
                       gfx::Rect(0, 0, 50, 50),
                       gfx::RoundedCornersF(5),
                       gfx::RoundedCornersF(5)),
        // Child layer (has rounded corners) fits inside the root layer.
        MakeTestParams(kRootBounds,
                       kZeroRadii,
                       gfx::Rect(10, 10, 20, 0),
                       gfx::RoundedCornersF(5),
                       gfx::RoundedCornersF(5))));

INSTANTIATE_TEST_SUITE_P(
    RootLayerAndChildLayerHaveRoundedCorners,
    UpdatingScopedLayerTreeSynchronizerTest,
    testing::Values(
        // Root and child layer overlaps completely.
        MakeTestParams(kRootBounds,
                       gfx::RoundedCornersF(10),
                       gfx::Rect(0, 0, 50, 50),
                       gfx::RoundedCornersF(10),
                       gfx::RoundedCornersF(10)),
        // Child layer fits inside the root.
        MakeTestParams(kRootBounds,
                       gfx::RoundedCornersF(10),
                       gfx::Rect(5, 5, 10, 10),
                       gfx::RoundedCornersF(2),
                       gfx::RoundedCornersF(2)),
        // Corners do not intersect but child layer corners
        // are outside root layer rounded bounds. (Test 1)
        MakeTestParams(kRootBounds,
                       gfx::RoundedCornersF(10),
                       gfx::Rect(0, 0, 50, 50),
                       gfx::RoundedCornersF(9),
                       gfx::RoundedCornersF(10)),
        // Corners do not intersect but child layer corners
        // are outside root layer rounded bounds. (Test 2)
        MakeTestParams(kRootBounds,
                       gfx::RoundedCornersF(20),
                       gfx::Rect(5, 0, 40, 50),
                       gfx::RoundedCornersF(5),
                       gfx::RoundedCornersF(20)),
        // Corners intersect. (Test 1)
        MakeTestParams(kRootBounds,
                       gfx::RoundedCornersF(20),
                       gfx::Rect(3, 5, 44, 40),
                       gfx::RoundedCornersF(5),
                       gfx::RoundedCornersF(20)),
        // Corners partially intersect.
        MakeTestParams(kRootBounds,
                       gfx::RoundedCornersF(20),
                       gfx::Rect(4, 5, 43, 40),
                       gfx::RoundedCornersF(5),
                       gfx::RoundedCornersF(5, 20, 20, 5))));

class ScopedLayerTreeSynchronizerTest : public testing::TestWithParam<bool> {
 public:
  ScopedLayerTreeSynchronizerTest() = default;

  ScopedLayerTreeSynchronizerTest(const ScopedLayerTreeSynchronizerTest&) =
      delete;
  ScopedLayerTreeSynchronizerTest& operator=(
      const ScopedLayerTreeSynchronizerTest&) = delete;

  ~ScopedLayerTreeSynchronizerTest() override = default;

 protected:
  bool restore_layer_tree() const { return GetParam(); }
};

TEST_P(ScopedLayerTreeSynchronizerTest, UpdatingLayerTree) {
  // Layer Tree:
  // +root         (has rounded corners)
  // +--layer1     (has intersecting rounded corners with root)
  // +----layer2   (has intersecting rounded corners with root)
  // +--layer3     (has rounded corners)
  constexpr gfx::Rect kLayer1Bounds(0, 0, 30, 30);
  constexpr gfx::Rect kLayer2Bounds(20, 20, 60, 60);
  constexpr gfx::Rect kLayer3Bounds(10, 10, 20, 20);

  constexpr gfx::RoundedCornersF kLayer1Radii(5);
  constexpr gfx::RoundedCornersF kLayer2Radii(7);
  constexpr gfx::RoundedCornersF kLayer3Radii(0);

  constexpr float kLayer2Scale = 0.5;

  std::unique_ptr<ui::Layer> root = CreateLayer();
  std::unique_ptr<ui::Layer> layer_1 = CreateLayer();
  std::unique_ptr<ui::Layer> layer_2 = CreateLayer();
  std::unique_ptr<ui::Layer> layer_3 = CreateLayer();

  // Set up layer tree.
  root->Add(layer_1.get());
  root->Add(layer_3.get());
  layer_1->Add(layer_2.get());

  // Set up layer bounds.
  root->SetBounds(kRootBounds);
  layer_1->SetBounds(kLayer1Bounds);
  layer_2->SetBounds(kLayer2Bounds);
  layer_3->SetBounds(kLayer3Bounds);

  // Set layer transform.
  gfx::Transform layer_2_transform;
  layer_2_transform.Scale(kLayer2Scale);
  layer_2->SetTransform(layer_2_transform);

  // Set layer properties.
  root->SetRoundedCornerRadius(kRootLayerRadii);
  root->SetIsFastRoundedCorner(true);

  layer_1->SetRoundedCornerRadius(kLayer1Radii);
  layer_1->SetIsFastRoundedCorner(true);

  layer_2->SetRoundedCornerRadius(kLayer2Radii);
  layer_2->SetIsFastRoundedCorner(true);

  layer_3->SetRoundedCornerRadius(kLayer3Radii);
  layer_3->SetIsFastRoundedCorner(true);

  auto layer_tree_synchronizer = std::make_unique<ScopedLayerTreeSynchronizer>(
      root.get(), restore_layer_tree());
  layer_tree_synchronizer->SynchronizeRoundedCorners(
      root.get(), gfx::RRectF(gfx::RectF(kRootBounds), kRootLayerRadii));

  EXPECT_EQ(root->rounded_corner_radii(), kRootLayerRadii);
  constexpr gfx::RoundedCornersF kUpdatedLayer1Radii =
      gfx::RoundedCornersF(10, 5, 5, 5);
  EXPECT_EQ(layer_1->rounded_corner_radii(), kUpdatedLayer1Radii);
  constexpr gfx::RoundedCornersF kUpdatedLayer2Radii =
      gfx::RoundedCornersF(7, 7, 20, 7);
  EXPECT_EQ(layer_2->rounded_corner_radii(), kUpdatedLayer2Radii);
  EXPECT_EQ(layer_3->rounded_corner_radii(), kLayer3Radii);

  layer_tree_synchronizer->Restore();

  EXPECT_EQ(root->rounded_corner_radii(), kRootLayerRadii);
  EXPECT_EQ(layer_1->rounded_corner_radii(),
            restore_layer_tree() ? kLayer1Radii : kUpdatedLayer1Radii);
  EXPECT_EQ(layer_2->rounded_corner_radii(),
            restore_layer_tree() ? kLayer2Radii : kUpdatedLayer2Radii);
  EXPECT_EQ(layer_3->rounded_corner_radii(), kLayer3Radii);
}

INSTANTIATE_TEST_SUITE_P(/* no prefix */,
                         ScopedLayerTreeSynchronizerTest,
                         testing::Bool());

using ScopedWindowTreeSynchronizerTest = AshTestBase;

TEST_F(ScopedWindowTreeSynchronizerTest, Basics) {
  // root
  // +---transient_parent
  // +------child_window
  // +---transient_window_1
  // +---transient_window_2

  std::unique_ptr<aura::Window> root(
      aura::test::CreateTestWindowWithId(0, nullptr));
  std::unique_ptr<aura::Window> transient_parent(
      aura::test::CreateTestWindowWithId(1, root.get()));
  std::unique_ptr<aura::Window> child_window(
      aura::test::CreateTestWindowWithId(2, transient_parent.get()));
  std::unique_ptr<aura::Window> transient_window_1(
      aura::test::CreateTestWindowWithId(3, root.get()));
  std::unique_ptr<aura::Window> transient_window_2(
      aura::test::CreateTestWindowWithId(4, root.get()));

  wm::AddTransientChild(transient_parent.get(), transient_window_1.get());
  wm::AddTransientChild(transient_parent.get(), transient_window_2.get());

  transient_parent->SetBounds(gfx::Rect(0, 0, 1000, 500));
  child_window->SetBounds(gfx::Rect(0, 0, 50, 50));
  transient_window_1->SetBounds(gfx::Rect(0, 0, 1000, 500));
  transient_window_2->SetBounds(gfx::Rect(20, 20, 200, 200));

  transient_parent->layer()->SetRoundedCornerRadius(gfx::RoundedCornersF(5));
  child_window->layer()->SetRoundedCornerRadius(gfx::RoundedCornersF(2));
  transient_window_1->layer()->SetRoundedCornerRadius(gfx::RoundedCornersF(5));
  transient_window_2->layer()->SetRoundedCornerRadius(gfx::RoundedCornersF());

  auto window_tree_synchronizer =
      std::make_unique<ScopedWindowTreeSynchronizer>(root.get(),
                                                     /*restore_tree=*/true);

  gfx::RRectF reference_bounds(gfx::RectF(0, 0, 1000, 500),
                               gfx::RoundedCornersF(10));
  window_tree_synchronizer->SynchronizeRoundedCorners(
      transient_parent.get(), /*consider_curvature=*/true, reference_bounds,
      base::NullCallback());

  // All the windows rooted at `transient_parent`(including transient windows)
  // should be synchronized again `reference_bounds`.
  EXPECT_EQ(transient_parent->layer()->rounded_corner_radii(),
            gfx::RoundedCornersF(10));
  EXPECT_EQ(child_window->layer()->rounded_corner_radii(),
            gfx::RoundedCornersF(10, 2, 2, 2));
  EXPECT_EQ(transient_window_1->layer()->rounded_corner_radii(),
            gfx::RoundedCornersF(10));
  EXPECT_EQ(transient_window_2->layer()->rounded_corner_radii(),
            gfx::RoundedCornersF());

  gfx::RRectF updated_reference_bounds(gfx::RectF(0, 0, 1000, 500),
                                       gfx::RoundedCornersF(15));
  window_tree_synchronizer->SynchronizeRoundedCorners(
      transient_parent.get(), /*consider_curvature=*/true,
      updated_reference_bounds, base::NullCallback());

  // All the windows rooted at `transient_parent` should now have new rounded
  // corners based on `updated_reference_bounds`.
  EXPECT_EQ(transient_parent->layer()->rounded_corner_radii(),
            gfx::RoundedCornersF(15));
  EXPECT_EQ(child_window->layer()->rounded_corner_radii(),
            gfx::RoundedCornersF(15, 2, 2, 2));
  EXPECT_EQ(transient_window_1->layer()->rounded_corner_radii(),
            gfx::RoundedCornersF(15));
  EXPECT_EQ(transient_window_2->layer()->rounded_corner_radii(),
            gfx::RoundedCornersF());

  window_tree_synchronizer->Restore();

  // All the windows should now be restored to their original state. (i.e
  // before calling SynchronizeRoundedCorners() for the first time)
  EXPECT_EQ(transient_parent->layer()->rounded_corner_radii(),
            gfx::RoundedCornersF(5));
  EXPECT_EQ(child_window->layer()->rounded_corner_radii(),
            gfx::RoundedCornersF(2));
  EXPECT_EQ(transient_window_1->layer()->rounded_corner_radii(),
            gfx::RoundedCornersF(5));
  EXPECT_EQ(transient_window_2->layer()->rounded_corner_radii(),
            gfx::RoundedCornersF());
}

}  // namespace
}  // namespace ash
