// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/bubble/bubble_apps_page.h"

#include <limits>
#include <memory>
#include <utility>

#include "ui/gfx/text_constants.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/box_layout.h"

using views::BoxLayout;

namespace ash {

BubbleAppsPage::BubbleAppsPage() {
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

  // TODO(https://crbug.com/1204551): Localized strings.
  // TODO(https://crbug.com/1204551): Styling.
  auto* continue_label = scroll_contents->AddChildView(
      std::make_unique<views::Label>(u"Continue"));
  continue_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  auto* continue_section =
      scroll_contents->AddChildView(std::make_unique<views::View>());
  // TODO(https://crbug.com/1204551): Extract SimpleGridLayout from
  // HoldingSpaceItemChipsContainer and use it here.
  const int kContinueSpacing = 16;
  auto* continue_layout =
      continue_section->SetLayoutManager(std::make_unique<BoxLayout>(
          BoxLayout::Orientation::kVertical, gfx::Insets(), kContinueSpacing));
  continue_layout->set_cross_axis_alignment(
      BoxLayout::CrossAxisAlignment::kStretch);
  for (int i = 0; i < 4; ++i) {
    continue_section->AddChildView(std::make_unique<views::Label>(u"Task"));
  }

  // TODO(https://crbug.com/1204551): Replace with real recent apps view.
  auto* recent_apps =
      scroll_contents->AddChildView(std::make_unique<views::View>());
  const int kRecentAppsSpacing = 16;
  auto* recent_apps_layout =
      recent_apps->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
          kRecentAppsSpacing));
  recent_apps_layout->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kCenter);
  for (int i = 0; i < 5; ++i) {
    recent_apps->AddChildView(std::make_unique<views::Label>(u"Recent"));
  }

  // TODO(https://crbug.com/1204551): Replace with real apps grid. For now,
  // create enough labels to force the scroll view to scroll.
  auto* all_apps =
      scroll_contents->AddChildView(std::make_unique<views::View>());
  const int kAllAppsSpacing = 16;
  all_apps->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      kAllAppsSpacing));
  for (int i = 0; i < 20; ++i) {
    all_apps->AddChildView(std::make_unique<views::Label>(u"App"));
  }

  scroll->SetContents(std::move(scroll_contents));
}

BubbleAppsPage::~BubbleAppsPage() = default;

}  // namespace ash
