// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/feature_tiles_container_view.h"

#include "ash/public/cpp/pagination/pagination_controller.h"
#include "ash/public/cpp/pagination/pagination_model.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/unified/feature_pod_button.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/background.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_view.h"

using views::FlexLayout;
using views::FlexLayoutView;

namespace ash {

namespace {

constexpr gfx::Size kFeatureTileRowSize(440, kFeatureTileHeight);
constexpr gfx::Size kFeatureTileDefaultSize(200, kFeatureTileHeight);
constexpr gfx::Size kFeatureTileCompactSize(96, kFeatureTileHeight);
constexpr gfx::Insets kFeatureTileRowMargins = gfx::Insets::VH(4, 0);
constexpr gfx::Insets kFeatureTileMargins = gfx::Insets::VH(0, 4);
constexpr gfx::Insets kFeatureTileRowPadding = gfx::Insets::VH(0, 16);
constexpr gfx::Insets kFeatureTileContainerPadding = gfx::Insets::VH(16, 0);

class FeatureTileRow : public views::FlexLayoutView {
 public:
  METADATA_HEADER(FeatureTileRow);

  FeatureTileRow() {
    SetPreferredSize(kFeatureTileRowSize);
    SetDefault(views::kMarginsKey, kFeatureTileMargins);
    SetIgnoreDefaultMainAxisMargins(true);
    SetInteriorMargin(kFeatureTileRowPadding);
  }

  FeatureTileRow(const FeatureTileRow&) = delete;
  FeatureTileRow& operator=(const FeatureTileRow&) = delete;
  ~FeatureTileRow() override = default;
};

BEGIN_METADATA(FeatureTileRow, views::FlexLayoutView)
END_METADATA

// Temp class for prototyping.
class FeatureTile : public views::Label {
 public:
  METADATA_HEADER(FeatureTile);

  explicit FeatureTile(bool compact = false) {
    SetPreferredSize(compact ? kFeatureTileCompactSize
                             : kFeatureTileDefaultSize);
    SetBackground(views::CreateSolidBackground(SK_ColorGRAY));
  }

  FeatureTile(const FeatureTile&) = delete;
  FeatureTile& operator=(const FeatureTile&) = delete;
  ~FeatureTile() override = default;
};

BEGIN_METADATA(FeatureTile, views::Label)
END_METADATA

}  // namespace

FeatureTilesContainerView::FeatureTilesContainerView(
    UnifiedSystemTrayController* controller)
    : controller_(controller),
      pagination_model_(controller->model()->pagination_model()),
      feature_tile_rows_(kFeatureTileMaxRows) {
  DCHECK(pagination_model_);
  DCHECK(controller_);
  pagination_model_->AddObserver(this);

  SetLayoutManager(std::make_unique<FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical)
      .SetDefault(views::kMarginsKey, kFeatureTileRowMargins)
      .SetIgnoreDefaultMainAxisMargins(true)
      .SetInteriorMargin(kFeatureTileContainerPadding)
      .SetCrossAxisAlignment(views::LayoutAlignment::kCenter);

  // Adds four rows with placeholder FeatureTile elements.
  AddPlaceholderFeatureTiles();
}

FeatureTilesContainerView::~FeatureTilesContainerView() {
  pagination_model_->RemoveObserver(this);
}

void FeatureTilesContainerView::AddPlaceholderFeatureTiles() {
  // TODO: add child rows based on `feature_tile_rows`.
  FeatureTileRow* row1 = AddChildView(std::make_unique<FeatureTileRow>());
  FeatureTileRow* row2 = AddChildView(std::make_unique<FeatureTileRow>());
  FeatureTileRow* row3 = AddChildView(std::make_unique<FeatureTileRow>());
  FeatureTileRow* row4 = AddChildView(std::make_unique<FeatureTileRow>());

  row1->AddChildView(std::make_unique<FeatureTile>());
  row1->AddChildView(std::make_unique<FeatureTile>(/*compact=*/true));
  row1->AddChildView(std::make_unique<FeatureTile>(/*compact=*/true));

  row2->AddChildView(std::make_unique<FeatureTile>());
  row2->AddChildView(std::make_unique<FeatureTile>());

  row3->AddChildView(std::make_unique<FeatureTile>());
  row3->AddChildView(std::make_unique<FeatureTile>());

  row4->AddChildView(std::make_unique<FeatureTile>());
  row4->AddChildView(std::make_unique<FeatureTile>());
}

void FeatureTilesContainerView::SetRowsFromHeight(int max_height) {
  int feature_tile_rows = CalculateRowsFromHeight(max_height);

  if (feature_tile_rows_ != feature_tile_rows) {
    feature_tile_rows_ = feature_tile_rows;
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
  return kFeatureTileItemsInRow * feature_tile_rows_;
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
