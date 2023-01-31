// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/feature_tiles_container_view.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/quick_settings_catalogs.h"
#include "ash/public/cpp/pagination/pagination_model.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/unified/feature_pod_button.h"
#include "ash/system/unified/feature_pod_controller_base.h"
#include "ash/system/unified/feature_tile.h"
#include "ash/system/unified/page_indicator_view.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/system/unified/unified_system_tray_bubble.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "ash/test/ash_test_base.h"
#include "base/memory/weak_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/test/views_test_utils.h"

namespace ash {

namespace {

class MockFeaturePodController : public FeaturePodControllerBase {
 public:
  MockFeaturePodController() = default;
  MockFeaturePodController(const MockFeaturePodController&) = delete;
  MockFeaturePodController& operator=(const MockFeaturePodController&) = delete;
  ~MockFeaturePodController() override = default;

  FeaturePodButton* CreateButton() override {
    return new FeaturePodButton(/*controller=*/this);
  }

  std::unique_ptr<FeatureTile> CreateTile(bool compact = false) override {
    auto tile = std::make_unique<FeatureTile>(
        base::BindRepeating(&FeaturePodControllerBase::OnIconPressed,
                            weak_ptr_factory_.GetWeakPtr()),
        /*togglable=*/true,
        compact ? FeatureTile::TileType::kCompact
                : FeatureTile::TileType::kPrimary);
    tile->SetVectorIcon(vector_icons::kDogfoodIcon);
    return tile;
  }

  QsFeatureCatalogName GetCatalogName() override {
    return QsFeatureCatalogName::kUnknown;
  }

  void OnIconPressed() override {}
  void OnLabelPressed() override {}

