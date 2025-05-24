// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_insert/views/quick_insert_image_item_row_view.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "ash/ash_element_identifiers.h"
#include "ash/quick_insert/views/quick_insert_image_item_view.h"
#include "ash/quick_insert/views/quick_insert_item_view.h"
#include "ash/quick_insert/views/quick_insert_pseudo_focus.h"
#include "ash/quick_insert/views/quick_insert_traversable_item_container.h"
#include "ash/style/icon_button.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/image_button.h"
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

std::unique_ptr<views::View> CreateEmptyCell() {
  auto cell_view = std::make_unique<views::View>();
  cell_view->SetUseDefaultFillLayout(true);
  cell_view->GetViewAccessibility().SetRole(ax::mojom::Role::kGridCell);
  return cell_view;
}

}  // namespace

QuickInsertImageItemRowView::QuickInsertImageItemRowView(
    base::RepeatingClosure more_items_callback,
    std::u16string more_items_accessible_name) {
  views::View* row_view = nullptr;
  views::Builder<QuickInsertImageItemRowView>(this)
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
              .CopyAddressTo(&row_view)
              .SetOrientation(views::BoxLayout::Orientation::kHorizontal)
              .SetBetweenChildSpacing(kImageItemPadding)
              .SetProperty(views::kBoxLayoutFlexKey,
                           views::BoxLayoutFlexSpecification().WithWeight(1))
              .AddChildren(
                  views::Builder<views::BoxLayoutView>()
                      .CopyAddressTo(&items_container_)
                      .SetOrientation(
                          views::BoxLayout::Orientation::kHorizontal)
                      .SetDefaultFlex(1)
                      .SetProperty(
                          views::kBoxLayoutFlexKey,
                          views::BoxLayoutFlexSpecification().WithWeight(1))
                      .SetBetweenChildSpacing(kImageItemPadding),
                  views::Builder<views::View>(CreateEmptyCell())
                      .AddChildren(
                          views::Builder<views::ImageButton>(
                              std::make_unique<IconButton>(
                                  std::move(more_items_callback),
                                  IconButton::Type::kMediumFloating,
                                  &vector_icons::kSubmenuArrowChromeRefreshIcon,
                                  std::move(more_items_accessible_name),
                                  /*is_togglable=*/false,
                                  /*has_border=*/false))
                              // The kSubmenuArrowChromeRefreshIcon flips
                              // itself, so don't flip it again.
                              .SetFlipCanvasOnPaintForRTLUI(false)
                              .CopyAddressTo(&more_items_button_))))
      .BuildChildren();
  GetViewAccessibility().SetRole(ax::mojom::Role::kGrid);
  row_view->GetViewAccessibility().SetRole(ax::mojom::Role::kRow);
  items_container_->GetViewAccessibility().SetRole(ax::mojom::Role::kNone);
  SetProperty(views::kElementIdentifierKey,
              kQuickInsertSearchResultsImageRowElementId);
}

QuickInsertImageItemRowView::~QuickInsertImageItemRowView() = default;

void QuickInsertImageItemRowView::SetLeadingIcon(const ui::ImageModel& icon) {
  leading_icon_view_->SetImage(icon);
}

QuickInsertImageItemView* QuickInsertImageItemRowView::AddImageItem(
    std::unique_ptr<QuickInsertImageItemView> image_item) {
  auto* view = items_container_->AddChildView(CreateEmptyCell())
                   ->AddChildView(std::move(image_item));
  on_items_changed_.Notify();
  return view;
}

gfx::Size QuickInsertImageItemRowView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  const int preferred_width =
      views::BoxLayoutView::CalculatePreferredSize(available_size).width();
  return gfx::Size(preferred_width, kHeight);
}

views::View* QuickInsertImageItemRowView::GetTopItem() {
  return GetLeftmostItem();
}

views::View* QuickInsertImageItemRowView::GetBottomItem() {
  return GetLeftmostItem();
}

views::View* QuickInsertImageItemRowView::GetItemAbove(views::View* item) {
  return nullptr;
}

views::View* QuickInsertImageItemRowView::GetItemBelow(views::View* item) {
  return nullptr;
}

views::View* QuickInsertImageItemRowView::GetItemLeftOf(views::View* item) {
  views::View* item_left_of = GetNextQuickInsertPseudoFocusableView(
      item, QuickInsertPseudoFocusDirection::kBackward, /*should_loop=*/false);
  return Contains(item_left_of) ? item_left_of : nullptr;
}

views::View* QuickInsertImageItemRowView::GetItemRightOf(views::View* item) {
  views::View* item_left_of = GetNextQuickInsertPseudoFocusableView(
      item, QuickInsertPseudoFocusDirection::kForward, /*should_loop=*/false);
  return Contains(item_left_of) ? item_left_of : nullptr;
}

bool QuickInsertImageItemRowView::ContainsItem(views::View* item) {
  return Contains(item);
}

views::View::Views QuickInsertImageItemRowView::GetItems() const {
  views::View::Views items;
  for (views::View* child : items_container_->children()) {
    items.push_back(child->children().front());
  }
  return items;
}

base::CallbackListSubscription
QuickInsertImageItemRowView::AddItemsChangedCallback(
    views::PropertyChangedCallback callback) {
  return on_items_changed_.Add(std::move(callback));
}

views::View* QuickInsertImageItemRowView::GetLeftmostItem() {
  if (GetFocusManager() == nullptr) {
    return nullptr;
  }
  views::View* leftmost_item = GetFocusManager()->GetNextFocusableView(
      this, GetWidget(), /*reverse=*/false,
      /*dont_loop=*/false);
  return Contains(leftmost_item) ? leftmost_item : nullptr;
}

BEGIN_METADATA(QuickInsertImageItemRowView)
END_METADATA

}  // namespace ash
