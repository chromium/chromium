// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/views_text_services_context_menu_ash.h"

#include "ash/public/cpp/clipboard_history_controller.h"
#include "chromeos/crosapi/mojom/clipboard_history.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/pointer/touch_editing_controller.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/controls/textfield/textfield.h"

namespace ash {

ViewsTextServicesContextMenuAsh::ViewsTextServicesContextMenuAsh(
    ui::SimpleMenuModel* menu,
    views::Textfield* client)
    : views::ViewsTextServicesContextMenuBase(menu, client) {
  // If the menu has a paste option, add a clipboard history option as well.
  const absl::optional<size_t> paste_index =
      menu->GetIndexOfCommandId(ui::TouchEditable::kPaste);

  if (!paste_index.has_value())
    return;

  const size_t target_index = paste_index.value() + 1;
  menu->InsertItemAt(target_index, IDS_APP_SHOW_CLIPBOARD_HISTORY,
                     l10n_util::GetStringUTF16(IDS_APP_SHOW_CLIPBOARD_HISTORY));
}

ViewsTextServicesContextMenuAsh::~ViewsTextServicesContextMenuAsh() = default;

bool ViewsTextServicesContextMenuAsh::GetAcceleratorForCommandId(
    int command_id,
    ui::Accelerator* accelerator) const {
  if (command_id == IDS_APP_SHOW_CLIPBOARD_HISTORY) {
    *accelerator = ui::Accelerator(ui::VKEY_V, ui::EF_COMMAND_DOWN);
    return true;
  }

  return ViewsTextServicesContextMenuBase::GetAcceleratorForCommandId(
      command_id, accelerator);
}

bool ViewsTextServicesContextMenuAsh::IsCommandIdChecked(int command_id) const {
  if (command_id == IDS_APP_SHOW_CLIPBOARD_HISTORY)
    return true;

  return ViewsTextServicesContextMenuBase::IsCommandIdChecked(command_id);
}

bool ViewsTextServicesContextMenuAsh::IsCommandIdEnabled(int command_id) const {
  if (command_id == IDS_APP_SHOW_CLIPBOARD_HISTORY)
    return ClipboardHistoryController::Get()->CanShowMenu();

  return ViewsTextServicesContextMenuBase::IsCommandIdEnabled(command_id);
}

void ViewsTextServicesContextMenuAsh::ExecuteCommand(int command_id,
                                                     int event_flags) {
  if (command_id == IDS_APP_SHOW_CLIPBOARD_HISTORY) {
    auto* clipboard_history_controller = ClipboardHistoryController::Get();

    // Calculate the menu source type from `event_flags`.
    ui::MenuSourceType source_type;
    if (event_flags & ui::EF_LEFT_MOUSE_BUTTON) {
      source_type = ui::MENU_SOURCE_MOUSE;
    } else if (event_flags & ui::EF_FROM_TOUCH) {
      source_type = ui::MENU_SOURCE_TOUCH;
    } else {
      source_type = ui::MENU_SOURCE_KEYBOARD;
    }

    clipboard_history_controller->ShowMenu(
        client()->GetCaretBounds(), source_type,
        crosapi::mojom::ClipboardHistoryControllerShowSource::
            kTextfieldContextMenu);
    return;
  }

  ViewsTextServicesContextMenuBase::ExecuteCommand(command_id, event_flags);
}

bool ViewsTextServicesContextMenuAsh::SupportsCommand(int command_id) const {
  if (command_id == IDS_APP_SHOW_CLIPBOARD_HISTORY)
    return true;

  return ViewsTextServicesContextMenuBase::SupportsCommand(command_id);
}

}  // namespace ash
