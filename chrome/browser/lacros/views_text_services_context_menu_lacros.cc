// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/views_text_services_context_menu_lacros.h"

#include "chrome/app/chrome_command_ids.h"
#include "chromeos/crosapi/mojom/clipboard_history.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"
#include "ui/base/l10n/l10n_util.h"
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
  const absl::optional<size_t> paste_index =
      menu->GetIndexOfCommandId(ui::TouchEditable::kPaste);

  if (!paste_index.has_value())
    return;

  const size_t target_index = paste_index.value() + 1;
  menu->InsertItemAt(target_index, IDS_APP_SHOW_CLIPBOARD_HISTORY,
                     l10n_util::GetStringUTF16(IDS_APP_SHOW_CLIPBOARD_HISTORY));
}

ViewsTextServicesContextMenuLacros::~ViewsTextServicesContextMenuLacros() =
    default;

bool ViewsTextServicesContextMenuLacros::GetAcceleratorForCommandId(
    int command_id,
    ui::Accelerator* accelerator) const {
  if (command_id == IDS_APP_SHOW_CLIPBOARD_HISTORY) {
    *accelerator = ui::Accelerator(ui::VKEY_V, ui::EF_COMMAND_DOWN);
    return true;
  }

  return ViewsTextServicesContextMenuBase::GetAcceleratorForCommandId(
      command_id, accelerator);
}

bool ViewsTextServicesContextMenuLacros::IsCommandIdChecked(
    int command_id) const {
  if (command_id == IDS_APP_SHOW_CLIPBOARD_HISTORY)
    return true;

  return ViewsTextServicesContextMenuBase::IsCommandIdChecked(command_id);
}

bool ViewsTextServicesContextMenuLacros::IsCommandIdEnabled(
    int command_id) const {
  if (command_id == IDS_APP_SHOW_CLIPBOARD_HISTORY) {
    return IsClipboardHistoryLacrosServiceAvailable() &&
           !IsClipboardHistoryEmpty();
  }

  return ViewsTextServicesContextMenuBase::IsCommandIdEnabled(command_id);
}

void ViewsTextServicesContextMenuLacros::ExecuteCommand(int command_id,
                                                        int event_flags) {
  if (command_id == IDS_APP_SHOW_CLIPBOARD_HISTORY) {
    if (!IsClipboardHistoryLacrosServiceAvailable())
      return;

    // Calculate the menu source type from `event_flags`.
    ui::MenuSourceType source_type;
    if (event_flags & ui::EF_LEFT_MOUSE_BUTTON) {
      source_type = ui::MENU_SOURCE_MOUSE;
    } else if (event_flags & ui::EF_FROM_TOUCH) {
      source_type = ui::MENU_SOURCE_TOUCH;
    } else {
      source_type = ui::MENU_SOURCE_KEYBOARD;
    }

    chromeos::LacrosService::Get()
        ->GetRemote<mojom::ClipboardHistory>()
        ->ShowClipboard(
            client()->GetCaretBounds(), source_type,
            mojom::ClipboardHistoryControllerShowSource::kTextfieldContextMenu);
    return;
  }

  ViewsTextServicesContextMenuBase::ExecuteCommand(command_id, event_flags);
}

bool ViewsTextServicesContextMenuLacros::SupportsCommand(int command_id) const {
  if (command_id == IDS_APP_SHOW_CLIPBOARD_HISTORY)
    return true;

  return ViewsTextServicesContextMenuBase::SupportsCommand(command_id);
}

}  // namespace crosapi
