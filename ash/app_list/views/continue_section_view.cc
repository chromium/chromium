// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/continue_section_view.h"

#include <memory>
#include <string>
#include <vector>

#include "ash/app_list/app_list_view_delegate.h"
#include "ash/app_list/views/continue_task_view.h"
#include "ash/bubble/bubble_utils.h"
#include "ash/bubble/simple_grid_layout.h"
#include "base/check.h"
#include "base/strings/string_util.h"
#include "extensions/common/constants.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

namespace ash {
namespace {

// Horizontal space between suggestions in dips.
constexpr int kHorizontalSpacing = 8;

constexpr int kContinueColumnCount = 2;
constexpr int kContinueColumnSpacing = 16;
constexpr int kContinueRowSpacing = 10;

std::unique_ptr<views::Label> CreateLabel(const std::u16string& text) {
  auto label = std::make_unique<views::Label>(text);
  bubble_utils::ApplyStyle(label.get(), bubble_utils::LabelStyle::kBody);
  return label;
}

}  // namespace

ContinueSectionView::ContinueSectionView(AppListViewDelegate* view_delegate)
    : view_delegate_(view_delegate) {
  DCHECK(view_delegate_);

  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      kHorizontalSpacing));
  layout->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kCenter);

  // TODO(https://crbug.com/1204551): Localized strings.
  // TODO(https://crbug.com/1204551): Styling.
  auto* continue_label = AddChildView(CreateLabel(u"Label"));
  continue_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  continue_suggestions_ = AddChildView(std::make_unique<views::View>());
  continue_suggestions_->SetLayoutManager(std::make_unique<SimpleGridLayout>(
      kContinueColumnCount, kContinueColumnSpacing, kContinueRowSpacing));

  for (int i = 0; i < 4; ++i) {
    continue_suggestions_->AddChildView(
        std::make_unique<ContinueTaskView>(u"Item"));
  }
}

ContinueSectionView::~ContinueSectionView() = default;

BEGIN_METADATA(ContinueSectionView, views::View)
END_METADATA

}  // namespace ash
