// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_submenu_view.h"

#include <memory>
#include <utility>

#include "ash/picker/views/picker_list_item_view.h"
#include "ash/picker/views/picker_section_view.h"
#include "ash/picker/views/picker_style.h"
#include "base/functional/bind.h"
#include "base/i18n/rtl.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/background.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace ash {
namespace {

constexpr int kSubmenuWidth = 256;
constexpr auto kInsets = gfx::Insets::VH(8, 0);
constexpr int kSubmenuHorizontalOverlap = 4;

std::unique_ptr<views::BubbleBorder> CreateBorder() {
  auto border = std::make_unique<views::BubbleBorder>(
      base::i18n::IsRTL() ? views::BubbleBorder::Arrow::RIGHT_TOP
                          : views::BubbleBorder::Arrow::LEFT_TOP,
      views::BubbleBorder::CHROMEOS_SYSTEM_UI_SHADOW);
  border->SetCornerRadius(kPickerContainerBorderRadius);
  border->SetColor(SK_ColorTRANSPARENT);
  return border;
}

}  // namespace

PickerSubmenuView::PickerSubmenuView(
    const gfx::Rect& anchor_rect,
    std::vector<std::unique_ptr<PickerListItemView>> items) {
  SetShowCloseButton(false);
  set_desired_bounds_delegate(
      base::BindRepeating(&PickerSubmenuView::GetDesiredBounds,
                          base::Unretained(this), anchor_rect));
  SetLayoutManager(std::make_unique<views::BoxLayout>(
                       views::BoxLayout::Orientation::kVertical,
                       /*inside_border_insets=*/kInsets))
      ->set_cross_axis_alignment(
          views::BoxLayout::CrossAxisAlignment::kStretch);
  SetBackground(views::CreateThemedRoundedRectBackground(
      kPickerContainerBackgroundColor, kPickerContainerBorderRadius));

  // Don't allow submenus within submenus.
  section_view_ = AddChildView(std::make_unique<PickerSectionView>(
      kSubmenuWidth, /*asset_fetcher=*/nullptr,
      /*submenu_controller=*/nullptr));

  for (std::unique_ptr<PickerListItemView>& item : items) {
    section_view_->AddListItem(std::move(item));
  }
}

PickerSubmenuView::~PickerSubmenuView() = default;

std::unique_ptr<views::NonClientFrameView>
PickerSubmenuView::CreateNonClientFrameView(views::Widget* widget) {
  auto frame =
      std::make_unique<views::BubbleFrameView>(gfx::Insets(), gfx::Insets());
  frame->SetBubbleBorder(CreateBorder());
  return frame;
}

gfx::Size PickerSubmenuView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  const int preferred_height =
      views::WidgetDelegateView::CalculatePreferredSize(available_size)
          .height();
  return gfx::Size(kSubmenuWidth, preferred_height);
}

views::View* PickerSubmenuView::GetTopItem() {
  return section_view_->GetTopItem();
}

views::View* PickerSubmenuView::GetBottomItem() {
  return section_view_->GetBottomItem();
}

views::View* PickerSubmenuView::GetItemAbove(views::View* item) {
  return section_view_->GetItemAbove(item);
}

views::View* PickerSubmenuView::GetItemBelow(views::View* item) {
  return section_view_->GetItemBelow(item);
}

views::View* PickerSubmenuView::GetItemLeftOf(views::View* item) {
  return section_view_->GetItemLeftOf(item);
}

views::View* PickerSubmenuView::GetItemRightOf(views::View* item) {
  return section_view_->GetItemRightOf(item);
}

bool PickerSubmenuView::ContainsItem(views::View* item) {
  return Contains(item);
}

gfx::Rect PickerSubmenuView::GetDesiredBounds(gfx::Rect anchor_rect) {
  // Inset the anchor rect so that the submenu overlaps slightly with the
  // anchor.
  anchor_rect.Inset(gfx::Insets::VH(0, kSubmenuHorizontalOverlap));

  auto* bubble_frame_view = static_cast<views::BubbleFrameView*>(
      GetWidget()->non_client_view()->frame_view());
  gfx::Rect bounds = bubble_frame_view->GetUpdatedWindowBounds(
      anchor_rect, bubble_frame_view->GetArrow(), GetPreferredSize({}),
      /*adjust_to_fit_available_bounds=*/true);

  // Adjust the bounds to be relative to the parent's bounds.
  const gfx::Rect parent_bounds =
      GetWidget()->parent()->GetWindowBoundsInScreen();
  bounds.Offset(-parent_bounds.OffsetFromOrigin());

  // Shift by the insets to align the first item with the anchor rect.
  bounds.Offset(0, -kInsets.top());
  return bounds;
}

BEGIN_METADATA(PickerSubmenuView)
END_METADATA

}  // namespace ash
