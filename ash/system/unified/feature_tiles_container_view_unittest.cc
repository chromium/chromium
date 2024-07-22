// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/feature_tiles_container_view.h"

#include "ash/constants/quick_settings_catalogs.h"
#include "ash/public/cpp/pagination/pagination_model.h"
#include "ash/shell.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/unified/feature_pod_button.h"
#include "ash/system/unified/feature_pod_controller_base.h"
#include "ash/system/unified/feature_tile.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/system/unified/unified_system_tray_bubble.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "ash/test/ash_test_base.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
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
  FeatureTilesContainerViewTest() = default;
  FeatureTilesContainerViewTest(const FeatureTilesContainerViewTest&) = delete;
  FeatureTilesContainerViewTest& operator=(
      const FeatureTilesContainerViewTest&) = delete;
  ~FeatureTilesContainerViewTest() override = default;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();

    tray_model_ =
        base::MakeRefCounted<UnifiedSystemTrayModel>(/*shelf=*/nullptr);
    tray_controller_ =
        std::make_unique<UnifiedSystemTrayController>(tray_model_.get());
    widget_ = CreateFramelessTestWidget();
    widget_->SetFullscreen(true);

    container_ = widget_->SetContentsView(
        std::make_unique<FeatureTilesContainerView>(tray_controller_.get()));
    container_->AddObserver(this);
  }

  void TearDown() override {
    container_->RemoveObserver(this);
    widget_.reset();
    tray_controller_.reset();
    tray_model_.reset();

    AshTestBase::TearDown();
  }

  void PressTab() {
    ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow());
    generator.PressKey(ui::KeyboardCode::VKEY_TAB, ui::EF_NONE);
  }

  void PressShiftTab() {
    ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow());
    generator.PressKey(ui::KeyboardCode::VKEY_TAB, ui::EF_SHIFT_DOWN);
  }

  FeatureTilesContainerView* container() { return container_; }

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

  void AdjustRowsForMediaViewVisibility(int height) {
    container()->AdjustRowsForMediaViewVisibility(true, height);
  }

  int GetRowCount() { return container()->row_count(); }

  int GetPageCount() { return container()->page_count(); }

  int GetVisibleCount() { return container()->GetVisibleFeatureTileCount(); }

  std::vector<raw_ptr<views::View, VectorExperimental>> pages() {
    return container()->children();
  }

  // Fills the container with a number of `pages` given the max amount of
  // displayable primary tiles per page.
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
  }

 private:
  std::unique_ptr<views::Widget> widget_;
  std::unique_ptr<UnifiedSystemTrayController> tray_controller_;
  scoped_refptr<UnifiedSystemTrayModel> tray_model_;
  raw_ptr<FeatureTilesContainerView, DanglingUntriaged> container_;
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

TEST_F(FeatureTilesContainerViewTest,
       DisplayableRowsIsLessWhenMediaViewIsShowing) {
  int row_height = kFeatureTileHeight;
  // Set height to equivalent of max+1 rows.
  const int max_height = (kFeatureTileMaxRows + 1) * row_height;

  // Expect default to cap at `kFeatureTileMaxRows`.
  EXPECT_EQ(kFeatureTileMaxRows, CalculateRowsFromHeight(max_height));

  AdjustRowsForMediaViewVisibility(max_height);

  // Expect height to be capped at `kFeatureTileMaxRowsWhenMediaViewIsShowing`
  // when media view is showing.
  EXPECT_EQ(kFeatureTileMaxRowsWhenMediaViewIsShowing,
            CalculateRowsFromHeight(max_height));
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
  EXPECT_EQ(1, GetRowCount());
  EXPECT_EQ(2, GetVisibleCount());

  // Add one primary, and two compact tiles. This should create a second row.
  std::vector<std::unique_ptr<FeatureTile>> one_primary_two_compact_tiles;
  one_primary_two_compact_tiles.push_back(mock_controller->CreateTile());
  one_primary_two_compact_tiles.push_back(
      mock_controller->CreateTile(/*compact=*/true));
  one_primary_two_compact_tiles.push_back(
      mock_controller->CreateTile(/*compact=*/true));
  container()->AddTiles(std::move(one_primary_two_compact_tiles));
  EXPECT_EQ(2, GetRowCount());
  EXPECT_EQ(5, GetVisibleCount());

  // Add one primary tile, this should result in a third row.
  std::vector<std::unique_ptr<FeatureTile>> one_primary_tile;
  one_primary_tile.push_back(mock_controller->CreateTile());
  container()->AddTiles(std::move(one_primary_tile));
  EXPECT_EQ(3, GetRowCount());
  EXPECT_EQ(6, GetVisibleCount());
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
  EXPECT_EQ(1, GetRowCount());
  EXPECT_EQ(2, GetVisibleCount());

  // Making the tile visible causes a second row to be created.
  tile1_ptr->SetVisible(true);
  EXPECT_EQ(2, GetRowCount());
  EXPECT_EQ(3, GetVisibleCount());

  // Making the tile invisible causes the second row to be removed.
  tile1_ptr->SetVisible(false);
  EXPECT_EQ(1, GetRowCount());
  EXPECT_EQ(2, GetVisibleCount());
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
  EXPECT_EQ(2, GetPageCount());
  EXPECT_EQ(5, GetVisibleCount());

  // Expect change in page count after updating visibility of a tile.
  tile1_ptr->SetVisible(false);
  EXPECT_EQ(1, GetPageCount());
  EXPECT_EQ(4, GetVisibleCount());

  // Expect change in page count after updating max displayable rows by updating
  // the available height.
  SetRowsFromHeight(kFeatureTileHeight);
  EXPECT_EQ(2, GetPageCount());
  EXPECT_EQ(4, GetVisibleCount());
}

