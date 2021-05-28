// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/bubble/bubble_apps_page.h"

#include <limits>
#include <memory>
#include <string>
#include <utility>

#include "ash/app_list/bubble/scrollable_apps_grid_view.h"
#include "ash/bubble/bubble_utils.h"
#include "base/check.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/box_layout.h"

using views::BoxLayout;

namespace ash {
namespace {

std::unique_ptr<views::Label> CreateLabel(const std::u16string& text) {
  auto label = std::make_unique<views::Label>(text);
  bubble_utils::ApplyStyle(label.get(), bubble_utils::LabelStyle::kBody);
  return label;
}

}  // namespace

BubbleAppsPage::BubbleAppsPage(AppListViewDelegate* view_delegate) {
  DCHECK(view_delegate);

  SetUseDefaultFillLayout(true);

  // The entire page scrolls.
  auto* scroll = AddChildView(std::make_unique<views::ScrollView>());
  scroll->ClipHeightTo(0, std::numeric_limits<int>::max());
  scroll->SetDrawOverflowIndicator(false);
  scroll->SetHorizontalScrollBarMode(
      views::ScrollView::ScrollBarMode::kDisabled);
  // Don't paint a background. The bubble already has one.
  scroll->SetBackgroundColor(absl::nullopt);

  auto scroll_contents = std::make_unique<views::View>();
  auto* layout = scroll_contents->SetLayoutManager(
      std::make_unique<BoxLayout>(BoxLayout::Orientation::kVertical));
  layout->set_cross_axis_alignment(BoxLayout::CrossAxisAlignment::kStretch);

  // TODO(https://crbug.com/1204551): Localized strings.
  // TODO(https://crbug.com/1204551): Styling.
  auto* continue_label = scroll_contents->AddChildView(CreateLabel(u"Label"));
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
    continue_section->AddChildView(CreateLabel(u"Item"));
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
    recent_apps->AddChildView(CreateLabel(u"Item"));
  }

  // All apps section.
  auto* apps_grid =
      scroll_contents->AddChildView(std::make_unique<ScrollableAppsGridView>(
          view_delegate, /*folder_delegate=*/nullptr));
  apps_grid->Init();
  AppListModel* model = view_delegate->GetModel();
  apps_grid->SetModel(model);
  apps_grid->SetItemList(model->top_level_item_list());
  apps_grid->ResetForShowApps();

  scroll->SetContents(std::move(scroll_contents));
}

BubbleAppsPage::~BubbleAppsPage() = default;

}  // namespace ash
