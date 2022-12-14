// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/feature_tiles_container_view.h"

#include "ash/public/cpp/pagination/pagination_controller.h"
#include "ash/public/cpp/pagination/pagination_model.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/unified/feature_tile.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/view_utils.h"

namespace ash {

namespace {

constexpr gfx::Size kFeatureTileRowSize(440, kFeatureTileHeight);
constexpr gfx::Insets kFeatureTileContainerInteriorMargin =
    gfx::Insets::VH(16, 0);
constexpr gfx::Insets kFeatureTileRowInteriorMargin = gfx::Insets::VH(0, 16);
constexpr gfx::Insets kFeatureTileRowMargins = gfx::Insets::VH(4, 0);
constexpr gfx::Insets kFeatureTileMargins = gfx::Insets::VH(0, 4);

// FeatureTileRow weight constants
constexpr int kCompactTileWeight = 1;
constexpr int kPrimaryTileWeight = 2;
constexpr int kMaxRowWeight = 4;

int GetTileWeight(FeatureTile::TileType type) {
  switch (type) {
    case FeatureTile::TileType::kPrimary:
      return kPrimaryTileWeight;
    case FeatureTile::TileType::kCompact:
      return kCompactTileWeight;
    default:
      NOTREACHED();
  }
}

}  // namespace

class FeatureTileRow : public views::FlexLayoutView {
 public:
  METADATA_HEADER(FeatureTileRow);

  explicit FeatureTileRow(FeatureTilesContainerView* container)
      : container_(container) {
    DCHECK(container_);
    SetPreferredSize(kFeatureTileRowSize);
    SetInteriorMargin(kFeatureTileRowInteriorMargin);
    SetDefault(views::kMarginsKey, kFeatureTileMargins);
    SetIgnoreDefaultMainAxisMargins(true);
  }

  FeatureTileRow(const FeatureTileRow&) = delete;
  FeatureTileRow& operator=(const FeatureTileRow&) = delete;
  ~FeatureTileRow() override = default;

  // views::View:
  void ChildVisibilityChanged(views::View* child) override {
    views::FlexLayoutView::ChildVisibilityChanged(child);
    container_->RelayoutTiles();
  }

 private:
  FeatureTilesContainerView* const container_;
};

BEGIN_METADATA(FeatureTileRow, views::FlexLayoutView)
END_METADATA

FeatureTilesContainerView::FeatureTilesContainerView(
    UnifiedSystemTrayController* controller)
    : controller_(controller),
      pagination_model_(controller->model()->pagination_model()),
      displayable_rows_(kFeatureTileMaxRows) {
  DCHECK(pagination_model_);
  DCHECK(controller_);
  pagination_model_->AddObserver(this);

  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical)
      .SetCrossAxisAlignment(views::LayoutAlignment::kCenter)
      .SetInteriorMargin(kFeatureTileContainerInteriorMargin)
      .SetDefault(views::kMarginsKey, kFeatureTileRowMargins)
      .SetIgnoreDefaultMainAxisMargins(true);
}

FeatureTilesContainerView::~FeatureTilesContainerView() {
  pagination_model_->RemoveObserver(this);
}

void FeatureTilesContainerView::AddTiles(
    std::vector<std::unique_ptr<FeatureTile>> tiles) {
  // A FeatureTileRow can hold a combination of primary and compact tiles
  // depending on the added tile weights.
  int row_weight = 0;
  bool create_row = true;
  for (auto& tile : tiles) {
    if (create_row) {
      // TODO(crbug/1371668): Create new page container if number of rows
      // surpasses `displayable_rows_`.
      feature_tile_rows_.push_back(
          AddChildView(std::make_unique<FeatureTileRow>(this)));
      create_row = false;
    }
    // Invisible tiles don't take any weight.
    if (tile->GetVisible())
      row_weight += GetTileWeight(tile->tile_type());
    DCHECK_LE(row_weight, kMaxRowWeight);
    feature_tile_rows_.back()->AddChildView(std::move(tile));

    if (row_weight == kMaxRowWeight) {
      row_weight = 0;
      create_row = true;
    }
  }
}

