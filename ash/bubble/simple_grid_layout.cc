// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/bubble/simple_grid_layout.h"

#include "ui/views/view.h"

namespace ash {

SimpleGridLayout::SimpleGridLayout(int column_count,
                                   std::optional<int> column_spacing,
                                   std::optional<int> row_spacing)
    : column_count_(column_count),
      column_spacing_(column_spacing),
      row_spacing_(row_spacing) {}

SimpleGridLayout::~SimpleGridLayout() = default;

views::ProposedLayout SimpleGridLayout::CalculateProposedLayout(
    const views::SizeBounds& size_bounds) const {
  views::ProposedLayout proposed_layout;

  auto calculate_spacing = [](const views::SizeBound& bound, int child_size,
                              int child_count) {
    if (!bound.is_bounded()) {
      return 0;
    }
    return std::max(
        0, (bound.value() - child_size * child_count) / (child_count - 1));
  };

  const gfx::Size child_size = GetChildPreferredSize();

  const int column_spacing = column_spacing_.value_or(calculate_spacing(
      size_bounds.width(), child_size.width(), column_count_));

  const int row_spacing = row_spacing_.value_or(0);

  proposed_layout.host_size =
      CalculatePreferredSize(column_spacing, row_spacing);

  int row = 0;
  int col = 0;
  for (views::View* child : host_view()->children()) {
    if (!IsChildIncludedInLayout(child))
      continue;

    int x = col * (column_spacing + child_size.width());
    int y = row * (row_spacing + child_size.height());
    proposed_layout.child_layouts.push_back(views::ChildLayout{
        child,
        child->GetVisible(),
        gfx::Rect(x, y, child_size.width(), child_size.height()),
        {child_size.width(), child_size.height()}});

    ++col;
    if (col % column_count_ == 0) {
      ++row;
      col = 0;
    }
  }
  return proposed_layout;
}

void SimpleGridLayout::OnLayoutChanged() {
  cached_child_preferred_size_.reset();
  LayoutManagerBase::OnLayoutChanged();
}

gfx::Size SimpleGridLayout::GetPreferredSize(
    const views::View* host,
    const views::SizeBounds& available_bounds) const {
  // If column spacing is forced, the preferred layout size does not depend on
  // the `available_bounds`. Using `GetPreferredSize(const view::View*)` will
  // avoid some calculation, as the base class caches preferred size calculation
  // results.
  if (column_spacing_) {
    return views::LayoutManagerBase::GetPreferredSize(host);
  }
  return views::LayoutManagerBase::GetPreferredSize(host, available_bounds);
}

gfx::Size SimpleGridLayout::GetChildPreferredSize() const {
  if (cached_child_preferred_size_)
    return *cached_child_preferred_size_;

  if (!host_view()->children().size())
    return gfx::Size();

  for (views::View* child : host_view()->children()) {
    if (IsChildIncludedInLayout(child)) {
      cached_child_preferred_size_ = child->GetPreferredSize();
      return *cached_child_preferred_size_;
    }
  }

  return gfx::Size();
}

gfx::Size SimpleGridLayout::CalculatePreferredSize(int column_spacing,
                                                   int row_spacing) const {
  int total_children = 0;
  for (views::View* child : host_view()->children()) {
    if (IsChildIncludedInLayout(child))
      ++total_children;
  }
  // Equivalent to `ceil(children().size() / column_count_)`.
  const int number_of_rows =
      (total_children + column_count_ - 1) / column_count_;

  if (!number_of_rows)
    return gfx::Size();

  // `SimpleGridLayout` assumes all children have identical sizes.
  const int child_height = GetChildPreferredSize().height();
  const int child_width = GetChildPreferredSize().width();

  const int height =
      (number_of_rows * (row_spacing + child_height)) - row_spacing;
  const int width =
      (column_count_ * (child_width + column_spacing)) - column_spacing;

  return gfx::Size(width, height);
}

}  // namespace ash
