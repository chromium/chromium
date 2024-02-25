// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/views_text_services_context_menu_ash.h"

#include "ash/public/cpp/clipboard_history_controller.h"
#include "base/functional/bind.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/crosapi/mojom/clipboard_history.mojom.h"
#include "chromeos/ui/clipboard_history/clipboard_history_submenu_model.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/menu_source_utils.h"
#include "ui/base/pointer/touch_editing_controller.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/controls/textfield/textfield.h"

namespace ash {

ViewsTextServicesContextMenuAsh::ViewsTextServicesContextMenuAsh(
    ui::SimpleMenuModel* menu,
    views::Textfield* client)
    : views::ViewsTextServicesContextMenuBase(menu, client) {
  // If the menu has a paste option, add a clipboard history option as well.
  const std::optional<size_t> paste_index =
      menu->GetIndexOfCommandId(ui::TouchEditable::kPaste);

  if (!paste_index.has_value()) {
    return;
  }

  const size_t target_index = paste_index.value() + 1;
  const int clipboard_history_string_id = GetClipboardHistoryStringId();

  // If the clipboard history refresh feature is enabled, insert a submenu of
  // clipboard history descriptors; otherwise, insert a menu option to trigger
  // the clipboard history menu.
  // NOTE: The string ID is reused as the command ID when inserting a menu item.
  // It is because Ash code does not have access to Chrome code where the IDC
  // commands are defined.
  if (chromeos::features::IsClipboardHistoryRefreshEnabled()) {
    // `submenu_model_` is a class member. Therefore, it is safe to use `this`
    // pointer in the callback.
    submenu_model_ = chromeos::clipboard_history::ClipboardHistorySubmenuModel::
        CreateClipboardHistorySubmenuModel(
            crosapi::mojom::ClipboardHistoryControllerShowSource::
                kTextfieldContextSubmenu,
            base::BindRepeating(
                &ViewsTextServicesContextMenuAsh::ShowClipboardHistoryMenu,
                base::Unretained(this)));

    menu->InsertSubMenuWithStringIdAt(target_index, clipboard_history_string_id,
                                      clipboard_history_string_id,
                                      submenu_model_.get());
  } else {
    menu->InsertItemWithStringIdAt(target_index, clipboard_history_string_id,
                                   clipboard_history_string_id);
  }
}

ViewsTextServicesContextMenuAsh::~ViewsTextServicesContextMenuAsh() = default;

bool ViewsTextServicesContextMenuAsh::GetAcceleratorForCommandId(
    int command_id,
    ui::Accelerator* accelerator) const {
  if (command_id == IDS_APP_SHOW_CLIPBOARD_HISTORY) {
    // When the clipboard history refresh feature is enabled,
    // `IDS_APP_SHOW_CLIPBOARD_HISTORY` is in the clipboard history submenu.
    // Therefore, the code below should not be executed.
    CHECK(!chromeos::features::IsClipboardHistoryRefreshEnabled());

    *accelerator = ui::Accelerator(ui::VKEY_V, ui::EF_COMMAND_DOWN);
    return true;
  }

  return ViewsTextServicesContextMenuBase::GetAcceleratorForCommandId(
      command_id, accelerator);
}

bool ViewsTextServicesContextMenuAsh::IsCommandIdChecked(int command_id) const {
  if (command_id == GetClipboardHistoryStringId()) {
    return true;
  }

  return ViewsTextServicesContextMenuBase::IsCommandIdChecked(command_id);
}

bool ViewsTextServicesContextMenuAsh::IsCommandIdEnabled(int command_id) const {
  if (command_id == GetClipboardHistoryStringId()) {
    return ClipboardHistoryController::Get()->HasAvailableHistoryItems();
  }

  return ViewsTextServicesContextMenuBase::IsCommandIdEnabled(command_id);
}

void ViewsTextServicesContextMenuAsh::ExecuteCommand(int command_id,
                                                     int event_flags) {
  if (command_id == IDS_APP_SHOW_CLIPBOARD_HISTORY) {
    // This code path is only executed when the clipboard history refresh
    // feature is disabled. When the feature is enabled, the menu option
    // corresponding to `IDS_APP_SHOW_CLIPBOARD_HISTORY` is added to a submenu.
    CHECK(!chromeos::features::IsClipboardHistoryRefreshEnabled());

    ShowClipboardHistoryMenu(event_flags);
    return;
  }

  ViewsTextServicesContextMenuBase::ExecuteCommand(command_id, event_flags);
}

bool ViewsTextServicesContextMenuAsh::SupportsCommand(int command_id) const {
  if (command_id == GetClipboardHistoryStringId()) {
    return true;
  }

  return ViewsTextServicesContextMenuBase::SupportsCommand(command_id);
}

void ViewsTextServicesContextMenuAsh::ShowClipboardHistoryMenu(
    int event_flags) {
  ClipboardHistoryController::Get()->ShowMenu(
      client()->GetCaretBounds(), ui::GetMenuSourceType(event_flags),
      crosapi::mojom::ClipboardHistoryControllerShowSource::
          kTextfieldContextMenu);
}

}  // namespace ash