void FeatureTilesContainerView::RelayoutTiles() {
  // Tile visibility changing may change the number of required rows. Rebuild
  // the rows from scratch.
  std::vector<std::unique_ptr<FeatureTile>> tiles;
  for (FeatureTileRow* row : feature_tile_rows_) {
    // Copy the list of children since we will be modifying it during iteration.
    std::vector<views::View*> children = row->children();
    for (views::View* child : children) {
      DCHECK(views::IsViewClass<FeatureTile>(child));
      FeatureTile* tile = static_cast<FeatureTile*>(child);
      // Transfer ownership of each FeatureTile to `tiles`.
      tiles.push_back(row->RemoveChildViewT(tile));
    }
    // Remove this row. It will be rebuilt by AddTiles().
    RemoveChildViewT(row);
  }
  feature_tile_rows_.clear();

  // Rebuild the rows of tiles.
  AddTiles(std::move(tiles));
}

void FeatureTilesContainerView::SetRowsFromHeight(int max_height) {
  int displayable_rows = CalculateRowsFromHeight(max_height);

  if (displayable_rows_ != displayable_rows) {
    displayable_rows_ = displayable_rows;
    UpdateTotalPages();
  }
}

// TODO(crbug/1371668): Update pagination.
void FeatureTilesContainerView::ShowPageWithButton(views::View* button) {
  auto index = visible_buttons_.GetIndexOfView(button->parent());
  if (!index.has_value())
    return;

  int tiles_per_page = GetTilesPerPage();
  size_t first_index = pagination_model_->selected_page() * tiles_per_page;
  size_t last_index =
      ((pagination_model_->selected_page() + 1) * tiles_per_page) - 1;
  if (index.value() < first_index || index.value() > last_index) {
    int page = ((index.value() + 1) / tiles_per_page) +
               ((index.value() + 1) % tiles_per_page ? 1 : 0) - 1;

    pagination_model_->SelectPage(page, true /*animate*/);
  }
}

// TODO(crbug/1371668): Update pagination.
void FeatureTilesContainerView::SelectedPageChanged(int old_selected,
                                                    int new_selected) {
  PaginationModelObserver::SelectedPageChanged(old_selected, new_selected);
  InvalidateLayout();
}

// TODO(crbug/1371668): Update pagination.
void FeatureTilesContainerView::OnGestureEvent(ui::GestureEvent* event) {}

// TODO(crbug/1371668): Update pagination.
void FeatureTilesContainerView::OnScrollEvent(ui::ScrollEvent* event) {}

// TODO(crbug/1371668): Update pagination.
bool FeatureTilesContainerView::OnMouseWheel(const ui::MouseWheelEvent& event) {
  return false;
}

// TODO(crbug/1371668): Update pagination.
int FeatureTilesContainerView::CalculateRowsFromHeight(int height) {
  int row_height = kFeatureTileRowSize.height();

  // Uses the max number of rows with the space available.
  int rows = kFeatureTileMaxRows;
  while (height < (rows * row_height) && rows > kFeatureTileMinRows)
    rows--;
  return rows;
}

// TODO(crbug/1371668): Update pagination.
int FeatureTilesContainerView::GetTilesPerPage() const {
  return kFeatureTileItemsInRow * displayable_rows_;
}

// TODO(crbug/1371668): Update pagination.
void FeatureTilesContainerView::UpdateTotalPages() {
  int total_pages = 0;

  size_t total_visible = visible_buttons_.view_size();
  int tiles_per_page = GetTilesPerPage();

  if (total_visible == 0 || tiles_per_page == 0) {
    total_pages = 0;
  } else {
    total_pages = (total_visible / tiles_per_page) +
                  (total_visible % tiles_per_page ? 1 : 0);
  }
  pagination_model_->SetTotalPages(total_pages);
}

// TODO(crbug/1371668): Update pagination.
void FeatureTilesContainerView::TransitionChanged() {
  const PaginationModel::Transition& transition =
      pagination_model_->transition();
  if (pagination_model_->is_valid_page(transition.target_page))
    Layout();
}

BEGIN_METADATA(FeatureTilesContainerView, views::View)
END_METADATA

}  // namespace ash
