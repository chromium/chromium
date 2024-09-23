// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/base/stack_layout.h"

#include <algorithm>
#include <numeric>

#include "base/ranges/algorithm.h"
#include "ui/views/view.h"

namespace ash {

StackLayout::StackLayout() = default;

StackLayout::~StackLayout() = default;

bool StackLayout::OnViewRemoved(views::View* host, views::View* view) {
  DCHECK(view);
  respect_dimension_map_.erase(view);
  vertical_alignment_map_.erase(view);
  return views::LayoutManagerBase::OnViewRemoved(host, view);
}

gfx::Size StackLayout::GetPreferredSize(const views::View* host) const {
  return GetPreferredSize(host, {});
}

gfx::Size StackLayout::GetPreferredSize(
    const views::View* host,
    const views::SizeBounds& available_size) const {
  return std::transform_reduce(
      host->children().cbegin(), host->children().cend(), gfx::Size(),
      [](gfx::Size a, const gfx::Size b) {
        a.SetToMax(b);
        return a;
      },
      [&available_size](const views::View* v) {
        return v->GetPreferredSize(available_size);
      });
}

int StackLayout::GetPreferredHeightForWidth(const views::View* host,
                                            int width) const {
  const auto& children = host->children();
  if (children.empty())
    return 0;
  std::vector<int> heights(children.size());
  base::ranges::transform(
      children, heights.begin(),
      [width](const views::View* v) { return v->GetHeightForWidth(width); });
  return *std::max_element(heights.cbegin(), heights.cend());
}

views::ProposedLayout StackLayout::CalculateProposedLayout(
    const views::SizeBounds& size_bounds) const {
  const int host_width =
      size_bounds.width().is_bounded() ? size_bounds.width().value() : 0;
  const int host_height =
      size_bounds.height().is_bounded() ? size_bounds.height().value() : 0;
  views::ProposedLayout layouts;

  for (views::View* child : host_view()->children()) {
    if (!IsChildIncludedInLayout(child)) {
      continue;
    }
    int child_width = host_width;
    int child_height = host_height;

    int child_x = 0;
    uint32_t dimension = static_cast<uint32_t>(RespectDimension::kAll);

    if (auto iter = respect_dimension_map_.find(child);
        iter != respect_dimension_map_.end()) {
      dimension = static_cast<uint32_t>(iter->second);
    }

    if (dimension & static_cast<uint32_t>(RespectDimension::kWidth)) {
      child_width = std::min(child->GetPreferredSize().width(), host_width);
      child_x = (host_width - child_width) / 2;
    }

    if (dimension & static_cast<uint32_t>(RespectDimension::kHeight))
      child_height = child->GetHeightForWidth(child_width);

    int child_y = 0;
    auto iter = vertical_alignment_map_.find(child);
    if (iter != vertical_alignment_map_.end()) {
      VerticalAlignment vertical_alignment = iter->second;
      if (vertical_alignment == VerticalAlignment::kCenter)
        child_y = std::max(0, (host_height - child_height) / 2);
    }

    layouts.child_layouts.emplace_back(
        child, child->GetVisible(),
        gfx::Rect(child_x, child_y, child_width, child_height), size_bounds);
  }
  layouts.host_size = gfx::Size(host_width, host_height);
  return layouts;
}

void StackLayout::SetRespectDimensionForView(views::View* view,
                                             RespectDimension dimension) {
  CHECK(host_view() && view->parent() == host_view());
  respect_dimension_map_[view] = dimension;
}

void StackLayout::SetVerticalAlignmentForView(views::View* view,
                                              VerticalAlignment alignment) {
  CHECK(host_view() && view->parent() == host_view());
  vertical_alignment_map_[view] = alignment;
}

}  // namespace ash
