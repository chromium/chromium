// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_insert/views/quick_insert_main_container_view.h"

#include <memory>
#include <utility>

#include "ash/public/cpp/style/color_provider.h"
#include "ash/quick_insert/views/quick_insert_contents_view.h"
#include "ash/quick_insert/views/quick_insert_page_view.h"
#include "ash/quick_insert/views/quick_insert_pseudo_focus.h"
#include "ash/quick_insert/views/quick_insert_search_field_view.h"
#include "ash/quick_insert/views/quick_insert_style.h"
#include "ash/style/system_shadow.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/views/background.h"
#include "ui/views/controls/separator.h"
#include "ui/views/highlight_border.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"

namespace ash {
namespace {

constexpr int kMainContainerMaxHeight = 300;

std::unique_ptr<views::Separator> CreateSeparator() {
  return views::Builder<views::Separator>()
      .SetOrientation(views::Separator::Orientation::kHorizontal)
      .SetColorId(cros_tokens::kCrosSysSeparator)
      .Build();
}

}  // namespace

QuickInsertMainContainerView::QuickInsertMainContainerView() {
  SetPaintToLayer();
  layer()->SetRoundedCornerRadius(
      gfx::RoundedCornersF{kQuickInsertContainerBorderRadius});
  layer()->SetFillsBoundsOpaquely(false);
  layer()->SetIsFastRoundedCorner(true);
  // We set background blur even though the main container background is opaque,
  // to avoid a flickering issue related to the container's scroll view
  // gradient. See b/351051291.
  layer()->SetBackgroundBlur(ColorProvider::kBackgroundBlurSigma);
  layer()->SetBackdropFilterQuality(ColorProvider::kBackgroundBlurQuality);

  SetBackground(views::CreateRoundedRectBackground(
      kQuickInsertContainerBackgroundColor, kQuickInsertContainerBorderRadius));
  SetBorder(std::make_unique<views::HighlightBorder>(
      kQuickInsertContainerBorderRadius,
      views::HighlightBorder::Type::kHighlightBorderOnShadow));
  shadow_ = SystemShadow::CreateShadowOnNinePatchLayerForView(
      this, kQuickInsertContainerShadowType);
  shadow_->SetRoundedCornerRadius(kQuickInsertContainerBorderRadius);

  SetLayoutManager(std::make_unique<views::BoxLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical);
}

QuickInsertMainContainerView::~QuickInsertMainContainerView() = default;

gfx::Size QuickInsertMainContainerView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  const int preferred_height =
      views::View::CalculatePreferredSize(available_size).height();
  return gfx::Size(kQuickInsertViewWidth,
                   std::min(preferred_height, kMainContainerMaxHeight));
}

views::View* QuickInsertMainContainerView::GetTopItem() {
  return active_page_->GetTopItem();
}

views::View* QuickInsertMainContainerView::GetBottomItem() {
  return active_page_->GetBottomItem();
}

views::View* QuickInsertMainContainerView::GetItemAbove(views::View* item) {
  if (search_field_view_->Contains(item)) {
    views::View* prev_item = GetNextQuickInsertPseudoFocusableView(
        item, QuickInsertPseudoFocusDirection::kBackward,
        /*should_loop=*/false);
    return Contains(prev_item) ? prev_item : nullptr;
  }
  // Try to get an item above `item`, skipping items outside of the active page
  // (such as search field buttons).
  return active_page_->GetItemAbove(item);
}

views::View* QuickInsertMainContainerView::GetItemBelow(views::View* item) {
  if (search_field_view_->Contains(item)) {
    views::View* next_item = GetNextQuickInsertPseudoFocusableView(
        item, QuickInsertPseudoFocusDirection::kForward, /*should_loop=*/false);
    return Contains(next_item) ? next_item : nullptr;
  }
  // Try to get an item below `item`, skipping items outside of the active page
  // (such as search field buttons).
  return active_page_->GetItemBelow(item);
}

views::View* QuickInsertMainContainerView::GetItemLeftOf(views::View* item) {
  return active_page_->GetItemLeftOf(item);
}

views::View* QuickInsertMainContainerView::GetItemRightOf(views::View* item) {
  return active_page_->GetItemRightOf(item);
}

bool QuickInsertMainContainerView::ContainsItem(views::View* item) {
  return Contains(item);
}

QuickInsertSearchFieldView* QuickInsertMainContainerView::AddSearchFieldView(
    std::unique_ptr<QuickInsertSearchFieldView> search_field_view) {
  search_field_view_ = AddChildView(std::move(search_field_view));
  return search_field_view_;
}

QuickInsertContentsView* QuickInsertMainContainerView::AddContentsView(
    QuickInsertLayoutType layout_type) {
  switch (layout_type) {
    case QuickInsertLayoutType::kMainResultsBelowSearchField:
      AddChildView(CreateSeparator());
      contents_view_ =
          AddChildView(std::make_unique<QuickInsertContentsView>(layout_type));
      break;
    case QuickInsertLayoutType::kMainResultsAboveSearchField:
      contents_view_ = AddChildViewAt(
          std::make_unique<QuickInsertContentsView>(layout_type), 0);
      AddChildViewAt(CreateSeparator(), 1);
      break;
  }

  contents_view_->SetProperty(
      views::kBoxLayoutFlexKey,
      views::BoxLayoutFlexSpecification().WithWeight(1));

  return contents_view_;
}

void QuickInsertMainContainerView::SetActivePage(
    QuickInsertPageView* page_view) {
  contents_view_->SetActivePage(page_view);
  active_page_ = page_view;
}

BEGIN_METADATA(QuickInsertMainContainerView)
END_METADATA

}  // namespace ash
