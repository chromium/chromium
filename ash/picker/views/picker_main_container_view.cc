// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_main_container_view.h"

#include <memory>
#include <utility>

#include "ash/picker/views/picker_contents_view.h"
#include "ash/picker/views/picker_page_view.h"
#include "ash/picker/views/picker_pseudo_focus.h"
#include "ash/picker/views/picker_search_field_view.h"
#include "ash/picker/views/picker_style.h"
#include "ash/public/cpp/style/color_provider.h"
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

PickerMainContainerView::PickerMainContainerView() {
  SetPaintToLayer();
  layer()->SetRoundedCornerRadius(
      gfx::RoundedCornersF{kPickerContainerBorderRadius});
  layer()->SetFillsBoundsOpaquely(false);
  layer()->SetIsFastRoundedCorner(true);
  // We set background blur even though the main container background is opaque,
  // to avoid a flickering issue related to the container's scroll view
  // gradient. See b/351051291.
  layer()->SetBackgroundBlur(ColorProvider::kBackgroundBlurSigma);
  layer()->SetBackdropFilterQuality(ColorProvider::kBackgroundBlurQuality);

  SetBackground(views::CreateThemedRoundedRectBackground(
      kPickerContainerBackgroundColor, kPickerContainerBorderRadius));
  SetBorder(std::make_unique<views::HighlightBorder>(
      kPickerContainerBorderRadius,
      views::HighlightBorder::Type::kHighlightBorderOnShadow));
  shadow_ = SystemShadow::CreateShadowOnNinePatchLayerForView(
      this, kPickerContainerShadowType);
  shadow_->SetRoundedCornerRadius(kPickerContainerBorderRadius);

  SetLayoutManager(std::make_unique<views::BoxLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical);
}

PickerMainContainerView::~PickerMainContainerView() = default;

gfx::Size PickerMainContainerView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  const int preferred_height =
      views::View::CalculatePreferredSize(available_size).height();
  return gfx::Size(kPickerViewWidth,
                   std::min(preferred_height, kMainContainerMaxHeight));
}

views::View* PickerMainContainerView::GetTopItem() {
  return active_page_->GetTopItem();
}

views::View* PickerMainContainerView::GetBottomItem() {
  return active_page_->GetBottomItem();
}

views::View* PickerMainContainerView::GetItemAbove(views::View* item) {
  if (search_field_view_->Contains(item)) {
    views::View* prev_item = GetNextPickerPseudoFocusableView(
        item, PickerPseudoFocusDirection::kBackward, /*should_loop=*/false);
    return Contains(prev_item) ? prev_item : nullptr;
  }
  // Try to get an item above `item`, skipping items outside of the active page
  // (such as search field buttons).
  return active_page_->GetItemAbove(item);
}

views::View* PickerMainContainerView::GetItemBelow(views::View* item) {
  if (search_field_view_->Contains(item)) {
    views::View* next_item = GetNextPickerPseudoFocusableView(
        item, PickerPseudoFocusDirection::kForward, /*should_loop=*/false);
    return Contains(next_item) ? next_item : nullptr;
  }
  // Try to get an item below `item`, skipping items outside of the active page
  // (such as search field buttons).
  return active_page_->GetItemBelow(item);
}

views::View* PickerMainContainerView::GetItemLeftOf(views::View* item) {
  return active_page_->GetItemLeftOf(item);
}

views::View* PickerMainContainerView::GetItemRightOf(views::View* item) {
  return active_page_->GetItemRightOf(item);
}

bool PickerMainContainerView::ContainsItem(views::View* item) {
  return Contains(item);
}

PickerSearchFieldView* PickerMainContainerView::AddSearchFieldView(
    std::unique_ptr<PickerSearchFieldView> search_field_view) {
  search_field_view_ = AddChildView(std::move(search_field_view));
  return search_field_view_;
}

PickerContentsView* PickerMainContainerView::AddContentsView(
    PickerLayoutType layout_type) {
  switch (layout_type) {
    case PickerLayoutType::kMainResultsBelowSearchField:
      AddChildView(CreateSeparator());
      contents_view_ =
          AddChildView(std::make_unique<PickerContentsView>(layout_type));
      break;
    case PickerLayoutType::kMainResultsAboveSearchField:
      contents_view_ =
          AddChildViewAt(std::make_unique<PickerContentsView>(layout_type), 0);
      AddChildViewAt(CreateSeparator(), 1);
      break;
  }

  contents_view_->SetProperty(
      views::kBoxLayoutFlexKey,
      views::BoxLayoutFlexSpecification().WithWeight(1));

  return contents_view_;
}

void PickerMainContainerView::SetActivePage(PickerPageView* page_view) {
  contents_view_->SetActivePage(page_view);
  active_page_ = page_view;
}

BEGIN_METADATA(PickerMainContainerView)
END_METADATA

}  // namespace ash
