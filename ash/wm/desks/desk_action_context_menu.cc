// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/desk_action_context_menu.h"

#include "ash/resources/vector_icons/vector_icons.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/view.h"

namespace ash {

namespace {

// An enum with identifiers to link context menu items to their associated
// functions.
enum CommandId {
  // Closes target desk and moves its windows to another desk.
  kCombineDesks,
  // Saves target desk in DesksController and gives user option to undo the
  // desk before the desk is fully removed and its windows are closed.
  kCloseAll,
};

}  // namespace

DeskActionContextMenu::DeskActionContextMenu(
    base::RepeatingClosure combine_desks_callback,
    base::RepeatingClosure close_all_callback,
    base::RepeatingClosure on_context_menu_closed_callback)
    : combine_desks_callback_(std::move(combine_desks_callback)),
      close_all_callback_(std::move(close_all_callback)),
      on_context_menu_closed_callback_(
          std::move(on_context_menu_closed_callback)) {}

DeskActionContextMenu::~DeskActionContextMenu() = default;

void DeskActionContextMenu::ExecuteCommand(int command_id, int event_flags) {
  switch (command_id) {
    case CommandId::kCombineDesks:
      combine_desks_callback_.Run();
      break;
    case CommandId::kCloseAll:
      close_all_callback_.Run();
      break;
    default:
      NOTREACHED();
      break;
  }
}

void DeskActionContextMenu::MenuClosed(ui::SimpleMenuModel* menu) {
  on_context_menu_closed_callback_.Run();
}

ui::SimpleMenuModel* DeskActionContextMenu::BuildMenuModel() {
  // TODO(crbug.com/1302030): Localize the strings here.
  context_menu_model_ = std::make_unique<ui::SimpleMenuModel>(this);
  context_menu_model_->AddItemWithIcon(
      CommandId::kCombineDesks, u"Combine with ",
      ui::ImageModel::FromVectorIcon(kCombineDesksIcon,
                                     ui::kColorAshSystemUIMenuIcon));

  context_menu_model_->AddItemWithIcon(
      CommandId::kCloseAll, u"Close desk and windows",
      ui::ImageModel::FromVectorIcon(kMediumOrLargeCloseButtonIcon,
                                     ui::kColorAshSystemUIMenuIcon));

  return context_menu_model_.get();
}

void DeskActionContextMenu::ShowContextMenuForViewImpl(
    views::View* source,
    const gfx::Point& point,
    ui::MenuSourceType source_type) {
  const int run_types = views::MenuRunner::USE_ASH_SYS_UI_LAYOUT |
                        views::MenuRunner::CONTEXT_MENU |
                        views::MenuRunner::FIXED_ANCHOR;

  context_menu_runner_ =
      std::make_unique<views::MenuRunner>(BuildMenuModel(), run_types);

  context_menu_runner_->RunMenuAt(
      source->GetWidget(), /*button_controller=*/nullptr,
      /*bounds=*/gfx::Rect(point, gfx::Size()),
      /*anchor=*/views::MenuAnchorPosition::kBubbleBottomRight, source_type);
}

}  // namespace ash