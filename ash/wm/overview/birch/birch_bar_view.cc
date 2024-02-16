// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/birch/birch_bar_view.h"

#include "ash/public/cpp/shelf_types.h"
#include "ash/root_window_settings.h"
#include "ash/shelf/shelf.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/views/metadata/view_factory_internal.h"
#include "ui/views/view_class_properties.h"

namespace ash {

namespace {

// The capacity of chips bar.
constexpr int kMaxChipsNum = 4;
// The spacing between chips and chips rows.
constexpr int kChipSpacing = 8;
// Horizontal paddings of the bar container.
constexpr int kContainerHorizontalPaddingNoShelf = 32;
constexpr int kContainerHorizontalPaddingWithShelf = 64;
// The bottom padding of bar container.
constexpr int kContainerBottomPadding = 16;
// The height of the chips.
constexpr int kChipHeight = 64;
// The optimal chip width for large screen.
constexpr int kOptimalChipWidth = 278;
// The threshold for large screen.
constexpr int kLargeScreenThreshold = 1450;

// Calculate the space for each chip according to the available space and
// number of chips.
int GetChipSpace(int available_size, int chips_num) {
  return chips_num
             ? (available_size - (chips_num - 1) * kChipSpacing) / chips_num
             : available_size;
}

}  // namespace

BirchBarView::BirchBarView(aura::Window* root_window)
    : root_window_(root_window), chip_size_(GetChipSize()) {
  // Build up a 3 levels nested box layout hierarchy.
  using MainAxisAlignment = views::BoxLayout::MainAxisAlignment;
  using CrossAxisAlignment = views::BoxLayout::CrossAxisAlignment;
  views::Builder<views::BoxLayoutView>(this)
      .SetOrientation(views::BoxLayout::Orientation::kVertical)
      .SetMainAxisAlignment(MainAxisAlignment::kCenter)
      .SetCrossAxisAlignment(CrossAxisAlignment::kStart)
      .SetBetweenChildSpacing(kChipSpacing)
      .AddChildren(views::Builder<views::BoxLayoutView>()
                       .CopyAddressTo(&primary_row_)
                       .SetMainAxisAlignment(MainAxisAlignment::kStart)
                       .SetCrossAxisAlignment(CrossAxisAlignment::kCenter)
                       .SetBetweenChildSpacing(kChipSpacing),
                   views::Builder<views::BoxLayoutView>()
                       .CopyAddressTo(&secondary_row_)
                       .SetMainAxisAlignment(MainAxisAlignment::kStart)
                       .SetCrossAxisAlignment(CrossAxisAlignment::kCenter)
                       .SetBetweenChildSpacing(kChipSpacing))
      .BuildChildren();
}

BirchBarView::~BirchBarView() = default;

gfx::Insets BirchBarView::GetContainerPaddings() const {
  // Calculate the insets according to the shelf position.
  const ShelfAlignment shelf_alignment =
      Shelf::ForWindow(root_window_)->alignment();
  const int left_inset = shelf_alignment == ShelfAlignment::kLeft
                             ? kContainerHorizontalPaddingWithShelf
                             : kContainerHorizontalPaddingNoShelf;
  const int right_inset = shelf_alignment == ShelfAlignment::kRight
                              ? kContainerHorizontalPaddingWithShelf
                              : kContainerHorizontalPaddingNoShelf;
  return gfx::Insets::TLBR(0, left_inset, kContainerBottomPadding, right_inset);
}

void BirchBarView::UpdateAvailableSpace(int available_space) {
  if (available_space_ == available_space) {
    return;
  }

  available_space_ = available_space;
  Relayout();
}

void BirchBarView::AddChip(
    const ui::ImageModel& icon,
    const std::u16string& title,
    const std::u16string& sub_title,
    views::Button::PressedCallback callback,
    std::optional<std::u16string> button_title,
    std::optional<views::Button::PressedCallback> button_callback) {
  if (static_cast<int>(chips_.size()) == kMaxChipsNum) {
    NOTREACHED() << "The number of birch chips reaches the limit of 4";
    return;
  }

  auto chip = views::Builder<BirchChipButton>()
                  .SetIconImage(icon)
                  .SetTitleText(title)
                  .SetSubtitleText(sub_title)
                  .SetCallback(std::move(callback))
                  .SetDelegate(this)
                  .SetPreferredSize(chip_size_)
                  .Build();
  if (button_title.has_value() && button_callback.has_value()) {
    chip->SetActionButton(button_title.value(),
                          std::move(button_callback.value()));
  }

  // Attach the chip to the secondary row if it is not empty, otherwise, to the
  // primary row.
  chips_.emplace_back(
      (secondary_row_->children().empty() ? primary_row_ : secondary_row_)
          ->AddChildView(std::move(chip)));
  Relayout();
}

void BirchBarView::RemoveChip(BirchChipButton* chip) {
  CHECK(base::Contains(chips_, chip));

  base::Erase(chips_, chip);
  // Remove the chip from its owner.
  if (primary_row_->Contains(chip)) {
    primary_row_->RemoveChildViewT(chip);
  } else {
    secondary_row_->RemoveChildViewT(chip);
  }
  Relayout();
}

gfx::Size BirchBarView::GetChipSize() const {
  const gfx::Rect display_bounds = display::Screen::GetScreen()
                                       ->GetDisplayNearestWindow(root_window_)
                                       .bounds();
  // Always use the longest side of the display to calculate the chip width.
  const int max_display_dim =
      std::max(display_bounds.width(), display_bounds.height());

  // When in a large screen, the optimal chip width is used.
  if (max_display_dim > kLargeScreenThreshold) {
    return gfx::Size(kOptimalChipWidth, kChipHeight);
  }

  // Otherwise, the bar tends to fill the longest side of the display with 4
  // chips.
  const int horizontal_insets = GetContainerPaddings().width();
  const int chip_width =
      GetChipSpace(max_display_dim - horizontal_insets, kMaxChipsNum);
  return gfx::Size(chip_width, kChipHeight);
}

BirchBarView::LayoutType BirchBarView::GetExpectedLayoutType() const {
  // Calculate the expected layout type according to the chip space estimated by
  // current available space and number of chips.
  const int chip_space = GetChipSpace(available_space_, chips_.size());
  return chip_space < chip_size_.width() ? LayoutType::kTwoByTwo
                                         : LayoutType::kOneByFour;
}

void BirchBarView::Relayout() {
  const size_t primary_size =
      GetExpectedLayoutType() == LayoutType::kOneByFour ? 4u : 2u;

  // Pop the extra chips from the end of the primary row and push to the head of
  // the secondary row.
  const views::View::Views& chips_in_primary = primary_row_->children();
  while (chips_in_primary.size() > primary_size) {
    secondary_row_->AddChildViewAt(
        primary_row_->RemoveChildViewT(chips_in_primary.back()), 0);
  }

  // Pop the chips from the head of the secondary row to the end of the primary
  // row if it still has available space.
  const views::View::Views& chips_in_secondary = secondary_row_->children();
  while (chips_in_primary.size() < primary_size &&
         !chips_in_secondary.empty()) {
    primary_row_->AddChildView(
        secondary_row_->RemoveChildViewT(chips_in_secondary.front()));
  }

  InvalidateLayout();
}

BEGIN_METADATA(BirchBarView)
END_METADATA

}  // namespace ash
