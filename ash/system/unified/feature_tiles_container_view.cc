// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/feature_tiles_container_view.h"

#include "ash/public/cpp/pagination/pagination_controller.h"
#include "ash/public/cpp/pagination/pagination_model.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/unified/feature_tile.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/view_utils.h"

namespace ash {

namespace {

// Size constants
constexpr gfx::Size kRowContainerSize(kWideTrayMenuWidth, kFeatureTileHeight);
constexpr gfx::Insets kFeatureTileContainerInteriorMargin =
    gfx::Insets::VH(16, 0);
constexpr gfx::Insets kRowContainerInteriorMargin = gfx::Insets::VH(0, 16);
constexpr gfx::Insets kRowContainerMargins = gfx::Insets::VH(4, 0);
constexpr gfx::Insets kFeatureTileMargins = gfx::Insets::VH(0, 4);

// `RowContainer` weight constants
constexpr int kCompactTileWeight = 1;
constexpr int kPrimaryTileWeight = 2;
constexpr int kMaxRowWeight = 4;

int GetTileWeight(FeatureTile::TileType type) {
  switch (type) {
    case FeatureTile::TileType::kPrimary:
      return kPrimaryTileWeight;
    case FeatureTile::TileType::kCompact:
      return kCompactTileWeight;
  }
}

int GetTileWidth(FeatureTile::TileType type) {
  switch (type) {
    case FeatureTile::TileType::kPrimary:
      return kPrimaryFeatureTileWidth;
    case FeatureTile::TileType::kCompact:
      return kCompactFeatureTileWidth;
  }
}

}  // namespace

// The row container that holds `FeatureTile` elements. Can hold a single
// primary tile, two primary tiles, or a primary and two compact tiles.
class FeatureTilesContainerView::RowContainer : public views::FlexLayoutView {
  METADATA_HEADER(RowContainer, views::FlexLayoutView)

 public:
  explicit RowContainer(FeatureTilesContainerView* container)
      : container_(container) {
    DCHECK(container_);
    SetPreferredSize(kRowContainerSize);
    SetInteriorMargin(kRowContainerInteriorMargin);
    SetDefault(views::kMarginsKey, kFeatureTileMargins);
    SetIgnoreDefaultMainAxisMargins(true);
  }
  RowContainer(const RowContainer&) = delete;
  RowContainer& operator=(const RowContainer&) = delete;
  ~RowContainer() override = default;

  // views::View:
  void ChildVisibilityChanged(views::View* child) override {
    views::FlexLayoutView::ChildVisibilityChanged(child);
    container_->RelayoutTiles();
  }

 private:
  const raw_ptr<FeatureTilesContainerView> container_;
};

BEGIN_METADATA(FeatureTilesContainerView, RowContainer)
END_METADATA

// The page container that holds `RowContainer` elements. Can hold from one up
// to four rows depending on the available space. More pages will be created if
// the available tiles do not fit a single page.
class FeatureTilesContainerView::PageContainer : public views::FlexLayoutView {
  METADATA_HEADER(PageContainer, views::FlexLayoutView)

 public:
  PageContainer() {
    SetOrientation(views::LayoutOrientation::kVertical);
    SetCrossAxisAlignment(views::LayoutAlignment::kCenter);
    SetInteriorMargin(kFeatureTileContainerInteriorMargin);
    SetDefault(views::kMarginsKey, kRowContainerMargins);
    SetIgnoreDefaultMainAxisMargins(true);
  }
  PageContainer(const PageContainer&) = delete;
  PageContainer& operator=(const PageContainer&) = delete;
  ~PageContainer() override = default;
};

BEGIN_METADATA(FeatureTilesContainerView, PageContainer)
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
      ->SetOrientation(views::LayoutOrientation::kHorizontal);
}

FeatureTilesContainerView::~FeatureTilesContainerView() {
  DCHECK(pagination_model_);
  pagination_model_->RemoveObserver(this);
}

