// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/ui/action_view_list_item.h"

#include <memory>

#include "ash/bubble/bubble_utils.h"
#include "ash/style/rounded_container.h"
#include "ash/style/typography.h"
#include "chrome/browser/ash/arc/input_overlay/actions/action.h"
#include "chrome/browser/ash/arc/input_overlay/display_overlay_controller.h"
#include "chrome/browser/ash/arc/input_overlay/ui/edit_labels.h"
#include "chrome/browser/ash/arc/input_overlay/ui/name_tag.h"
#include "chrome/browser/ash/arc/input_overlay/ui/ui_utils.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/views/background.h"
#include "ui/views/layout/table_layout.h"

namespace arc::input_overlay {

ActionViewListItem::ActionViewListItem(DisplayOverlayController* controller,
                                       Action* action)
    : controller_(controller), action_(action) {
  Init();
}

ActionViewListItem::~ActionViewListItem() = default;

void ActionViewListItem::OnActionUpdated() {
  labels_view_->OnActionUpdated();
  labels_name_tag_->SetSubtitle(labels_view_->GetTextForNameTag());
}

void ActionViewListItem::Init() {
  SetUseDefaultFillLayout(true);
  auto* container = AddChildView(std::make_unique<ash::RoundedContainer>());
  container->SetBorderInsets(gfx::Insets::VH(14, 16));
  container->SetBackground(
      views::CreateThemedSolidBackground(cros_tokens::kCrosSysSystemOnBase));
  container->SetLayoutManager(std::make_unique<views::TableLayout>())
      ->AddColumn(/*h_align=*/views::LayoutAlignment::kStart,
                  /*v_align=*/views::LayoutAlignment::kStart,
                  /*horizontal_resize=*/1.0f,
                  /*size_type=*/views::TableLayout::ColumnSize::kUsePreferred,
                  /*fixed_width=*/0, /*min_width=*/0)
      .AddColumn(/*h_align=*/views::LayoutAlignment::kEnd,
                 /*v_align=*/views::LayoutAlignment::kCenter,
                 /*horizontal_resize=*/1.0f,
                 /*size_type=*/views::TableLayout::ColumnSize::kUsePreferred,
                 /*fixed_width=*/0, /*min_width=*/0)
      .AddRows(1, /*vertical_resize=*/views::TableLayout::kFixedSize);

  auto labels_view = EditLabels::CreateEditLabels(controller_, action_);
  // TODO(b/270969479): Replace the hardcoded string.
  labels_name_tag_ = container->AddChildView(
      NameTag::CreateNameTag(u"title", labels_view->GetTextForNameTag()));
  labels_view_ = container->AddChildView(std::move(labels_view));
}

}  // namespace arc::input_overlay
