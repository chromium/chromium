// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/views_text_services_context_menu_impl.h"

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/clipboard_history_controller.h"
#include "chromeos/crosapi/mojom/clipboard_history.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/pointer/touch_editing_controller.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/controls/textfield/textfield.h"

namespace ash {

ViewsTextServicesContextMenuImpl::ViewsTextServicesContextMenuImpl(
    ui::SimpleMenuModel* menu,
    views::Textfield* client)
    : views::ViewsTextServicesContextMenuBase(menu, client) {
  if (chromeos::features::IsClipboardHistoryEnabled())
    AddClipboardHistoryMenuOption(menu);
}

ViewsTextServicesContextMenuImpl::~ViewsTextServicesContextMenuImpl() = default;

bool ViewsTextServicesContextMenuImpl::GetAcceleratorForCommandId(
    int command_id,
    ui::Accelerator* accelerator) const {
  if (command_id == IDS_APP_SHOW_CLIPBOARD_HISTORY) {
    *accelerator = ui::Accelerator(ui::VKEY_V, ui::EF_COMMAND_DOWN);
    return true;
  }

  return ViewsTextServicesContextMenuBase::GetAcceleratorForCommandId(
      command_id, accelerator);
}

bool ViewsTextServicesContextMenuImpl::IsCommandIdChecked(
    int command_id) const {
  if (command_id == IDS_APP_SHOW_CLIPBOARD_HISTORY)
    return true;

  return ViewsTextServicesContextMenuBase::IsCommandIdChecked(command_id);
}

bool ViewsTextServicesContextMenuImpl::IsCommandIdEnabled(
    int command_id) const {
  if (command_id == IDS_APP_SHOW_CLIPBOARD_HISTORY)
    return ClipboardHistoryController::Get()->CanShowMenu();

  return ViewsTextServicesContextMenuBase::IsCommandIdEnabled(command_id);
}

void ViewsTextServicesContextMenuImpl::ExecuteCommand(int command_id,
                                                      int event_flags) {
  if (command_id == IDS_APP_SHOW_CLIPBOARD_HISTORY) {
    auto* clipboard_history_controller = ClipboardHistoryController::Get();

    // Calculate the menu source type from `event_flags`.
    ui::MenuSourceType source_type;
    if (event_flags & ui::EF_LEFT_MOUSE_BUTTON)
      source_type = ui::MENU_SOURCE_MOUSE;
    else if (event_flags & ui::EF_FROM_TOUCH)
      source_type = ui::MENU_SOURCE_TOUCH;
    else
      source_type = ui::MENU_SOURCE_KEYBOARD;

    clipboard_history_controller->ShowMenu(
        client()->GetCaretBounds(), source_type,
        crosapi::mojom::ClipboardHistoryControllerShowSource::
            kTextfieldContextMenu);
    return;
  }

  ViewsTextServicesContextMenuBase::ExecuteCommand(command_id, event_flags);
}

bool ViewsTextServicesContextMenuImpl::SupportsCommand(int command_id) const {
  if (command_id == IDS_APP_SHOW_CLIPBOARD_HISTORY)
    return true;

  return ViewsTextServicesContextMenuBase::SupportsCommand(command_id);
}

void ViewsTextServicesContextMenuImpl::AddClipboardHistoryMenuOption(
    ui::SimpleMenuModel* menu) {
  const int index_of_paste =
      menu->GetIndexOfCommandId(ui::TouchEditable::kPaste);

  // Only add the clipboard history menu option when having the menu option
  // for paste.
  if (index_of_paste == -1)
    return;

  const int target_index = index_of_paste + 1;
  menu->InsertItemAt(target_index, IDS_APP_SHOW_CLIPBOARD_HISTORY,
                     l10n_util::GetStringUTF16(IDS_APP_SHOW_CLIPBOARD_HISTORY));
  if (ClipboardHistoryController::Get()->ShouldShowNewFeatureBadge()) {
    menu->SetIsNewFeatureAt(target_index, true);
    ClipboardHistoryController::Get()->MarkNewFeatureBadgeShown();
  }
}

}  // namespace ash