// TODO(b/263185068): Use EventGenerator.
TEST_F(FeatureTilesContainerViewTest, PaginationGesture) {
  constexpr int kNumberOfPages = 4;
  FillContainerWithPrimaryTiles(kNumberOfPages);

  gfx::Point container_origin = container()->GetBoundsInScreen().origin();
  ui::GestureEvent swipe_left_begin(
      container_origin.x(), container_origin.y(), 0, base::TimeTicks(),
      ui::GestureEventDetails(ui::EventType::kGestureScrollBegin, -1, 0));
  ui::GestureEvent swipe_left_update(
      container_origin.x(), container_origin.y(), 0, base::TimeTicks(),
      ui::GestureEventDetails(ui::EventType::kGestureScrollUpdate, -1000, 0));
  ui::GestureEvent swipe_right_begin(
      container_origin.x(), container_origin.y(), 0, base::TimeTicks(),
      ui::GestureEventDetails(ui::EventType::kGestureScrollBegin, 1, 0));
  ui::GestureEvent swipe_right_update(
      container_origin.x(), container_origin.y(), 0, base::TimeTicks(),
      ui::GestureEventDetails(ui::EventType::kGestureScrollUpdate, 1000, 0));
  ui::GestureEvent swipe_end(
      container_origin.x(), container_origin.y(), 0, base::TimeTicks(),
      ui::GestureEventDetails(ui::EventType::kGestureEnd));

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

  ui::ScrollEvent fling_up_start(ui::EventType::kScrollFlingStart,
                                 container_origin, base::TimeTicks(), 0, 0, 100,
                                 0, 10, kNumberOfFingers);

  ui::ScrollEvent fling_down_start(ui::EventType::kScrollFlingStart,
                                   container_origin, base::TimeTicks(), 0, 0,
                                   -100, 0, 10, kNumberOfFingers);

  ui::ScrollEvent fling_cancel(ui::EventType::kScrollFlingCancel,
                               container_origin, base::TimeTicks(), 0, 0, 0, 0,
                               0, kNumberOfFingers);

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

TEST_F(FeatureTilesContainerViewTest, SwitchPageWithFocus) {
  FillContainerWithPrimaryTiles(/*pages=*/2);

  // View starts at page with index zero.
  EXPECT_EQ(0, pagination_model()->selected_page());

  // Tab until the `selected_page` index changes to 1.
  while (pagination_model()->selected_page() == 0) {
    PressTab();
  }
  EXPECT_EQ(1, pagination_model()->selected_page());

  // Pressing shift tab returns to the previous page.
  PressShiftTab();
  EXPECT_EQ(0, pagination_model()->selected_page());
}

TEST_F(FeatureTilesContainerViewTest, PaginationTransition) {
  FillContainerWithPrimaryTiles(/*pages=*/3);
  views::test::RunScheduledLayout(container());

  gfx::Rect initial_bounds = pages()[0]->bounds();
  gfx::Rect current_bounds;
  gfx::Rect previous_bounds = initial_bounds;

  // Page bounds should slide to the left during a transition to the next page.
  PaginationModel::Transition transition(
      pagination_model()->selected_page() + 1, 0);

  for (double i = 0.1; i <= 1.0; i += 0.1) {
    transition.progress = i;
    pagination_model()->SetTransition(transition);

    current_bounds = pages()[0]->bounds();

    EXPECT_LT(current_bounds.x(), previous_bounds.x());
    EXPECT_EQ(current_bounds.y(), previous_bounds.y());

    previous_bounds = current_bounds;
  }

  // Page position after the transition ends should be a page offset to the
  // left.
  int page_offset = kWideTrayMenuWidth;
  gfx::Rect final_bounds =
      gfx::Rect(initial_bounds.x() - page_offset, initial_bounds.y(),
                initial_bounds.width(), initial_bounds.height());
  pagination_model()->SelectPage(1, false);
  views::test::RunScheduledLayout(container());
  EXPECT_EQ(final_bounds, pages()[0]->bounds());
}

}  // namespace ash
