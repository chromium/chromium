// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PRINTING_PRINT_PREVIEW_PRINT_PREVIEW_UI_WRAPPER_H_
#define CHROME_BROWSER_CHROMEOS_PRINTING_PRINT_PREVIEW_PRINT_PREVIEW_UI_WRAPPER_H_

#include <stdint.h>

#include "components/printing/common/print.mojom.h"

namespace chromeos {

// Handles webcontent-associated print preview commands. Since webcontents
// cannot be passed to ash, this class serves as a front for handling print
// preview UI-related mojo calls from the renderer process.
class PrintPreviewUiWrapper : public printing::mojom::PrintPreviewUI {
 public:
  PrintPreviewUiWrapper() = default;
  PrintPreviewUiWrapper(const PrintPreviewUiWrapper&) = delete;
  PrintPreviewUiWrapper& operator=(const PrintPreviewUiWrapper&) = delete;

  ~PrintPreviewUiWrapper() override = default;

  // mojom::PrintPreviewUI::
  void DidPreviewPage(printing::mojom::DidPreviewPageParamsPtr params,
                      int32_t request_id) override;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PRINTING_PRINT_PREVIEW_PRINT_PREVIEW_UI_WRAPPER_H_
