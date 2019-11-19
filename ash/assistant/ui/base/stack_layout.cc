// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/base/stack_layout.h"

#include <numeric>

#include "ui/views/view.h"

namespace ash {

StackLayout::StackLayout() = default;

StackLayout::~StackLayout() = default;

void StackLayout::Installed(views::View* host) {
  host_ = host;
}

void StackLayout::ViewRemoved(views::View* host, views::View* view) {
  DCHECK(view);
  respect_dimension_map_.erase(view);
  vertical_alignment_map_.erase(view);
}

gfx::Size StackLayout::GetPreferredSize(const views::View* host) const {
  return std::accumulate(host->children().cbegin(), host->children().cend(),
                         gfx::Size(), [](gfx::Size size, const auto* v) {
                           size.SetToMax(v->GetPreferredSize());
                           return size;
                         });
}

int StackLayout::GetPreferredHeightForWidth(const views::View* host,
                                            int width) const {
  const auto& children = host->children();
  if (children.empty())
    return 0;
  std::vector<int> heights(children.size());
  std::transform(
      children.cbegin(), children.cend(), heights.begin(),
      [width](const views::View* v) { return v->GetHeightForWidth(width); });
  return *std::max_element(heights.cbegin(), heights.cend());
}

void StackLayout::Layout(views::View* host) {
  const int host_width = host->GetContentsBounds().width();
  const int host_height = host->GetContentsBounds().height();

  for (auto* child : host->children()) {
    int child_width = host_width;
    int child_height = host_height;

    int child_x = 0;
    uint32_t dimension = static_cast<uint32_t>(RespectDimension::kAll);

    if (respect_dimension_map_.find(child) != respect_dimension_map_.end())
      dimension = static_cast<uint32_t>(respect_dimension_map_[child]);

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

    child->SetBounds(child_x, child_y, child_width, child_height);
  }
}

void StackLayout::SetRespectDimensionForView(views::View* view,
                                             RespectDimension dimension) {
  DCHECK(host_ && view->parent() == host_);
  respect_dimension_map_[view] = dimension;
}

void StackLayout::SetVerticalAlignmentForView(views::View* view,
                                              VerticalAlignment alignment) {
  DCHECK(host_ && view->parent() == host_);
  vertical_alignment_map_[view] = alignment;
}

}  // namespace ash
