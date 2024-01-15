// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/views_text_services_context_menu_lacros.h"

#include "chrome/app/chrome_command_ids.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/crosapi/mojom/clipboard_history.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "chromeos/ui/clipboard_history/clipboard_history_submenu_model.h"
#include "chromeos/ui/clipboard_history/clipboard_history_util.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/menu_source_utils.h"
#include "ui/base/pointer/touch_editing_controller.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/controls/views_text_services_context_menu_base.h"

namespace {

bool IsClipboardHistoryLacrosServiceAvailable() {
  auto* const service = chromeos::LacrosService::Get();
  return service && service->IsAvailable<crosapi::mojom::ClipboardHistory>();
}

bool IsClipboardHistoryEmpty() {
  ui::DataTransferEndpoint dte(ui::EndpointType::kClipboardHistory);
  std::vector<std::u16string> types;
  ui::Clipboard::GetForCurrentThread()->ReadAvailableTypes(
      ui::ClipboardBuffer::kCopyPaste, &dte, &types);
  return types.empty();
}

}  // namespace

namespace crosapi {

ViewsTextServicesContextMenuLacros::ViewsTextServicesContextMenuLacros(
    ui::SimpleMenuModel* menu,
    views::Textfield* client)
    : views::ViewsTextServicesContextMenuBase(menu, client) {
  if (!IsClipboardHistoryLacrosServiceAvailable())
    return;

  // If the menu has a paste option, add a clipboard history option as well.
  const std::optional<size_t> paste_index =
      menu->GetIndexOfCommandId(ui::TouchEditable::kPaste);

  if (!paste_index.has_value())
    return;

  const size_t target_index = paste_index.value() + 1;
  const int clipboard_history_string_id = GetClipboardHistoryStringId();

  // If the clipboard history refresh feature is enabled, insert a submenu of
  // clipboard history descriptors; otherwise, insert a menu option to trigger
  // the clipboard history menu.
  // NOTE: The string ID is reused as the command ID when inserting a menu item.
  // It is because the Ash version of this class does not have access to Chrome
  // code where the IDC commands are defined.
  if (chromeos::features::IsClipboardHistoryRefreshEnabled()) {
    // `submenu_model_` is a class member. Therefore, it is safe to use `this`
    // pointer in the callback.
    submenu_model_ = chromeos::clipboard_history::ClipboardHistorySubmenuModel::
        CreateClipboardHistorySubmenuModel(
            crosapi::mojom::ClipboardHistoryControllerShowSource::
                kTextfieldContextSubmenu,
            base::BindRepeating(
                &ViewsTextServicesContextMenuLacros::ShowClipboardHistoryMenu,
                base::Unretained(this)));
    menu->InsertSubMenuWithStringIdAt(target_index, clipboard_history_string_id,
                                      clipboard_history_string_id,
                                      submenu_model_.get());
  } else {
    menu->InsertItemWithStringIdAt(target_index, clipboard_history_string_id,
                                   clipboard_history_string_id);
  }
}

ViewsTextServicesContextMenuLacros::~ViewsTextServicesContextMenuLacros() =
    default;

bool ViewsTextServicesContextMenuLacros::GetAcceleratorForCommandId(
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

bool ViewsTextServicesContextMenuLacros::IsCommandIdChecked(
    int command_id) const {
  if (command_id == GetClipboardHistoryStringId()) {
    return true;
  }

  return ViewsTextServicesContextMenuBase::IsCommandIdChecked(command_id);
}

bool ViewsTextServicesContextMenuLacros::IsCommandIdEnabled(
    int command_id) const {
  if (command_id == GetClipboardHistoryStringId()) {
    // If the clipboard history refresh feature is enabled, enable the clipboard
    // history command id if there are clipboard history item descriptors.
    return IsClipboardHistoryLacrosServiceAvailable() &&
           (chromeos::features::IsClipboardHistoryRefreshEnabled()
                ? !chromeos::clipboard_history::QueryItemDescriptors().empty()
                : !IsClipboardHistoryEmpty());
  }

  return ViewsTextServicesContextMenuBase::IsCommandIdEnabled(command_id);
}

void ViewsTextServicesContextMenuLacros::ExecuteCommand(int command_id,
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

bool ViewsTextServicesContextMenuLacros::SupportsCommand(int command_id) const {
  if (command_id == GetClipboardHistoryStringId()) {
    return true;
  }

  return ViewsTextServicesContextMenuBase::SupportsCommand(command_id);
}

void ViewsTextServicesContextMenuLacros::ShowClipboardHistoryMenu(
    int event_flags) {
  if (IsClipboardHistoryLacrosServiceAvailable()) {
    chromeos::LacrosService::Get()
        ->GetRemote<mojom::ClipboardHistory>()
        ->ShowClipboard(
            client()->GetCaretBounds(), ui::GetMenuSourceType(event_flags),
            mojom::ClipboardHistoryControllerShowSource::kTextfieldContextMenu);
  }
}

}  // namespace crosapi
