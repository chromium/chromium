// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PRINTING_PRINT_PREVIEW_PRINT_PREVIEW_DIALOG_CONTROLLER_CROS_H_
#define CHROME_BROWSER_ASH_PRINTING_PRINT_PREVIEW_PRINT_PREVIEW_DIALOG_CONTROLLER_CROS_H_

#include <map>

#include "base/observer_list_types.h"
#include "base/unguessable_token.h"
#include "chrome/browser/ui/webui/ash/print_preview_cros/print_preview_cros_dialog.h"
#include "components/printing/common/print.mojom.h"

namespace ash {

// For ChromeOS print preview, this is a singleton class that is responsible for
// creation and destruction of print preview dialogs. It maintains a 1:1
// relationship between the base::UnguessableToken representation of the
// source's Webcontent and its print dialog.
class PrintPreviewDialogControllerCros
    : public printing::print_preview::PrintPreviewCrosDialog ::
          PrintPreviewCrosDialogObserver {
 public:
  // Observer to inform clients that a print dialog has been closed and no
  // longer tracked by PrintPreviewDialogControllerCros.
  class DialogControllerObserver : public base::CheckedObserver {
   public:
    ~DialogControllerObserver() override = default;
    virtual void OnDialogClosed(const base::UnguessableToken& token) = 0;
  };

  PrintPreviewDialogControllerCros();
  PrintPreviewDialogControllerCros(const PrintPreviewDialogControllerCros&) =
      delete;
  PrintPreviewDialogControllerCros& operator=(
      const PrintPreviewDialogControllerCros&) = delete;
  ~PrintPreviewDialogControllerCros() override;

  void AddObserver(DialogControllerObserver* observer);
  void RemoveObserver(DialogControllerObserver* observer);

  // True if the print preview dialog was successfully created.
  // `token` refers to the ID of the webcontent requesting a print dialog.
  printing::print_preview::PrintPreviewCrosDialog*
  GetOrCreatePrintPreviewDialog(
      base::UnguessableToken token,
      ::printing::mojom::RequestPrintPreviewParams params);

  printing::print_preview::PrintPreviewCrosDialog* CreatePrintPreviewDialog(
      base::UnguessableToken token,
      ::printing::mojom::RequestPrintPreviewParams params);

  void RemovePrintPreviewDialog(base::UnguessableToken token);

  // printing::print_preview::PrintPreviewCrosDialogObserver:
  void OnDialogClosed(base::UnguessableToken token) override;

  bool HasDialogForToken(base::UnguessableToken token);

 private:
  struct InitiatorData {
    base::UnguessableToken token;
    ::printing::mojom::RequestPrintPreviewParams request_params;
  };

  // 1:1 relationship between a print preview dialog and its initiator data.
  using PrintPreviewDialogMap =
      std::map<printing::print_preview::PrintPreviewCrosDialog*, InitiatorData>;

  printing::print_preview::PrintPreviewCrosDialog*
  GetPrintPreviewDialogForToken(base::UnguessableToken token);

  PrintPreviewDialogMap dialog_initiator_data_map_;
  base::ObserverList<DialogControllerObserver> observer_list_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_PRINTING_PRINT_PREVIEW_PRINT_PREVIEW_DIALOG_CONTROLLER_CROS_H_
