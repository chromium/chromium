// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_insert/views/quick_insert_submenu_view.h"

#include <memory>
#include <utility>

#include "ash/quick_insert/views/quick_insert_list_item_view.h"
#include "ash/quick_insert/views/quick_insert_section_view.h"
#include "ash/quick_insert/views/quick_insert_style.h"
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
  border->set_rounded_corners(
      gfx::RoundedCornersF(kQuickInsertContainerBorderRadius));
  border->SetColor(SK_ColorTRANSPARENT);
  return border;
}

}  // namespace

QuickInsertSubmenuView::QuickInsertSubmenuView(
    const gfx::Rect& anchor_rect,
    std::vector<std::unique_ptr<QuickInsertListItemView>> items) {
  SetShowCloseButton(false);
  set_desired_bounds_delegate(
      base::BindRepeating(&QuickInsertSubmenuView::GetDesiredBounds,
                          base::Unretained(this), anchor_rect));
  SetLayoutManager(std::make_unique<views::BoxLayout>(
                       views::BoxLayout::Orientation::kVertical,
                       /*inside_border_insets=*/kInsets))
      ->set_cross_axis_alignment(
          views::BoxLayout::CrossAxisAlignment::kStretch);
  SetBackground(views::CreateRoundedRectBackground(
      kQuickInsertContainerBackgroundColor, kQuickInsertContainerBorderRadius));

  // Don't allow submenus within submenus.
  section_view_ = AddChildView(std::make_unique<QuickInsertSectionView>(
      kSubmenuWidth, /*asset_fetcher=*/nullptr,
      /*submenu_controller=*/nullptr));

  for (std::unique_ptr<QuickInsertListItemView>& item : items) {
    section_view_->AddListItem(std::move(item));
  }
}

QuickInsertSubmenuView::~QuickInsertSubmenuView() = default;

std::unique_ptr<views::FrameView> QuickInsertSubmenuView::CreateFrameView(
    views::Widget* widget) {
  auto frame =
      std::make_unique<views::BubbleFrameView>(gfx::Insets(), gfx::Insets());
  frame->SetBubbleBorder(CreateBorder());
  return frame;
}

gfx::Size QuickInsertSubmenuView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  const int preferred_height =
      views::WidgetDelegateView::CalculatePreferredSize(available_size)
          .height();
  return gfx::Size(kSubmenuWidth, preferred_height);
}

views::View* QuickInsertSubmenuView::GetTopItem() {
  return section_view_->GetTopItem();
}

views::View* QuickInsertSubmenuView::GetBottomItem() {
  return section_view_->GetBottomItem();
}

views::View* QuickInsertSubmenuView::GetItemAbove(views::View* item) {
  return section_view_->GetItemAbove(item);
}

views::View* QuickInsertSubmenuView::GetItemBelow(views::View* item) {
  return section_view_->GetItemBelow(item);
}

views::View* QuickInsertSubmenuView::GetItemLeftOf(views::View* item) {
  return section_view_->GetItemLeftOf(item);
}

views::View* QuickInsertSubmenuView::GetItemRightOf(views::View* item) {
  return section_view_->GetItemRightOf(item);
}

bool QuickInsertSubmenuView::ContainsItem(views::View* item) {
  return Contains(item);
}

gfx::Rect QuickInsertSubmenuView::GetDesiredBounds(gfx::Rect anchor_rect) {
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

BEGIN_METADATA(QuickInsertSubmenuView)
END_METADATA

}  // namespace ash
