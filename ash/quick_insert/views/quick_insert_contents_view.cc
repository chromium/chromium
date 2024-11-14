// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_insert/views/quick_insert_contents_view.h"

#include <limits>
#include <memory>
#include <optional>
#include <utility>

#include "ash/controls/rounded_scroll_bar.h"
#include "ash/quick_insert/views/quick_insert_style.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/border.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/scrollbar/scroll_bar.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/view_class_properties.h"

namespace ash {
namespace {

gfx::Insets GetScrollViewContentsBorderInsets(
    QuickInsertLayoutType layout_type) {
  switch (layout_type) {
    case QuickInsertLayoutType::kMainResultsBelowSearchField:
      return gfx::Insets::TLBR(0, 0, 8, 0);
    case QuickInsertLayoutType::kMainResultsAboveSearchField:
      return gfx::Insets::TLBR(8, 0, 8, 0);
  }
}

gfx::Insets GetQuickInsertScrollBarInsets(QuickInsertLayoutType layout_type) {
  switch (layout_type) {
    case QuickInsertLayoutType::kMainResultsBelowSearchField:
      return gfx::Insets::TLBR(1, 0, 12, 0);
    case QuickInsertLayoutType::kMainResultsAboveSearchField:
      return gfx::Insets::TLBR(12, 0, 1, 0);
  }
}

// Scroll view to contain the main Quick Insert contents.
class QuickInsertScrollView : public views::ScrollView {
  METADATA_HEADER(QuickInsertScrollView, views::ScrollView)

 public:
  QuickInsertScrollView()
      : views::ScrollView(views::ScrollView::ScrollWithLayers::kEnabled) {
    views::Builder<views::ScrollView>(this)
        .ClipHeightTo(0, std::numeric_limits<int>::max())
        .SetDrawOverflowIndicator(false)
        .SetBackgroundColor(std::nullopt)
        .SetHorizontalScrollBarMode(views::ScrollView::ScrollBarMode::kDisabled)
        .BuildChildren();
  }
  QuickInsertScrollView(const QuickInsertScrollView&) = delete;
  QuickInsertScrollView& operator=(const QuickInsertScrollView&) = delete;
  ~QuickInsertScrollView() override = default;
};

BEGIN_METADATA(QuickInsertScrollView)
END_METADATA

}  // namespace

QuickInsertContentsView::QuickInsertContentsView(
    QuickInsertLayoutType layout_type) {
  SetLayoutManager(std::make_unique<views::FillLayout>());

  auto* scroll_view = AddChildView(std::make_unique<QuickInsertScrollView>());
  auto vertical_scroll_bar = std::make_unique<RoundedScrollBar>(
      views::ScrollBar::Orientation::kVertical);
  vertical_scroll_bar->SetInsets(GetQuickInsertScrollBarInsets(layout_type));
  vertical_scroll_bar->SetAlwaysShowThumb(true);
  scroll_view->SetVerticalScrollBar(std::move(vertical_scroll_bar));

  page_container_ = scroll_view->SetContents(
      views::Builder<views::BoxLayoutView>()
          .SetOrientation(views::LayoutOrientation::kVertical)
          .SetCrossAxisAlignment(views::LayoutAlignment::kStretch)
          .SetInsideBorderInsets(GetScrollViewContentsBorderInsets(layout_type))
          .Build());
}

QuickInsertContentsView::~QuickInsertContentsView() = default;

void QuickInsertContentsView::SetActivePage(views::View* view) {
  for (views::View* child : page_container_->children()) {
    child->SetVisible(child == view);
  }
}

BEGIN_METADATA(QuickInsertContentsView)
END_METADATA

}  // namespace ash
