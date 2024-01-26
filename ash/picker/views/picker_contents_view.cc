// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_contents_view.h"

#include <limits>
#include <memory>
#include <optional>

#include "ash/controls/rounded_scroll_bar.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/view_class_properties.h"

namespace ash {

PickerContentsView::PickerContentsView() {
  SetLayoutManager(std::make_unique<views::FillLayout>());

  auto* scroll_view = AddChildView(std::make_unique<views::ScrollView>());
  scroll_view->ClipHeightTo(0, std::numeric_limits<int>::max());
  scroll_view->SetDrawOverflowIndicator(false);
  scroll_view->SetBackgroundColor(std::nullopt);
  scroll_view->SetHorizontalScrollBarMode(
      views::ScrollView::ScrollBarMode::kDisabled);
  scroll_view->SetVerticalScrollBar(
      std::make_unique<RoundedScrollBar>(/*horizontal=*/false));

  auto page_container = std::make_unique<views::FlexLayoutView>();
  page_container->SetOrientation(views::LayoutOrientation::kVertical);
  page_container->SetCrossAxisAlignment(views::LayoutAlignment::kStretch);
  page_container_ = scroll_view->SetContents(std::move(page_container));
}

PickerContentsView::~PickerContentsView() = default;

void PickerContentsView::SetActivePage(views::View* view) {
  for (views::View* child : page_container_->children()) {
    child->SetVisible(child == view);
  }
}

BEGIN_METADATA(PickerContentsView, views::View)
END_METADATA

}  // namespace ash
