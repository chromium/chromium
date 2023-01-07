// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/desk_action_context_menu.h"

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/view.h"

namespace ash {

DeskActionContextMenu::DeskActionContextMenu(
    const std::u16string& initial_combine_desks_target_name,
    base::RepeatingClosure combine_desks_callback,
    base::RepeatingClosure close_all_callback,
    base::RepeatingClosure on_context_menu_closed_callback)
    : combine_desks_callback_(std::move(combine_desks_callback)),
      close_all_callback_(std::move(close_all_callback)),
      on_context_menu_closed_callback_(
          std::move(on_context_menu_closed_callback)),
      context_menu_model_(this) {
  context_menu_model_.AddItemWithIcon(
      CommandId::kCombineDesks,
      l10n_util::GetStringFUTF16(IDS_ASH_DESKS_COMBINE_DESKS_DESCRIPTION,
                                 initial_combine_desks_target_name),
      ui::ImageModel::FromVectorIcon(kCombineDesksIcon,
                                     ui::kColorAshSystemUIMenuIcon));

  context_menu_model_.AddItemWithIcon(
      CommandId::kCloseAll,
      l10n_util::GetStringUTF16(IDS_ASH_DESKS_CLOSE_ALL_DESCRIPTION),
      ui::ImageModel::FromVectorIcon(kMediumOrLargeCloseButtonIcon,
                                     ui::kColorAshSystemUIMenuIcon));
}

DeskActionContextMenu::~DeskActionContextMenu() = default;

void DeskActionContextMenu::UpdateCombineDesksTargetName(
    const std::u16string& new_combine_desks_target_name) {
  context_menu_model_.SetLabel(
      CommandId::kCombineDesks,
      l10n_util::GetStringFUTF16(IDS_ASH_DESKS_COMBINE_DESKS_DESCRIPTION,
                                 new_combine_desks_target_name));
}

void DeskActionContextMenu::SetCombineDesksMenuItemVisibility(bool visible) {
  context_menu_model_.SetVisibleAt(CommandId::kCombineDesks, visible);
}

void DeskActionContextMenu::MaybeCloseMenu() {
  if (context_menu_runner_)
    context_menu_runner_->Cancel();
}

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

void DeskActionContextMenu::ShowContextMenuForViewImpl(
    views::View* source,
    const gfx::Point& point,
    ui::MenuSourceType source_type) {
  const int run_types = views::MenuRunner::USE_ASH_SYS_UI_LAYOUT |
                        views::MenuRunner::CONTEXT_MENU |
                        views::MenuRunner::FIXED_ANCHOR |
                        views::MenuRunner::SEND_GESTURE_EVENTS_TO_OWNER;

  context_menu_runner_ =
      std::make_unique<views::MenuRunner>(&context_menu_model_, run_types);

  context_menu_runner_->RunMenuAt(
      source->GetWidget(), /*button_controller=*/nullptr,
      /*bounds=*/gfx::Rect(point, gfx::Size()),
      /*anchor=*/views::MenuAnchorPosition::kBubbleBottomRight, source_type);
}

}  // namespace ash