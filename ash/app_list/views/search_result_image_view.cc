// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/search_result_image_view.h"

#include "ash/app_list/model/search/search_result.h"
#include "ash/app_list/views/search_result_image_view_delegate.h"
#include "ash/public/cpp/app_list/app_list_color_provider.h"
#include "ash/style/ash_color_id.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/fill_layout.h"

namespace ash {

namespace {

// Sizing and spacing values for `result_image_`.
constexpr int kTopBottomMargin = 10;
constexpr int kLeftRightMargin = 25;
constexpr int kIconSize = 100;

}  // namespace

SearchResultImageView::SearchResultImageView(std::string dummy_result_id) {
  SetLayoutManager(std::make_unique<views::FillLayout>());
  result_image_ = AddChildView(std::make_unique<views::ImageView>());
  result_image_->SetCanProcessEventsWithinSubtree(false);
  result_image_->SetBorder(views::CreateEmptyBorder(gfx::Insets::TLBR(
      kTopBottomMargin, kLeftRightMargin, kTopBottomMargin, kLeftRightMargin)));

  dummy_result_ptr = std::make_unique<SearchResult>();
  dummy_result_ptr->set_id(dummy_result_id);
  SetResult(dummy_result_ptr.get());

  set_context_menu_controller(SearchResultImageViewDelegate::Get());
  set_drag_controller(SearchResultImageViewDelegate::Get());
}

void SearchResultImageView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->role = ax::mojom::Role::kListBoxOption;
  node_data->SetName("Search Result Image View");
  // TODO(crbug.com/1352636) update with internationalized accessible name if we
  // launch this feature.
}

void SearchResultImageView::OnThemeChanged() {
  SearchResultBaseView::OnThemeChanged();

  // TODO(crbug.com/1352636) remove placeholder image.
  result_image_->SetImage(gfx::CreateVectorIcon(
      vector_icons::kGoogleColorIcon, kIconSize,
      GetWidget()->GetColorProvider()->GetColor(kColorAshButtonIconColor)));
}

void SearchResultImageView::OnGestureEvent(ui::GestureEvent* event) {
  SearchResultImageViewDelegate::Get()->HandleSearchResultImageViewGestureEvent(
      this, *event);
  SearchResultBaseView::OnGestureEvent(event);
}

void SearchResultImageView::OnMouseEvent(ui::MouseEvent* event) {
  SearchResultImageViewDelegate::Get()->HandleSearchResultImageViewMouseEvent(
      this, *event);
  SearchResultBaseView::OnMouseEvent(event);
}

SearchResultImageView::~SearchResultImageView() = default;

BEGIN_METADATA(SearchResultImageView, SearchResultBaseView)
END_METADATA

}  // namespace ash
