// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_image_item_row_view.h"

#include <memory>
#include <utility>

#include "ash/picker/views/picker_image_item_view.h"
#include "ash/picker/views/picker_item_view.h"
#include "ash/picker/views/picker_pseudo_focus.h"
#include "ash/picker/views/picker_traversable_item_container.h"
#include "base/ranges/algorithm.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"

namespace ash {
namespace {

constexpr auto kMargin = gfx::Insets::TLBR(4, 16, 4, 12);
// Padding between the leading icon and the image items.
constexpr int kBetweenIconAndImageItemsPadding = 16;
// Padding between image items.
constexpr int kImageItemPadding = 8;
constexpr gfx::Size kLeadingIconSizeDip(20, 20);
constexpr int kHeight = 64;

}  // namespace

PickerImageItemRowView::PickerImageItemRowView() {
  views::Builder<PickerImageItemRowView>(this)
      .SetOrientation(views::BoxLayout::Orientation::kHorizontal)
      .SetCrossAxisAlignment(views::BoxLayout::CrossAxisAlignment::kStart)
      .SetInsideBorderInsets(kMargin)
      .SetBetweenChildSpacing(kBetweenIconAndImageItemsPadding)
      .AddChildren(
          views::Builder<views::ImageView>()
              .CopyAddressTo(&leading_icon_view_)
              .SetPreferredSize(kLeadingIconSizeDip)
              .SetCanProcessEventsWithinSubtree(false),
          views::Builder<views::BoxLayoutView>()
              .CopyAddressTo(&items_container_)
              .SetOrientation(views::BoxLayout::Orientation::kHorizontal)
              .SetDefaultFlex(1)
              .SetProperty(views::kBoxLayoutFlexKey,
                           views::BoxLayoutFlexSpecification().WithWeight(1))
              .SetBetweenChildSpacing(kImageItemPadding))
      .BuildChildren();
}

PickerImageItemRowView::~PickerImageItemRowView() = default;

void PickerImageItemRowView::SetLeadingIcon(const ui::ImageModel& icon) {
  leading_icon_view_->SetImage(icon);
}

PickerImageItemView* PickerImageItemRowView::AddImageItem(
    std::unique_ptr<PickerImageItemView> image_item) {
  return items_container_->AddChildView(std::move(image_item));
}

gfx::Size PickerImageItemRowView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  const int preferred_width =
      views::BoxLayoutView::CalculatePreferredSize(available_size).width();
  return gfx::Size(preferred_width, kHeight);
}

views::View* PickerImageItemRowView::GetTopItem() {
  return GetLeftmostItem();
}

views::View* PickerImageItemRowView::GetBottomItem() {
  return GetLeftmostItem();
}

views::View* PickerImageItemRowView::GetItemAbove(views::View* item) {
  return nullptr;
}

views::View* PickerImageItemRowView::GetItemBelow(views::View* item) {
  return nullptr;
}

views::View* PickerImageItemRowView::GetItemLeftOf(views::View* item) {
  views::View* item_left_of = GetNextPickerPseudoFocusableView(
      item, PickerPseudoFocusDirection::kBackward, /*should_loop=*/false);
  return Contains(item_left_of) ? item_left_of : nullptr;
}

views::View* PickerImageItemRowView::GetItemRightOf(views::View* item) {
  views::View* item_left_of = GetNextPickerPseudoFocusableView(
      item, PickerPseudoFocusDirection::kForward, /*should_loop=*/false);
  return Contains(item_left_of) ? item_left_of : nullptr;
}

bool PickerImageItemRowView::ContainsItem(views::View* item) {
  return Contains(item);
}

views::View* PickerImageItemRowView::GetLeftmostItem() {
  if (GetFocusManager() == nullptr) {
    return nullptr;
  }
  views::View* leftmost_item = GetFocusManager()->GetNextFocusableView(
      this, GetWidget(), /*reverse=*/false,
      /*dont_loop=*/false);
  return Contains(leftmost_item) ? leftmost_item : nullptr;
}

BEGIN_METADATA(PickerImageItemRowView)
END_METADATA

}  // namespace ash
