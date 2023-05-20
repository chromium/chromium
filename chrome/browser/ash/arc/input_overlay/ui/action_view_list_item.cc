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

  switch (action_->GetType()) {
    case ActionType::TAP:
      SetActionTapListItem(container);
      break;
    case ActionType::MOVE:
      SetActionMoveListItem(container);
      break;
    default:
      NOTREACHED();
  }
}

ActionViewListItem::~ActionViewListItem() = default;

void ActionViewListItem::SetActionTapListItem(views::View* container) {
  // Set list item as:
  // --------------------------
  // | |Name tag|         |a| |
  // --------------------------
  // TODO(b/270969479): Replace the hardcoded string.
  container->AddChildView(CreateNameTag(u"title", u"sub-title"));
  container->AddChildView(CreateActionTapEditForKeyboard(action_));
}

void ActionViewListItem::SetActionMoveListItem(views::View* container) {
  // Set list item as:
  // -----------------------------
  // | |Name tag|           |w|  |
  // |                    |a|s|d||
  // -----------------------------
  // TODO(b/270969479): Replace the hardcoded string.
  container->AddChildView(CreateNameTag(u"title", u"sub-title"));
  container->AddChildView(CreateActionMoveEditForKeyboard(action_));
}

}  // namespace arc::input_overlay
