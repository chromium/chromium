// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/continue_task_view.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include "ash/app_list/app_list_util.h"
#include "ash/app_list/model/search/search_result.h"
#include "ash/bubble/bubble_utils.h"
#include "ash/style/ash_color_provider.h"
#include "base/strings/string_util.h"
#include "extensions/common/constants.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

namespace ash {
namespace {
constexpr int kPreferredWidth = 242;
constexpr int kPreferredHeight = 52;

constexpr int kLabelLeftPadding = 8;
constexpr int kLabelRightPadding = 16;
constexpr int kLabelContainerHeight = 38;

constexpr int kIconSize = 36;
constexpr int kIconVericalPadding = 8;

}  // namespace

ContinueTaskView::ContinueTaskView(SearchResult* task_result)
    : result_(task_result) {
  SetFocusBehavior(FocusBehavior::ALWAYS);
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal));
  GetViewAccessibility().OverrideName(result()->title() + u" " +
                                      result()->details());
  GetViewAccessibility().OverrideRole(ax::mojom::Role::kListItem);

  icon_ = AddChildView(std::make_unique<views::ImageView>());
  icon_->SetVerticalAlignment(views::ImageView::Alignment::kCenter);
  icon_->SetHorizontalAlignment(views::ImageView::Alignment::kCenter);
  icon_->SetBorder(views::CreateEmptyBorder(
      gfx::Insets((kPreferredHeight - kIconSize) / 2, kIconVericalPadding)));
  SetIcon(result()->chip_icon());

  auto* label_container = AddChildView(std::make_unique<views::View>());
  label_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  label_container->SetBorder(views::CreateEmptyBorder(gfx::Insets(
      (kPreferredHeight - kLabelContainerHeight) / 2, kLabelLeftPadding,
      (kPreferredHeight - kLabelContainerHeight) / 2, kLabelRightPadding)));

  title_ = label_container->AddChildView(
      std::make_unique<views::Label>(result()->title()));
  title_->SetAccessibleName(result()->accessible_name());
  title_->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
  subtitle_ = label_container->AddChildView(
      std::make_unique<views::Label>(result()->details()));
  subtitle_->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);

  SetPreferredSize(gfx::Size(kPreferredWidth, kPreferredHeight));
}

ContinueTaskView::~ContinueTaskView() = default;

void ContinueTaskView::OnThemeChanged() {
  views::View::OnThemeChanged();
  bubble_utils::ApplyStyle(title_, bubble_utils::LabelStyle::kBody);
  bubble_utils::ApplyStyle(subtitle_, bubble_utils::LabelStyle::kSubtitle);
}

void ContinueTaskView::SetIcon(const gfx::ImageSkia& icon) {
  gfx::ImageSkia resized(gfx::ImageSkiaOperations::CreateResizedImage(
      icon, skia::ImageOperations::RESIZE_BEST, GetIconSize()));
  icon_->SetImage(resized);
  icon_->SetImageSize(GetIconSize());
}

gfx::Size ContinueTaskView::GetIconSize() const {
  return gfx::Size(kIconSize, kIconSize);
}

BEGIN_METADATA(ContinueTaskView, views::View)
END_METADATA

}  // namespace ash
