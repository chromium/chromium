// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_BUBBLE_SIMPLE_GRID_LAYOUT_H_
#define ASH_BUBBLE_SIMPLE_GRID_LAYOUT_H_

#include "ash/ash_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/layout/layout_manager_base.h"
#include "ui/views/layout/proposed_layout.h"

namespace ash {

// A custom grid layout that facilitates the removal of views from the grid,
// which can change the number of rows required. `views::GridLayout` makes this
// case difficult. `SimpleGridLayout` assumes all children have identical sizes.
class ASH_EXPORT SimpleGridLayout : public views::LayoutManagerBase {
 public:
  SimpleGridLayout(int column_count, int column_spacing, int row_spacing);
  SimpleGridLayout(const SimpleGridLayout&) = delete;
  SimpleGridLayout& operator=(const SimpleGridLayout&) = delete;
  ~SimpleGridLayout() override;

 protected:
  // views::LayoutManagerBase:
  views::ProposedLayout CalculateProposedLayout(
      const views::SizeBounds& size_bounds) const override;
  void OnLayoutChanged() override;

 private:
  gfx::Size GetChildPreferredSize() const;
  gfx::Size CalculatePreferredSize() const;

  mutable absl::optional<gfx::Size> cached_child_preferred_size_;

  const int column_count_;
  const int column_spacing_;
  const int row_spacing_;
};

}  // namespace ash

#endif  // ASH_BUBBLE_SIMPLE_GRID_LAYOUT_H_
