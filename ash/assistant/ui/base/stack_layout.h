// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_UI_BASE_STACK_LAYOUT_H_
#define ASH_ASSISTANT_UI_BASE_STACK_LAYOUT_H_

#include <map>

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "ui/views/layout/layout_manager_base.h"
#include "ui/views/layout/proposed_layout.h"

namespace ash {

// A layout manager which lays out its views atop each other. This differs from
// FillLayout in that we respect the preferred size of views during layout. It's
// possible to explicitly specify which dimension to respect. In contrast,
// FillLayout will cause its views to match the bounds of the host.
class COMPONENT_EXPORT(ASSISTANT_UI) StackLayout
    : public views::LayoutManagerBase {
 public:
  enum class RespectDimension : uint32_t {
    // Respect width. If enabled, child's preferred width will be used and will
    // be horizontally center positioned. Otherwise, the child will be stretched
    // to match parent width.
    kWidth = 1,
    // Respect height. If enabled, child's preferred height will be used.
    // Otherwise, the child will be stretched to match parent height.
    // Note that the child is always top-aligned.
    kHeight = 1 << 1,
    kAll = kWidth | kHeight,
  };

  enum class VerticalAlignment {
    kCenter = 1,
  };

  StackLayout();

  StackLayout(const StackLayout&) = delete;
  StackLayout& operator=(const StackLayout&) = delete;

  ~StackLayout() override;

  // views::LayoutManagerBase:
  gfx::Size GetPreferredSize(const views::View* host) const override;
  gfx::Size GetPreferredSize(
      const views::View* host,
      const views::SizeBounds& available_size) const override;
  int GetPreferredHeightForWidth(const views::View* host,
                                 int width) const override;
  void SetRespectDimensionForView(views::View* view,
                                  RespectDimension dimension);

  void SetVerticalAlignmentForView(views::View* view,
                                   VerticalAlignment alignment);

 protected:
  // views::LayoutManagerBase:
  bool OnViewRemoved(views::View* host, views::View* view) override;
  views::ProposedLayout CalculateProposedLayout(
      const views::SizeBounds& size_bounds) const override;

 private:
  std::map<views::View*, RespectDimension> respect_dimension_map_;
  std::map<views::View*, VerticalAlignment> vertical_alignment_map_;
};

}  // namespace ash

#endif  // ASH_ASSISTANT_UI_BASE_STACK_LAYOUT_H_
