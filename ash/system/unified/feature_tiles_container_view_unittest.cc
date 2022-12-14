// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/feature_tiles_container_view.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/quick_settings_catalogs.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/unified/feature_pod_button.h"
#include "ash/system/unified/feature_pod_controller_base.h"
#include "ash/system/unified/feature_tile.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/system/unified/unified_system_tray_bubble.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "ash/test/ash_test_base.h"
#include "base/memory/weak_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/views/test/views_test_utils.h"

namespace ash {

namespace {

class MockFeaturePodController : public FeaturePodControllerBase {
 public:
  explicit MockFeaturePodController(FeatureTile::TileType type) : type_(type) {}

  MockFeaturePodController(const MockFeaturePodController&) = delete;
  MockFeaturePodController& operator=(const MockFeaturePodController&) = delete;

  ~MockFeaturePodController() override = default;

  FeaturePodButton* CreateButton() override {
    return new FeaturePodButton(/*controller=*/this);
  }

  std::unique_ptr<FeatureTile> CreateTile() override {
    auto tile = std::make_unique<FeatureTile>(
        base::BindRepeating(&FeaturePodControllerBase::OnIconPressed,
                            weak_ptr_factory_.GetWeakPtr()),
        /*togglable=*/true, type_);
    tile->SetVectorIcon(vector_icons::kDogfoodIcon);
    return tile;
  }

  QsFeatureCatalogName GetCatalogName() override {
    return QsFeatureCatalogName::kUnknown;
  }

  void OnIconPressed() override {}
  void OnLabelPressed() override {}

 private:
  FeatureTile::TileType type_;

  base::WeakPtrFactory<MockFeaturePodController> weak_ptr_factory_{this};
};

}  // namespace

class FeatureTilesContainerViewTest : public AshTestBase,
                                      public views::ViewObserver {
 public:
  FeatureTilesContainerViewTest() {
    feature_list_.InitWithFeatures(
        {features::kQsRevamp, features::kQsRevampWip}, {});
  }

  FeatureTilesContainerViewTest(const FeatureTilesContainerViewTest&) = delete;
  FeatureTilesContainerViewTest& operator=(
      const FeatureTilesContainerViewTest&) = delete;
  ~FeatureTilesContainerViewTest() override = default;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();
    GetPrimaryUnifiedSystemTray()->ShowBubble();
    container_ = std::make_unique<FeatureTilesContainerView>(
        GetPrimaryUnifiedSystemTray()
            ->bubble()
            ->unified_system_tray_controller());
    container_->AddObserver(this);
  }

  void TearDown() override {
    container_->RemoveObserver(this);
    container_.reset();
    GetPrimaryUnifiedSystemTray()->CloseBubble();
    AshTestBase::TearDown();
  }

  FeatureTilesContainerView* container() { return container_.get(); }

  int CalculateRowsFromHeight(int height) {
    return container()->CalculateRowsFromHeight(height);
  }

  int FeatureTileRowCount() { return container()->FeatureTileRowCount(); }

 private:
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<FeatureTilesContainerView> container_;
};

// Tests `CalculateRowsFromHeight` which returns the number of max displayable
// feature tile rows given the available height.
TEST_F(FeatureTilesContainerViewTest, DisplayableRows) {
  int row_height = kFeatureTileHeight;

  // Expect max number of rows even if available height could fit another row.
  EXPECT_EQ(kFeatureTileMaxRows,
            CalculateRowsFromHeight((kFeatureTileMaxRows + 1) * row_height));

  // Expect appropriate number of rows with available height.
  EXPECT_EQ(3, CalculateRowsFromHeight(3 * row_height));

  // Expect min number of rows even with zero height.
  EXPECT_EQ(kFeatureTileMinRows, CalculateRowsFromHeight(0));
}

// Tests that rows are dynamically added by adding FeatureTile elements to the
// container.
TEST_F(FeatureTilesContainerViewTest, FeatureTileRows) {
  std::unique_ptr<MockFeaturePodController> primary_tile_controller =
      std::make_unique<MockFeaturePodController>(
          FeatureTile::TileType::kPrimary);
  std::unique_ptr<MockFeaturePodController> compact_tile_controller =
      std::make_unique<MockFeaturePodController>(
          FeatureTile::TileType::kCompact);

  // Expect one row by adding two primary tiles.
  std::vector<std::unique_ptr<FeatureTile>> two_primary_tiles;
  two_primary_tiles.push_back(primary_tile_controller->CreateTile());
  two_primary_tiles.push_back(primary_tile_controller->CreateTile());
  container()->AddTiles(std::move(two_primary_tiles));
  EXPECT_EQ(FeatureTileRowCount(), 1);

  // Expect one other row by adding a primary and two compact tiles.
  std::vector<std::unique_ptr<FeatureTile>> one_primary_two_compact_tiles;
  one_primary_two_compact_tiles.push_back(
      primary_tile_controller->CreateTile());
  one_primary_two_compact_tiles.push_back(
      compact_tile_controller->CreateTile());
  one_primary_two_compact_tiles.push_back(
      compact_tile_controller->CreateTile());
  container()->AddTiles(std::move(one_primary_two_compact_tiles));
  EXPECT_EQ(FeatureTileRowCount(), 2);

  // Expect one other row by adding a single primary tile.
  std::vector<std::unique_ptr<FeatureTile>> one_primary_tile;
  one_primary_tile.push_back(primary_tile_controller->CreateTile());
  container()->AddTiles(std::move(one_primary_tile));
  EXPECT_EQ(FeatureTileRowCount(), 3);
}

TEST_F(FeatureTilesContainerViewTest, ChangeTileVisibility) {
  // Create 3 full-size tiles. Normally they would require 2 rows.
  auto tile_controller = std::make_unique<MockFeaturePodController>(
      FeatureTile::TileType::kPrimary);
  std::unique_ptr<FeatureTile> tile1 = tile_controller->CreateTile();
  std::unique_ptr<FeatureTile> tile2 = tile_controller->CreateTile();
  std::unique_ptr<FeatureTile> tile3 = tile_controller->CreateTile();

  // Make the first tile invisible.
  FeatureTile* tile1_ptr = tile1.get();
  tile1_ptr->SetVisible(false);

  // Add the tiles to the container.
  std::vector<std::unique_ptr<FeatureTile>> tiles;
  tiles.push_back(std::move(tile1));
  tiles.push_back(std::move(tile2));
  tiles.push_back(std::move(tile3));
  container()->AddTiles(std::move(tiles));

  // Only one row is created because the first tile is not visible.
  EXPECT_EQ(FeatureTileRowCount(), 1);

  // Making the tile visible causes a second row to be created.
  tile1_ptr->SetVisible(true);
  EXPECT_EQ(FeatureTileRowCount(), 2);

  // Making the tile invisible causes the second row to be removed.
  tile1_ptr->SetVisible(false);
  EXPECT_EQ(FeatureTileRowCount(), 1);
}

}  // namespace ash
