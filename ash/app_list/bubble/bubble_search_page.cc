// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/bubble/bubble_search_page.h"

#include <limits>
#include <memory>
#include <utility>

#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/layout/box_layout.h"

using views::BoxLayout;

namespace ash {

BubbleSearchPage::BubbleSearchPage() {
  SetUseDefaultFillLayout(true);

  // The entire page scrolls.
  auto* scroll = AddChildView(std::make_unique<views::ScrollView>());
  scroll->ClipHeightTo(0, std::numeric_limits<int>::max());
  scroll->SetDrawOverflowIndicator(false);
  scroll->SetHorizontalScrollBarMode(
      views::ScrollView::ScrollBarMode::kDisabled);

  auto scroll_contents = std::make_unique<views::View>();
  scroll_contents->SetLayoutManager(
      std::make_unique<BoxLayout>(BoxLayout::Orientation::kVertical));

  // TODO(https://crbug.com/1204551): Replace with real search results. For now,
  // create enough labels to force the scroll view to scroll.
  auto* results =
      scroll_contents->AddChildView(std::make_unique<views::View>());
  const int kSpacing = 16;
  results->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(), kSpacing));
  for (int i = 0; i < 20; ++i) {
    results->AddChildView(std::make_unique<views::Label>(u"Result"));
  }

  scroll->SetContents(std::move(scroll_contents));
}

BubbleSearchPage::~BubbleSearchPage() = default;

}  // namespace ash
