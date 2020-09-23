// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/holding_space/holding_space_item_chips_container.h"

#include "ash/public/cpp/holding_space/holding_space_constants.h"
#include "ui/views/layout/layout_manager_base.h"
#include "ui/views/layout/proposed_layout.h"

namespace ash {
namespace {

// Need a custom grid layout to facilitate removal of views from the grid,
// which can change the number of rows required. views::GridLayout makes this
// case difficult.
class SimpleGridLayout : public views::LayoutManagerBase {
 public:
  SimpleGridLayout(int column_count, int column_spacing, int row_spacing)
      : column_count_(column_count),
        column_spacing_(column_spacing),
        row_spacing_(row_spacing) {}

  gfx::Size GetChildPreferredSize() const {
    if (cached_child_preferred_size_)
      return *cached_child_preferred_size_;

    if (!host_view()->children().size())
      return gfx::Size();

    cached_child_preferred_size_ =
        host_view()->children()[0]->GetPreferredSize();
    return *cached_child_preferred_size_;
  }

  gfx::Size CalculatePreferredSize() const {
    int total_children = 0;
    for (auto* child : host_view()->children()) {
      if (IsChildIncludedInLayout(child))
        ++total_children;
    }
    // Equivalent to `ceil(children().size() / column_count_)`.
    int number_of_rows = (total_children + column_count_ - 1) / column_count_;

    if (!number_of_rows)
      return gfx::Size();

    // SimpleGridLayout assumes all children have identical sizes.
    int child_height = GetChildPreferredSize().height();
    int child_width = GetChildPreferredSize().width();

    int height =
        (number_of_rows * (row_spacing_ + child_height)) - row_spacing_;
    int width =
        (column_count_ * (child_width + column_spacing_)) - column_spacing_;

    return gfx::Size(width, height);
  }

  views::ProposedLayout CalculateProposedLayout(
      const views::SizeBounds& size_bounds) const override {
    views::ProposedLayout proposed_layout;

    if (size_bounds.is_fully_bounded()) {
      proposed_layout.host_size =
          gfx::Size(size_bounds.width().value(), size_bounds.height().value());
    } else {
      proposed_layout.host_size = CalculatePreferredSize();
    }

    gfx::Size size = GetChildPreferredSize();
    int row = 0;
    int col = 0;
    for (auto* child : host_view()->children()) {
      if (!IsChildIncludedInLayout(child))
        continue;

      int x = (col * (column_spacing_ + size.width()));
      int y = (row * (row_spacing_ + size.height()));
      proposed_layout.child_layouts.push_back(
          views::ChildLayout{child,
                             child->GetVisible(),
                             gfx::Rect(x, y, size.width(), size.height()),
                             {size.width(), size.height()}});

      ++col;
      if (col % column_count_ == 0) {
        ++row;
        col = 0;
      }
    }
    return proposed_layout;
  }

  void OnLayoutChanged() override {
    LayoutManagerBase::OnLayoutChanged();
    cached_child_preferred_size_.reset();
    host_view()->SetPreferredSize(CalculatePreferredSize());
  }

 private:
  mutable base::Optional<gfx::Size> cached_child_preferred_size_;

  const int column_count_;
  const int column_spacing_;
  const int row_spacing_;
};
}  // namespace

HoldingSpaceItemChipsContainer::HoldingSpaceItemChipsContainer() {
  SetLayoutManager(std::make_unique<SimpleGridLayout>(
      kHoldingSpaceChipsPerRow, kHoldingSpaceColumnSpacing,
      kHoldingSpaceRowSpacing));
}

HoldingSpaceItemChipsContainer::~HoldingSpaceItemChipsContainer() = default;

const char* HoldingSpaceItemChipsContainer::GetClassName() const {
  return "HoldingSpaceItemChipsContainer";
}

}  // namespace ash