void FeatureTilesContainerView::AddTiles(
    std::vector<std::unique_ptr<FeatureTile>> tiles) {
  // A `RowContainer` can hold a combination of primary and compact tiles
  // depending on the added tile weights.
  int row_weight = 0;
  bool create_row = true;

  if (tiles.size() > 0) {
    pages_.push_back(AddChildView(std::make_unique<PageContainer>()));
  }

  for (auto& tile : tiles) {
    if (create_row && (tile->GetVisible() || rows_.empty())) {
      int current_page_rows = pages_.back()->children().size();
      // Add a new page if we have reached the max displayable rows per page.
      if (current_page_rows == displayable_rows_) {
        pages_.push_back(AddChildView(std::make_unique<PageContainer>()));
      }

      rows_.push_back(
          pages_.back()->AddChildView(std::make_unique<RowContainer>(this)));
      create_row = false;
    }
    // Invisible tiles don't take any weight.
    if (tile->GetVisible()) {
      row_weight += GetTileWeight(tile->tile_type());
      tile->SetPreferredSize(
          gfx::Size(GetTileWidth(tile->tile_type()), kFeatureTileHeight));
      tile->SetProperty(
          views::kFlexBehaviorKey,
          views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                                   views::MaximumFlexSizeRule::kPreferred,
                                   /*adjust_height_for_width=*/true));
    }
    DCHECK_LE(row_weight, kMaxRowWeight);
    rows_.back()->AddChildView(std::move(tile));

    if (row_weight == kMaxRowWeight) {
      row_weight = 0;
      create_row = true;
    }
  }

  UpdateTotalPages();
}

void FeatureTilesContainerView::RelayoutTiles() {
  // Tile visibility or UI size changes may change the number of required pages
  // and rows so we have to rebuild them from scratch.
  std::vector<std::unique_ptr<FeatureTile>> tiles;
  for (PageContainer* page : pages_) {
    for (views::View* row : page->children()) {
      // Copy the list of children since it will be modified during iteration.
      std::vector<raw_ptr<views::View, VectorExperimental>> children =
          row->children();
      for (views::View* child : children) {
        DCHECK(views::IsViewClass<FeatureTile>(child));
        FeatureTile* tile = static_cast<FeatureTile*>(child);
        // Transfer ownership of each `FeatureTile` to `tiles`.
        tiles.push_back(row->RemoveChildViewT(tile));
      }
    }
    // Remove current page and child rows. It will be re-built by `AddTiles()`.
    page->RemoveAllChildViews();
    RemoveChildViewT(page);
  }
  pages_.clear();
  rows_.clear();

  // Re-add tiles to container.
  AddTiles(std::move(tiles));

  // Update bubble height in case number of rows changed.
  controller_->UpdateBubble();
}

void FeatureTilesContainerView::SetRowsFromHeight(int max_height) {
  int displayable_rows = CalculateRowsFromHeight(max_height);
  if (displayable_rows_ != displayable_rows) {
    displayable_rows_ = displayable_rows;
    RelayoutTiles();
  }
}

void FeatureTilesContainerView::AdjustRowsForMediaViewVisibility(
    bool visible,
    int max_height) {
  if (is_media_view_shown_ != visible) {
    is_media_view_shown_ = visible;
    SetRowsFromHeight(max_height);
  }
}

void FeatureTilesContainerView::SelectedPageChanged(int old_selected,
                                                    int new_selected) {
  const int origin = kWideTrayMenuWidth * -old_selected;
  const int selection_offset =
      kWideTrayMenuWidth * (old_selected - new_selected);

  for (size_t i = 0; i < pages_.size(); ++i) {
    const int page_offset = i * kWideTrayMenuWidth;
    const int final_x = origin + page_offset + selection_offset;
    pages_[i]->SetX(final_x);
  }
}

void FeatureTilesContainerView::TransitionChanged() {
  const int target_page = pagination_model_->transition().target_page;
  if (pagination_model_->is_valid_page(target_page)) {
    UpdateAnimatingPagesBounds(pagination_model_->selected_page(), target_page);
  }
}

