// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/birch/birch_bar_view.h"

#include <array>
#include <vector>

#include "ash/birch/birch_item.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/root_window_settings.h"
#include "ash/shelf/shelf.h"
#include "ash/wm/overview/birch/birch_chip_loader_view.h"
#include "ash/wm/window_properties.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/time/time.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/metadata/view_factory_internal.h"
#include "ui/views/view_class_properties.h"

namespace ash {

namespace {

// The spacing between chips and chips rows.
constexpr int kChipSpacing = 8;
// Horizontal paddings of the bar container.
constexpr int kContainerHorizontalPaddingNoShelf = 32;
constexpr int kContainerHorizontalPaddingWithShelf = 64;
// The height of the chips.
constexpr int kChipHeight = 64;
// The optimal chip width for large screen.
constexpr int kOptimalChipWidth = 278;
// The threshold for large screen.
constexpr int kLargeScreenThreshold = 1450;
// The chips row capacity for different layout types.
constexpr unsigned kRowCapacityOf2x2Layout = 2;
constexpr unsigned kRowCapacityOf1x4Layout = 4;

// The delays of chip loading animations corresponding to the chip positions on
// the bar.
constexpr std::array<base::TimeDelta, 4> kLoaderAnimationDelays{
    base::Milliseconds(250), base::Milliseconds(450), base::Milliseconds(600),
    base::Milliseconds(700)};

// The delays of chip reloading animations corresponding to the chip positions
// on the bar.
constexpr std::array<base::TimeDelta, 4> kReloaderAnimationDelays{
    base::Milliseconds(0), base::Milliseconds(200), base::Milliseconds(350),
    base::Milliseconds(450)};

// Calculates the space for each chip according to the available space and
// number of chips.
int GetChipSpace(int available_size, int chips_num) {
  return chips_num
             ? (available_size - (chips_num - 1) * kChipSpacing) / chips_num
             : available_size;
}

// Creates a chips row.
std::unique_ptr<views::BoxLayoutView> CreateChipsRow() {
  return views::Builder<views::BoxLayoutView>()
      .SetMainAxisAlignment(views::BoxLayout::MainAxisAlignment::kStart)
      .SetCrossAxisAlignment(views::BoxLayout::CrossAxisAlignment::kCenter)
      .SetBetweenChildSpacing(kChipSpacing)
      .Build();
}

}  // namespace

BirchBarView::BirchBarView(aura::Window* root_window)
    : root_window_(root_window), chip_size_(GetChipSize()) {
  // Build up a 2 levels nested box layout hierarchy.
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
                       .SetBetweenChildSpacing(kChipSpacing))
      .BuildChildren();
}

BirchBarView::~BirchBarView() = default;

// static
std::unique_ptr<views::Widget> BirchBarView::CreateBirchBarWidget(
    aura::Window* root_window) {
  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.activatable = views::Widget::InitParams::Activatable::kYes;
  params.accept_events = true;
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.context = root_window;
  params.name = "BirchBarWidget";
  params.activatable = views::Widget::InitParams::Activatable::kNo;
  params.init_properties_container.SetProperty(kOverviewUiKey, true);
  params.init_properties_container.SetProperty(kHideInDeskMiniViewKey, true);

  auto widget = std::make_unique<views::Widget>(std::move(params));
  widget->SetContentsView(std::make_unique<BirchBarView>(root_window));
  widget->Show();
  return widget;
}

void BirchBarView::Loading() {
  CHECK(chips_.empty());

  views::AnimationBuilder loading_animation;
  for (const base::TimeDelta& delay : kLoaderAnimationDelays) {
    auto* chip_loader = primary_row_->AddChildView(
        views::Builder<BirchChipLoaderView>()
            .SetPreferredSize(chip_size_)
            .SetDelay(delay)
            .SetType(BirchChipLoaderView::Type::kInit)
            .Build());
    chip_loader->AddAnimationToBuilder(loading_animation);
    chips_.emplace_back(chip_loader);
  }

  Relayout(RelayoutReason::kAddRemoveChip);
}

void BirchBarView::Reloading() {
  const size_t chip_num = chips_.size();
  Clear();

  views::AnimationBuilder reloading_animation;

  for (size_t i = 0; i < chip_num; i++) {
    auto* chip_loader = primary_row_->AddChildView(
        views::Builder<BirchChipLoaderView>()
            .SetPreferredSize(chip_size_)
            .SetDelay(kReloaderAnimationDelays[i])
            .SetType(BirchChipLoaderView::Type::kReload)
            .Build());
    chip_loader->AddAnimationToBuilder(reloading_animation);
    chips_.emplace_back(chip_loader);
  }

  Relayout(RelayoutReason::kAddRemoveChip);
}

void BirchBarView::Shutdown() {
  for (BirchChipButtonBase* chip : chips_) {
    chip->Shutdown();
  }
}

void BirchBarView::UpdateAvailableSpace(int available_space) {
  if (available_space_ == available_space) {
    return;
  }

  available_space_ = available_space;
  Relayout(RelayoutReason::kAvailableSpaceChanged);
}

base::CallbackListSubscription BirchBarView::AddRelayoutCallback(
    RelayoutCallback callback) {
  return relayout_callback_list_.Add(std::move(callback));
}

