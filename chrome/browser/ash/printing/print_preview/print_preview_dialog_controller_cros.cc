// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/print_preview/print_preview_dialog_controller_cros.h"

#include <map>

#include "base/unguessable_token.h"
#include "chrome/browser/ui/webui/ash/print_preview_cros/print_preview_cros_dialog.h"
#include "components/device_event_log/device_event_log.h"
#include "components/printing/common/print.mojom.h"

using ash::printing::print_preview::PrintPreviewCrosDialog;
using ::printing::mojom::RequestPrintPreviewParams;

namespace ash {

PrintPreviewDialogControllerCros::PrintPreviewDialogControllerCros() = default;
PrintPreviewDialogControllerCros::~PrintPreviewDialogControllerCros() = default;

void PrintPreviewDialogControllerCros::AddObserver(
    DialogControllerObserver* observer) {
  observer_list_.AddObserver(observer);
}

void PrintPreviewDialogControllerCros::RemoveObserver(
    DialogControllerObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

PrintPreviewCrosDialog*
PrintPreviewDialogControllerCros::GetOrCreatePrintPreviewDialog(
    base::UnguessableToken token,
    RequestPrintPreviewParams params) {
  PrintPreviewCrosDialog* found_dialog = GetPrintPreviewDialogForToken(token);
  if (found_dialog) {
    // The token already has a dialog associated with it, don't attempt to open
    // a new print dialog.
    return found_dialog;
  }

  return CreatePrintPreviewDialog(token, params);
}

PrintPreviewCrosDialog*
PrintPreviewDialogControllerCros::CreatePrintPreviewDialog(
    base::UnguessableToken token,
    RequestPrintPreviewParams params) {
  PrintPreviewCrosDialog* dialog = PrintPreviewCrosDialog::ShowDialog(token);
  DCHECK(dialog);
  dialog_initiator_data_map_[dialog] = {token, params};
  dialog->AddObserver(this);
  return dialog;
  // TODO(jimmyxgong): Add dialog to task manager.
}

void PrintPreviewDialogControllerCros::RemovePrintPreviewDialog(
    base::UnguessableToken token) {
  PrintPreviewCrosDialog* found_dialog = GetPrintPreviewDialogForToken(token);
  if (!found_dialog) {
    PRINTER_LOG(ERROR)
        << "Attempted to remove non-existent print preview with token: "
        << token.ToString();
    return;
  }

  found_dialog->RemoveObserver(this);
  dialog_initiator_data_map_.erase(found_dialog);
}

void PrintPreviewDialogControllerCros::OnDialogClosed(
    base::UnguessableToken token) {
  RemovePrintPreviewDialog(token);

  for (auto& observer : observer_list_) {
    observer.OnDialogClosed(token);
  }
}

bool PrintPreviewDialogControllerCros::HasDialogForToken(
    base::UnguessableToken token) {
  return GetPrintPreviewDialogForToken(token);
}

PrintPreviewCrosDialog*
PrintPreviewDialogControllerCros::GetPrintPreviewDialogForToken(
    base::UnguessableToken token) {
  // Search for the dialog that corresponds to the relevant initiator token.
  for (const auto& it : dialog_initiator_data_map_) {
    if (token == it.second.token) {
      return it.first;
    }
  }
  return nullptr;
}

}  // namespace ash
