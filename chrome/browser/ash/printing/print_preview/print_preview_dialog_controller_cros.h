// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PRINTING_PRINT_PREVIEW_PRINT_PREVIEW_DIALOG_CONTROLLER_CROS_H_
#define CHROME_BROWSER_ASH_PRINTING_PRINT_PREVIEW_PRINT_PREVIEW_DIALOG_CONTROLLER_CROS_H_

#include "base/unguessable_token.h"
#include "components/printing/common/print.mojom.h"

namespace ash {

// For ChromeOS print preview, this is a singleton class that is responsible for
// creation and destruction of print preview dialogs. It maintains a 1:1
// relationship between the base::UnguessableToken representation of the
// source's Webcontent and its print dialog.
class PrintPreviewDialogControllerCros {
 public:
  PrintPreviewDialogControllerCros() = default;
  PrintPreviewDialogControllerCros(const PrintPreviewDialogControllerCros&) =
      delete;
  PrintPreviewDialogControllerCros& operator=(
      const PrintPreviewDialogControllerCros&) = delete;
  ~PrintPreviewDialogControllerCros() = default;

  // TODO(jimmyxgong): Implement these stubs.
  void CreatePrintPreview(base::UnguessableToken token,
                          printing::mojom::RequestPrintPreviewParams params);
  void RemovePrintPreview(base::UnguessableToken token);
};

}  // namespace ash

#endif  // CHROME_BROWSER_PRINTING_PRINT_PREVIEW_DIALOG_CONTROLLER_H_
