// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/persistent_desks_bar/persistent_desks_bar_context_menu.h"

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/wm/desks/persistent_desks_bar/persistent_desks_bar_controller.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/view.h"

namespace ash {

PersistentDesksBarContextMenu::PersistentDesksBarContextMenu(
    base::RepeatingClosure on_menu_closed_callback)
    : on_menu_closed_callback_(std::move(on_menu_closed_callback)) {}

PersistentDesksBarContextMenu::~PersistentDesksBarContextMenu() = default;

void PersistentDesksBarContextMenu::ShowContextMenuForViewImpl(
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
      gfx::Rect(source->GetBoundsInScreen().bottom_right(), gfx::Size()),
      views::MenuAnchorPosition::kBubbleBottomRight, source_type);
}

void PersistentDesksBarContextMenu::ExecuteCommand(int command_id,
                                                   int event_flags) {
  auto* shell = Shell::Get();
  switch (static_cast<CommandId>(command_id)) {
    case CommandId::kFeedBack:
      shell->shell_delegate()->OpenFeedbackPageForPersistentDesksBar();
      break;
    case CommandId::kShowOrHideBar:
      shell->persistent_desks_bar_controller()->ToggleEnabledState();
      break;
    default:
      NOTREACHED();
      break;
  }
}

void PersistentDesksBarContextMenu::MenuClosed(ui::SimpleMenuModel* menu) {
  on_menu_closed_callback_.Run();
}

ui::SimpleMenuModel* PersistentDesksBarContextMenu::BuildMenuModel() {
  context_menu_model_ = std::make_unique<ui::SimpleMenuModel>(this);
  context_menu_model_->AddItemWithIcon(
      static_cast<int>(CommandId::kFeedBack),
      l10n_util::GetStringUTF16(
          IDS_ASH_PERSISTENT_DESKS_BAR_CONTEXT_MENU_FEEDBACK),
      ui::ImageModel::FromVectorIcon(kPersistentDesksBarFeedbackIcon,
                                     ui::kColorAshSystemUIMenuIcon));

  auto* bar_controller = Shell::Get()->persistent_desks_bar_controller();
  const bool is_enabled = bar_controller->IsEnabled();
  context_menu_model_->AddItemWithIcon(
      static_cast<int>(CommandId::kShowOrHideBar),
      l10n_util::GetStringUTF16(
          is_enabled
              ? IDS_ASH_PERSISTENT_DESKS_BAR_CONTEXT_MENU_HIDE_DESKS_BAR
              : IDS_ASH_PERSISTENT_DESKS_BAR_CONTEXT_MENU_SHOW_DESKS_BAR),
      ui::ImageModel::FromVectorIcon(is_enabled
                                         ? kPersistentDesksBarNotVisibleIcon
                                         : kPersistentDesksBarVisibleIcon,
                                     ui::kColorAshSystemUIMenuIcon));

  return context_menu_model_.get();
}

}  // namespace ash