 private:
  base::WeakPtrFactory<MockFeaturePodController> weak_ptr_factory_{this};
};

constexpr int kMaxPrimaryTilesPerRow = 2;

}  // namespace

class FeatureTilesContainerViewTest : public AshTestBase,
                                      public views::ViewObserver {
 public:
  FeatureTilesContainerViewTest() {
    feature_list_.InitAndEnableFeature(features::kQsRevamp);
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

  PageIndicatorView* GetPageIndicatorView() {
    return GetPrimaryUnifiedSystemTray()
        ->bubble()
        ->quick_settings_view()
        ->page_indicator_view_for_test();
  }

  std::vector<views::View*> GetPageIndicatorButtons() {
    return GetPageIndicatorView()->buttons_container()->children();
  }

  int GetPageIndicatorButtonCount() { return GetPageIndicatorButtons().size(); }

  PaginationModel* pagination_model() { return container()->pagination_model_; }

  void AddTiles(std::vector<std::unique_ptr<FeatureTile>> tiles) {
    container()->AddTiles(std::move(tiles));
  }

  void SetRowsFromHeight(int max_height) {
    return container()->SetRowsFromHeight(max_height);
  }

  int CalculateRowsFromHeight(int height) {
    return container()->CalculateRowsFromHeight(height);
  }

  int GetRowCount() { return container()->row_count(); }

  int GetPageCount() { return container()->page_count(); }

  void FillContainerWithPrimaryTiles(int pages) {
    auto mock_controller = std::make_unique<MockFeaturePodController>();
    std::vector<std::unique_ptr<FeatureTile>> tiles;

    size_t number_of_tiles =
        pages * container()->displayable_rows() * kMaxPrimaryTilesPerRow;

    while (tiles.size() < number_of_tiles) {
      tiles.push_back(mock_controller->CreateTile());
    }
    AddTiles(std::move(tiles));

    EXPECT_EQ(pages, GetPageCount());
    EXPECT_EQ(pages, pagination_model()->total_pages());
    EXPECT_EQ(pages, GetPageIndicatorButtonCount());
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<FeatureTilesContainerView> container_;
};

// Tests `CalculateRowsFromHeight()` which returns the number of max displayable
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

// Tests that rows are dynamically added by adding `FeatureTile` elements to the
// container.
TEST_F(FeatureTilesContainerViewTest, FeatureTileRows) {
  auto mock_controller = std::make_unique<MockFeaturePodController>();

  // Expect one row by adding two primary tiles.
  std::vector<std::unique_ptr<FeatureTile>> two_primary_tiles;
  two_primary_tiles.push_back(mock_controller->CreateTile());
  two_primary_tiles.push_back(mock_controller->CreateTile());
  container()->AddTiles(std::move(two_primary_tiles));
  EXPECT_EQ(GetRowCount(), 1);

  // Expect one other row by adding a primary and two compact tiles.
  std::vector<std::unique_ptr<FeatureTile>> one_primary_two_compact_tiles;
  one_primary_two_compact_tiles.push_back(mock_controller->CreateTile());
  one_primary_two_compact_tiles.push_back(
      mock_controller->CreateTile(/*compact=*/true));
  one_primary_two_compact_tiles.push_back(
      mock_controller->CreateTile(/*compact=*/true));
  container()->AddTiles(std::move(one_primary_two_compact_tiles));
  EXPECT_EQ(GetRowCount(), 2);

  // Expect one other row by adding a single primary tile.
  std::vector<std::unique_ptr<FeatureTile>> one_primary_tile;
  one_primary_tile.push_back(mock_controller->CreateTile());
  container()->AddTiles(std::move(one_primary_tile));
  EXPECT_EQ(GetRowCount(), 3);
}

TEST_F(FeatureTilesContainerViewTest, ChangeTileVisibility) {
  // Create 3 full-size tiles. Normally they would require 2 rows.
  auto mock_controller = std::make_unique<MockFeaturePodController>();
  std::unique_ptr<FeatureTile> tile1 = mock_controller->CreateTile();
  std::unique_ptr<FeatureTile> tile2 = mock_controller->CreateTile();
  std::unique_ptr<FeatureTile> tile3 = mock_controller->CreateTile();

  // Make the first tile invisible.
  FeatureTile* tile1_ptr = tile1.get();
  tile1_ptr->SetVisible(false);

  // Add the tiles to the container.
  std::vector<std::unique_ptr<FeatureTile>> tiles;
  tiles.push_back(std::move(tile1));
  tiles.push_back(std::move(tile2));
  tiles.push_back(std::move(tile3));
  AddTiles(std::move(tiles));

  // Only one row is created because the first tile is not visible.
  EXPECT_EQ(GetRowCount(), 1);

  // Making the tile visible causes a second row to be created.
  tile1_ptr->SetVisible(true);
  EXPECT_EQ(GetRowCount(), 2);

  // Making the tile invisible causes the second row to be removed.
  tile1_ptr->SetVisible(false);
  EXPECT_EQ(GetRowCount(), 1);
}

TEST_F(FeatureTilesContainerViewTest, PageCountUpdated) {
  auto mock_controller = std::make_unique<MockFeaturePodController>();

  // Set the container height to have two displayable rows per page.
  SetRowsFromHeight(kFeatureTileHeight * 2);

  std::vector<std::unique_ptr<FeatureTile>> tiles;

  // Get pointer of one tile so we can make invisible later.
  std::unique_ptr<FeatureTile> tile1 = mock_controller->CreateTile();
  FeatureTile* tile1_ptr = tile1.get();
  tiles.push_back(std::move(tile1));

  // Add a total of five tiles to the container.
  while (tiles.size() < 5) {
    tiles.push_back(mock_controller->CreateTile());
  }

  // Since a row fits two primary tiles, expect two pages for five primary
  // tiles.
  AddTiles(std::move(tiles));
  EXPECT_EQ(GetPageCount(), 2);

  // Expect change in page count after updating visibility of a tile.
  tile1_ptr->SetVisible(false);
  EXPECT_EQ(GetPageCount(), 1);

  // Expect change in page count after updating max displayable rows by updating
  // the available height.
  SetRowsFromHeight(kFeatureTileHeight);
  EXPECT_EQ(GetPageCount(), 2);
}

// TODO(b/263185068): Use EventGenerator.
TEST_F(FeatureTilesContainerViewTest, PaginationGesture) {
  constexpr int kNumberOfPages = 4;
  FillContainerWithPrimaryTiles(kNumberOfPages);

  gfx::Point container_origin = container()->GetBoundsInScreen().origin();
  ui::GestureEvent swipe_left_begin(
      container_origin.x(), container_origin.y(), 0, base::TimeTicks(),
      ui::GestureEventDetails(ui::ET_GESTURE_SCROLL_BEGIN, -1, 0));
  ui::GestureEvent swipe_left_update(
      container_origin.x(), container_origin.y(), 0, base::TimeTicks(),
      ui::GestureEventDetails(ui::ET_GESTURE_SCROLL_UPDATE, -1000, 0));
  ui::GestureEvent swipe_right_begin(
      container_origin.x(), container_origin.y(), 0, base::TimeTicks(),
      ui::GestureEventDetails(ui::ET_GESTURE_SCROLL_BEGIN, 1, 0));
  ui::GestureEvent swipe_right_update(
      container_origin.x(), container_origin.y(), 0, base::TimeTicks(),
      ui::GestureEventDetails(ui::ET_GESTURE_SCROLL_UPDATE, 1000, 0));
  ui::GestureEvent swipe_end(container_origin.x(), container_origin.y(), 0,
                             base::TimeTicks(),
                             ui::GestureEventDetails(ui::ET_GESTURE_END));

  int previous_page = pagination_model()->selected_page();

  // Swipe left takes to next page
  for (int i = 0; i < kNumberOfPages - 1; i++) {
    // Simulate swipe left
    container()->OnGestureEvent(&swipe_left_begin);
    container()->OnGestureEvent(&swipe_left_update);
    container()->OnGestureEvent(&swipe_end);

    int current_page = pagination_model()->selected_page();
    // Expect next page
    EXPECT_EQ(previous_page + 1, current_page);
    previous_page = current_page;
  }

  // Swipe left on last page does nothing
  container()->OnGestureEvent(&swipe_left_begin);
  container()->OnGestureEvent(&swipe_left_update);
  container()->OnGestureEvent(&swipe_end);

  EXPECT_EQ(previous_page, pagination_model()->selected_page());

  // Swipe right takes to previous page
  for (int i = 0; i < kNumberOfPages - 1; i++) {
    // Simulate swipe right
    container()->OnGestureEvent(&swipe_right_begin);
    container()->OnGestureEvent(&swipe_right_update);
    container()->OnGestureEvent(&swipe_end);

    int current_page = pagination_model()->selected_page();
    // Expect previous page
    EXPECT_EQ(previous_page - 1, current_page);
    previous_page = current_page;
  }

  // Swipe right on first page does nothing
  container()->OnGestureEvent(&swipe_right_begin);
  container()->OnGestureEvent(&swipe_right_update);
  container()->OnGestureEvent(&swipe_end);

  EXPECT_EQ(previous_page, pagination_model()->selected_page());
}

// TODO(b/263185068): Use EventGenerator.
TEST_F(FeatureTilesContainerViewTest, PaginationScroll) {
  constexpr int kNumberOfFingers = 2;
  constexpr int kNumberOfPages = 4;
  FillContainerWithPrimaryTiles(kNumberOfPages);

  gfx::Point container_origin = container()->GetBoundsInScreen().origin();

  ui::ScrollEvent fling_up_start(ui::ET_SCROLL_FLING_START, container_origin,
                                 base::TimeTicks(), 0, 0, 100, 0, 10,
                                 kNumberOfFingers);

  ui::ScrollEvent fling_down_start(ui::ET_SCROLL_FLING_START, container_origin,
                                   base::TimeTicks(), 0, 0, -100, 0, 10,
                                   kNumberOfFingers);

  ui::ScrollEvent fling_cancel(ui::ET_SCROLL_FLING_CANCEL, container_origin,
                               base::TimeTicks(), 0, 0, 0, 0, 0,
                               kNumberOfFingers);

  int previous_page = pagination_model()->selected_page();

  // Scroll down takes to next page
  for (int i = 0; i < kNumberOfPages - 1; i++) {
    // Simulate Scroll left
    container()->OnScrollEvent(&fling_down_start);
    container()->OnScrollEvent(&fling_cancel);
    pagination_model()->FinishAnimation();

    int current_page = pagination_model()->selected_page();
    // Expect next page
    EXPECT_EQ(previous_page + 1, current_page);
    previous_page = current_page;
  }

  // Scroll up takes to previous page
  for (int i = 0; i < kNumberOfPages - 1; i++) {
    // Simulate Scroll up
    container()->OnScrollEvent(&fling_up_start);
    container()->OnScrollEvent(&fling_cancel);
    pagination_model()->FinishAnimation();

    int current_page = pagination_model()->selected_page();
    // Expect previous page
    EXPECT_EQ(previous_page - 1, current_page);
    previous_page = current_page;
  }
}

// TODO(b/263185068): Use EventGenerator.
TEST_F(FeatureTilesContainerViewTest, PaginationMouseWheel) {
  constexpr int kNumberOfPages = 4;
  FillContainerWithPrimaryTiles(kNumberOfPages);

  gfx::Point container_origin = container()->GetBoundsInScreen().origin();
  ui::MouseWheelEvent wheel_up(gfx::Vector2d(0, 1000), container_origin,
                               container_origin, base::TimeTicks(), 0, 0);

  ui::MouseWheelEvent wheel_down(gfx::Vector2d(0, -1000), container_origin,
                                 container_origin, base::TimeTicks(), 0, 0);

  int previous_page = pagination_model()->selected_page();

  // Mouse wheel down takes to next page
  for (int i = 0; i < kNumberOfPages - 1; i++) {
    // Simulate mouse wheel down
    container()->OnMouseWheel(wheel_down);
    pagination_model()->FinishAnimation();

    int current_page = pagination_model()->selected_page();
    // Expect next page
    EXPECT_EQ(previous_page + 1, current_page);
    previous_page = current_page;
  }

  // Mouse wheel up takes to previous page
  for (int i = 0; i < kNumberOfPages - 1; i++) {
    // Simulate mouse wheel up
    container()->OnMouseWheel(wheel_up);
    pagination_model()->FinishAnimation();

    int current_page = pagination_model()->selected_page();
    // Expect previous page
    EXPECT_EQ(previous_page - 1, current_page);
    previous_page = current_page;
  }
}

TEST_F(FeatureTilesContainerViewTest, PaginationDots) {
  constexpr int kNumberOfPages = 4;
  FillContainerWithPrimaryTiles(kNumberOfPages);

  // Expect the current_page to increase with each pagination dot click.
  int current_page = pagination_model()->selected_page();
  for (auto* button : GetPageIndicatorButtons()) {
    LeftClickOn(button);
    pagination_model()->FinishAnimation();
    EXPECT_EQ(current_page++, pagination_model()->selected_page());
  }
}

TEST_F(FeatureTilesContainerViewTest, ResetPagination) {
  constexpr int kNumberOfPages = 4;
  FillContainerWithPrimaryTiles(kNumberOfPages);

  // Expect page with index 2 to be selected after clicking its dot.
  LeftClickOn(GetPageIndicatorButtons()[2]);
  pagination_model()->FinishAnimation();
  EXPECT_EQ(2, pagination_model()->selected_page());

  // Expect page reset after closing and opening bubble.
  GetPrimaryUnifiedSystemTray()->CloseBubble();
  GetPrimaryUnifiedSystemTray()->ShowBubble();
  EXPECT_EQ(0, pagination_model()->selected_page());
}

}  // namespace ash