int BirchBarView::GetChipsNum() const {
  return chips_.size();
}

void BirchBarView::SetupChips(const std::vector<raw_ptr<BirchItem>>& items) {
  // Clear current chips.
  Clear();

  for (auto item : items) {
    chips_.emplace_back(
        primary_row_->AddChildView(views::Builder<BirchChipButton>()
                                       .Init(item)
                                       .SetPreferredSize(chip_size_)
                                       .Build()));
  }

  Relayout(RelayoutReason::kAddRemoveChip);
}

void BirchBarView::AddChip(BirchItem* item) {
  if (static_cast<int>(chips_.size()) == kMaxChipsNum) {
    NOTREACHED() << "The number of birch chips reaches the limit of 4";
    return;
  }

  auto chip = views::Builder<BirchChipButton>()
                  .Init(item)
                  .SetPreferredSize(chip_size_)
                  .Build();

  // Attach the chip to the secondary row if it is not empty, otherwise, to the
  // primary row.
  chips_.emplace_back((secondary_row_ ? secondary_row_ : primary_row_)
                          ->AddChildView(std::move(chip)));
  Relayout(RelayoutReason::kAddRemoveChip);
}

void BirchBarView::RemoveChip(BirchItem* item) {
  auto iter = std::find_if(
      chips_.begin(), chips_.end(),
      [item](BirchChipButtonBase* chip) { return chip->GetItem() == item; });

  if (iter == chips_.end()) {
    return;
  }

  BirchChipButtonBase* chip = *iter;
  chips_.erase(iter);

  // Remove the chip from its owner.
  if (primary_row_->Contains(chip)) {
    primary_row_->RemoveChildViewT(chip);
  } else {
    CHECK(secondary_row_);
    secondary_row_->RemoveChildViewT(chip);
  }

  Relayout(RelayoutReason::kAddRemoveChip);
}

void BirchBarView::Clear() {
  chips_.clear();
  primary_row_->RemoveAllChildViews();
  if (secondary_row_) {
    auto secondary_row = RemoveChildViewT(secondary_row_);
    secondary_row_ = nullptr;
  }
}

int BirchBarView::GetMaximumHeight() const {
  return GetExpectedLayoutType(kMaxChipsNum) == LayoutType::kOneByFour
             ? kChipHeight
             : 2 * kChipHeight + kChipSpacing;
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
  const ShelfAlignment shelf_alignment =
      Shelf::ForWindow(root_window_)->alignment();

  const int left_inset = shelf_alignment == ShelfAlignment::kLeft
                             ? kContainerHorizontalPaddingWithShelf
                             : kContainerHorizontalPaddingNoShelf;
  const int right_inset = shelf_alignment == ShelfAlignment::kRight
                              ? kContainerHorizontalPaddingWithShelf
                              : kContainerHorizontalPaddingNoShelf;
  const int chip_width =
      GetChipSpace(max_display_dim - left_inset - right_inset, kMaxChipsNum);
  return gfx::Size(chip_width, kChipHeight);
}

BirchBarView::LayoutType BirchBarView::GetExpectedLayoutType(
    int chip_num) const {
  // Calculate the expected layout type according to the chip space estimated by
  // current available space and number of chips.
  const int chip_space = GetChipSpace(available_space_, chip_num);
  return chip_space < chip_size_.width() ? LayoutType::kTwoByTwo
                                         : LayoutType::kOneByFour;
}

void BirchBarView::Relayout(RelayoutReason reason) {
  base::ScopedClosureRunner scoped_closure(base::BindOnce(
      &BirchBarView::OnRelayout, base::Unretained(this), reason));

  const size_t primary_size =
      GetExpectedLayoutType(chips_.size()) == LayoutType::kOneByFour
          ? kRowCapacityOf1x4Layout
          : kRowCapacityOf2x2Layout;

  // Create a secondary row for 2x2 layout if there is no secondary row.
  if (primary_size == kRowCapacityOf2x2Layout && !secondary_row_) {
    secondary_row_ = AddChildView(CreateChipsRow());
  }

  // Pop the extra chips from the end of the primary row and push to the head of
  // the secondary row.
  const views::View::Views& chips_in_primary = primary_row_->children();
  while (chips_in_primary.size() > primary_size) {
    secondary_row_->AddChildViewAt(
        primary_row_->RemoveChildViewT(chips_in_primary.back()), 0);
  }

  if (!secondary_row_) {
    return;
  }

  // Pop the chips from the head of the secondary row to the end of the primary
  // row if it still has available space.
  const views::View::Views& chips_in_secondary = secondary_row_->children();
  while (chips_in_primary.size() < primary_size &&
         !chips_in_secondary.empty()) {
    primary_row_->AddChildView(
        secondary_row_->RemoveChildViewT(chips_in_secondary.front()));
  }

  // Remove the secondary row if it is empty.
  if (chips_in_secondary.empty()) {
    auto secondary_row = RemoveChildViewT(secondary_row_);
    secondary_row_ = nullptr;
  }
}

void BirchBarView::OnRelayout(RelayoutReason reason) {
  InvalidateLayout();
  relayout_callback_list_.Notify(reason);
}

BEGIN_METADATA(BirchBarView)
END_METADATA

}  // namespace ash