void FeatureTilesContainerView::OnGestureEvent(ui::GestureEvent* event) {
  if (controller_->pagination_controller()->OnGestureEvent(
          *event, GetContentsBounds())) {
    event->SetHandled();
  }
}

void FeatureTilesContainerView::OnScrollEvent(ui::ScrollEvent* event) {
  controller_->pagination_controller()->OnScroll(
      gfx::Vector2d(event->x_offset(), event->y_offset()), event->type());
  event->SetHandled();
}

bool FeatureTilesContainerView::OnMouseWheel(const ui::MouseWheelEvent& event) {
  return controller_->pagination_controller()->OnScroll(event.offset(),
                                                        event.type());
}

void FeatureTilesContainerView::Layout(PassKey) {
  LayoutSuperclass<views::View>(this);

  // `SelectedPageChanged` is called to recalculate the pages bounds after a
  // Layout (e.g. when changing the UI scale).
  SelectedPageChanged(0, pagination_model_->selected_page());
}

void FeatureTilesContainerView::UpdateAnimatingPagesBounds(int old_selected,
                                                           int new_selected) {
  DCHECK(pagination_model_->is_valid_page(old_selected));
  DCHECK(pagination_model_->is_valid_page(new_selected));

  // Transition to next page means negative offset.
  const int direction = new_selected > old_selected ? -1 : 1;

  const int page_offset = kWideTrayMenuWidth * direction;
  const int transition_offset =
      pagination_model_->transition().progress * page_offset;
  pages_[old_selected]->SetX(transition_offset);
  pages_[new_selected]->SetX(transition_offset - page_offset);
}

void FeatureTilesContainerView::AddedToWidget() {
  GetFocusManager()->AddFocusChangeListener(this);
}

void FeatureTilesContainerView::RemovedFromWidget() {
  GetFocusManager()->RemoveFocusChangeListener(this);
}

void FeatureTilesContainerView::OnWillChangeFocus(views::View* before,
                                                  views::View* now) {}

void FeatureTilesContainerView::OnDidChangeFocus(views::View* before,
                                                 views::View* now) {
  if (!now || !views::IsViewClass<FeatureTile>(now) || !Contains(now)) {
    return;
  }

  auto* current_page = now->parent()->parent();
  DCHECK(views::IsViewClass<PageContainer>(current_page));
  auto page_index = GetIndexOf(current_page);
  if (!page_index.has_value()) {
    return;
  }
  if (pagination_model_->selected_page() !=
      static_cast<int>(page_index.value())) {
    pagination_model_->SelectPage(page_index.value(), false /*animate*/);
  }
}

int FeatureTilesContainerView::CalculateRowsFromHeight(int height) {
  int row_height = kRowContainerSize.height();

  // Uses the max number of rows with the space available.
  int rows = is_media_view_shown_ ? kFeatureTileMaxRowsWhenMediaViewIsShowing
                                  : kFeatureTileMaxRows;
  while (height < (rows * row_height) && rows > kFeatureTileMinRows) {
    rows--;
  }
  return rows;
}

void FeatureTilesContainerView::UpdateTotalPages() {
  const int total_rows = rows_.size();
  int total_pages = (total_rows / displayable_rows_) +
                    (total_rows % displayable_rows_ ? 1 : 0);
  pagination_model_->SetTotalPages(total_pages);
  pagination_model_->SelectPage(0, false /*animate*/);
}

int FeatureTilesContainerView::GetVisibleFeatureTileCount() const {
  int count = 0;
  for (PageContainer* page : pages_) {
    for (views::View* row : page->children()) {
      for (views::View* child : row->children()) {
        DCHECK(views::IsViewClass<FeatureTile>(child));
        if (child->GetVisible()) {
          ++count;
        }
      }
    }
  }
  return count;
}

BEGIN_METADATA(FeatureTilesContainerView)
END_METADATA

}  // namespace ash
